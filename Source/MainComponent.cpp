#include "MainComponent.h"
#include "Engine/ProjectFile.h"
#include "UI/StudioLookAndFeel.h"

namespace sas
{

namespace
{
    enum FileMenuItem { newItem = 1, openItem, saveItem, saveAsItem };
}

MainComponent::MainComponent()
{
    addAndMakeVisible (transport);
    addAndMakeVisible (browser);
    addAndMakeVisible (timeline);
    addAndMakeVisible (mixer);

    auto& project = engine.getProject();
    const auto showMessage = [this] (const juce::String& message, bool isError) { transport.showMessage (message, isError); };

    transport.setPanelToggles (browserVisible, mixerVisible);
    transport.onPlayPause      = [this] { togglePlayback(); };
    transport.onStop           = [this] { stop(); };
    transport.onReturnToStart  = [this] { if (! engine.isRecording()) engine.getMixer().setPosition (0); };
    transport.onRecord         = [this] { toggleRecording(); };
    transport.onShowFileMenu   = [this] (juce::Component& button) { showFileMenu (button); };
    transport.onAddTrack       = [&project] { project.addTrack(); };
    transport.onImport         = [this] { chooseFilesToImport(); };
    transport.onShowSettings   = [this] { setSettingsVisible (true); };
    transport.onBrowserToggled = [this] (bool shown) { browserVisible = shown; resized(); };
    transport.onMixerToggled   = [this] (bool shown) { mixerVisible = shown; resized(); };

    browser.onFileActivated = [this] (const juce::File& file) { timeline.importFilesAtPlayhead ({ file }); };
    timeline.onStatusMessage = showMessage;
    document.onMessage = showMessage;
    document.onStateChanged = [this] { refreshTitle(); };
    document.onBeforeReplacingProject = [this] { if (engine.isRecording()) stopRecording(); };

    engine.getDeviceManager().addChangeListener (this);
    juce::Desktop::getInstance().addFocusChangeListener (this);

    if (const auto error = engine.openDefaultDevice(); error.isNotEmpty())
        transport.showMessage ("Could not open the audio device: " + error, true);

    refreshDeviceStatus();

    // Wait until the window is on screen before asking anything.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this)]
    {
        if (safeThis != nullptr)
            safeThis->document.offerToRecoverAutosave();
    });

    setWantsKeyboardFocus (true);
    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    juce::Desktop::getInstance().removeFocusChangeListener (this);
    engine.getDeviceManager().removeChangeListener (this);
}

void MainComponent::openFiles (const juce::Array<juce::File>& files)
{
    juce::Array<juce::File> audioFiles;

    for (const auto& file : files)
    {
        if (file.hasFileExtension (ProjectFile::fileExtension))
            document.open (file);
        else
            audioFiles.add (file);
    }

    if (! audioFiles.isEmpty())
        timeline.importFiles (audioFiles, 0, 0.0);
}

void MainComponent::requestQuit (std::function<void()> quit)
{
    if (engine.isRecording())
        stopRecording();

    document.runAfterCheckingForUnsavedChanges (std::move (quit));
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    transport.setBounds (area.removeFromTop (transportHeight));

    mixer.setVisible (mixerVisible);
    if (mixerVisible)
        mixer.setBounds (area.removeFromBottom (std::min (mixerHeight, area.getHeight() / 2)));

    browser.setVisible (browserVisible);
    if (browserVisible)
        browser.setBounds (area.removeFromLeft (std::min (browserWidth, area.getWidth() / 3)));

    timeline.setBounds (area);

    if (settings != nullptr)
        settings->setBounds (getLocalBounds());
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto command = key.getModifiers().isCommandDown();
    const auto shift = key.getModifiers().isShiftDown();
    const auto keyCode = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode());

    if (command && keyCode == 'S')
    {
        if (shift)
            document.saveAs();
        else
            document.save();

        return true;
    }

    if (command && keyCode == 'O')  { document.openWithDialog(); return true; }
    if (command && keyCode == 'N')  { document.newProject();     return true; }

    if (command)
        return false;

    if (key == juce::KeyPress::spaceKey)
    {
        togglePlayback();
        return true;
    }

    if (keyCode == 'R')
    {
        toggleRecording();
        return true;
    }

    if (key == juce::KeyPress::homeKey || key == juce::KeyPress::returnKey)
    {
        if (! engine.isRecording())
            engine.getMixer().setPosition (0);

        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        return timeline.deleteSelectedClip();

    if (key.getTextCharacter() == '+' || key.getTextCharacter() == '=')
    {
        timeline.zoom (1.5);
        return true;
    }

    if (key.getTextCharacter() == '-')
    {
        timeline.zoom (1.0 / 1.5);
        return true;
    }

    return false;
}

//==============================================================================
void MainComponent::togglePlayback()
{
    auto& mixerEngine = engine.getMixer();

    if (engine.isRecording())
        stopRecording();
    else if (mixerEngine.isPlaying())
        mixerEngine.pause();
    else
        mixerEngine.play();
}

void MainComponent::stop()
{
    auto& mixerEngine = engine.getMixer();

    if (engine.isRecording())
    {
        stopRecording();
        return;
    }

    // Pressing stop while already stopped returns to the start, as in most DAWs.
    if (! mixerEngine.isPlaying())
        mixerEngine.setPosition (0);

    mixerEngine.pause();
}

