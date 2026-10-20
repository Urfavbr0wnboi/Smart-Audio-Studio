#include "Recorder.h"

namespace sas
{

namespace
{
    constexpr int bitsPerSample = 24;
    constexpr double fifoSeconds = 4.0;

    float magnitude (const float* data, int numSamples) noexcept
    {
        const auto range = juce::FloatVectorOperations::findMinAndMax (data, numSamples);
        return std::max (-range.getStart(), range.getEnd());
    }
}

Recorder::Recorder()
{
    writerThread.startThread (juce::Thread::Priority::normal);
}

Recorder::~Recorder()
{
    stop();
    writerThread.stopThread (2000);
}

juce::Result Recorder::start (const juce::File& file, double sampleRate, int numChannels)
{
    stop();

    if (numChannels <= 0)
        return juce::Result::fail ("There is no audio input to record from.");

    numChannels = std::min (numChannels, 2);

    if (const auto created = file.getParentDirectory().createDirectory(); created.failed())
        return juce::Result::fail ("Could not create the folder for recordings: " + created.getErrorMessage());

    file.deleteFile();
    auto fileStream = std::make_unique<juce::FileOutputStream> (file);

    if (! fileStream->openedOk())
        return juce::Result::fail ("Could not create " + file.getFullPathName());

    std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);

    juce::WavAudioFormat wav;
    auto fileWriter = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                                                    .withNumChannels (numChannels)
                                                                                    .withBitsPerSample (bitsPerSample));
    if (fileWriter == nullptr)
        return juce::Result::fail ("Could not start writing " + file.getFileName());

    writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (fileWriter.release(), writerThread,
                                                                        (int) (sampleRate * fifoSeconds));
    currentFile = file;
    firstPosition = -1;
    samplesWritten = 0;
    overflowed = false;

    const juce::SpinLock::ScopedLockType lock (writerLock);
    activeWriter = writer.get();
    numWriterChannels = numChannels;
    return juce::Result::ok();
}

std::optional<Recorder::Take> Recorder::stop()
{
    if (writer == nullptr)
        return std::nullopt;

    {
        // Once we hold the lock, the audio thread has finished with the writer.
        const juce::SpinLock::ScopedLockType lock (writerLock);
        activeWriter = nullptr;
    }

    writer.reset();   // flushes the FIFO and closes the file

    Take take;
    take.file = currentFile;
    take.timelineStart = std::max<juce::int64> (0, firstPosition.load());
    take.numSamples = samplesWritten.load();
    take.droppedSamples = overflowed.load();
    return take;
}

void Recorder::process (const float* const* inputs, int numInputs, int numSamples, juce::int64 timelinePosition) noexcept
{
    if (numInputs <= 0 || inputs == nullptr || inputs[0] == nullptr)
        return;

    const auto left = magnitude (inputs[0], numSamples);
    updatePeak (inputLevels.peakLeft, left);
    updatePeak (inputLevels.peakRight, numInputs > 1 && inputs[1] != nullptr ? magnitude (inputs[1], numSamples) : left);

    const juce::SpinLock::ScopedTryLockType lock (writerLock);

    if (! lock.isLocked() || activeWriter == nullptr)
        return;

    // A mono microphone feeding a stereo file is copied to both sides.
    const float* channels[2] {};

    for (int ch = 0; ch < numWriterChannels && ch < 2; ++ch)
    {
        const auto* input = inputs[std::min (ch, numInputs - 1)];
        channels[ch] = input != nullptr ? input : inputs[0];
    }

    if (firstPosition.load() < 0)
        firstPosition = timelinePosition;

    if (activeWriter->write (channels, numSamples))
        samplesWritten += numSamples;
    else
        overflowed = true;
}

} // namespace sas
