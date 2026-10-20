#include "TimelineView.h"
#include "BrowserPanel.h"
#include "StudioLookAndFeel.h"

namespace sas
{

namespace
{
    struct GridSpacing
    {
        double interval;        // seconds between labelled ticks
        int minorDivisions;     // unlabelled ticks per interval
    };

    /** Picks a ruler spacing that keeps labels at least ~70 pixels apart. */
    GridSpacing chooseGridSpacing (double pixelsPerSecond)
    {
        static constexpr GridSpacing spacings[] {
            { 0.1, 2 }, { 0.25, 5 }, { 0.5, 5 }, { 1.0, 4 }, { 2.0, 4 }, { 5.0, 5 }, { 10.0, 5 },
            { 15.0, 3 }, { 30.0, 3 }, { 60.0, 4 }, { 120.0, 4 }, { 300.0, 5 }, { 600.0, 5 },
        };

        for (const auto& spacing : spacings)
            if (spacing.interval * pixelsPerSecond >= 70.0)
                return spacing;

        return { 1200.0, 4 };
    }

    juce::String formatRulerTime (double seconds, double interval)
    {
        const auto minutes = (int) (seconds / 60.0);
        const auto remainder = seconds - minutes * 60.0;
        const auto secondsText = interval < 1.0 ? juce::String (remainder, interval < 0.5 ? 2 : 1)
                                                : juce::String ((int) std::round (remainder));

        return juce::String (minutes) + ":" + (remainder < 10.0 ? "0" : "") + secondsText;
    }

    juce::File fileFromDragSource (const juce::DragAndDropTarget::SourceDetails& details)
    {
        if (details.description.toString() != BrowserPanel::dragDescription)
            return {};

        auto* source = details.sourceComponent.get();
        auto* tree = dynamic_cast<juce::FileTreeComponent*> (source);

        if (tree == nullptr && source != nullptr)
            tree = source->findParentComponentOfClass<juce::FileTreeComponent>();

        return tree != nullptr ? tree->getSelectedFile() : juce::File();
    }
}

//==============================================================================
class TimelineView::Ruler final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    explicit Ruler (TimelineView& o) : owner (o)
    {
        setTooltip ("Click or drag to move the playhead");
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);

