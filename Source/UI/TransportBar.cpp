#include "TransportBar.h"
#include "StudioLookAndFeel.h"

namespace sas
{

IconButton::IconButton (const juce::String& name, Icon i)
    : juce::Button (name), icon (i)
{
    setTooltip (name);
    setWantsKeyboardFocus (false);
}

void IconButton::setIcon (Icon newIcon)
{
    if (icon != newIcon)
    {
        icon = newIcon;
        repaint();
    }
}

void IconButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    getLookAndFeel().drawButtonBackground (g, *this, Theme::panelRaised, isHighlighted, isDown);

    const auto area = getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 14.0f);
    juce::Path path;

    switch (icon)
    {
        case Icon::play:
            path.addTriangle (area.getTopLeft(), area.getBottomLeft(), { area.getRight(), area.getCentreY() });
            break;

        case Icon::pause:
            path.addRectangle (area.withWidth (5.0f));
            path.addRectangle (area.withTrimmedLeft (9.0f));
            break;

        case Icon::stop:
            path.addRectangle (area.reduced (1.0f));
            break;

        case Icon::toStart:
            path.addRectangle (area.withWidth (3.0f));
            path.addTriangle ({ area.getX() + 3.0f, area.getCentreY() }, area.getTopRight(), area.getBottomRight());
            break;
    }

    g.setColour (icon == Icon::pause ? Theme::accent : Theme::text);
    g.fillPath (path);
}

//==============================================================================
TransportBar::TransportBar (MixEngine& m) : mixer (m)
{
    toStartButton.onClick = [this] { returnToStart(); };
    playButton.onClick    = [this] { togglePlayback(); };
    stopButton.onClick    = [this] { stop(); };

    clock.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::plain));
    clock.setJustificationType (juce::Justification::centred);
    clock.setColour (juce::Label::backgroundColourId, Theme::background);
    clock.setColour (juce::Label::textColourId, Theme::accent);
    clock.setTooltip ("Playhead position (minutes:seconds.milliseconds)");

    addTrackButton.setTooltip ("Add an empty track");
    importButton.setTooltip ("Choose audio files to place on the selected track at the playhead");
    browserButton.setTooltip ("Show or hide the file browser");
    mixerButton.setTooltip ("Show or hide the mixer");
    settingsButton.setTooltip ("Choose which sound card and microphone to use");

    addTrackButton.onClick = [this] { if (onAddTrack) onAddTrack(); };
    importButton.onClick   = [this] { if (onImport) onImport(); };
    settingsButton.onClick = [this] { if (onShowSettings) onShowSettings(); };

    for (auto* toggle : { &browserButton, &mixerButton })
        toggle->setClickingTogglesState (true);

    browserButton.onClick = [this] { if (onBrowserToggled) onBrowserToggled (browserButton.getToggleState()); };
    mixerButton.onClick   = [this] { if (onMixerToggled)   onMixerToggled   (mixerButton.getToggleState()); };

    statusLabel.setColour (juce::Label::textColourId, Theme::textDim);
    deviceLabel.setColour (juce::Label::textColourId, Theme::textDim);
    deviceLabel.setJustificationType (juce::Justification::centredRight);
    deviceLabel.setMinimumHorizontalScale (0.6f);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &toStartButton, &playButton, &stopButton, &clock, &addTrackButton, &importButton,
             &browserButton, &mixerButton, &settingsButton, &statusLabel, &deviceLabel })
    {
        c->setWantsKeyboardFocus (false);
        addAndMakeVisible (c);
    }

    startTimerHz (20);
    timerCallback();
}

void TransportBar::setPanelToggles (bool browserVisible, bool mixerVisible)
{
    browserButton.setToggleState (browserVisible, juce::dontSendNotification);
    mixerButton.setToggleState (mixerVisible, juce::dontSendNotification);
}

void TransportBar::showMessage (const juce::String& message, bool isError)
{
    statusLabel.setText (message, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, isError ? Theme::error : Theme::text);
    messageExpiryTime = juce::Time::getMillisecondCounter() + (isError ? 8000u : 4000u);
}

void TransportBar::setDeviceDescription (const juce::String& description, bool isProblem)
{
    deviceLabel.setText (description, juce::dontSendNotification);
    deviceLabel.setColour (juce::Label::textColourId, isProblem ? Theme::error : Theme::textDim);
}

void TransportBar::togglePlayback()
{
    if (mixer.isPlaying())
        mixer.pause();
    else
        mixer.play();

    refreshPlayButton();
}

void TransportBar::stop()
{
    // Pressing stop while already stopped returns to the start, as in most DAWs.
    if (! mixer.isPlaying())
        mixer.setPosition (0);

    mixer.pause();
    refreshPlayButton();
}

void TransportBar::returnToStart()
{
    mixer.setPosition (0);
}

juce::String TransportBar::formatTime (double seconds)
{
    const auto totalMillis = (juce::int64) std::llround (std::max (0.0, seconds) * 1000.0);
    const auto minutes = totalMillis / 60000;
    const auto secs    = (totalMillis / 1000) % 60;
    const auto millis  = totalMillis % 1000;

    return juce::String (minutes).paddedLeft ('0', 2) + ":"
         + juce::String (secs).paddedLeft ('0', 2) + "."
         + juce::String (millis).paddedLeft ('0', 3);
}

void TransportBar::refreshPlayButton()
{
    const auto playing = mixer.isPlaying();
    playButton.setIcon (playing ? IconButton::Icon::pause : IconButton::Icon::play);
    playButton.setTooltip (playing ? "Pause (Space)" : "Play (Space)");
}

void TransportBar::timerCallback()
{
    clock.setText (formatTime (mixer.getPositionSeconds()), juce::dontSendNotification);
    refreshPlayButton();

    if (messageExpiryTime != 0 && juce::Time::getMillisecondCounter() > messageExpiryTime)
    {
        statusLabel.setText ({}, juce::dontSendNotification);
        messageExpiryTime = 0;
    }
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced (10, 9);

    const auto place = [&area] (juce::Component& c, int width, int gapAfter = 6)
    {
        c.setBounds (area.removeFromLeft (width));
        area.removeFromLeft (gapAfter);
    };

    place (toStartButton, 36);
    place (playButton, 36);
    place (stopButton, 36, 10);
    place (clock, 130, 16);
    place (addTrackButton, 80);
    place (importButton, 104, 16);
    place (browserButton, 76);
    place (mixerButton, 64, 16);

    settingsButton.setBounds (area.removeFromRight (120));
    area.removeFromRight (10);

    deviceLabel.setBounds (area.removeFromRight (std::min (300, area.getWidth() / 2)));
    statusLabel.setBounds (area);
}

} // namespace sas
