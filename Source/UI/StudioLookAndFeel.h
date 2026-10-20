#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sas
{

/** The colours of the dark, high-contrast studio theme. */
namespace Theme
{
    inline const juce::Colour background   { 0xff141519 };
    inline const juce::Colour panel        { 0xff1c1d22 };
    inline const juce::Colour panelRaised  { 0xff25272e };
    inline const juce::Colour outline      { 0xff34363f };
    inline const juce::Colour text         { 0xffe8e9ed };
    inline const juce::Colour textDim      { 0xff9496a1 };
    inline const juce::Colour accent       { 0xff4fc3f7 };
    inline const juce::Colour mute         { 0xffffa726 };
    inline const juce::Colour solo         { 0xffffe14d };
    inline const juce::Colour error        { 0xffef5350 };
    inline const juce::Colour playhead     { 0xffffffff };

    /** The colour for a track, cycling through a palette chosen to stay distinct on a dark background. */
    juce::Colour trackColour (int index);
}

/** Flat, dark styling for every standard JUCE widget, plus a studio-style fader and pan knob. */
class StudioLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    StudioLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

} // namespace sas
