#include "ProjectDocument.h"
#include "Engine/ProjectFile.h"

namespace sas
{

namespace
{
    const juce::String untitledName ("Untitled");
    const juce::String originExtension (".origin");   // stores which project an auto-save belongs to

    void resetChannel (ChannelParams& params)
    {
        params.gain = 1.0f;
        params.pan = 0.0f;
    }
}

ProjectDocument::ProjectDocument (AudioEngine& e) : engine (e)
{
    autosaveFile = getAutosaveFolder().getChildFile ("Session " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S")
                                                     + " " + juce::String (juce::Random::getSystemRandom().nextInt (100000))
                                                     + ProjectFile::fileExtension);

    // A new project starts with one empty track, ready to record onto.
    if (engine.getProject().getTracks().empty())
        engine.getProject().addTrack();

    markSaved();
    lastAutosaveTime = juce::Time::getMillisecondCounter();
    startTimer (1000);
}

ProjectDocument::~ProjectDocument()
{
    // Closing normally: the auto-save is no longer needed for recovery.
    deleteAutosave();
}

juce::File ProjectDocument::getAppDataFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Smart Audio Studio");
}

juce::File ProjectDocument::getDefaultProjectsFolder()
{
    auto folder = getAppDataFolder().getChildFile ("Projects");
    folder.createDirectory();
    return folder;
}

juce::File ProjectDocument::getAutosaveFolder()
{
    return getAppDataFolder().getChildFile ("Autosave");
}

juce::String ProjectDocument::getName() const
{
    return file == juce::File() ? untitledName : file.getFileNameWithoutExtension();
}

juce::File ProjectDocument::getRecordingsFolder() const
{
    if (file == juce::File())
        return getAppDataFolder().getChildFile ("Recordings");

    return file.getParentDirectory().getChildFile (file.getFileNameWithoutExtension() + " Recordings");
}

juce::File ProjectDocument::getFileForPaths() const
{
    return file != juce::File() ? file : getAppDataFolder().getChildFile (untitledName + ProjectFile::fileExtension);
}

juce::String ProjectDocument::getCurrentJson() const
{
    return ProjectFile::toJson (engine.getProject(), engine.getMixer().getMasterParams(), getFileForPaths());
}

bool ProjectDocument::hasUnsavedChanges() const
{
    // Comparing with what was saved (rather than counting edits) means that
    // undoing a change by hand, e.g. moving a fader back, counts as unchanged.
    return getCurrentJson() != savedJson;
}

void ProjectDocument::setFile (const juce::File& newFile)
{
    file = newFile;
    markSaved();
}

void ProjectDocument::markSaved()
{
    savedJson = getCurrentJson();
    autosavedJson = savedJson;
    lastReportedUnsaved = false;
    deleteAutosave();

    if (onStateChanged)
        onStateChanged();
}

//==============================================================================
void ProjectDocument::newProject()
{
    runAfterCheckingForUnsavedChanges ([this]
    {
        if (onBeforeReplacingProject)
            onBeforeReplacingProject();

        auto& mixer = engine.getMixer();
        mixer.pause();
        mixer.setPosition (0);
        resetChannel (mixer.getMasterParams());

        auto& project = engine.getProject();
        project.clear();
        project.addTrack();
        setFile ({});
    });
}

void ProjectDocument::openWithDialog()
{
    runAfterCheckingForUnsavedChanges ([this]
    {
        const auto startFolder = file != juce::File() ? file.getParentDirectory() : getDefaultProjectsFolder();
        chooser = std::make_unique<juce::FileChooser> ("Open a project", startFolder, ProjectFile::fileWildcard);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult() != juce::File())
                                      openWithoutAsking (fc.getResult());
                              });
    });
}

void ProjectDocument::open (const juce::File& projectFile)
{
    runAfterCheckingForUnsavedChanges ([this, projectFile] { openWithoutAsking (projectFile); });
}

void ProjectDocument::openWithoutAsking (const juce::File& projectFile)
{
    if (onBeforeReplacingProject)
        onBeforeReplacingProject();

    ProjectFile::LoadReport report;
    const auto result = ProjectFile::load (engine.getProject(), engine.getMixer().getMasterParams(), projectFile, report);

    if (result.failed())
    {
        if (onMessage)
            onMessage (result.getErrorMessage(), true);

        return;
    }

    engine.getMixer().pause();
    engine.getMixer().setPosition (0);
    setFile (projectFile);

    if (onMessage == nullptr)
        return;

    if (report.missingFiles.isEmpty())
        onMessage ("Opened " + projectFile.getFileName(), false);
    else
        onMessage (juce::String (report.missingFiles.size()) + " audio file(s) could not be found, e.g. "
                       + juce::File (report.missingFiles[0]).getFileName()
                       + ". Their clips were left out; saving now will remove them from the project.", true);
}

void ProjectDocument::save (std::function<void (bool)> onDone)
{
    if (file == juce::File())
    {
        saveAs (std::move (onDone));
        return;
    }

    const auto saved = writeTo (file);

    if (onDone)
        onDone (saved);
}

