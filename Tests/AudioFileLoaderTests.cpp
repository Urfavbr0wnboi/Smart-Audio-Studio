#include "Engine/AudioFileLoader.h"
#include "TestHelpers.h"

namespace sas
{

class AudioFileLoaderTests final : public juce::UnitTest
{
public:
    AudioFileLoaderTests() : juce::UnitTest ("AudioFileLoader", "Smart Audio Studio") {}

    void runTest() override
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        const juce::TemporaryFile tempWav (".wav");

        beginTest ("A WAV file is decoded with its sample rate");
        {
            juce::AudioBuffer<float> ramp (2, 4800);

            for (int i = 0; i < ramp.getNumSamples(); ++i)
            {
                ramp.setSample (0, i, (float) i / (float) ramp.getNumSamples());
                ramp.setSample (1, i, -0.25f);
            }

            writeWav (tempWav.getFile(), ramp, 48000.0, 3);

            DecodedAudio decoded;
            const auto result = AudioFileLoader::load (formats, tempWav.getFile(), decoded);

            expect (result.wasOk(), result.getErrorMessage());
            expectEquals (decoded.sampleRate, 48000.0);
            expectEquals (decoded.audio->getNumChannels(), AudioFileLoader::maxChannels);
            expectEquals (decoded.audio->getNumSamples(), 4800);
            expectWithinAbsoluteError (decoded.audio->getSample (0, 2400), 0.5f, 1.0e-4f);
            expectWithinAbsoluteError (decoded.audio->getSample (1, 100), -0.25f, 1.0e-4f);
            expectEquals (decoded.audio->getSample (0, 0), ramp.getSample (0, 0));
        }

        beginTest ("Missing and unreadable files fail with a message");
        {
            DecodedAudio decoded;
            const auto missing = AudioFileLoader::load (formats, tempWav.getFile().getSiblingFile ("does-not-exist.wav"), decoded);
            expect (missing.failed());
            expect (missing.getErrorMessage().contains ("could not be found"));

            const juce::TemporaryFile notAudio (".wav");
            notAudio.getFile().replaceWithText ("this is not audio");
            const auto garbage = AudioFileLoader::load (formats, notAudio.getFile(), decoded);
            expect (garbage.failed());
            expect (decoded.audio == nullptr);
        }

        beginTest ("Resampling to the same rate shares the buffer");
        {
            const auto audio = test::makeConstantAudio (1, 1000, 0.5f);
            expect (AudioFileLoader::resample (audio, 44100.0, 44100.0) == audio);
        }

        beginTest ("Resampling changes the length and keeps the level");
        {
            const auto audio = test::makeConstantAudio (2, 44100, 0.5f);
            const auto converted = AudioFileLoader::resample (audio, 44100.0, 48000.0);

            expectEquals (converted->getNumChannels(), 2);
            expectEquals (converted->getNumSamples(), 48000);
            expectWithinAbsoluteError (converted->getSample (1, 24000), 0.5f, 1.0e-4f);
        }
    }

private:
    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate, int numChannels)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

        juce::AudioBuffer<float> multiChannel (numChannels, audio.getNumSamples());

        for (int ch = 0; ch < numChannels; ++ch)
            multiChannel.copyFrom (ch, 0, audio, std::min (ch, audio.getNumChannels() - 1), 0, audio.getNumSamples());

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                                                  .withNumChannels (numChannels)
                                                                                  .withBitsPerSample (24));
        expect (writer != nullptr);

        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (multiChannel, 0, multiChannel.getNumSamples());
    }
};

static AudioFileLoaderTests audioFileLoaderTests;

} // namespace sas
