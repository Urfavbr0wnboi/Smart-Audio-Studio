#include "BrowserPanel.h"
#include "StudioLookAndFeel.h"

namespace sas
{

namespace
{
    juce::File defaultFolder()
    {
        const auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        return music.isDirectory() ? music : juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    }
}

BrowserPanel::BrowserPanel (juce::AudioFormatManager& formats)
    : filter (formats.getWildcardForAllFormats(), "*", "Audio files")
{
    title.setText ("Browser", juce::dontSendNotification);
    title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (title);

    folderLabel.setColour (juce::Label::textColourId, Theme::textDim);
    folderLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (folderLabel);

    upButton.setTooltip ("Go to the parent folder");
    upButton.onClick = [this] { setRoot (contents.getDirectory().getParentDirectory()); };
    chooseFolderButton.setTooltip ("Pick the folder to browse");
    chooseFolderButton.onClick = [this] { chooseFolder(); };

    for (auto* button : { &upButton, &chooseFolderButton })
    {
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (button);
    }

    tree.setDragAndDropDescription (dragDescription);
    tree.addListener (this);
    tree.setTooltip ("Drag a file onto a track, or double-click it to add it at the playhead");
    addAndMakeVisible (tree);

    scanThread.startThread (juce::Thread::Priority::low);
    setRoot (defaultFolder());
}

BrowserPanel::~BrowserPanel()
{
    tree.removeListener (this);
    scanThread.stopThread (2000);
}

void BrowserPanel::setRoot (const juce::File& folder)
{
    if (! folder.isDirectory())
        return;

    contents.setDirectory (folder, true, true);
    folderLabel.setText (folder.getFullPathName(), juce::dontSendNotification);
    folderLabel.setTooltip (folder.getFullPathName());
}

void BrowserPanel::fileDoubleClicked (const juce::File& file)
{
    if (file.existsAsFile() && onFileActivated)
        onFileActivated (file);
}

void BrowserPanel::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a folder to browse", contents.getDirectory());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() != juce::File())
                                  setRoot (fc.getResult());
                          });
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void BrowserPanel::resized()
{
    auto area = getLocalBounds().reduced (8).withTrimmedRight (1);

    auto header = area.removeFromTop (26);
    chooseFolderButton.setBounds (header.removeFromRight (78));
    header.removeFromRight (4);
    upButton.setBounds (header.removeFromRight (36));
    title.setBounds (header);

    folderLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);
    tree.setBounds (area);
}

} // namespace sas
