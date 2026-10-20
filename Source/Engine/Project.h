#pragma once

#include "AudioFileLoader.h"
#include "ChannelParams.h"
#include "MixEngine.h"
#include "Waveform.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace sas
{

/** A piece of audio placed on a track's timeline. */
struct Clip
{
    int id = 0;
    juce::String name;
    juce::File sourceFile;
    double startSeconds = 0.0;

    SharedAudio sourceAudio;              // as decoded from the file
    double sourceSampleRate = 44100.0;
    SharedAudio playbackAudio;            // converted to the project's sample rate
    std::shared_ptr<const Waveform> waveform;

    double getLengthSeconds() const noexcept { return sourceAudio->getNumSamples() / sourceSampleRate; }
    double getEndSeconds() const noexcept    { return startSeconds + getLengthSeconds(); }
};

/** One row of the arrangement, with its own channel strip in the mixer. */
struct Track
{
    int id = 0;
    juce::String name;
    int colourIndex = 0;
    std::shared_ptr<ChannelParams> params = std::make_shared<ChannelParams>();
    std::vector<Clip> clips;
};

/** The document the user is editing: its tracks and the clips on them.

    The Project lives on the message thread. After every structural edit it
    publishes a fresh PlaybackGraph to the MixEngine, so playback always matches
    what is on screen without the audio thread ever waiting for the editor.
*/
class Project
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        /** Tracks or clips were added, removed, moved or renamed. */
        virtual void projectStructureChanged() {}

        /** A track's mute or solo state changed. */
        virtual void trackStateChanged (int /*trackId*/) {}
    };

    explicit Project (MixEngine& mixer);

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    // Tracks -------------------------------------------------------------------
    const std::vector<Track>& getTracks() const noexcept { return tracks; }
    const Track* findTrack (int trackId) const noexcept;

    /** Adds a track at the bottom and returns its id. */
    int addTrack (const juce::String& name = {});
    void removeTrack (int trackId);
    void renameTrack (int trackId, const juce::String& newName);
    void setTrackMuted (int trackId, bool shouldBeMuted);
    void setTrackSoloed (int trackId, bool shouldBeSoloed);

    // Clips --------------------------------------------------------------------
    const Clip* findClip (int trackId, int clipId) const noexcept;

    /** Decodes an audio file and places it on a track. */
    juce::Result addClipFromFile (int trackId, const juce::File& file, double startSeconds, int* newClipId = nullptr);

    /** Places audio that is already in memory on a track and returns the new clip's id. */
    int addClip (int trackId, const juce::String& name, SharedAudio audio, double audioSampleRate, double startSeconds);

    void moveClip (int trackId, int clipId, double newStartSeconds);

    /** Moves a clip onto a different track (or the same one) at a new time. */
    void moveClipToTrack (int fromTrackId, int clipId, int toTrackId, double newStartSeconds);

    void removeClip (int trackId, int clipId);

    // Timing -------------------------------------------------------------------
    /** Converts every clip to a new playback rate, e.g. after the audio device changes. */
    void setSampleRate (double newSampleRate);
    double getSampleRate() const noexcept { return sampleRate; }

    /** The end of the last clip, in seconds. */
    double getLengthSeconds() const noexcept;

private:
    Track* findTrackForEdit (int trackId) noexcept;
    Clip* findClipForEdit (int trackId, int clipId) noexcept;
    void publish();

    MixEngine& mixer;
    juce::AudioFormatManager formatManager;
    std::vector<Track> tracks;
    double sampleRate;
    int nextId = 1;
    int nextColourIndex = 0;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Project)
};

} // namespace sas
