#include "ChannelStrip.h"
#include "StudioLookAndFeel.h"

namespace sas
{

namespace
{
    constexpr double faderMinDecibels = -60.0;   // the bottom of the fader is silence
    constexpr double faderMaxDecibels = 6.0;

    float faderDecibelsToGain (double decibels)
    {
        return decibels <= faderMinDecibels ? 0.0f : juce::Decibels::decibelsToGain ((float) decibels);
    }

    double gainToFaderDecibels (float gain)
    {
        return juce::Decibels::gainToDecibels ((double) gain, faderMinDecibels);
    }
}

ChannelStrip::ChannelStrip (Project& p, const Track& track)
    : ChannelStrip (&p, track.id, track.params, *track.params, track.name, Theme::trackColour (track.colourIndex))
{
}

ChannelStrip::ChannelStrip (ChannelParams& masterParams)
    : ChannelStrip (nullptr, 0, nullptr, masterParams, "Master", Theme::text)
{
}

ChannelStrip::ChannelStrip (Project* p, int id, std::shared_ptr<ChannelParams> owner, ChannelParams& channel,
                            const juce::String& name, juce::Colour c)
    : project (p), trackId (id), keepAlive (std::move (owner)), params (channel), colour (c), meter (channel)
{
    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    nameLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (nameLabel);

    panKnob.setRange (-1.0, 1.0);
    panKnob.setValue (params.pan.load(), juce::dontSendNotification);
    panKnob.setDoubleClickReturnValue (true, 0.0);
    panKnob.setTooltip ("Pan: places the sound between the left and right speakers. Double-click to centre.");
    panKnob.onValueChange = [this] { params.pan.store ((float) panKnob.getValue()); };
    addAndMakeVisible (panKnob);

    fader.textFromValueFunction = [] (double dB) { return dB <= faderMinDecibels ? juce::String ("-inf dB")
                                                                                   : juce::String (dB, 1) + " dB"; };
    fader.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.containsIgnoreCase ("inf") ? faderMinDecibels
                                               : text.retainCharacters ("-0123456789.").getDoubleValue();
    };
    fader.setRange (faderMinDecibels, faderMaxDecibels, 0.1);
    fader.setSkewFactorFromMidPoint (-12.0);
    fader.setValue (gainToFaderDecibels (params.gain.load()), juce::dontSendNotification);
    fader.updateText();
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, width - 12, 18);
    fader.setTooltip ("Volume. Double-click to reset to 0 dB.");
    fader.setColour (juce::Slider::trackColourId, colour);
    fader.onValueChange = [this] { params.gain.store (faderDecibelsToGain (fader.getValue())); };
    addAndMakeVisible (fader);

    addAndMakeVisible (meter);

    if (project != nullptr)
    {
        muteButton.setClickingTogglesState (false);
        muteButton.setColour (juce::TextButton::buttonOnColourId, Theme::mute);
        muteButton.setTooltip ("Mute: silence this track");
        muteButton.onClick = [this] { project->setTrackMuted (trackId, ! params.mute.load()); };
        addAndMakeVisible (muteButton);

        soloButton.setColour (juce::TextButton::buttonOnColourId, Theme::solo);
        soloButton.setTooltip ("Solo: hear only this track (and any other soloed tracks)");
        soloButton.onClick = [this] { project->setTrackSoloed (trackId, ! params.solo.load()); };
        addAndMakeVisible (soloButton);

        for (auto* button : { &muteButton, &soloButton })
            button->setWantsKeyboardFocus (false);

        refreshState();
    }

    for (auto* slider : { &panKnob, &fader })
        slider->setWantsKeyboardFocus (false);
}

void ChannelStrip::refreshState()
{
    muteButton.setToggleState (params.mute.load(), juce::dontSendNotification);
    soloButton.setToggleState (params.solo.load(), juce::dontSendNotification);
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Theme::panelRaised);
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds.removeFromTop (4.0f), 2.0f);
}

void ChannelStrip::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (4);

    nameLabel.setBounds (area.removeFromTop (20));
    panKnob.setBounds (area.removeFromTop (40).withSizeKeepingCentre (40, 40));
    area.removeFromTop (4);

    if (project != nullptr)
    {
        auto buttons = area.removeFromBottom (24);
        muteButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 0));
        soloButton.setBounds (buttons.reduced (2, 0));
        area.removeFromBottom (4);
    }

    meter.setBounds (area.removeFromRight (12).withTrimmedBottom (22));
    fader.setBounds (area);
}

} // namespace sas
