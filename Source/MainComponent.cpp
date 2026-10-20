#include "MainComponent.h"
#include "UI/StudioLookAndFeel.h"

namespace sas
{

MainComponent::MainComponent()
{
    addAndMakeVisible (transport);
    addAndMakeVisible (browser);
    addAndMakeVisible (timeline);
    addAndMakeVisible (mixer);

    auto& project = engine.getProject();

    transport.setPanelToggles (browserVisible, mixerVisible);
    transport.onAddTrack       = [&project] { project.addTrack(); };
    transport.onImport         = [this] { chooseFilesToImport(); };
    transport.onShowSettings   = [this] { setSettingsVisible (true); };
    transport.onBrowserToggled = [this] (bool shown) { browserVisible = shown; resized(); };
    transport.onMixerToggled   = [this] (bool shown) { mixerVisible = shown; resized(); };

    browser.onFileActivated = [this] (const juce::File& file) { timeline.importFilesAtPlayhead ({ file }); };
    timeline.onStatusMessage = [this] (const juce::String& message, bool isError) { transport.showMessage (message, isError); };

    project.addTrack();

    engine.getDeviceManager().addChangeListener (this);

    if (const auto error = engine.openDefaultDevice(); error.isNotEmpty())
        transport.showMessage ("Could not open the audio device: " + error, true);

    refreshDeviceStatus();

    setWantsKeyboardFocus (true);
    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    engine.getDeviceManager().removeChangeListener (this);
}

void MainComponent::openFiles (const juce::Array<juce::File>& files)
{
    timeline.importFiles (files, 0, 0.0);
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
    if (key == juce::KeyPress::spaceKey)
    {
        transport.togglePlayback();
        return true;
    }

    if (key == juce::KeyPress::homeKey || key == juce::KeyPress::returnKey)
    {
        transport.returnToStart();
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

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshDeviceStatus();
}

void MainComponent::refreshDeviceStatus()
{
    transport.setDeviceDescription (engine.getDeviceDescription(), ! engine.hasOutputDevice());
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
