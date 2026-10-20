#pragma once

#include "AudioFileLoader.h"
#include "ChannelParams.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace sas
{

/** An immutable snapshot of everything the audio thread needs to play the
    project. The message thread builds a new one whenever the arrangement
    changes and hands it over with MixEngine::setGraph(), so the audio thread
    never reads data that is being edited.
*/
struct PlaybackGraph
{
    struct Clip
    {
        SharedAudio audio;              // already at the engine's sample rate
        juce::int64 startSample = 0;
    };

    struct Track
    {
        std::shared_ptr<ChannelParams> params;
        std::vector<Clip> clips;
    };

    std::vector<Track> tracks;
};

/** Mixes the tracks of a PlaybackGraph into a stereo output and owns the
    transport (play state and playhead position).

    It knows nothing about audio devices, so it can render to a sound card, to a
    file, or inside a unit test.

    Threading: process() runs on the audio thread and never locks, allocates or
    frees memory. Everything else is called from the message thread, apart from
    the transport and level accessors, which are safe from any thread.
*/
class MixEngine
{
public:
    MixEngine();

    /** Allocates working memory. Call while no audio is being processed. */
    void prepare (double sampleRate, int maximumBlockSize);

    double getSampleRate() const noexcept { return sampleRate.load(); }

    /** Hands a new arrangement to the audio thread. */
    void setGraph (std::shared_ptr<const PlaybackGraph> newGraph);

    /** Frees graphs the audio thread has finished with. Call this regularly on
        the message thread, so that memory is never freed on the audio thread.
    */
    void releaseUnusedGraphs();

    /** Renders `numSamples` of the mix into channels 0 and 1 of `output`, and
        clears any further channels. `output` must have at least two channels.
    */
    void process (juce::AudioBuffer<float>& output, int numSamples) noexcept;

    // Transport ----------------------------------------------------------------
    void play() noexcept                     { playing.store (true); }
    void pause() noexcept                    { playing.store (false); }
    bool isPlaying() const noexcept          { return playing.load(); }

    void setPosition (juce::int64 sample) noexcept { position.store (std::max<juce::int64> (sample, 0)); }
    juce::int64 getPosition() const noexcept       { return position.load(); }
    double getPositionSeconds() const noexcept     { return (double) getPosition() / getSampleRate(); }

    ChannelParams& getMasterParams() noexcept { return master; }

private:
    void collectPendingGraph() noexcept;
    void renderBlock (juce::AudioBuffer<float>& output, int offset, int numSamples) noexcept;
    void mixClipsInto (const PlaybackGraph::Track&, juce::int64 blockStart, int numSamples) noexcept;

    std::atomic<double> sampleRate { 44100.0 };
    juce::AudioBuffer<float> trackBuffer;

    std::atomic<bool> playing { false };
    std::atomic<juce::int64> position { 0 };

    ChannelParams master;

    juce::SpinLock pendingLock;
    std::shared_ptr<const PlaybackGraph> pendingGraph;   // guarded by pendingLock
    std::shared_ptr<const PlaybackGraph> activeGraph;    // audio thread only
    std::vector<std::shared_ptr<const PlaybackGraph>> graphPool;   // message thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixEngine)
};

} // namespace sas
