#pragma once

#include "Engine/AudioEngine.h"
#include "ProjectDocument.h"
#include "UI/BrowserPanel.h"
#include "UI/MixerView.h"
#include "UI/SettingsOverlay.h"
#include "UI/TimelineView.h"
#include "UI/TransportBar.h"

namespace sas
{

/** The single main window: transport bar on top, browser on the left, the
    timeline in the middle and the mixer docked along the bottom. Nothing
    needed for everyday work opens in a separate window.
*/
class MainComponent final : public juce::Component,
                            public juce::DragAndDropContainer,
                            private juce::ChangeListener,
                            private juce::FocusChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    /** Called with the text for the window's title bar. */
    std::function<void (const juce::String&)> onTitleChanged;
    void refreshTitle();

    /** Opens a project, or places audio files on new tracks, e.g. files passed on the command line. */
    void openFiles (const juce::Array<juce::File>& files);

    /** Asks about unsaved changes, then calls `quit` unless the user cancels. */
    void requestQuit (std::function<void()> quit);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void globalFocusChanged (juce::Component* focusedComponent) override;
    void refreshDeviceStatus();
    void chooseFilesToImport();
    void showFileMenu (juce::Component& button);
    void setSettingsVisible (bool);

    void togglePlayback();
    void stop();
    void toggleRecording();
    void stopRecording();

    AudioEngine engine;
    ProjectDocument document { engine };

    TransportBar transport { engine.getMixer(), engine.getInputLevels() };
    BrowserPanel browser { engine.getProject().getFormatManager() };
    TimelineView timeline { engine.getProject(), engine.getMixer() };
    MixerView mixer { engine.getProject(), engine.getMixer() };
    std::unique_ptr<SettingsOverlay> settings;

    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> importChooser;

    bool browserVisible = true, mixerVisible = true;
    int trackCreatedForRecording = 0;

    static constexpr int transportHeight = 52;
    static constexpr int browserWidth = 250;
    static constexpr int mixerHeight = 260;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace sas
