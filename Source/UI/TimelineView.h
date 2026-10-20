#pragma once

#include "Engine/Project.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sas
{

/** The arrangement: a time ruler, one row per track (header + lane of clips),
    and the playhead.

    Clips can be dragged along their lane or onto another track, and audio files
    can be dropped in from the browser or from the operating system.
*/
class TimelineView final : public juce::Component,
                           public juce::DragAndDropTarget,
                           public juce::FileDragAndDropTarget,
                           private Project::Listener,
                           private juce::ScrollBar::Listener,
                           private juce::Timer
{
public:
    TimelineView (Project&, MixEngine&);
    ~TimelineView() override;

    /** Called with a message for the status line. */
    std::function<void (const juce::String& message, bool isError)> onStatusMessage;

    /** Places files on a track, starting at `startSeconds`. The first file goes on
        `trackId` (or a new track if it is 0) and any others go on new tracks below.
    */
    void importFiles (const juce::Array<juce::File>& files, int trackId, double startSeconds);

    /** Places files at the playhead, on the selected track if there is one. */
    void importFilesAtPlayhead (const juce::Array<juce::File>& files);

    /** Removes the selected clip. Returns false if no clip was selected. */
    bool deleteSelectedClip();

    /** The track that was last clicked, or 0 if none is selected. */
    int getSelectedTrackId() const noexcept { return selectedTrackId; }
    void selectTrack (int trackId);

    /** Shows a recording in progress on a track, from `startSeconds` to the
        playhead. Pass 0 to hide it.
    */
    void showRecording (int trackId, double startSeconds);

    void zoom (double factor);

    // juce::Component
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

    // juce::DragAndDropTarget (files dragged from the browser)
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    // juce::FileDragAndDropTarget (files dragged from the operating system)
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    class Ruler;
    class TrackHeader;
    class TrackLane;

    struct DropLocation
    {
        int trackId = 0;            // 0 means "on a new track"
        int rowIndex = 0;
        double seconds = 0.0;
    };

    void projectStructureChanged() override;
    void trackStateChanged (int trackId) override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void timerCallback() override;

    void rebuildRows();
    void layoutRows();
    void updateScrollBars();
    void repaintTimeline();

    void zoomAround (double factor, float anchorLaneX);
    void confirmRemoveTrack (int trackId);
    void selectClip (int trackId, int clipId);
    void setScrollSeconds (double seconds);
    void setScrollY (int y);
    void seekTo (double seconds);

    juce::Rectangle<int> getRowArea() const;
    juce::Rectangle<int> getLaneArea() const;
    double laneXToSeconds (float laneX) const noexcept;
    float secondsToLaneX (double seconds) const noexcept;
    float getPlayheadX() const;   // in TimelineView coordinates
    double getContentLengthSeconds() const;

    DropLocation getDropLocation (juce::Point<int>) const;
    void showDropTarget (juce::Point<int>);
    void hideDropTarget();
    static bool isAudioFileName (const juce::String& path, juce::AudioFormatManager&);

    Project& project;
    MixEngine& mixer;

    std::unique_ptr<Ruler> ruler;
    juce::Component rowArea;   // clips the rows to the space between the ruler and the scrollbar
    std::vector<std::unique_ptr<TrackHeader>> headers;
    std::vector<std::unique_ptr<TrackLane>> lanes;
    std::vector<int> rowTrackIds;
    juce::ScrollBar horizontalScrollBar { false }, verticalScrollBar { true };

    double pixelsPerSecond = 40.0;
    double scrollSeconds = 0.0;
    int scrollY = 0;

    int selectedTrackId = 0;
    int recordingTrackId = 0;
    double recordingStartSeconds = 0.0;
    int selectedClipTrackId = 0, selectedClipId = 0;

    std::optional<DropLocation> dropTarget;
    float lastPlayheadX = -1.0f;
    juce::int64 lastPosition = -1;
    juce::ScopedMessageBox confirmBox;

    static constexpr int headerWidth = 190;
    static constexpr int rulerHeight = 28;
    static constexpr int trackHeight = 90;
    static constexpr int scrollBarThickness = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineView)
};

} // namespace sas