void ProjectDocument::saveAs (std::function<void (bool)> onDone)
{
    const auto suggested = file != juce::File()
                         ? file
                         : getDefaultProjectsFolder().getChildFile (untitledName + ProjectFile::fileExtension);

    chooser = std::make_unique<juce::FileChooser> ("Save the project", suggested, ProjectFile::fileWildcard);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, onDone = std::move (onDone)] (const juce::FileChooser& fc)
                          {
                              auto target = fc.getResult();
                              const auto saved = target != juce::File()
                                              && writeTo (target.withFileExtension (ProjectFile::fileExtension));

                              if (onDone)
                                  onDone (saved);
                          });
}

bool ProjectDocument::writeTo (const juce::File& target)
{
    const auto result = ProjectFile::save (engine.getProject(), engine.getMixer().getMasterParams(), target);

    if (result.failed())
    {
        if (onMessage)
            onMessage ("Could not save: " + result.getErrorMessage(), true);

        return false;
    }

    setFile (target);

    if (onMessage)
        onMessage ("Saved " + target.getFileName(), false);

    return true;
}

void ProjectDocument::runAfterCheckingForUnsavedChanges (std::function<void()> proceed)
{
    if (! hasUnsavedChanges())
    {
        proceed();
        return;
    }

    const auto options = juce::MessageBoxOptions()
                             .withIconType (juce::MessageBoxIconType::NoIcon)
                             .withTitle ("Save changes to \"" + getName() + "\"?")
                             .withMessage ("Your changes will be lost if you don't save them.")
                             .withButton ("Save")
                             .withButton ("Don't Save")
                             .withButton ("Cancel");

    messageBox = juce::AlertWindow::showScopedAsync (options, [this, proceed = std::move (proceed)] (int result)
    {
        if (result == 1)        // Save
            save ([proceed] (bool saved) { if (saved) proceed(); });
        else if (result == 2)   // Don't Save
            proceed();
    });
}

//==============================================================================
void ProjectDocument::timerCallback()
{
    const auto unsaved = hasUnsavedChanges();

    if (unsaved != lastReportedUnsaved)
    {
        lastReportedUnsaved = unsaved;

        if (onStateChanged)
            onStateChanged();
    }

    if (unsaved && juce::Time::getMillisecondCounter() - lastAutosaveTime >= (juce::uint32) autosaveIntervalSeconds * 1000u)
        autosave();
}

void ProjectDocument::autosave()
{
    lastAutosaveTime = juce::Time::getMillisecondCounter();
    const auto json = getCurrentJson();

    if (json == autosavedJson)
        return;

    autosaveFile.getParentDirectory().createDirectory();

    if (ProjectFile::save (engine.getProject(), engine.getMixer().getMasterParams(), autosaveFile).wasOk())
    {
        autosaveFile.withFileExtension (originExtension).replaceWithText (file.getFullPathName());
        autosavedJson = json;
    }
}

void ProjectDocument::deleteAutosave()
{
    autosaveFile.deleteFile();
    autosaveFile.withFileExtension (originExtension).deleteFile();
}

void ProjectDocument::offerToRecoverAutosave()
{
    auto leftovers = getAutosaveFolder().findChildFiles (juce::File::findFiles, false, ProjectFile::fileWildcard);
    leftovers.removeFirstMatchingValue (autosaveFile);

    if (leftovers.isEmpty())
        return;

    std::sort (leftovers.begin(), leftovers.end(),
               [] (const auto& a, const auto& b) { return a.getLastModificationTime() > b.getLastModificationTime(); });

    const auto newest = leftovers.getFirst();
    const auto origin = juce::File (newest.withFileExtension (originExtension).loadFileAsString().trim());
    const auto name = origin != juce::File() ? origin.getFileNameWithoutExtension() : untitledName;

    const auto discardAll = [leftovers]
    {
        for (const auto& f : leftovers)
        {
            f.deleteFile();
            f.withFileExtension (originExtension).deleteFile();
        }
    };

    const auto options = juce::MessageBoxOptions()
                             .withIconType (juce::MessageBoxIconType::NoIcon)
                             .withTitle ("Recover unsaved work?")
                             .withMessage ("Smart Audio Studio didn't close properly last time. There is an auto-saved copy of \""
                                           + name + "\" from " + newest.getLastModificationTime().toString (true, true, false) + ".")
                             .withButton ("Recover")
                             .withButton ("Discard");

    messageBox = juce::AlertWindow::showScopedAsync (options, [this, newest, origin, discardAll] (int result)
    {
        if (result == 1)
        {
            ProjectFile::LoadReport report;

            if (ProjectFile::load (engine.getProject(), engine.getMixer().getMasterParams(), newest, report).wasOk())
            {
                // The recovered work belongs to the original project, but hasn't been saved there yet.
                file = origin.existsAsFile() ? origin : juce::File();
                savedJson = {};
                autosavedJson = {};

                if (onStateChanged)
                    onStateChanged();

                if (onMessage)
                    onMessage ("Recovered \"" + getName() + "\". Save it to keep it.", false);
            }
        }

        discardAll();
    });
}

} // namespace sas
