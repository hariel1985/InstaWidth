#include "CorrelationMeter.h"
#include "LookAndFeel.h"

namespace
{
    constexpr int kNumBands = 3;
    const char* kBandLabels[kNumBands] = { "LOW", "MID", "HIGH" };
}

CorrelationMeter::CorrelationMeter()
{
    setOpaque (true);
    startTimerHz (30);
}

CorrelationMeter::~CorrelationMeter() { stopTimer(); }

void CorrelationMeter::timerCallback()
{
    if (source == nullptr)
    {
        repaint();
        return;
    }

    const float overallTarget = source->getOverallCorrelation();
    displayCorrelation += (overallTarget - displayCorrelation) * 0.3f;

    for (int b = 0; b < kNumBands; ++b)
    {
        const float t = source->getBandCorrelation (b);
        bandCorr[b] += (t - bandCorr[b]) * 0.3f;
    }

    constexpr float kThreshold[kNumBands] = { -0.05f, -0.30f, -0.45f };
    constexpr int   kHoldTicks            = 10;
    constexpr int   kReleaseStep          = 2;

    for (int b = 0; b < kNumBands; ++b)
    {
        if (bandCorr[b] < kThreshold[b])
            negativeHold[b] = std::min (negativeHold[b] + 1, kHoldTicks * 3);
        else
            negativeHold[b] = std::max (0, negativeHold[b] - kReleaseStep);

        bandTriggered[b] = negativeHold[b] >= kHoldTicks;
    }

    const bool anyTriggered = bandTriggered[0] || bandTriggered[1] || bandTriggered[2];
    if (anyTriggered) warnFlash = std::min (1.0f, warnFlash + 0.12f);
    else              warnFlash = std::max (0.0f, warnFlash - 0.04f);

    repaint();
}

void CorrelationMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (juce::Colour (0xff0a0a14));
    g.fillRect (bounds);

    const int totalH    = bounds.getHeight();
    const int labelColW = 44;
    const int padding   = 3;
    const int overallH  = juce::jlimit (16, 26, totalH * 38 / 100);
    const int bandsH    = totalH - overallH - padding;
    const int perBandH  = (bandsH - padding * (kNumBands - 1)) / kNumBands;

    auto barColour = [] (float c) -> juce::Colour
    {
        if (c >= 0.5f) return juce::Colour (0xff00ff88);
        if (c >= 0.0f) return juce::Colour (0xffeedd44);
        return juce::Colour (0xffff4444);
    };

    auto drawCorrStrip = [&] (juce::Rectangle<int> area, float corrValue,
                              const juce::String& leftLabel, bool drawScale,
                              float safetyDuck)
    {
        if (leftLabel.isNotEmpty())
        {
            g.setColour (InstaWidthLookAndFeel::textSecondary);
            g.setFont (juce::FontOptions().withHeight ((float) std::min (12, area.getHeight() - 2)));
            g.drawText (leftLabel, area.removeFromLeft (labelColW).reduced (4, 0),
                        juce::Justification::centredLeft);
        }

        area.removeFromLeft (2);

        g.setColour (juce::Colour (0xff141422));
        g.fillRect (area);

        const float midX = (float) area.getCentreX();
        g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.6f));
        g.drawLine (midX, (float) area.getY(), midX, (float) area.getBottom(), 1.0f);

        const float c = juce::jlimit (-1.0f, 1.0f, corrValue);
        const float halfWidth = (area.getWidth() - 4.0f) * 0.5f;
        const float endX = midX + c * halfWidth;
        auto bar = (c >= 0)
                     ? juce::Rectangle<float> (midX, (float) area.getY() + 2.0f, endX - midX,
                                                (float) area.getHeight() - 4.0f)
                     : juce::Rectangle<float> (endX, (float) area.getY() + 2.0f, midX - endX,
                                                (float) area.getHeight() - 4.0f);
        g.setColour (barColour (c));
        g.fillRect (bar);

        // Auto-safety indicator: small cyan tick at the top of the bar showing how much
        // ducking is being applied right now (0 = none, full bar = full mono).
        if (safetyDuck > 0.01f)
        {
            const float w = (area.getWidth() - 4.0f) * safetyDuck;
            const float y = (float) area.getY();
            g.setColour (juce::Colour (0xff44ccff).withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> ((float) area.getX() + 2.0f, y, w, 2.0f));
        }

        g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.5f));
        g.drawRect (area, 1);

        if (drawScale)
        {
            g.setColour (InstaWidthLookAndFeel::textSecondary);
            g.setFont (juce::FontOptions().withHeight (9.0f));
            g.drawText ("-1", area.withTrimmedRight (area.getWidth() - 18),  juce::Justification::centredLeft);
            g.drawText ("0",  area.withSizeKeepingCentre (24, area.getHeight()), juce::Justification::centred);
            g.drawText ("+1", area.withTrimmedLeft  (area.getWidth() - 18),  juce::Justification::centredRight);
        }
    };

    auto getDuck = [this] (int band) -> float
    {
        if (! autoSafetyOn || ! safetyProvider) return 0.0f;
        return juce::jlimit (0.0f, 1.0f, 1.0f - safetyProvider (band));
    };

    auto stripArea = bounds;
    for (int b = 0; b < kNumBands; ++b)
    {
        auto band = stripArea.removeFromTop (perBandH);
        drawCorrStrip (band, bandCorr[b], kBandLabels[b], false, getDuck (b));
        if (b < kNumBands - 1) stripArea.removeFromTop (padding);
    }

    stripArea.removeFromTop (padding);
    auto overallStrip = stripArea.removeFromTop (overallH);
    drawCorrStrip (overallStrip, displayCorrelation, "OVERALL", true, 0.0f);

    if (warnFlash > 0.01f)
    {
        juce::String msg = "MONO-INCOMPATIBLE";
        juce::StringArray names;
        for (int b = 0; b < kNumBands; ++b)
            if (bandTriggered[b]) names.add (kBandLabels[b]);
        if (! names.isEmpty())
            msg += ": " + names.joinIntoString (" + ");
        if (autoSafetyOn) msg += "  (auto-ducked)";

        g.setColour (InstaWidthLookAndFeel::warningCol.withAlpha (warnFlash * 0.30f));
        g.fillRect (bounds);

        g.setColour (juce::Colours::white.withAlpha (warnFlash));
        const float fontH = juce::jlimit (11.0f, 18.0f, (float) (overallH - 4));
        g.setFont (juce::FontOptions().withHeight (fontH).withStyle ("Bold"));
        g.drawText (msg, overallStrip, juce::Justification::centred);
    }

    g.setColour (InstaWidthLookAndFeel::bgLight);
    g.drawRect (bounds, 1);
}
