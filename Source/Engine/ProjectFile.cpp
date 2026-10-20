#include "ProjectFile.h"

namespace sas
{

namespace
{
    const juce::Identifier formatId ("format"), versionId ("version"), masterId ("master"), tracksId ("tracks"),
                           nameId ("name"), colourId ("colour"), gainId ("gain"), panId ("pan"), muteId ("mute"),
                           soloId ("solo"), clipsId ("clips"), fileId ("file"), startId ("start"),
                           sourceOffsetId ("sourceOffset"), sourceLengthId ("sourceLength");

    const juce::String formatName ("Smart Audio Studio project");

    juce::var channelToVar (const ChannelParams& params)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty (gainId, params.gain.load());
        object->setProperty (panId, params.pan.load());
        return object.release();
    }

    void channelFromVar (const juce::var& v, ChannelParams& params)
    {
        params.gain = juce::jlimit (0.0f, 4.0f, (float) v.getProperty (gainId, 1.0f));
        params.pan  = juce::jlimit (-1.0f, 1.0f, (float) v.getProperty (panId, 0.0f));
    }

    juce::String pathForSaving (const juce::File& audioFile, const juce::File& projectFile)
    {
        const auto projectFolder = projectFile.getParentDirectory();

        if (audioFile.isAChildOf (projectFolder))
            return audioFile.getRelativePathFrom (projectFolder).replaceCharacter ('\\', '/');

        return audioFile.getFullPathName();
    }

    juce::File resolvePath (const juce::String& path, const juce::File& projectFile)
    {
        const auto projectFolder = projectFile.getParentDirectory();
        const auto file = juce::File::isAbsolutePath (path) ? juce::File (path) : projectFolder.getChildFile (path);

        if (file.existsAsFile())
            return file;

        // If the project was moved along with its audio, look next to it.
        const auto besideProject = projectFolder.getChildFile (file.getFileName());
        return besideProject.existsAsFile() ? besideProject : file;
    }
}

juce::String ProjectFile::toJson (const Project& project, const ChannelParams& master, const juce::File& projectFile)
{
    juce::Array<juce::var> tracks;

    for (const auto& track : project.getTracks())
    {
        auto trackObject = std::make_unique<juce::DynamicObject>();
        trackObject->setProperty (nameId, track.name);
        trackObject->setProperty (colourId, track.colourIndex);
        trackObject->setProperty (gainId, track.params->gain.load());
        trackObject->setProperty (panId, track.params->pan.load());
        trackObject->setProperty (muteId, track.params->mute.load());
        trackObject->setProperty (soloId, track.params->solo.load());

        juce::Array<juce::var> clips;

        for (const auto& clip : track.clips)
        {
            if (clip.sourceFile == juce::File())
                continue;   // audio that only exists in memory can't be saved

            auto clipObject = std::make_unique<juce::DynamicObject>();
            clipObject->setProperty (nameId, clip.name);
            clipObject->setProperty (fileId, pathForSaving (clip.sourceFile, projectFile));
            clipObject->setProperty (startId, clip.startSeconds);
            clipObject->setProperty (sourceOffsetId, clip.sourceOffset);
            clipObject->setProperty (sourceLengthId, clip.sourceLength);
            clips.add (clipObject.release());
        }

        trackObject->setProperty (clipsId, clips);
        tracks.add (trackObject.release());
    }

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty (formatId, formatName);
    root->setProperty (versionId, currentVersion);
    root->setProperty (masterId, channelToVar (master));
    root->setProperty (tracksId, tracks);

    return juce::JSON::toString (juce::var (root.release()));
}

juce::Result ProjectFile::save (const Project& project, const ChannelParams& master, const juce::File& projectFile)
{
    const juce::TemporaryFile temp (projectFile);

    if (! temp.getFile().replaceWithText (toJson (project, master, projectFile)))
        return juce::Result::fail ("Could not write to " + projectFile.getParentDirectory().getFullPathName());

    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Could not replace " + projectFile.getFullPathName());

    return juce::Result::ok();
}

juce::Result ProjectFile::load (Project& project, ChannelParams& master, const juce::File& projectFile, LoadReport& report)
{
    if (! projectFile.existsAsFile())
        return juce::Result::fail (projectFile.getFileName() + " could not be found.");

    juce::var root;

    if (const auto parsed = juce::JSON::parse (projectFile.loadFileAsString(), root); parsed.failed())
        return juce::Result::fail (projectFile.getFileName() + " is damaged: " + parsed.getErrorMessage());

    if (root.getProperty (formatId, {}).toString() != formatName)
        return juce::Result::fail (projectFile.getFileName() + " is not a Smart Audio Studio project.");

    if ((int) root.getProperty (versionId, 0) > currentVersion)
        return juce::Result::fail (projectFile.getFileName() + " was saved by a newer version of Smart Audio Studio.");

    project.clear();
    channelFromVar (root.getProperty (masterId, {}), master);

    if (const auto* tracks = root.getProperty (tracksId, {}).getArray())
    {
        for (const auto& trackVar : *tracks)
        {
            const auto trackId = project.addTrack (trackVar.getProperty (nameId, {}).toString(),
                                                   std::max (0, (int) trackVar.getProperty (colourId, 0)));
            channelFromVar (trackVar, *project.findTrack (trackId)->params);
            project.setTrackMuted (trackId, (bool) trackVar.getProperty (muteId, false));
            project.setTrackSoloed (trackId, (bool) trackVar.getProperty (soloId, false));

            if (const auto* clips = trackVar.getProperty (clipsId, {}).getArray())
            {
                for (const auto& clipVar : *clips)
                {
                    const auto file = resolvePath (clipVar.getProperty (fileId, {}).toString(), projectFile);
                    int clipId = 0;

                    if (project.addClipFromFile (trackId, file, (double) clipVar.getProperty (startId, 0.0), &clipId).failed())
                    {
                        report.missingFiles.addIfNotAlreadyThere (file.getFullPathName());
                        continue;
                    }

                    project.setClipSourceRange (trackId, clipId,
                                                (juce::int64) clipVar.getProperty (sourceOffsetId, 0),
                                                (juce::int64) clipVar.getProperty (sourceLengthId, std::numeric_limits<juce::int64>::max()));
                    project.renameClip (trackId, clipId, clipVar.getProperty (nameId, {}).toString());
                }
            }
        }
    }

    return juce::Result::ok();
}

} // namespace sas
