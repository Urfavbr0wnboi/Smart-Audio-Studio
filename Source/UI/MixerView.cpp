#include "MixerView.h"
#include "StudioLookAndFeel.h"

namespace sas
{

MixerView::MixerView (Project& p, MixEngine& mixer)
    : project (p), masterStrip (mixer.getMasterParams())
{
    viewport.setViewedComponent (&stripHolder, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);
    addAndMakeVisible (masterStrip);

    project.addListener (this);
    rebuildStrips();
}

MixerView::~MixerView()
{
    project.removeListener (this);
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void MixerView::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromTop (1);

    masterStrip.setBounds (area.removeFromRight (ChannelStrip::width));
    area.removeFromRight (8);
    viewport.setBounds (area);

    const auto stripHeight = area.getHeight() - (viewport.isHorizontalScrollBarShown() ? viewport.getScrollBarThickness() : 0);
    stripHolder.setSize ((int) strips.size() * ChannelStrip::width, stripHeight);

    for (size_t i = 0; i < strips.size(); ++i)
        strips[i]->setBounds ((int) i * ChannelStrip::width, 0, ChannelStrip::width, stripHeight);
}

void MixerView::projectStructureChanged()
{
    std::vector<std::pair<int, juce::String>> current;

    for (const auto& track : project.getTracks())
        current.emplace_back (track.id, track.name);

    if (current != builtFor)
        rebuildStrips();
}

void MixerView::trackStateChanged (int)
{
    // Solo on one track changes what every other track's buttons mean, so refresh them all.
    for (auto& strip : strips)
        strip->refreshState();
}

void MixerView::rebuildStrips()
{
    strips.clear();
    builtFor.clear();

    for (const auto& track : project.getTracks())
    {
        auto& strip = strips.emplace_back (std::make_unique<ChannelStrip> (project, track));
        stripHolder.addAndMakeVisible (*strip);
        builtFor.emplace_back (track.id, track.name);
    }

    resized();
}

} // namespace sas
