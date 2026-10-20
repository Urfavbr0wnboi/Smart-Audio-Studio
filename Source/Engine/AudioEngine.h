#pragma once

#include "MixEngine.h"
#include "Project.h"

#include <juce_audio_devices/juce_audio_devices.h>

namespace sas
{

/** Connects the mixer to the computer's sound card, and owns the project.

    On start-up it opens the system's default output, so a beginner never has to
    think about drivers before they hear something.
*/
class AudioEngine final : private juce::AudioIODeviceCallback,
                          private juce::AsyncUpdater,
                          private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    /** Opens the default audio device. Returns an error message, or an empty string on success. */
    juce::String openDefaultDevice();

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    MixEngine& getMixer() noexcept                        { return mixer; }
    Project& getProject() noexcept                        { return project; }

    /** A short summary such as "Speakers · 48 kHz · 10.7 ms", for the status bar. */
    juce::String getDeviceDescription() const;
    bool hasOutputDevice() const;

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

    void handleAsyncUpdate() override;
    void timerCallback() override;

    juce::AudioDeviceManager deviceManager;
    MixEngine mixer;
    Project project { mixer };

    juce::AudioBuffer<float> mixBuffer;
    std::atomic<double> deviceSampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace sas