        g.setColour (Theme::textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("TRACKS", juce::Rectangle<int> (12, 0, headerWidth - 12, getHeight()), juce::Justification::centredLeft);

        g.setColour (Theme::outline);
        g.drawVerticalLine (headerWidth - 1, 0.0f, (float) getHeight());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        g.reduceClipRegion (getLocalBounds().withTrimmedLeft (headerWidth));

        const auto spacing = chooseGridSpacing (owner.pixelsPerSecond);
        const auto interval = spacing.interval;
        const auto minorInterval = interval / spacing.minorDivisions;
        const auto first = (juce::int64) std::floor (owner.scrollSeconds / minorInterval);

        g.setFont (juce::FontOptions (11.0f));

        for (auto i = first;; ++i)
        {
            const auto seconds = (double) i * minorInterval;
            const auto x = (float) headerWidth + owner.secondsToLaneX (seconds);

            if (x > (float) getWidth())
                break;

            const auto isMajor = i % spacing.minorDivisions == 0;

            g.setColour (isMajor ? Theme::textDim : Theme::outline);
            g.drawVerticalLine ((int) x, isMajor ? (float) getHeight() * 0.45f : (float) getHeight() * 0.7f, (float) getHeight());

            if (isMajor)
                g.drawText (formatRulerTime (seconds, interval), juce::Rectangle<float> (x + 4.0f, 2.0f, 60.0f, 14.0f),
                            juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { seek (e); }
    void mouseDrag (const juce::MouseEvent& e) override { seek (e); }

private:
    void seek (const juce::MouseEvent& e)
    {
        if (e.x >= headerWidth)
            owner.seekTo (owner.laneXToSeconds ((float) (e.x - headerWidth)));
    }

    TimelineView& owner;
};

//==============================================================================
class TimelineView::TrackHeader final : public juce::Component
{
public:
    TrackHeader (TimelineView& o, const Track& track)
        : owner (o), trackId (track.id), colour (Theme::trackColour (track.colourIndex))
    {
        nameLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        nameLabel.setEditable (false, true, false);
        nameLabel.setTooltip ("Double-click to rename the track");
        nameLabel.onTextChange = [this] { owner.project.renameTrack (trackId, nameLabel.getText()); };
        nameLabel.addMouseListener (this, false);
        addAndMakeVisible (nameLabel);

        muteButton.setColour (juce::TextButton::buttonOnColourId, Theme::mute);
        muteButton.setTooltip ("Mute: silence this track");
        muteButton.onClick = [this] { toggle (&ChannelParams::mute); };

        soloButton.setColour (juce::TextButton::buttonOnColourId, Theme::solo);
        soloButton.setTooltip ("Solo: hear only this track (and any other soloed tracks)");
        soloButton.onClick = [this] { toggle (&ChannelParams::solo); };

        removeButton.setTooltip ("Delete this track");
        removeButton.onClick = [this] { owner.confirmRemoveTrack (trackId); };

        for (auto* button : { &muteButton, &soloButton, &removeButton })
        {
            button->setWantsKeyboardFocus (false);
            addAndMakeVisible (button);
        }

        refresh (track);
    }

    int getTrackId() const noexcept { return trackId; }

    void refresh (const Track& track)
    {
        if (! nameLabel.isBeingEdited())
            nameLabel.setText (track.name, juce::dontSendNotification);

        muteButton.setToggleState (track.params->mute.load(), juce::dontSendNotification);
        soloButton.setToggleState (track.params->solo.load(), juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        const auto isSelected = owner.selectedTrackId == trackId;
        g.fillAll (isSelected ? Theme::panelRaised.brighter (0.06f) : Theme::panelRaised);

        g.setColour (colour);
        g.fillRect (0, 0, 5, getHeight());

        g.setColour (Theme::outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (5).reduced (8, 10);
        nameLabel.setBounds (area.removeFromTop (24));

        auto buttons = area.removeFromBottom (26);
        muteButton.setBounds (buttons.removeFromLeft (30));
        buttons.removeFromLeft (4);
        soloButton.setBounds (buttons.removeFromLeft (30));
        removeButton.setBounds (buttons.removeFromRight (26));
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        owner.selectTrack (trackId);
    }

private:
    void toggle (std::atomic<bool> ChannelParams::* flag)
    {
        if (const auto* track = owner.project.findTrack (trackId))
        {
            const auto newState = ! ((*track->params).*flag).load();

            if (flag == &ChannelParams::mute)
                owner.project.setTrackMuted (trackId, newState);
            else
                owner.project.setTrackSoloed (trackId, newState);
        }
    }

    TimelineView& owner;
    const int trackId;
    const juce::Colour colour;

    juce::Label nameLabel;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    juce::TextButton removeButton { juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) };
};

//==============================================================================
class TimelineView::TrackLane final : public juce::Component
{
public:
    TrackLane (TimelineView& o, int id) : owner (o), trackId (id) {}

    void paint (juce::Graphics& g) override
    {
        const auto* track = owner.project.findTrack (trackId);

        if (track == nullptr)
            return;

        g.fillAll (Theme::background);

        // Grid lines matching the ruler's major ticks.
        const auto interval = chooseGridSpacing (owner.pixelsPerSecond).interval;
        g.setColour (Theme::outline.withAlpha (0.4f));

        for (auto i = (juce::int64) std::floor (owner.scrollSeconds / interval);; ++i)
        {
            const auto x = owner.secondsToLaneX ((double) i * interval);

            if (x > (float) getWidth())
                break;

            g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
        }

        g.setColour (Theme::outline);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

        const auto colour = Theme::trackColour (track->colourIndex);

        for (const auto& clip : track->clips)
            drawClip (g, clip, colour, owner.selectedClipTrackId == trackId && owner.selectedClipId == clip.id);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (clipAt ((float) e.x) != 0 ? juce::MouseCursor::DraggingHandCursor
                                                  : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.selectTrack (trackId);
        drag.reset();

        if (const auto clipId = clipAt ((float) e.x); clipId != 0)
        {
            owner.selectClip (trackId, clipId);
            drag = ClipDrag { trackId, clipId, owner.project.findClip (trackId, clipId)->startSeconds };
        }
        else
        {
            owner.selectClip (0, 0);
            owner.seekTo (owner.laneXToSeconds ((float) e.x));
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! drag.has_value())
        {
            owner.seekTo (owner.laneXToSeconds ((float) e.x));
            return;
        }

        const auto newStart = std::max (0.0, drag->originalStart + e.getDistanceFromDragStartX() / owner.pixelsPerSecond);
        const auto location = owner.getDropLocation (e.getEventRelativeTo (&owner).getPosition());
        const auto targetTrack = location.trackId != 0 ? location.trackId : drag->currentTrackId;

        owner.project.moveClipToTrack (drag->currentTrackId, drag->clipId, targetTrack, newStart);
        drag->currentTrackId = targetTrack;
        owner.selectClip (targetTrack, drag->clipId);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        drag.reset();
    }

private:
    struct ClipDrag
    {
        int currentTrackId = 0;
        int clipId = 0;
        double originalStart = 0.0;
    };

    /** Returns the id of the topmost clip at a horizontal position, or 0. */
    int clipAt (float x) const
    {
        if (const auto* track = owner.project.findTrack (trackId))
        {
            const auto seconds = owner.laneXToSeconds (x);

            for (auto it = track->clips.rbegin(); it != track->clips.rend(); ++it)
                if (seconds >= it->startSeconds && seconds < it->getEndSeconds())
                    return it->id;
        }

        return 0;
    }

    void drawClip (juce::Graphics& g, const Clip& clip, juce::Colour colour, bool isSelected) const
    {
        const auto x1 = owner.secondsToLaneX (clip.startSeconds);
        const auto x2 = owner.secondsToLaneX (clip.getEndSeconds());

        if (x2 < 0.0f || x1 > (float) getWidth())
            return;

        const auto bounds = juce::Rectangle<float> (x1, 3.0f, std::max (2.0f, x2 - x1), (float) getHeight() - 7.0f);

        g.setColour (colour.withAlpha (isSelected ? 0.42f : 0.26f));
        g.fillRoundedRectangle (bounds, 4.0f);

        // Title bar.
        auto body = bounds;
        const auto titleBar = body.removeFromTop (16.0f);
        g.setColour (colour.withAlpha (0.85f));
        g.fillRoundedRectangle (titleBar, 4.0f);
        g.fillRect (titleBar.withTrimmedTop (8.0f));

        g.setColour (Theme::background);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (clip.name, titleBar.withTrimmedLeft (std::max (0.0f, -x1) + 6.0f).withTrimmedRight (4.0f),
                    juce::Justification::centredLeft, true);

        // Waveform, one column per pixel.
        body.reduce (0.0f, 4.0f);
        const auto centreY = body.getCentreY();
        const auto halfHeight = body.getHeight() / 2.0f;
        const auto samplesPerPixel = clip.sourceSampleRate / owner.pixelsPerSecond;
        const auto firstPixel = std::max ((int) x1, 0);
        const auto lastPixel  = std::min ((int) std::ceil (x2), getWidth());

        g.setColour (colour.brighter (0.3f));

        for (int px = firstPixel; px < lastPixel; ++px)
        {
            const auto start = (juce::int64) (((float) px - x1) * samplesPerPixel);
            const auto range = clip.waveform->getRange (start, start + (juce::int64) samplesPerPixel + 1);
            const auto top    = centreY - juce::jlimit (-1.0f, 1.0f, range.getEnd())   * halfHeight;
            const auto bottom = centreY - juce::jlimit (-1.0f, 1.0f, range.getStart()) * halfHeight;
            g.fillRect ((float) px, top, 1.0f, std::max (1.0f, bottom - top));
        }

        g.setColour (isSelected ? Theme::text : colour);
        g.drawRoundedRectangle (bounds, 4.0f, isSelected ? 2.0f : 1.0f);
    }

    TimelineView& owner;
    const int trackId;
    std::optional<ClipDrag> drag;
};

//==============================================================================
TimelineView::TimelineView (Project& p, MixEngine& m)
    : project (p), mixer (m), ruler (std::make_unique<Ruler> (*this))
{
    rowArea.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (rowArea);
    addAndMakeVisible (*ruler);

    for (auto* bar : { &horizontalScrollBar, &verticalScrollBar })
    {
        bar->setAutoHide (false);
        bar->addListener (this);
        addAndMakeVisible (bar);
    }

    project.addListener (this);
    rebuildRows();
    startTimerHz (30);
}

TimelineView::~TimelineView()
{
    project.removeListener (this);
}

//==============================================================================
void TimelineView::importFiles (const juce::Array<juce::File>& files, int trackId, double startSeconds)
{
    int added = 0;
    juce::String lastError;
    auto target = trackId;

    for (const auto& file : files)
    {
        const auto createdTrack = target == 0 || project.findTrack (target) == nullptr;

        if (createdTrack)
            target = project.addTrack (file.getFileNameWithoutExtension());

        int clipId = 0;
        const auto result = project.addClipFromFile (target, file, startSeconds, &clipId);

        if (result.wasOk())
        {
            ++added;
            selectTrack (target);
            selectClip (target, clipId);
        }
        else
        {
            lastError = result.getErrorMessage();

            if (createdTrack)
                project.removeTrack (target);
        }

        target = 0;   // any further files go on new tracks
    }

    if (onStatusMessage == nullptr)
        return;

    if (lastError.isNotEmpty())
        onStatusMessage (lastError, true);
    else if (added > 0)
        onStatusMessage (added == 1 ? "Added " + files.getFirst().getFileName()
                                    : "Added " + juce::String (added) + " files", false);
}

void TimelineView::importFilesAtPlayhead (const juce::Array<juce::File>& files)
{
    importFiles (files, selectedTrackId, mixer.getPositionSeconds());
}

bool TimelineView::deleteSelectedClip()
{
    if (project.findClip (selectedClipTrackId, selectedClipId) == nullptr)
        return false;

    project.removeClip (selectedClipTrackId, selectedClipId);
    selectClip (0, 0);
    return true;
}

void TimelineView::confirmRemoveTrack (int trackId)
{
    const auto* track = project.findTrack (trackId);

    if (track == nullptr)
        return;

    if (track->clips.empty())
    {
        project.removeTrack (trackId);
        return;
    }

    const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
        juce::MessageBoxIconType::QuestionIcon,
        "Delete \"" + track->name + "\"?",
        "This track and its " + juce::String (track->clips.size()) + " clip(s) will be removed. This cannot be undone yet.",
        "Delete", "Cancel", this);

    confirmBox = juce::AlertWindow::showScopedAsync (options, [this, trackId] (int result)
    {
        if (result == 1)
            project.removeTrack (trackId);
    });
}

void TimelineView::zoom (double factor)
{
    zoomAround (factor, (float) getLaneArea().getWidth() / 2.0f);
}

void TimelineView::zoomAround (double factor, float anchorLaneX)
{
    const auto anchorSeconds = laneXToSeconds (anchorLaneX);
    pixelsPerSecond = juce::jlimit (2.0, 2000.0, pixelsPerSecond * factor);
    scrollSeconds = std::max (0.0, anchorSeconds - anchorLaneX / pixelsPerSecond);
    updateScrollBars();
    repaint();
}

//==============================================================================
void TimelineView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    // The strip below the last track, where dropped files create a new track.
    g.setColour (Theme::panel);
    g.fillRect (getRowArea().withWidth (headerWidth));
}

void TimelineView::paintOverChildren (juce::Graphics& g)
{
    const auto lane = getLaneArea();
    const auto hasClips = std::any_of (project.getTracks().begin(), project.getTracks().end(),
                                       [] (const auto& t) { return ! t.clips.empty(); });

    if (! hasClips && ! dropTarget.has_value())
    {
        g.setColour (Theme::textDim);
        g.setFont (juce::FontOptions (16.0f));
        g.drawFittedText ("Drag audio files here, or double-click one in the Browser.\n"
                          "Space plays and pauses. Delete removes the selected clip.",
                          lane.reduced (24), juce::Justification::centred, 3);
    }

    if (dropTarget.has_value())
    {
        const auto row = juce::Rectangle<int> (lane.getX(), lane.getY() + dropTarget->rowIndex * trackHeight - scrollY,
                                               lane.getWidth(), trackHeight).getIntersection (lane);
        g.setColour (Theme::accent.withAlpha (0.12f));
        g.fillRect (row);
        g.setColour (Theme::accent);
        g.drawRect (row, 1);

        const auto x = (float) lane.getX() + secondsToLaneX (dropTarget->seconds);
        g.drawVerticalLine ((int) x, (float) row.getY(), (float) row.getBottom());

        if (dropTarget->trackId == 0)
            g.drawText ("New track", row.reduced (10, 0), juce::Justification::centredLeft);
    }

    const auto playheadX = getPlayheadX();

    if (playheadX >= (float) lane.getX() && playheadX <= (float) lane.getRight())
    {
        g.setColour (Theme::playhead);
        g.drawVerticalLine ((int) playheadX, 0.0f, (float) lane.getBottom());

        juce::Path marker;
        marker.addTriangle (playheadX - 5.0f, 0.0f, playheadX + 5.0f, 0.0f, playheadX, 7.0f);
        g.fillPath (marker);
    }
}

void TimelineView::resized()
{
    const auto bounds = getLocalBounds();

    ruler->setBounds (bounds.withHeight (rulerHeight).withTrimmedRight (scrollBarThickness));
    rowArea.setBounds (getRowArea());
    horizontalScrollBar.setBounds (headerWidth, bounds.getBottom() - scrollBarThickness,
                                   bounds.getWidth() - headerWidth - scrollBarThickness, scrollBarThickness);
    verticalScrollBar.setBounds (bounds.getRight() - scrollBarThickness, rulerHeight,
                                 scrollBarThickness, bounds.getHeight() - rulerHeight - scrollBarThickness);

    layoutRows();
    updateScrollBars();
}

void TimelineView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto mouseLaneX = e.position.x - (float) getLaneArea().getX();

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoomAround (std::pow (2.0, wheel.deltaY * 2.0), mouseLaneX);
        return;
    }

    const auto horizontal = e.mods.isShiftDown() ? wheel.deltaY : wheel.deltaX;
    const auto vertical   = e.mods.isShiftDown() ? 0.0f : wheel.deltaY;

    if (! juce::exactlyEqual (horizontal, 0.0f))
        setScrollSeconds (scrollSeconds - horizontal * 300.0 / pixelsPerSecond);

    if (! juce::exactlyEqual (vertical, 0.0f))
        setScrollY (scrollY - juce::roundToInt (vertical * 300.0f));
}

void TimelineView::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoomAround (scaleFactor, e.position.x - (float) getLaneArea().getX());
}

