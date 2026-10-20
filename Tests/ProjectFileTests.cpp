#include "Engine/ProjectFile.h"
#include "TestHelpers.h"

namespace sas
{

class ProjectFileTests final : public juce::UnitTest
{
public:
    ProjectFileTests() : juce::UnitTest ("ProjectFile", "Smart Audio Studio") {}

    void runTest() override
    {
        const auto root = juce::File::createTempFile ("sas-project-tests");
        const auto songFolder = root.getChildFile ("Song");
        const auto outsideFolder = root.getChildFile ("Elsewhere");
        const auto projectFile = songFolder.getChildFile ("Song.sasproj");
        const auto insideAudio = songFolder.getChildFile ("Audio/vocal.wav");
        const auto outsideAudio = outsideFolder.getChildFile ("beat.wav");

        juce::AudioBuffer<float> ramp (1, 4410);
        for (int i = 0; i < ramp.getNumSamples(); ++i)
            ramp.setSample (0, i, (float) i / (float) ramp.getNumSamples());

        expect (test::writeWav (insideAudio, ramp, 44100.0, 1));
        expect (test::writeWav (outsideAudio, ramp, 44100.0, 2));

        beginTest ("A saved project loads back the same");
        {
            MixEngine engine;
            Project project (engine);
            engine.getMasterParams().gain = 0.5f;

            const auto vocals = project.addTrack ("Vocals", 3);
            const auto beat = project.addTrack ("Beat");
            setParams (*project.findTrack (vocals)->params, 0.8f, -0.5f, true, false);
            setParams (*project.findTrack (beat)->params, 1.2f, 0.25f, false, true);

            int vocalClip = 0;
            expect (project.addClipFromFile (vocals, insideAudio, 1.5, &vocalClip).wasOk());
            project.setClipSourceRange (vocals, vocalClip, 100, 2000);
            project.renameClip (vocals, vocalClip, "Verse take");
            expect (project.addClipFromFile (beat, outsideAudio, 0.0).wasOk());

            expect (ProjectFile::save (project, engine.getMasterParams(), projectFile).wasOk());

            const auto json = projectFile.loadFileAsString();
            expect (json.contains ("\"Audio/vocal.wav\""), "audio inside the project folder is saved as a relative path");
            expect (json.contains (outsideAudio.getFullPathName().replace ("\\", "\\\\")), "other audio is saved as an absolute path");

            MixEngine otherEngine;
            Project loaded (otherEngine);
            loaded.addTrack ("Will be replaced");
            ProjectFile::LoadReport report;
            const auto result = ProjectFile::load (loaded, otherEngine.getMasterParams(), projectFile, report);

            expect (result.wasOk(), result.getErrorMessage());
            expect (report.missingFiles.isEmpty());
            expectEquals (otherEngine.getMasterParams().gain.load(), 0.5f);
            expectEquals ((int) loaded.getTracks().size(), 2);

            const auto& loadedVocals = loaded.getTracks()[0];
            expectEquals (loadedVocals.name, juce::String ("Vocals"));
            expectEquals (loadedVocals.colourIndex, 3);
            expectEquals (loadedVocals.params->gain.load(), 0.8f);
            expectEquals (loadedVocals.params->pan.load(), -0.5f);
            expect (loadedVocals.params->mute.load());
            expect (! loadedVocals.params->solo.load());

            const auto& clip = loadedVocals.clips.at (0);
            expectEquals (clip.name, juce::String ("Verse take"));
            expectEquals (clip.sourceFile.getFullPathName(), insideAudio.getFullPathName());
            expectEquals (clip.startSeconds, 1.5);
            expectEquals (clip.sourceOffset, (juce::int64) 100);
            expectEquals (clip.sourceLength, (juce::int64) 2000);

            const auto& loadedBeat = loaded.getTracks()[1];
            expectEquals (loadedBeat.colourIndex, 4);
            expect (loadedBeat.params->solo.load());
            expectEquals (loadedBeat.clips.at (0).sourceFile.getFullPathName(), outsideAudio.getFullPathName());
        }

        beginTest ("A project folder can be moved");
        {
            const auto movedFolder = root.getChildFile ("Moved Song");
            expect (songFolder.copyDirectoryTo (movedFolder));

            MixEngine engine;
            Project loaded (engine);
            ProjectFile::LoadReport report;
            expect (ProjectFile::load (loaded, engine.getMasterParams(), movedFolder.getChildFile ("Song.sasproj"), report).wasOk());
            expectEquals (loaded.getTracks()[0].clips.at (0).sourceFile.getFullPathName(),
                          movedFolder.getChildFile ("Audio/vocal.wav").getFullPathName());
        }

        beginTest ("Missing audio is reported and the rest still loads");
        {
            const auto missingFolder = root.getChildFile ("Missing");
            expect (songFolder.copyDirectoryTo (missingFolder));
            missingFolder.getChildFile ("Audio/vocal.wav").deleteFile();

            MixEngine engine;
            Project loaded (engine);
            ProjectFile::LoadReport report;
            expect (ProjectFile::load (loaded, engine.getMasterParams(), missingFolder.getChildFile ("Song.sasproj"), report).wasOk());
            expectEquals (report.missingFiles.size(), 1);
            expect (loaded.getTracks()[0].clips.empty());
            expectEquals ((int) loaded.getTracks()[1].clips.size(), 1);
        }

        beginTest ("Files that are not projects are refused without changing anything");
        {
            MixEngine engine;
            Project project (engine);
            project.addTrack ("Keep me");
            ProjectFile::LoadReport report;

            const auto notJson = root.getChildFile ("notes.sasproj");
            notJson.replaceWithText ("hello");
            expect (ProjectFile::load (project, engine.getMasterParams(), notJson, report).failed());

            const auto otherJson = root.getChildFile ("other.sasproj");
            otherJson.replaceWithText ("{\"format\": \"something else\"}");
            expect (ProjectFile::load (project, engine.getMasterParams(), otherJson, report).failed());

            const auto newer = root.getChildFile ("newer.sasproj");
            newer.replaceWithText ("{\"format\": \"Smart Audio Studio project\", \"version\": 99}");
            expect (ProjectFile::load (project, engine.getMasterParams(), newer, report).failed());

            expect (ProjectFile::load (project, engine.getMasterParams(), root.getChildFile ("nope.sasproj"), report).failed());

            expectEquals ((int) project.getTracks().size(), 1);
            expectEquals (project.getTracks()[0].name, juce::String ("Keep me"));
        }

        root.deleteRecursively();
    }

private:
    static void setParams (ChannelParams& params, float gain, float pan, bool mute, bool solo)
    {
        params.gain = gain;
        params.pan = pan;
        params.mute = mute;
        params.solo = solo;
    }
};

static ProjectFileTests projectFileTests;

} // namespace sas
