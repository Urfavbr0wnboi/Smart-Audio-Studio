#pragma once

#include "ChannelParams.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace sas
{

/** Captures audio input to a WAV file.

    The audio thread hands each block of input to process(), which copies it
    into a FIFO without blocking. A background thread empties the FIFO to disk,
    so a slow hard drive can never cause a glitch in playback.
*/
class Recorder
{
public:
    /** What a finished recording contains. */
    struct Take
    {
        juce::File file;
        juce::int64 timelineStart = 0;   // engine position of the first recorded sample
        juce::int64 numSamples = 0;
        bool droppedSamples = false;     // true if the disk could not keep up
    };

    Recorder();
    ~Recorder();

    /** Message thread: opens a new WAV file and starts capturing input into it.
        Files have at most two channels.
    */
    juce::Result start (const juce::File& file, double sampleRate, int numChannels);

    /** Message thread: stops capturing, finishes writing the file and describes it.
        Returns nothing if no recording was in progress.
    */
    std::optional<Take> stop();

    bool isRecording() const noexcept { return writer != nullptr; }

    /** Audio thread: meters the input and, while recording, queues it for writing.
        `timelinePosition` is the engine position at the start of this block.
    */
    void process (const float* const* inputs, int numInputs, int numSamples, juce::int64 timelinePosition) noexcept;

    /** Input peak levels, for a meter that shows whether the microphone is working. */
    ChannelParams& getInputLevels() noexcept { return inputLevels; }

private:
    juce::TimeSliceThread writerThread { "Recording writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;   // message thread only

    juce::SpinLock writerLock;
    juce::AudioFormatWriter::ThreadedWriter* activeWriter = nullptr;   // guarded by writerLock
    int numWriterChannels = 0;                                         // guarded by writerLock

    std::atomic<juce::int64> firstPosition { -1 }, samplesWritten { 0 };
    std::atomic<bool> overflowed { false };
    juce::File currentFile;

    ChannelParams inputLevels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recorder)
};

} // namespace sas