//==============================================================================
bool TimelineView::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString() == BrowserPanel::dragDescription;
}

void TimelineView::itemDragMove (const SourceDetails& details)
{
    showDropTarget (details.localPosition);
}

void TimelineView::itemDragExit (const SourceDetails&)
{
    hideDropTarget();
}

void TimelineView::itemDropped (const SourceDetails& details)
{
    const auto location = getDropLocation (details.localPosition);
    hideDropTarget();

    if (const auto file = fileFromDragSource (details); file.existsAsFile())
        importFiles ({ file }, location.trackId, location.seconds);
}

bool TimelineView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(),
                        [this] (const auto& path) { return isAudioFileName (path, project.getFormatManager()); });
}

void TimelineView::fileDragMove (const juce::StringArray&, int x, int y)
{
    showDropTarget ({ x, y });
}

void TimelineView::fileDragExit (const juce::StringArray&)
{
    hideDropTarget();
}

void TimelineView::filesDropped (const juce::StringArray& paths, int x, int y)
{
    const auto location = getDropLocation ({ x, y });
    hideDropTarget();

    juce::Array<juce::File> files;

    for (const auto& path : paths)
        if (isAudioFileName (path, project.getFormatManager()))
            files.add (juce::File (path));

    importFiles (files, location.trackId, location.seconds);
}

