#include "Engine/Recorder.h"
#include "TestHelpers.h"

namespace sas
{

class RecorderTests final : public juce::UnitTest
{
public:
    RecorderTests() : juce::UnitTest ("Recorder", "Smart Audio Studio") {}

    void runTest() override
    {
        const juce::TemporaryFile tempFile (".wav");
        const auto file = tempFile.getFile();

        beginTest ("Stopping when not recording does nothing");
        {
            Recorder recorder;
            expect (! recorder.isRecording());
            expect (! recorder.stop().has_value());
        }

        beginTest ("Input is metered even when not recording");
        {
            Recorder recorder;
            auto input = makeInput (2, 64, 0.25f);
            input.setSample (1, 10, -0.5f);
            recorder.process (input.getArrayOfReadPointers(), 2, 64, 0);

            expectEquals (recorder.getInputLevels().peakLeft.load(), 0.25f);
            expectEquals (recorder.getInputLevels().peakRight.load(), 0.5f);
        }

        beginTest ("Recorded blocks are written to the file in order");
        {
            Recorder recorder;
            expect (recorder.start (file, 44100.0, 1).wasOk());
            expect (recorder.isRecording());

            auto first = makeInput (1, 512, 0.25f);
            auto second = makeInput (1, 512, -0.125f);
            recorder.process (first.getArrayOfReadPointers(), 1, 512, 1000);
            recorder.process (second.getArrayOfReadPointers(), 1, 512, 1512);

            const auto take = recorder.stop();
            expect (take.has_value() && ! recorder.isRecording());
            expectEquals (take->file.getFullPathName(), file.getFullPathName());
            expectEquals (take->timelineStart, (juce::int64) 1000);
            expectEquals (take->numSamples, (juce::int64) 1024);
            expect (! take->droppedSamples);

            double sampleRate = 0.0;
            const auto audio = test::readAudio (file, &sampleRate);
            expectEquals (sampleRate, 44100.0);
            expectEquals (audio.getNumChannels(), 1);
            expectEquals (audio.getNumSamples(), 1024);
            expectWithinAbsoluteError (audio.getSample (0, 0), 0.25f, 1.0e-5f);
            expectWithinAbsoluteError (audio.getSample (0, 1023), -0.125f, 1.0e-5f);
        }

        beginTest ("A mono input fills both sides of a stereo recording");
        {
            Recorder recorder;
            expect (recorder.start (file, 48000.0, 2).wasOk());

            auto input = makeInput (1, 256, 0.5f);
            recorder.process (input.getArrayOfReadPointers(), 1, 256, 0);
            recorder.stop();

            const auto audio = test::readAudio (file);
            expectEquals (audio.getNumChannels(), 2);
            expectWithinAbsoluteError (audio.getSample (1, 100), 0.5f, 1.0e-5f);
        }

        beginTest ("Recording needs an input");
        {
            Recorder recorder;
            expect (recorder.start (file, 44100.0, 0).failed());
            expect (! recorder.isRecording());
        }
    }

private:
    static juce::AudioBuffer<float> makeInput (int numChannels, int numSamples, float value)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), value, numSamples);

        return buffer;
    }
};

static RecorderTests recorderTests;

} // namespace sas
