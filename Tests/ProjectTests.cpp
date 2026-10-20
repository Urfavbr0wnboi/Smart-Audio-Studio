#include "Engine/Project.h"
#include "TestHelpers.h"

namespace sas
{

class ProjectTests final : public juce::UnitTest
{
public:
    ProjectTests() : juce::UnitTest ("Project", "Smart Audio Studio") {}

    void runTest() override
    {
        beginTest ("Tracks get unique ids and default names");
        {
            MixEngine engine;
            Project project (engine);
            const auto a = project.addTrack();
            const auto b = project.addTrack ("Vocals");

            expect (a != b);
            expectEquals (project.findTrack (a)->name, juce::String ("Track 1"));
            expectEquals (project.findTrack (b)->name, juce::String ("Vocals"));

            project.removeTrack (a);
            expect (project.findTrack (a) == nullptr);
            expectEquals ((int) project.getTracks().size(), 1);
        }

        beginTest ("Listeners hear about edits");
        {
            MixEngine engine;
            Project project (engine);
            CountingListener listener;
            project.addListener (&listener);

            const auto track = project.addTrack();
            project.renameTrack (track, "Drums");
            project.renameTrack (track, "   ");   // ignored
            project.setTrackMuted (track, true);

            expectEquals (listener.structureChanges, 2);
            expectEquals (listener.stateChanges, 1);
            expectEquals (project.findTrack (track)->name, juce::String ("Drums"));
            expect (project.findTrack (track)->params->mute.load());

            project.removeListener (&listener);
        }

        beginTest ("Edits are heard on the next audio block");
        {
            MixEngine engine;
            Project project (engine);
            const auto track = project.addTrack();
            const auto clip = project.addClip (track, "Tone", test::makeConstantAudio (1, 44100, 0.5f), 44100.0, 1.0);
            engine.play();

            engine.setPosition (44100);
            expectEquals (render (engine).getSample (0, 0), 0.5f);

            project.moveClip (track, clip, 2.0);
            engine.setPosition (44100);
            expectEquals (render (engine).getMagnitude (0, 0, 64), 0.0f);

            engine.setPosition (88200);
            expectEquals (render (engine).getSample (0, 0), 0.5f);

            project.removeClip (track, clip);
            engine.setPosition (88200);
            expectEquals (render (engine).getMagnitude (0, 0, 64), 0.0f);
        }

        beginTest ("Clips are converted to the playback rate");
        {
            MixEngine engine;
            Project project (engine);
            const auto track = project.addTrack();
            const auto clipId = project.addClip (track, "Tone", test::makeConstantAudio (2, 48000, 0.5f), 48000.0, 0.5);
            const auto* clip = project.findClip (track, clipId);

            expectEquals (clip->getLengthSeconds(), 1.0);
            expectEquals (clip->getEndSeconds(), 1.5);
            expectEquals (clip->playbackAudio->getNumSamples(), 44100);
            expectEquals (project.getLengthSeconds(), 1.5);

            project.setSampleRate (48000.0);
            expect (project.findClip (track, clipId)->playbackAudio == clip->sourceAudio);
        }

        beginTest ("Clips can move between tracks");
        {
            MixEngine engine;
            Project project (engine);
            const auto first = project.addTrack();
            const auto second = project.addTrack();
            const auto clip = project.addClip (first, "Tone", test::makeConstantAudio (1, 100, 0.5f), 44100.0, 0.0);

            project.moveClipToTrack (first, clip, second, 2.5);
            expect (project.findClip (first, clip) == nullptr);
            expectEquals (project.findClip (second, clip)->startSeconds, 2.5);

            project.moveClipToTrack (second, clip, second + 1000, 1.0);   // unknown track: nothing happens
            expectEquals (project.findClip (second, clip)->startSeconds, 2.5);
        }

        beginTest ("Clips cannot start before zero");
        {
            MixEngine engine;
            Project project (engine);
            const auto track = project.addTrack();
            const auto clip = project.addClip (track, "Tone", test::makeConstantAudio (1, 100, 0.5f), 44100.0, -3.0);
            expectEquals (project.findClip (track, clip)->startSeconds, 0.0);

            project.moveClip (track, clip, -1.0);
            expectEquals (project.findClip (track, clip)->startSeconds, 0.0);
        }

        beginTest ("Bad files are reported, not added");
        {
            MixEngine engine;
            Project project (engine);
            const auto track = project.addTrack();
            const auto result = project.addClipFromFile (track, juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                                     .getChildFile ("sas-missing.wav"), 0.0);
            expect (result.failed());
            expect (project.findTrack (track)->clips.empty());

            expect (project.addClipFromFile (track + 1000, {}, 0.0).failed());
        }
    }

private:
    struct CountingListener final : Project::Listener
    {
        void projectStructureChanged() override { ++structureChanges; }
        void trackStateChanged (int) override   { ++stateChanges; }

        int structureChanges = 0, stateChanges = 0;
    };

    static juce::AudioBuffer<float> render (MixEngine& engine)
    {
        juce::AudioBuffer<float> output (2, 64);
        engine.process (output, 64);
        return output;
    }
};

static ProjectTests projectTests;

} // namespace sas
