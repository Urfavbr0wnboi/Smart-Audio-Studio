#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace sas
{

/** A compact min/max summary of an audio buffer, used to draw clip waveforms
    without touching every sample on each repaint.
*/
class Waveform
{
public:
    static constexpr int samplesPerBin = 256;

    /** Summarises all channels of `audio` into one min/max lane. */
    explicit Waveform (const juce::AudioBuffer<float>& audio);

    int getNumBins() const noexcept           { return (int) minima.size(); }
    juce::int64 getNumSamples() const noexcept { return numSamples; }

    /** Returns the lowest and highest sample values in [startSample, endSample).
        The result is accurate to the nearest bin. Returns an empty range at zero
        if the span lies outside the audio.
    */
    juce::Range<float> getRange (juce::int64 startSample, juce::int64 endSample) const noexcept;

private:
    std::vector<float> minima, maxima;
    juce::int64 numSamples = 0;
};

} // namespace sas
