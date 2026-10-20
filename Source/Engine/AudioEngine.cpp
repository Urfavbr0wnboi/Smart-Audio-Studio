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

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputs, int numOutputs,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
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
