#include "Engine/MixEngine.h"
#include "TestHelpers.h"

namespace sas
{

class MixEngineTests final : public juce::UnitTest
{
public:
    MixEngineTests() : juce::UnitTest ("MixEngine", "Smart Audio Studio") {}

    void runTest() override
    {
        beginTest ("The pan law is unity at the centre");
        {
            const auto centre = panToGains (0.0f);
            expectEquals (centre.left, 1.0f);
            expectEquals (centre.right, 1.0f);

            const auto hardLeft = panToGains (-1.0f);
            expectEquals (hardLeft.left, 1.0f);
            expectEquals (hardLeft.right, 0.0f);

            const auto halfRight = panToGains (0.5f);
            expectEquals (halfRight.left, 0.5f);
            expectEquals (halfRight.right, 1.0f);

            expectEquals (panToGains (3.0f).left, 0.0f);
        }

        beginTest ("Nothing is rendered and the playhead stays put while stopped");
        {
            MixEngine engine;
            engine.setGraph (makeGraph ({ { 0, 0.5f } }));
            auto output = render (engine, 256);

            expectEquals (output.getMagnitude (0, 256), 0.0f);
            expectEquals (engine.getPosition(), (juce::int64) 0);
        }

        beginTest ("A clip starts exactly at its position");
        {
            MixEngine engine;
            engine.prepare (44100.0, 64);
            engine.setGraph (makeGraph ({ { 100, 0.5f } }));
            engine.play();

            // A block larger than the prepared size checks that rendering is chunked.
            auto output = render (engine, 300);

            expectEquals (output.getMagnitude (0, 0, 100), 0.0f);
            expectEquals (output.getSample (0, 100), 0.5f);
            expectEquals (output.getSample (1, 299), 0.5f);   // mono is sent to both sides
            expectEquals (engine.getPosition(), (juce::int64) 300);
        }

        beginTest ("A clip stops at its end");
        {
            MixEngine engine;
            engine.setGraph (makeGraph ({ { 0, 0.5f, 1, 50 } }));
            engine.play();
            auto output = render (engine, 100);

            expectEquals (output.getSample (0, 49), 0.5f);
            expectEquals (output.getMagnitude (0, 50, 50), 0.0f);
        }

        beginTest ("A clip plays only its source range");
        {
            auto ramp = std::make_shared<juce::AudioBuffer<float>> (1, 1000);

            for (int i = 0; i < 1000; ++i)
                ramp->setSample (0, i, (float) i / 1000.0f);

            auto graph = std::make_shared<PlaybackGraph>();
            auto& track = graph->tracks.emplace_back();
            track.params = std::make_shared<ChannelParams>();
            track.clips.push_back ({ ramp, 10, 500, 100 });   // samples 500..599, placed at 10

            MixEngine engine;
            engine.setGraph (graph);
            engine.play();
            auto output = render (engine, 200);

            expectEquals (output.getSample (0, 9), 0.0f);
            expectEquals (output.getSample (0, 10), 0.5f);
            expectEquals (output.getSample (0, 109), 0.599f);
            expectEquals (output.getMagnitude (0, 110, 90), 0.0f);
        }

        beginTest ("Overlapping tracks are summed, with gain and pan");
        {
            MixEngine engine;
            auto graph = makeGraph ({ { 0, 0.25f }, { 0, 0.5f } });
            graph->tracks[1].params->gain = 0.5f;
            graph->tracks[1].params->pan  = 1.0f;
            engine.setGraph (graph);
            engine.play();
            auto output = render (engine, 64);

            expectEquals (output.getSample (0, 10), 0.25f);            // track 2 is panned right
            expectEquals (output.getSample (1, 10), 0.25f + 0.25f);
        }

        beginTest ("Mute and solo");
        {
            MixEngine engine;
            auto graph = makeGraph ({ { 0, 0.1f }, { 0, 0.2f }, { 0, 0.4f } });
            engine.setGraph (graph);
            engine.play();

            graph->tracks[0].params->mute = true;
            expectWithinAbsoluteError (render (engine, 16).getSample (0, 0), 0.6f, 1.0e-6f);

            graph->tracks[2].params->solo = true;
            expectWithinAbsoluteError (render (engine, 16).getSample (0, 0), 0.4f, 1.0e-6f);

            graph->tracks[0].params->solo = true;   // mute wins over solo
            expectWithinAbsoluteError (render (engine, 16).getSample (0, 0), 0.4f, 1.0e-6f);
        }

        beginTest ("Master gain and meters");
        {
            MixEngine engine;
            auto graph = makeGraph ({ { 0, 0.5f } });
            engine.setGraph (graph);
            engine.getMasterParams().gain = 0.5f;
            engine.play();
            auto output = render (engine, 32);

            expectEquals (output.getSample (0, 0), 0.25f);
            expectEquals (graph->tracks[0].params->peakLeft.load(), 0.5f);
            expectEquals (engine.getMasterParams().peakRight.load(), 0.25f);
        }

        beginTest ("Extra output channels are silent");
        {
            MixEngine engine;
            engine.setGraph (makeGraph ({ { 0, 0.5f } }));
            engine.play();

            juce::AudioBuffer<float> output (4, 32);
            output.clear();
            output.setSample (3, 5, 1.0f);
            engine.process (output, 32);
            expectEquals (output.getMagnitude (2, 0, 32), 0.0f);
            expectEquals (output.getMagnitude (3, 0, 32), 0.0f);
        }

        beginTest ("Old graphs are freed on the message thread only");
        {
            MixEngine engine;
            auto first = makeGraph ({ { 0, 0.5f } });
            std::weak_ptr<const PlaybackGraph> watcher = first;

            engine.setGraph (std::move (first));
            render (engine, 16);   // the audio thread picks the graph up

            engine.setGraph (makeGraph ({}));
            engine.releaseUnusedGraphs();
            expect (! watcher.expired(), "the audio thread is still using the first graph");

            render (engine, 16);   // the audio thread switches to the new graph
            expect (! watcher.expired(), "the audio thread must never free a graph itself");

            engine.releaseUnusedGraphs();
            expect (watcher.expired());
        }
    }

private:
    struct TestClip
    {
        juce::int64 start;
        float level;
        int channels = 1;
        int length = 10000;
    };

    /** One track per clip. */
    static std::shared_ptr<PlaybackGraph> makeGraph (std::initializer_list<TestClip> clips)
    {
        auto graph = std::make_shared<PlaybackGraph>();

        for (const auto& clip : clips)
        {
            auto& track = graph->tracks.emplace_back();
            track.params = std::make_shared<ChannelParams>();
            track.clips.push_back ({ test::makeConstantAudio (clip.channels, clip.length, clip.level), clip.start, 0, clip.length });
        }

        return graph;
    }

    static juce::AudioBuffer<float> render (MixEngine& engine, int numSamples)
    {
        juce::AudioBuffer<float> output (2, numSamples);
        engine.process (output, numSamples);
        return output;
    }
};

static MixEngineTests mixEngineTests;

} // namespace sas
