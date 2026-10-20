#include "LevelMeter.h"
#include "StudioLookAndFeel.h"

namespace sas
{

namespace
{
    constexpr int refreshHz = 30;
    constexpr float decayPerTick = 0.82f;   // roughly 20 dB per second at 30 Hz
    constexpr int holdTicks = refreshHz;    // peak markers hold for a second

    void applyBallistics (float newPeak, float& level, float& hold, int& ticks)
    {
        level = std::max (newPeak, level * decayPerTick);

        if (newPeak >= hold)
        {
            hold = newPeak;
            ticks = holdTicks;
        }
        else if (--ticks <= 0)
        {
            hold = level;
        }
    }
}

LevelMeter::LevelMeter (ChannelParams& p) : params (p)
{
    startTimerHz (refreshHz);
}

float LevelMeter::decibelsToProportion (float decibels) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (decibels - minDecibels) / (maxDecibels - minDecibels));
}

void LevelMeter::timerCallback()
{
    applyBallistics (params.peakLeft.exchange (0.0f),  levelLeft,  holdLeft,  holdTicksLeft);
    applyBallistics (params.peakRight.exchange (0.0f), levelRight, holdRight, holdTicksRight);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::background);
    g.fillRoundedRectangle (bounds, 2.0f);

    const auto gradient = juce::ColourGradient::vertical (juce::Colour (0xff66bb6a), bounds.getBottom(),
                                                          juce::Colour (0xffef5350), bounds.getY());
    auto withStops = gradient;
    withStops.addColour (decibelsToProportion (-12.0f), juce::Colour (0xff66bb6a));
    withStops.addColour (decibelsToProportion (-3.0f),  juce::Colour (0xffffca28));

    const auto drawBar = [&] (juce::Rectangle<float> area, float level, float hold)
    {
        const auto height = area.getHeight();
        const auto levelProportion = decibelsToProportion (juce::Decibels::gainToDecibels (level, minDecibels));
        const auto holdProportion  = decibelsToProportion (juce::Decibels::gainToDecibels (hold,  minDecibels));

        g.setGradientFill (withStops);
        g.fillRect (area.withTop (area.getBottom() - height * levelProportion));

        if (holdProportion > 0.0f)
            g.fillRect (area.withHeight (2.0f).withY (area.getBottom() - height * holdProportion));
    };

    bounds.reduce (1.0f, 1.0f);
    const auto barWidth = (bounds.getWidth() - 1.0f) / 2.0f;
    drawBar (bounds.withWidth (barWidth), levelLeft, holdLeft);
    drawBar (bounds.withTrimmedLeft (barWidth + 1.0f), levelRight, holdRight);

    // A tick at 0 dB, so clipping is easy to spot.
    g.setColour (Theme::textDim.withAlpha (0.6f));
    g.drawHorizontalLine ((int) (bounds.getBottom() - bounds.getHeight() * decibelsToProportion (0.0f)),
                          bounds.getX(), bounds.getRight());
}

} // namespace sas
