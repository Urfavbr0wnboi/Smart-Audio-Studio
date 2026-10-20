#include "MixEngine.h"

namespace sas
{

MixEngine::MixEngine()
{
    prepare (44100.0, 512);
}

void MixEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    jassert (newSampleRate > 0.0 && maximumBlockSize > 0);

    sampleRate = newSampleRate;
    trackBuffer.setSize (2, maximumBlockSize, false, false, true);
}

void MixEngine::setGraph (std::shared_ptr<const PlaybackGraph> newGraph)
{
    // The pool keeps every graph alive until releaseUnusedGraphs() sees that the
    // audio thread has let go of it, so the audio thread never drops the last
    // reference and never runs a destructor.
    graphPool.push_back (newGraph);

    const juce::SpinLock::ScopedLockType lock (pendingLock);
    pendingGraph = std::move (newGraph);
}

void MixEngine::releaseUnusedGraphs()
{
    std::erase_if (graphPool, [] (const auto& graph) { return graph.use_count() <= 1; });
}

void MixEngine::collectPendingGraph() noexcept
{
    const juce::SpinLock::ScopedTryLockType lock (pendingLock);

    // If the message thread holds the lock, keep playing the current graph and
    // pick up the new one on the next callback.
    if (lock.isLocked() && pendingGraph != nullptr)
        activeGraph = std::move (pendingGraph);
}

void MixEngine::process (juce::AudioBuffer<float>& output, int numSamples) noexcept
{
    jassert (output.getNumChannels() >= 2 && numSamples <= output.getNumSamples());

    collectPendingGraph();
    output.clear (0, numSamples);

    if (! isPlaying())
        return;

    // The device may deliver larger blocks than we prepared for, so render in
    // chunks that fit the working buffer.
    for (int offset = 0; offset < numSamples;)
    {
        const auto chunk = std::min (numSamples - offset, trackBuffer.getNumSamples());
        renderBlock (output, offset, chunk);
        offset += chunk;
    }
}

void MixEngine::renderBlock (juce::AudioBuffer<float>& output, int offset, int numSamples) noexcept
{
    auto blockStart = position.load();

    if (activeGraph != nullptr)
    {
        const auto& tracks = activeGraph->tracks;
        const auto anySoloed = std::any_of (tracks.begin(), tracks.end(),
                                            [] (const auto& t) { return t.params->solo.load(); });

        for (const auto& track : tracks)
        {
            auto& params = *track.params;

            if (params.mute.load() || (anySoloed && ! params.solo.load()))
                continue;

            trackBuffer.clear (0, numSamples);
            mixClipsInto (track, blockStart, numSamples);

            const auto gain = params.gain.load();
            const auto pan  = panToGains (params.pan.load());
            trackBuffer.applyGain (0, 0, numSamples, gain * pan.left);
            trackBuffer.applyGain (1, 0, numSamples, gain * pan.right);

            updatePeak (params.peakLeft,  trackBuffer.getMagnitude (0, 0, numSamples));
            updatePeak (params.peakRight, trackBuffer.getMagnitude (1, 0, numSamples));

            output.addFrom (0, offset, trackBuffer, 0, 0, numSamples);
            output.addFrom (1, offset, trackBuffer, 1, 0, numSamples);
        }
    }

    const auto masterGain = master.gain.load();
    const auto masterPan  = panToGains (master.pan.load());
    output.applyGain (0, offset, numSamples, masterGain * masterPan.left);
    output.applyGain (1, offset, numSamples, masterGain * masterPan.right);

    updatePeak (master.peakLeft,  output.getMagnitude (0, offset, numSamples));
    updatePeak (master.peakRight, output.getMagnitude (1, offset, numSamples));

    // If the user moved the playhead while we were rendering, keep their position.
    position.compare_exchange_strong (blockStart, blockStart + numSamples);
}

void MixEngine::mixClipsInto (const PlaybackGraph::Track& track, juce::int64 blockStart, int numSamples) noexcept
{
    const auto blockEnd = blockStart + numSamples;

    for (const auto& clip : track.clips)
    {
        const auto& audio = *clip.audio;
        const auto clipEnd = clip.startSample + audio.getNumSamples();
        const auto overlapStart = std::max (blockStart, clip.startSample);
        const auto overlapEnd   = std::min (blockEnd, clipEnd);

        if (overlapEnd <= overlapStart)
            continue;

        const auto sourceOffset = (int) (overlapStart - clip.startSample);
        const auto destOffset   = (int) (overlapStart - blockStart);
        const auto count        = (int) (overlapEnd - overlapStart);

        // Mono clips are sent to both sides.
        for (int ch = 0; ch < 2; ++ch)
            trackBuffer.addFrom (ch, destOffset, audio, std::min (ch, audio.getNumChannels() - 1), sourceOffset, count);
    }
}

} // namespace sas
