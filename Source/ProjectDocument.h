#pragma once

#include "Engine/AudioEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sas
{

/** Everything to do with the project as a file: new, open, save, save as,
    asking about unsaved changes, auto-saving, and recovering work after a crash.

    Auto-saves never overwrite the user's own project file. They go to a
    separate folder and are deleted when the app closes normally, so finding one
    at start-up means the last session ended unexpectedly.
*/
class ProjectDocument final : private juce::Timer
{
public:
    explicit ProjectDocument (AudioEngine&);
    ~ProjectDocument() override;

    /** Called when the name or the unsaved-changes state changes. */
    std::function<void()> onStateChanged;

    /** Called before the project is replaced by New or Open, e.g. to stop a recording. */
    std::function<void()> onBeforeReplacingProject;

    /** Called with a message for the status line. */
    std::function<void (const juce::String& message, bool isError)> onMessage;

    juce::File getFile() const { return file; }
    juce::String getName() const;
    bool hasUnsavedChanges() const;

    /** Where new recordings are saved: next to the project once it has been saved. */
    juce::File getRecordingsFolder() const;

    void newProject();
    void openWithDialog();
    void open (const juce::File& projectFile);
    void save (std::function<void (bool saved)> onDone = {});
    void saveAs (std::function<void (bool saved)> onDone = {});

    /** If there are unsaved changes, asks whether to save them, then calls
        `proceed` unless the user cancelled.
    */
    void runAfterCheckingForUnsavedChanges (std::function<void()> proceed);

    /** At start-up, offers to restore work from a session that didn't close normally. */
    void offerToRecoverAutosave();

    /** The folder for the app's own files, inside the user's Documents folder. */
    static juce::File getAppDataFolder();

    static constexpr int autosaveIntervalSeconds = 5 * 60;

private:
    void timerCallback() override;

    juce::String getCurrentJson() const;
    juce::File getFileForPaths() const;
    void setFile (const juce::File&);
    void markSaved();
    bool writeTo (const juce::File& target);
    void openWithoutAsking (const juce::File& projectFile);
    void autosave();
    void deleteAutosave();
    static juce::File getDefaultProjectsFolder();
    static juce::File getAutosaveFolder();

    AudioEngine& engine;
    juce::File file;
    juce::String savedJson, autosavedJson;
    juce::File autosaveFile;
    juce::uint32 lastAutosaveTime = 0;
    bool lastReportedUnsaved = false;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::ScopedMessageBox messageBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectDocument)
};

} // namespace sas
