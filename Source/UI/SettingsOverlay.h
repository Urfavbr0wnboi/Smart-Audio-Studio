#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace sas
{

/** Audio device settings, shown as a panel over the main window rather than
    as a separate floating window.
*/
class SettingsOverlay final : public juce::Component
{
public:
    explicit SettingsOverlay (juce::AudioDeviceManager&);

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::Rectangle<int> getPanelBounds() const;
    void close();

    juce::Label title;
    juce::AudioDeviceSelectorComponent selector;
    juce::TextButton doneButton { "Done" };
};

} // namespace sas
