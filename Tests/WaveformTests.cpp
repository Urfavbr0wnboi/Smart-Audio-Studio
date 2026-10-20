#include "Engine/Waveform.h"

namespace sas
{

class WaveformTests final : public juce::UnitTest
{
public:
    WaveformTests() : juce::UnitTest ("Waveform", "Smart Audio Studio") {}

    void runTest() override
    {
        const auto numSamples = Waveform::samplesPerBin * 4;
        juce::AudioBuffer<float> audio (2, numSamples);
        audio.clear();
        audio.setSample (0, 10, 0.8f);                                // first bin, left
        audio.setSample (1, Waveform::samplesPerBin * 2 + 5, -0.6f);  // third bin, right

        const Waveform waveform (audio);

        beginTest ("Bins cover the whole buffer");
        expectEquals (waveform.getNumBins(), 4);
        expectEquals (waveform.getNumSamples(), (juce::int64) numSamples);

        beginTest ("Ranges combine both channels");
        expectEquals (waveform.getRange (0, numSamples).getEnd(), 0.8f);
        expectEquals (waveform.getRange (0, numSamples).getStart(), -0.6f);

        beginTest ("Ranges only include the requested bins");
        const auto secondBin = waveform.getRange (Waveform::samplesPerBin, Waveform::samplesPerBin * 2);
        expectEquals (secondBin.getStart(), 0.0f);
        expectEquals (secondBin.getEnd(), 0.0f);

        beginTest ("Spans outside the audio are empty");
        expect (waveform.getRange (numSamples, numSamples + 100).isEmpty());
        expect (waveform.getRange (-200, -100).isEmpty());

        beginTest ("A partial final bin is summarised");
        juce::AudioBuffer<float> shortAudio (1, Waveform::samplesPerBin + 3);
        shortAudio.clear();
        shortAudio.setSample (0, Waveform::samplesPerBin + 2, 0.5f);
        const Waveform shortWaveform (shortAudio);
        expectEquals (shortWaveform.getNumBins(), 2);
        expectEquals (shortWaveform.getRange (Waveform::samplesPerBin, Waveform::samplesPerBin + 3).getEnd(), 0.5f);
    }
};

static WaveformTests waveformTests;

} // namespace sas