bool TimelineView::isAudioFileName (const juce::String& path, juce::AudioFormatManager& formats)
{
    return formats.findFormatForFileExtension (juce::File (path).getFileExtension()) != nullptr;
}

//==============================================================================
void TimelineView::projectStructureChanged()
{
    std::vector<int> ids;

    for (const auto& track : project.getTracks())
        ids.push_back (track.id);

    // Only rebuild when tracks come or go. Rebuilding while a clip is being
    // dragged would delete the component that is handling the drag.
    if (ids != rowTrackIds)
    {
        rebuildRows();
    }
    else
    {
        for (auto& header : headers)
            if (const auto* track = project.findTrack (header->getTrackId()))
                header->refresh (*track);
    }

    if (project.findTrack (selectedTrackId) == nullptr)
        selectedTrackId = 0;

    if (project.findClip (selectedClipTrackId, selectedClipId) == nullptr)
        selectedClipTrackId = selectedClipId = 0;

    updateScrollBars();
    repaint();
}

void TimelineView::trackStateChanged (int)
{
    for (auto& header : headers)
        if (const auto* track = project.findTrack (header->getTrackId()))
            header->refresh (*track);
}

void TimelineView::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &horizontalScrollBar)
        setScrollSeconds (newRangeStart);
    else
        setScrollY (juce::roundToInt (newRangeStart));
}

