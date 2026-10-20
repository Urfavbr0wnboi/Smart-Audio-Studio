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

/** Writes `audio` to a 24-bit WAV file, padding or dropping channels to `numChannels`. */
inline bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate, int numChannels)
{
    file.deleteFile();
    file.getParentDirectory().createDirectory();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    juce::AudioBuffer<float> channels (numChannels, audio.getNumSamples());

    for (int ch = 0; ch < numChannels; ++ch)
        channels.copyFrom (ch, 0, audio, std::min (ch, audio.getNumChannels() - 1), 0, audio.getNumSamples());

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate)
                                                                              .withNumChannels (numChannels)
                                                                              .withBitsPerSample (24));
    return writer != nullptr && writer->writeFromAudioSampleBuffer (channels, 0, channels.getNumSamples());
}

/** Decodes a whole file, or returns an empty buffer. */
inline juce::AudioBuffer<float> readAudio (const juce::File& file, double* sampleRate = nullptr)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return {};

    juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);

    if (sampleRate != nullptr)
        *sampleRate = reader->sampleRate;

    return audio;
}

} // namespace sas::test
