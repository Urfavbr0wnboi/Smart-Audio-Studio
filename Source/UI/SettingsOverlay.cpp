#include "SettingsOverlay.h"
#include "StudioLookAndFeel.h"

namespace sas
{

SettingsOverlay::SettingsOverlay (juce::AudioDeviceManager& deviceManager)
    : selector (deviceManager, 0, 2, 1, 2, false, false, true, false)
{
    title.setText ("Audio Settings", juce::dontSendNotification);
    title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (title);

    addAndMakeVisible (selector);

    doneButton.onClick = [this] { close(); };
    addAndMakeVisible (doneButton);

    setWantsKeyboardFocus (true);
}

juce::Rectangle<int> SettingsOverlay::getPanelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (std::min (560, getWidth() - 32),
                                                   std::min (480, getHeight() - 32));
}

void SettingsOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));

    const auto panel = getPanelBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (Theme::outline);
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);
}

void SettingsOverlay::resized()
{
    auto area = getPanelBounds().reduced (16);
    title.setBounds (area.removeFromTop (28));
    doneButton.setBounds (area.removeFromBottom (30).removeFromRight (90));
    area.removeFromBottom (8);
    selector.setBounds (area);
}

void SettingsOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! getPanelBounds().contains (e.getPosition()))
        close();
}

bool SettingsOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }

    return false;
}

void SettingsOverlay::close()
{
    if (onClose)
        onClose();
}

} // namespace sas