void TimelineView::timerCallback()
{
    const auto position = mixer.getPosition();
    const auto lane = getLaneArea();
    auto x = getPlayheadX();

    // Follow the playhead when it moves out of view, e.g. while playing or after
    // "return to start", but leave the view alone when the user scrolls away.
    if (position != lastPosition && (x < (float) lane.getX() || x > (float) lane.getRight() - 10.0f))
    {
        setScrollSeconds (mixer.getPositionSeconds() - (mixer.isPlaying() ? 0.0 : 1.0));
        x = getPlayheadX();
    }

    lastPosition = position;

    if (! juce::approximatelyEqual (x, lastPlayheadX))
    {
        repaint ((int) lastPlayheadX - 6, 0, 13, getHeight());
        repaint ((int) x - 6, 0, 13, getHeight());
        lastPlayheadX = x;
    }
}

//==============================================================================
void TimelineView::rebuildRows()
{
    headers.clear();
    lanes.clear();
    rowTrackIds.clear();

    for (const auto& track : project.getTracks())
    {
        auto& header = headers.emplace_back (std::make_unique<TrackHeader> (*this, track));
        auto& lane   = lanes.emplace_back (std::make_unique<TrackLane> (*this, track.id));
        rowArea.addAndMakeVisible (*header);
        rowArea.addAndMakeVisible (*lane);
        rowTrackIds.push_back (track.id);
    }

    setScrollY (scrollY);
    layoutRows();
}

