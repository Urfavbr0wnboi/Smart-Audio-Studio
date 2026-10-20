#include "AudioFileLoader.h"

namespace sas
{

juce::Result AudioFileLoader::load (juce::AudioFormatManager& formats, const juce::File& file, DecodedAudio& result)
{
    if (! file.existsAsFile())
        return juce::Result::fail ("The file " + file.getFileName() + " could not be found.");

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return juce::Result::fail (file.getFileName() + " is not an audio format Smart Audio Studio can open.");

    if (reader->sampleRate <= 0.0 || reader->numChannels == 0)
        return juce::Result::fail (file.getFileName() + " does not contain any audio.");

    if ((double) reader->lengthInSamples / reader->sampleRate > maxLengthSeconds)
        return juce::Result::fail (file.getFileName() + " is longer than 30 minutes.");

    const auto numChannels = std::min ((int) reader->numChannels, maxChannels);
    const auto numSamples  = (int) reader->lengthInSamples;

    auto buffer = std::make_shared<juce::AudioBuffer<float>> (numChannels, numSamples);

    if (! reader->read (buffer->getArrayOfWritePointers(), numChannels, 0, numSamples))
        return juce::Result::fail (file.getFileName() + " could not be read. It may be damaged.");

    result.audio = std::move (buffer);
    result.sampleRate = reader->sampleRate;
    return juce::Result::ok();
}

SharedAudio AudioFileLoader::resample (const SharedAudio& source, double sourceRate, double targetRate)
{
    jassert (source != nullptr && sourceRate > 0.0 && targetRate > 0.0);

    if (juce::approximatelyEqual (sourceRate, targetRate))
        return source;

    const auto ratio = sourceRate / targetRate;
    const auto outputLength = (int) std::ceil (source->getNumSamples() / ratio);

    auto output = std::make_shared<juce::AudioBuffer<float>> (source->getNumChannels(), outputLength);

    for (int ch = 0; ch < source->getNumChannels(); ++ch)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.process (ratio,
                              source->getReadPointer (ch),
                              output->getWritePointer (ch),
                              outputLength,
                              source->getNumSamples(),
                              0);
    }

    return output;
}

} // namespace sas
