#include "Project.h"

namespace sas
{

Project::Project (MixEngine& m)
    : mixer (m), sampleRate (m.getSampleRate())
{
    formatManager.registerBasicFormats();
    publish();
}

//==============================================================================
const Track* Project::findTrack (int trackId) const noexcept
{
    const auto it = std::find_if (tracks.begin(), tracks.end(), [=] (const auto& t) { return t.id == trackId; });
    return it != tracks.end() ? &*it : nullptr;
}

Track* Project::findTrackForEdit (int trackId) noexcept
{
    return const_cast<Track*> (std::as_const (*this).findTrack (trackId));
}

int Project::addTrack (const juce::String& name)
{
    Track track;
    track.id = nextId++;
    track.name = name.isNotEmpty() ? name : "Track " + juce::String (tracks.size() + 1);
    track.colourIndex = nextColourIndex++;
    tracks.push_back (std::move (track));

    publish();
    return tracks.back().id;
}

void Project::removeTrack (int trackId)
{
    if (std::erase_if (tracks, [=] (const auto& t) { return t.id == trackId; }) > 0)
        publish();
}

void Project::renameTrack (int trackId, const juce::String& newName)
{
    if (auto* track = findTrackForEdit (trackId); track != nullptr && newName.trim().isNotEmpty())
    {
        track->name = newName.trim();
        listeners.call (&Listener::projectStructureChanged);
    }
}

void Project::setTrackMuted (int trackId, bool shouldBeMuted)
{
    if (auto* track = findTrackForEdit (trackId))
    {
        track->params->mute.store (shouldBeMuted);
        listeners.call (&Listener::trackStateChanged, trackId);
    }
}

void Project::setTrackSoloed (int trackId, bool shouldBeSoloed)
{
    if (auto* track = findTrackForEdit (trackId))
    {
        track->params->solo.store (shouldBeSoloed);
        listeners.call (&Listener::trackStateChanged, trackId);
    }
}

//==============================================================================
const Clip* Project::findClip (int trackId, int clipId) const noexcept
{
    if (const auto* track = findTrack (trackId))
        for (const auto& clip : track->clips)
            if (clip.id == clipId)
                return &clip;

    return nullptr;
}

Clip* Project::findClipForEdit (int trackId, int clipId) noexcept
{
    return const_cast<Clip*> (std::as_const (*this).findClip (trackId, clipId));
}

juce::Result Project::addClipFromFile (int trackId, const juce::File& file, double startSeconds, int* newClipId)
{
    if (findTrack (trackId) == nullptr)
        return juce::Result::fail ("That track no longer exists.");

    DecodedAudio decoded;
    const auto result = AudioFileLoader::load (formatManager, file, decoded);

    if (result.failed())
        return result;

    const auto clipId = addClip (trackId, file.getFileNameWithoutExtension(), decoded.audio, decoded.sampleRate, startSeconds);
    findClipForEdit (trackId, clipId)->sourceFile = file;

    if (newClipId != nullptr)
        *newClipId = clipId;

    return juce::Result::ok();
}

int Project::addClip (int trackId, const juce::String& name, SharedAudio audio, double audioSampleRate, double startSeconds)
{
    auto* track = findTrackForEdit (trackId);

    if (track == nullptr || audio == nullptr)
    {
        jassertfalse;
        return 0;
    }

    Clip clip;
    clip.id = nextId++;
    clip.name = name;
    clip.startSeconds = std::max (0.0, startSeconds);
    clip.sourceAudio = audio;
    clip.sourceSampleRate = audioSampleRate;
    clip.playbackAudio = AudioFileLoader::resample (audio, audioSampleRate, sampleRate);
    clip.waveform = std::make_shared<Waveform> (*audio);
    track->clips.push_back (std::move (clip));

    publish();
    return track->clips.back().id;
}

void Project::moveClip (int trackId, int clipId, double newStartSeconds)
{
    if (auto* clip = findClipForEdit (trackId, clipId))
    {
        clip->startSeconds = std::max (0.0, newStartSeconds);
        publish();
    }
}

void Project::moveClipToTrack (int fromTrackId, int clipId, int toTrackId, double newStartSeconds)
{
    if (fromTrackId == toTrackId)
    {
        moveClip (fromTrackId, clipId, newStartSeconds);
        return;
    }

    auto* from = findTrackForEdit (fromTrackId);
    auto* to   = findTrackForEdit (toTrackId);

    if (from == nullptr || to == nullptr)
        return;

    const auto it = std::find_if (from->clips.begin(), from->clips.end(), [=] (const auto& c) { return c.id == clipId; });

    if (it == from->clips.end())
        return;

    auto clip = std::move (*it);
    from->clips.erase (it);
    clip.startSeconds = std::max (0.0, newStartSeconds);
    to->clips.push_back (std::move (clip));
    publish();
}

void Project::removeClip (int trackId, int clipId)
{
    if (auto* track = findTrackForEdit (trackId))
        if (std::erase_if (track->clips, [=] (const auto& c) { return c.id == clipId; }) > 0)
            publish();
}

//==============================================================================
void Project::setSampleRate (double newSampleRate)
{
    if (newSampleRate <= 0.0 || juce::approximatelyEqual (newSampleRate, sampleRate))
        return;

    sampleRate = newSampleRate;

    for (auto& track : tracks)
        for (auto& clip : track.clips)
            clip.playbackAudio = AudioFileLoader::resample (clip.sourceAudio, clip.sourceSampleRate, sampleRate);

    publish();
}

double Project::getLengthSeconds() const noexcept
{
    double length = 0.0;

    for (const auto& track : tracks)
        for (const auto& clip : track.clips)
            length = std::max (length, clip.getEndSeconds());

    return length;
}

void Project::publish()
{
    auto graph = std::make_shared<PlaybackGraph>();
    graph->tracks.reserve (tracks.size());

    for (const auto& track : tracks)
    {
        auto& playbackTrack = graph->tracks.emplace_back();
        playbackTrack.params = track.params;

        for (const auto& clip : track.clips)
            playbackTrack.clips.push_back ({ clip.playbackAudio, (juce::int64) std::llround (clip.startSeconds * sampleRate) });
    }

    mixer.setGraph (std::move (graph));
    listeners.call (&Listener::projectStructureChanged);
}

} // namespace sas
