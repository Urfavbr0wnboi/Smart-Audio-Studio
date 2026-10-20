#include "StudioLookAndFeel.h"

namespace sas
{

juce::Colour Theme::trackColour (int index)
{
    static const juce::Colour palette[] {
        juce::Colour (0xff4fc3f7), juce::Colour (0xffff8a65), juce::Colour (0xff81c784),
        juce::Colour (0xffba68c8), juce::Colour (0xffffd54f), juce::Colour (0xff4db6ac),
        juce::Colour (0xfff06292), juce::Colour (0xff9575cd),
    };

    return palette[(size_t) index % std::size (palette)];
}

//==============================================================================
StudioLookAndFeel::StudioLookAndFeel()
    : juce::LookAndFeel_V4 ({ Theme::background,    // windowBackground
                              Theme::panelRaised,   // widgetBackground
                              Theme::panel,         // menuBackground
                              Theme::outline,       // outline
                              Theme::text,          // defaultText
                              Theme::panelRaised,   // defaultFill
                              Theme::background,    // highlightedText
                              Theme::accent,        // highlightedFill
                              Theme::text })        // menuText
{
    setColour (juce::TextButton::buttonColourId, Theme::panelRaised);
    setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    setColour (juce::TextButton::textColourOffId, Theme::text);
    setColour (juce::TextButton::textColourOnId, Theme::background);
    setColour (juce::Label::textColourId, Theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, Theme::textDim);
    setColour (juce::Slider::thumbColourId, Theme::text);
    setColour (juce::Slider::trackColourId, Theme::accent);
    setColour (juce::Slider::rotarySliderFillColourId, Theme::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, Theme::outline);
    setColour (juce::ScrollBar::thumbColourId, Theme::outline);
    setColour (juce::TreeView::backgroundColourId, Theme::panel);
    setColour (juce::TreeView::selectedItemBackgroundColourId, Theme::accent.withAlpha (0.25f));
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, Theme::accent.withAlpha (0.25f));
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, Theme::text);
    setColour (juce::TooltipWindow::backgroundColourId, Theme::panelRaised);
    setColour (juce::TooltipWindow::textColourId, Theme::text);
    setColour (juce::TooltipWindow::outlineColourId, Theme::outline);
}

void StudioLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                              bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);

    if (isDown)
        colour = colour.brighter (0.2f);
    else if (isHighlighted)
        colour = colour.brighter (0.08f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 4.0f);

    if (! button.getToggleState())
    {
        g.setColour (Theme::outline);
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }
}

juce::Font StudioLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (std::min (14.0f, (float) buttonHeight * 0.55f));
}

void StudioLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    // A mixing-desk fader: a thin slot with a wide, flat cap.
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto slot = juce::Rectangle<float> (4.0f, bounds.getHeight()).withCentre (bounds.getCentre());

    g.setColour (Theme::background);
    g.fillRoundedRectangle (slot, 2.0f);

    g.setColour (slider.findColour (juce::Slider::trackColourId).withAlpha (0.7f));
    g.fillRoundedRectangle (slot.withTop (sliderPos), 2.0f);

    const auto cap = juce::Rectangle<float> (std::min (bounds.getWidth(), 28.0f), 14.0f)
                         .withCentre ({ bounds.getCentreX(), sliderPos });
    g.setColour (Theme::text);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (Theme::background);
    g.drawHorizontalLine ((int) sliderPos, cap.getX() + 4.0f, cap.getRight() - 4.0f);
}

void StudioLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);

    // Draw the arc from the centre of the range, so a pan knob shows which way it leans.
    const auto midAngle = (startAngle + endAngle) / 2.0f;
    const auto arcRadius = radius - 2.0f;

    juce::Path background;
    background.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (background, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (std::abs (angle - midAngle) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             std::min (angle, midAngle), std::max (angle, midAngle), true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto pointer = juce::Line<float>::fromStartAndAngle (centre, radius * 0.6f, angle);
    g.setColour (Theme::text);
    g.drawLine (pointer, 2.0f);
}

} // namespace sas
