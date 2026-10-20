#pragma once

#include "Engine/MixEngine.h"
#include "LevelMeter.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sas
{

/** A flat button that draws a transport symbol instead of text, so it looks
    the same on every platform regardless of which fonts are installed.
*/
class IconButton final : public juce::Button
{
public:
    enum class Icon { toStart, play, pause, stop, record };

    IconButton (const juce::String& name, Icon);

    void setIcon (Icon);

private:
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;

    Icon icon;
};

/** The strip across the top of the window: the File menu, transport controls,
    the playhead clock, project actions, panel toggles and a status line.

    The buttons only report what was pressed; the owner decides what happens,
    because the right response depends on whether a recording is in progress.
*/
class TransportBar final : public juce::Component,
                           private juce::Timer
{
public:
    TransportBar (MixEngine&, ChannelParams& inputLevels);

    std::function<void()> onPlayPause, onStop, onReturnToStart, onRecord;
    std::function<void()> onAddTrack, onImport, onShowSettings;
    std::function<void (juce::Component& menuButton)> onShowFileMenu;
    std::function<void (bool)> onBrowserToggled, onMixerToggled;

    /** Shows whether a recording is in progress. */
    void setRecording (bool isRecording);

    void setPanelToggles (bool browserVisible, bool mixerVisible);

    /** Shows a message in the status area for a few seconds. */
    void showMessage (const juce::String& message, bool isError = false);

    /** Sets the permanent description of the audio device. */
    void setDeviceDescription (const juce::String& description, bool isProblem);

    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::String formatTime (double seconds);

private:
    void timerCallback() override;
    void refreshPlayButton();

    MixEngine& mixer;

    juce::TextButton fileButton { "File" };
    IconButton toStartButton { "Return to start (Home)", IconButton::Icon::toStart };
    IconButton playButton    { "Play (Space)", IconButton::Icon::play };
    IconButton stopButton    { "Stop", IconButton::Icon::stop };
    IconButton recordButton  { "Record (R)", IconButton::Icon::record };
    LevelMeter inputMeter;
    juce::Label clock;

    juce::TextButton addTrackButton { "+ Track" }, importButton { "Import Audio" };
    juce::TextButton browserButton { "Browser" }, mixerButton { "Mixer" }, settingsButton { "Audio Settings" };

    juce::Label statusLabel, deviceLabel;
    juce::uint32 messageExpiryTime = 0;
};

} // namespace sas
