#pragma once

#include <algorithm>
#include <atomic>

namespace sas
{

/** Mixer settings shared by the message thread, which writes them, and the audio
    thread, which reads them. Every field is atomic, so moving a fader never
    needs a lock and never blocks the audio callback.
*/
struct ChannelParams
{
    std::atomic<float> gain { 1.0f };    // linear gain
    std::atomic<float> pan  { 0.0f };    // -1 = hard left, 0 = centre, +1 = hard right
    std::atomic<bool>  mute { false };
    std::atomic<bool>  solo { false };

    // Peak levels, written by the audio thread and read (then reset) by the meters.
    std::atomic<float> peakLeft  { 0.0f };
    std::atomic<float> peakRight { 0.0f };
};

struct PanGains
{
    float left  = 1.0f;
    float right = 1.0f;
};

/** Balance pan law: both sides are at unity in the centre, and the opposite side
    fades out as the control moves towards one side. Unlike a constant-power law,
    this keeps a centred track at exactly the level it was recorded at, which is
    what beginners expect.
*/
inline PanGains panToGains (float pan) noexcept
{
    pan = std::clamp (pan, -1.0f, 1.0f);
    return { pan > 0.0f ? 1.0f - pan : 1.0f,
             pan < 0.0f ? 1.0f + pan : 1.0f };
}

/** Raises a stored peak to `level` if it is higher. Called on the audio thread. */
inline void updatePeak (std::atomic<float>& peak, float level) noexcept
{
    if (level > peak.load (std::memory_order_relaxed))
        peak.store (level, std::memory_order_relaxed);
}

} // namespace sas
