#pragma once

#include "Engine/Project.h"
#include "LevelMeter.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sas
{

/** A mixer channel: name, pan knob, fader, meter, and mute/solo buttons.
    The master strip has no mute or solo, and does not belong to a track.
*/
class ChannelStrip final : public juce::Component
{
public:
    /** A strip for one of the project's tracks. */
    ChannelStrip (Project& project, const Track& track);

    /** The strip for the master bus. */
    explicit ChannelStrip (ChannelParams& masterParams);

    int getTrackId() const noexcept { return trackId; }

    /** Updates the mute and solo buttons from the track's current state. */
    void refreshState();

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int width = 96;

private:
    ChannelStrip (Project*, int trackId, std::shared_ptr<ChannelParams> keepAlive, ChannelParams&,
                  const juce::String& name, juce::Colour);

    Project* project;
    int trackId;
    std::shared_ptr<ChannelParams> keepAlive;
    ChannelParams& params;
    juce::Colour colour;

    juce::Label nameLabel;
    juce::Slider panKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider fader   { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    LevelMeter meter;
    juce::TextButton muteButton { "M" }, soloButton { "S" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

} // namespace sas