void TimelineView::layoutRows()
{
    for (size_t i = 0; i < headers.size(); ++i)
    {
        const auto y = (int) i * trackHeight - scrollY;
        headers[i]->setBounds (0, y, headerWidth, trackHeight);
        lanes[i]->setBounds (headerWidth, y, rowArea.getWidth() - headerWidth, trackHeight);
    }
}

void TimelineView::updateScrollBars()
{
    const auto lane = getLaneArea();
    const auto visibleSeconds = lane.getWidth() / pixelsPerSecond;

    horizontalScrollBar.setRangeLimits (0.0, std::max (getContentLengthSeconds(), scrollSeconds + visibleSeconds),
                                        juce::dontSendNotification);
    horizontalScrollBar.setCurrentRange (scrollSeconds, visibleSeconds, juce::dontSendNotification);

    // One spare row below the last track gives somewhere to drop a new track.
    const auto contentHeight = ((int) rowTrackIds.size() + 1) * trackHeight;
    verticalScrollBar.setRangeLimits (0.0, std::max (contentHeight, scrollY + lane.getHeight()), juce::dontSendNotification);
    verticalScrollBar.setCurrentRange (scrollY, lane.getHeight(), juce::dontSendNotification);
}

void TimelineView::selectTrack (int trackId)
{
    if (selectedTrackId != trackId)
    {
        selectedTrackId = trackId;

        for (auto& header : headers)
            header->repaint();
    }
}

