#include "Waveform.h"

namespace sas
{

Waveform::Waveform (const juce::AudioBuffer<float>& audio)
    : numSamples (audio.getNumSamples())
{
    const auto numBins = (size_t) ((numSamples + samplesPerBin - 1) / samplesPerBin);
    minima.assign (numBins, 0.0f);
    maxima.assign (numBins, 0.0f);

    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    {
        const auto* data = audio.getReadPointer (ch);

        for (size_t bin = 0; bin < numBins; ++bin)
        {
            const auto start = (int) bin * samplesPerBin;
            const auto count = std::min (samplesPerBin, audio.getNumSamples() - start);
            const auto range = juce::FloatVectorOperations::findMinAndMax (data + start, count);

            minima[bin] = std::min (minima[bin], range.getStart());
            maxima[bin] = std::max (maxima[bin], range.getEnd());
        }
    }
}

juce::Range<float> Waveform::getRange (juce::int64 startSample, juce::int64 endSample) const noexcept
{
    startSample = std::max<juce::int64> (startSample, 0);
    endSample   = std::min (endSample, numSamples);

    if (endSample <= startSample)
        return {};

    const auto firstBin = (size_t) (startSample / samplesPerBin);
    const auto lastBin  = (size_t) ((endSample - 1) / samplesPerBin);

    auto low = minima[firstBin], high = maxima[firstBin];

    for (auto bin = firstBin + 1; bin <= lastBin; ++bin)
    {
        low  = std::min (low,  minima[bin]);
        high = std::max (high, maxima[bin]);
    }

    return { low, high };
}

} // namespace sas
