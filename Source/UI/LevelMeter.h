#pragma once

#include "Engine/ChannelParams.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace sas
{

/** A stereo peak meter that reads the levels the audio thread stores in a ChannelParams. */
class LevelMeter final : public juce::Component,
                         private juce::Timer
{
public:
    /** `params` must outlive the meter. */
    explicit LevelMeter (ChannelParams& params);

    void paint (juce::Graphics&) override;

    static constexpr float minDecibels = -60.0f;
    static constexpr float maxDecibels = 6.0f;

    /** Maps a level in decibels to 0..1 up the meter. */
    static float decibelsToProportion (float decibels) noexcept;

private:
    void timerCallback() override;

    ChannelParams& params;
    float levelLeft = 0.0f, levelRight = 0.0f;
    float holdLeft = 0.0f, holdRight = 0.0f;
    int holdTicksLeft = 0, holdTicksRight = 0;
};

} // namespace sas
