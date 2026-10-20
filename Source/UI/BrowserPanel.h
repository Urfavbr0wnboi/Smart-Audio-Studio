#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace sas
{

/** A docked file browser showing the audio files in a folder. Files can be
    dragged onto the timeline, or double-clicked to import them.
*/
class BrowserPanel final : public juce::Component,
                           private juce::FileBrowserListener
{
public:
    explicit BrowserPanel (juce::AudioFormatManager&);
    ~BrowserPanel() override;

    /** The drag description used when files are dragged out of the browser. */
    static inline const juce::String dragDescription { "SmartAudioStudio.BrowserFile" };

    std::function<void (const juce::File&)> onFileActivated;

    void setRoot (const juce::File& folder);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void selectionChanged() override {}
    void fileClicked (const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked (const juce::File&) override;
    void browserRootChanged (const juce::File&) override {}

    void chooseFolder();

    juce::TimeSliceThread scanThread { "Browser scanner" };
    juce::WildcardFileFilter filter;
    juce::DirectoryContentsList contents { &filter, scanThread };
    juce::FileTreeComponent tree { contents };

    juce::Label title, folderLabel;
    juce::TextButton chooseFolderButton { "Choose..." }, upButton { "Up" };
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace sas
