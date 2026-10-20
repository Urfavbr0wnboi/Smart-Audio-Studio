#pragma once

#include "Engine/AudioFileLoader.h"

namespace sas::test
{

/** A buffer in which every sample of every channel holds `value`. */
inline SharedAudio makeConstantAudio (int numChannels, int numSamples, float value)
{
    auto buffer = std::make_shared<juce::AudioBuffer<float>> (numChannels, numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::fill (buffer->getWritePointer (ch), value, numSamples);

    return buffer;
}

} // namespace sas::test