void MainComponent::toggleRecording()
{
    if (engine.isRecording())
    {
        stopRecording();
        return;
    }

    // Record onto the selected track, or a new one if nothing is selected:
    // no separate "arm" step.
    auto& project = engine.getProject();
    auto trackId = timeline.getSelectedTrackId();
    trackCreatedForRecording = 0;

    if (project.findTrack (trackId) == nullptr)
        trackId = trackCreatedForRecording = project.addTrack ("Recording");

    const auto started = engine.startRecording (trackId, document.getRecordingsFolder());

    if (started.failed())
    {
        if (trackCreatedForRecording != 0)
            project.removeTrack (std::exchange (trackCreatedForRecording, 0));

        transport.showMessage (started.getErrorMessage(), true);
        return;
    }

    timeline.selectTrack (trackId);
    timeline.showRecording (trackId, engine.getRecordingStartSeconds());
    transport.setRecording (true);
    transport.showMessage ("Recording onto \"" + project.findTrack (trackId)->name + "\". Press R or Space to stop.");
}

void MainComponent::stopRecording()
{
    const auto trackId = engine.getRecordingTrackId();
    const auto result = engine.stopRecording();

    timeline.showRecording (0, 0.0);
    transport.setRecording (false);

    // Don't leave behind an empty track that was only made for a failed take.
    if (const auto* track = engine.getProject().findTrack (trackCreatedForRecording);
        track != nullptr && track->clips.empty())
        engine.getProject().removeTrack (trackCreatedForRecording);

    trackCreatedForRecording = 0;

    if (result.failed())
        transport.showMessage (result.getErrorMessage(), true);
    else if (const auto* track = engine.getProject().findTrack (trackId))
        transport.showMessage ("Recorded a new take on \"" + track->name + "\".");
}

//==============================================================================
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshDeviceStatus();
}

void MainComponent::globalFocusChanged (juce::Component* focusedComponent)
{
    // When a dialog closes, focus can end up nowhere, which would leave the
    // keyboard shortcuts dead until the user clicks. Take it back.
    if (focusedComponent != nullptr)
        return;

    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this)]
    {
        if (safeThis != nullptr
            && safeThis->isShowing()
            && juce::Component::getCurrentlyFocusedComponent() == nullptr
            && juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0
            && juce::Process::isForegroundProcess())
        {
            safeThis->grabKeyboardFocus();
        }
    });
}

void MainComponent::refreshDeviceStatus()
{
    transport.setDeviceDescription (engine.getDeviceDescription(), ! engine.hasOutputDevice());
}

void MainComponent::refreshTitle()
{
    if (onTitleChanged)
        onTitleChanged (document.getName() + (document.hasUnsavedChanges() ? "*" : "") + " - Smart Audio Studio");
}

void MainComponent::showFileMenu (juce::Component& button)
{
    const auto commandKey = juce::String (
       #if JUCE_MAC
        "Cmd"
       #else
        "Ctrl"
       #endif
    );

    const auto item = [&commandKey] (const juce::String& text, int id, const juce::String& shortcut)
    {
        juce::PopupMenu::Item result (text);
        result.itemID = id;
        result.shortcutKeyDescription = commandKey + "+" + shortcut;
        return result;
    };

    juce::PopupMenu menu;
    menu.addItem (item ("New Project", newItem, "N"));
    menu.addItem (item ("Open...", openItem, "O"));
    menu.addSeparator();
    menu.addItem (item ("Save", saveItem, "S"));
    menu.addItem (item ("Save As...", saveAsItem, "Shift+S"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                        [safeThis = juce::Component::SafePointer (this)] (int result)
                        {
                            if (safeThis == nullptr)
                                return;

                            auto& doc = safeThis->document;

                            switch (result)
                            {
                                case newItem:    doc.newProject();     break;
                                case openItem:   doc.openWithDialog(); break;
                                case saveItem:   doc.save();           break;
                                case saveAsItem: doc.saveAs();         break;
                                default: break;
                            }
                        });
}

void MainComponent::chooseFilesToImport()
{
    const auto& formats = engine.getProject().getFormatManager();
    importChooser = std::make_unique<juce::FileChooser> ("Choose audio files to import",
                                                         juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                         formats.getWildcardForAllFormats());

    importChooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::canSelectMultipleItems,
                                [this] (const juce::FileChooser& chooser)
                                {
                                    if (! chooser.getResults().isEmpty())
                                        timeline.importFilesAtPlayhead (chooser.getResults());
                                });
}

void MainComponent::setSettingsVisible (bool shouldBeVisible)
{
    if (shouldBeVisible)
    {
        // Changing devices mid-take would break the recording.
        if (engine.isRecording())
            stopRecording();

        settings = std::make_unique<SettingsOverlay> (engine.getDeviceManager());
        settings->onClose = [this]
        {
            // Defer, because this is called from inside the overlay.
            juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer (this)]
            {
                if (safeThis != nullptr)
                    safeThis->setSettingsVisible (false);
            });
        };

        addAndMakeVisible (*settings);
        settings->setBounds (getLocalBounds());
        settings->grabKeyboardFocus();
    }
    else
    {
        settings.reset();
        grabKeyboardFocus();
    }
}

} // namespace sas
