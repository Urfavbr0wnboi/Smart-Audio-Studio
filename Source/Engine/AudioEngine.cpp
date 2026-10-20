#include "AudioEngine.h"

namespace sas
{

AudioEngine::AudioEngine()
{
    mixBuffer.setSize (2, 512);
    deviceManager.addAudioCallback (this);

    // Frees old playback graphs on the message thread.
    startTimer (500);
}

AudioEngine::~AudioEngine()
{
    recorder.stop();
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

juce::String AudioEngine::openDefaultDevice()
{
    // Inputs are opened when recording arrives; asking for them now would make
    // some systems prompt for microphone access before the user needs it.
    return deviceManager.initialiseWithDefaultDevices (0, 2);
}

bool AudioEngine::hasOutputDevice() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr;
}

juce::String AudioEngine::getDeviceDescription() const
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return "No audio output. Open Audio Settings to choose one.";

    const auto sampleRate = device->getCurrentSampleRate();
    const auto latencySamples = device->getOutputLatencyInSamples() + device->getCurrentBufferSizeSamples();

    return device->getName()
         + "  |  " + juce::String (sampleRate / 1000.0, 1) + " kHz"
         + "  |  " + juce::String (1000.0 * latencySamples / sampleRate, 1) + " ms";
}

juce::Result AudioEngine::openDefaultInput()
{
    const auto countInputs = [this]
    {
        auto* device = deviceManager.getCurrentAudioDevice();
        return device != nullptr ? device->getActiveInputChannels().countNumberOfSetBits() : 0;
    };

    if (countInputs() > 0)
        return juce::Result::ok();

    if (deviceManager.getCurrentAudioDevice() == nullptr)
        return juce::Result::fail ("No audio device is open. Choose one in Audio Settings.");

    auto setup = deviceManager.getAudioDeviceSetup();

    if (setup.inputDeviceName.isEmpty())
    {
        if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        {
            const auto names = type->getDeviceNames (true);
            const auto index = type->getDefaultDeviceIndex (true);
            setup.inputDeviceName = names[juce::isPositiveAndBelow (index, names.size()) ? index : 0];
        }
    }

    if (setup.inputDeviceName.isEmpty())
        return juce::Result::fail ("No microphone or audio input was found. Plug one in, or choose one in Audio Settings.");

    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    setup.inputChannels.setRange (0, 2, true);

    if (const auto error = deviceManager.setAudioDeviceSetup (setup, true); error.isNotEmpty())
        return juce::Result::fail ("Could not open " + setup.inputDeviceName + ": " + error);

    if (countInputs() == 0)
        return juce::Result::fail (setup.inputDeviceName + " has no inputs to record from.");

    return juce::Result::ok();
}

juce::Result AudioEngine::startRecording (int trackId, const juce::File& folder)
{
    if (isRecording())
        return juce::Result::ok();

    if (project.findTrack (trackId) == nullptr)
        return juce::Result::fail ("That track no longer exists.");

    if (const auto opened = openDefaultInput(); opened.failed())
        return opened;

    auto* device = deviceManager.getCurrentAudioDevice();
    const auto numInputs = device->getActiveInputChannels().countNumberOfSetBits();
    const auto file = folder.getNonexistentChildFile ("Recording " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S"),
                                                      ".wav", false);

    // Start playback before capturing, so every recorded block is tagged with a
    // playhead position that is actually moving.
    const auto wasPlaying = mixer.isPlaying();
    recordingStartSeconds = mixer.getPositionSeconds();
    mixer.play();

    if (const auto started = recorder.start (file, device->getCurrentSampleRate(), std::min (numInputs, 2)); started.failed())
    {
        if (! wasPlaying)
            mixer.pause();

        return started;
    }

    // Sound reaches the input later than the playhead says, by the time it takes
    // to come out of the speakers and back in. Remember how much to shift the take.
    recordingLatencySamples = device->getInputLatencyInSamples() + device->getOutputLatencyInSamples();
    recordingTrackId = trackId;
    return juce::Result::ok();
}

juce::Result AudioEngine::stopRecording()
{
    const auto take = recorder.stop();
    mixer.pause();

    if (! take.has_value())
        return juce::Result::ok();

    const auto trackId = std::exchange (recordingTrackId, 0);

    if (take->numSamples == 0)
    {
        take->file.deleteFile();
        return juce::Result::fail ("Nothing was recorded. Check that your microphone is selected in Audio Settings.");
    }

    const auto targetTrack = project.findTrack (trackId) != nullptr ? trackId : project.addTrack ("Recording");
    const auto sampleRate = mixer.getSampleRate();
    const auto shiftedStart = take->timelineStart - recordingLatencySamples;

    int clipId = 0;
    const auto added = project.addClipFromFile (targetTrack, take->file, (double) std::max<juce::int64> (0, shiftedStart) / sampleRate, &clipId);

    if (added.failed())
        return added;

    // If recording began at the very start, the latency shift would put the
    // beginning of the take before zero, so skip that part of the audio instead.
    if (shiftedStart < 0)
        if (const auto* clip = project.findClip (targetTrack, clipId))
            project.setClipSourceRange (targetTrack, clipId, -shiftedStart, clip->sourceLength + shiftedStart);

    if (take->droppedSamples)
        return juce::Result::fail ("The take was saved, but the computer could not keep up and some audio was lost.");

    return juce::Result::ok();
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const auto sampleRate = device->getCurrentSampleRate();
    const auto blockSize = device->getCurrentBufferSizeSamples();

    mixer.prepare (sampleRate, blockSize);
    mixBuffer.setSize (2, blockSize, false, false, true);

    // Clips must be converted to the new rate, which allocates, so do it on the message thread.
    deviceSampleRate.store (sampleRate);
    triggerAsyncUpdate();
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                                    float* const* outputs, int numOutputs,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    // Record first, tagged with the playhead position this block will play at.
    recorder.process (inputs, numInputs, numSamples, mixer.getPosition());

    for (int offset = 0; offset < numSamples;)
    {
        const auto chunk = std::min (numSamples - offset, mixBuffer.getNumSamples());
        mixer.process (mixBuffer, chunk);

        const auto* left  = mixBuffer.getReadPointer (0);
        const auto* right = mixBuffer.getReadPointer (1);

        for (int ch = 0; ch < numOutputs; ++ch)
        {
            auto* out = outputs[ch];

            if (out == nullptr)
                continue;

            out += offset;

            if (numOutputs == 1)
            {
                juce::FloatVectorOperations::copy (out, left, chunk);
                juce::FloatVectorOperations::add (out, right, chunk);
                juce::FloatVectorOperations::multiply (out, 0.5f, chunk);
            }
            else if (ch < 2)
            {
                juce::FloatVectorOperations::copy (out, ch == 0 ? left : right, chunk);
            }
            else
            {
                juce::FloatVectorOperations::clear (out, chunk);
            }
        }

        offset += chunk;
    }
}

void AudioEngine::handleAsyncUpdate()
{
    project.setSampleRate (deviceSampleRate.load());
}

void AudioEngine::timerCallback()
{
    mixer.releaseUnusedGraphs();
}

} // namespace sas
