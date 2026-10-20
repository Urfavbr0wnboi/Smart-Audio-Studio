#pragma once

#include "ChannelStrip.h"

namespace sas
{

/** The docked mixer: one channel strip per track, with the master strip pinned on the right. */
class MixerView final : public juce::Component,
                        private Project::Listener
{
public:
    MixerView (Project&, MixEngine&);
    ~MixerView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void projectStructureChanged() override;
    void trackStateChanged (int trackId) override;
    void rebuildStrips();

    Project& project;
    juce::Viewport viewport;
    juce::Component stripHolder;
    std::vector<std::unique_ptr<ChannelStrip>> strips;
    ChannelStrip masterStrip;

    // The tracks the strips were built for, so unrelated edits don't rebuild them.
    std::vector<std::pair<int, juce::String>> builtFor;
};

} // namespace sas