void TimelineView::selectClip (int trackId, int clipId)
{
    if (selectedClipTrackId != trackId || selectedClipId != clipId)
    {
        selectedClipTrackId = trackId;
        selectedClipId = clipId;
        rowArea.repaint();
    }
}

void TimelineView::setScrollSeconds (double seconds)
{
    seconds = std::max (0.0, seconds);

    if (! juce::approximatelyEqual (seconds, scrollSeconds))
    {
        scrollSeconds = seconds;
        updateScrollBars();
        repaint();
    }
}

void TimelineView::setScrollY (int y)
{
    const auto contentHeight = ((int) rowTrackIds.size() + 1) * trackHeight;
    y = juce::jlimit (0, std::max (0, contentHeight - getLaneArea().getHeight()), y);

    if (y != scrollY)
    {
        scrollY = y;
        layoutRows();
        updateScrollBars();
        repaint();
    }
}

void TimelineView::seekTo (double seconds)
{
    mixer.setPosition ((juce::int64) std::llround (std::max (0.0, seconds) * mixer.getSampleRate()));
}

//==============================================================================
juce::Rectangle<int> TimelineView::getRowArea() const
{
    return getLocalBounds().withTrimmedTop (rulerHeight)
                           .withTrimmedBottom (scrollBarThickness)
                           .withTrimmedRight (scrollBarThickness);
}

juce::Rectangle<int> TimelineView::getLaneArea() const
{
    return getRowArea().withTrimmedLeft (headerWidth);
}

double TimelineView::laneXToSeconds (float laneX) const noexcept
{
    return scrollSeconds + laneX / pixelsPerSecond;
}

float TimelineView::secondsToLaneX (double seconds) const noexcept
{
    return (float) ((seconds - scrollSeconds) * pixelsPerSecond);
}

float TimelineView::getPlayheadX() const
{
    return (float) getLaneArea().getX() + secondsToLaneX (mixer.getPositionSeconds());
}

double TimelineView::getContentLengthSeconds() const
{
    return std::max (60.0, project.getLengthSeconds() + 30.0);
}

TimelineView::DropLocation TimelineView::getDropLocation (juce::Point<int> position) const
{
    const auto lane = getLaneArea();
    const auto numRows = (int) rowTrackIds.size();
    const auto row = juce::jlimit (0, numRows, (std::max (position.y, lane.getY()) - lane.getY() + scrollY) / trackHeight);

    DropLocation location;
    location.rowIndex = row;
    location.trackId = row < numRows ? rowTrackIds[(size_t) row] : 0;
    location.seconds = position.x > lane.getX() ? std::max (0.0, laneXToSeconds ((float) (position.x - lane.getX()))) : 0.0;
    return location;
}

void TimelineView::showDropTarget (juce::Point<int> position)
{
    dropTarget = getDropLocation (position);
    repaint();
}

void TimelineView::hideDropTarget()
{
    dropTarget.reset();
    repaint();
}

} // namespace sas
