#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace sas
{

using SharedAudio = std::shared_ptr<const juce::AudioBuffer<float>>;

struct DecodedAudio
{
    SharedAudio audio;
    double sampleRate = 0.0;
};

namespace AudioFileLoader
{
    /** Files longer than this are refused, because clips are held in memory. */
    constexpr double maxLengthSeconds = 30.0 * 60.0;

    /** Files with more channels than this are reduced to their first two. */
    constexpr int maxChannels = 2;

    /** Decodes an entire audio file into memory. */
    juce::Result load (juce::AudioFormatManager& formats, const juce::File& file, DecodedAudio& result);

    /** Converts audio to a different sample rate. Returns `source` itself when the
        rates already match, so no memory is wasted on an identical copy.
    */
    SharedAudio resample (const SharedAudio& source, double sourceRate, double targetRate);
}

} // namespace sas
