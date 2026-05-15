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

void CorrelationMeter::prepare (double sampleRate)
{
    sr = sampleRate;
    alpha = (float) std::exp (-1.0 / (0.200 * sampleRate));  // ~200 ms integration

    juce::dsp::ProcessSpec spec { sampleRate, 256u, 1 };
    for (auto* f : { &lpFLowL, &hpFLowL, &lpFHighL, &hpFHighL,
                     &lpFLowR, &hpFLowR, &lpFHighR, &hpFHighR })
        f->prepare (spec);

    overall.sLR.store (0.0f); overall.sLL.store (0.0f); overall.sRR.store (0.0f);
    for (auto& b : perBand) { b.sLR.store (0.0f); b.sLL.store (0.0f); b.sRR.store (0.0f); }

    coeffsDirty.store (true);
    updateCoefficientsIfNeeded();
}

void CorrelationMeter::setCrossovers (float fl, float fh)
{
    const float clFL = juce::jlimit (40.0f, 1000.0f, fl);
    const float clFH = juce::jlimit (500.0f, 18000.0f, std::max (fh, fl + 50.0f));
    bool changed = false;
    if (pendingFLow.load()  != clFL) { pendingFLow.store  (clFL); changed = true; }
    if (pendingFHigh.load() != clFH) { pendingFHigh.store (clFH); changed = true; }
    if (changed) coeffsDirty.store (true);
}

void CorrelationMeter::updateCoefficientsIfNeeded()
{
    if (! coeffsDirty.exchange (false)) return;

    const float fLow  = pendingFLow.load();
    const float fHigh = pendingFHigh.load();
    constexpr float Q = 0.7071067811865476f;

    auto lpLow  = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, fLow,  Q);
    auto hpLow  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, fLow,  Q);
    auto lpHigh = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, fHigh, Q);
    auto hpHigh = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, fHigh, Q);

    *lpFLowL .coefficients = *lpLow;  *hpFLowL .coefficients = *hpLow;
    *lpFHighL.coefficients = *lpHigh; *hpFHighL.coefficients = *hpHigh;
    *lpFLowR .coefficients = *lpLow;  *hpFLowR .coefficients = *hpLow;
    *lpFHighR.coefficients = *lpHigh; *hpFHighR.coefficients = *hpHigh;
}

void CorrelationMeter::pushSample (float l, float r)
{
    updateCoefficientsIfNeeded();

    // 3-band split per channel
    const float lLow  = lpFLowL .processSample (l);
    const float lTemp = hpFLowL .processSample (l);
    const float lMid  = lpFHighL.processSample (lTemp);
    const float lHigh = hpFHighL.processSample (lTemp);

    const float rLow  = lpFLowR .processSample (r);
    const float rTemp = hpFLowR .processSample (r);
    const float rMid  = lpFHighR.processSample (rTemp);
    const float rHigh = hpFHighR.processSample (rTemp);

    const float a = alpha;
    const float oneMinusA = 1.0f - a;

    auto smoothAccum = [a, oneMinusA] (std::atomic<float>& s, float x)
    {
        s.store (a * s.load (std::memory_order_relaxed) + oneMinusA * x,
                 std::memory_order_relaxed);
    };

    smoothAccum (overall.sLR, l * r);
    smoothAccum (overall.sLL, l * l);
    smoothAccum (overall.sRR, r * r);

    smoothAccum (perBand[0].sLR, lLow  * rLow);
    smoothAccum (perBand[0].sLL, lLow  * lLow);
    smoothAccum (perBand[0].sRR, rLow  * rLow);

    smoothAccum (perBand[1].sLR, lMid  * rMid);
    smoothAccum (perBand[1].sLL, lMid  * lMid);
    smoothAccum (perBand[1].sRR, rMid  * rMid);

    smoothAccum (perBand[2].sLR, lHigh * rHigh);
    smoothAccum (perBand[2].sLL, lHigh * lHigh);
    smoothAccum (perBand[2].sRR, rHigh * rHigh);
}

namespace
{
    float computeCorr (float lr, float ll, float rr)
    {
        float denom = std::sqrt (std::max (ll * rr, 1e-12f));
        return (denom > 1e-9f) ? juce::jlimit (-1.0f, 1.0f, lr / denom) : 1.0f;
    }
}

void CorrelationMeter::timerCallback()
{
    const float overallTarget = computeCorr (overall.sLR.load(), overall.sLL.load(), overall.sRR.load());
    displayCorrelation += (overallTarget - displayCorrelation) * 0.3f;

    for (int b = 0; b < kNumBands; ++b)
    {
        const float t = computeCorr (perBand[b].sLR.load(), perBand[b].sLL.load(), perBand[b].sRR.load());
        bandCorr[b] += (t - bandCorr[b]) * 0.3f;
    }

    const bool anyNegative = displayCorrelation < 0.0f
                          || bandCorr[0] < 0.0f
                          || bandCorr[1] < 0.0f
                          || bandCorr[2] < 0.0f;
    if (anyNegative) warnFlash = std::min (1.0f, warnFlash + 0.12f);
    else             warnFlash = std::max (0.0f, warnFlash - 0.04f);

    repaint();
}

void CorrelationMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (juce::Colour (0xff0a0a14));
    g.fillRect (bounds);

    // Layout: 3 band strips on top, 1 overall strip on bottom.
    const int totalH = bounds.getHeight();
    const int labelColW = 44;
    const int padding = 3;
    const int overallH = juce::jlimit (16, 26, totalH * 38 / 100);
    const int bandsH = totalH - overallH - padding;
    const int perBandH = (bandsH - padding * (kNumBands - 1)) / kNumBands;

    auto barColour = [] (float c) -> juce::Colour
    {
        if (c >= 0.5f) return juce::Colour (0xff00ff88);
        if (c >= 0.0f) return juce::Colour (0xffeedd44);
        return juce::Colour (0xffff4444);
    };

    auto drawCorrStrip = [&] (juce::Rectangle<int> area, float corrValue,
                              const juce::String& leftLabel, bool drawScale)
    {
        // Label column
        if (leftLabel.isNotEmpty())
        {
            g.setColour (InstaWidthLookAndFeel::textSecondary);
            g.setFont (juce::FontOptions().withHeight ((float) std::min (12, area.getHeight() - 2)));
            g.drawText (leftLabel, area.removeFromLeft (labelColW).reduced (4, 0),
                        juce::Justification::centredLeft);
        }

        area.removeFromLeft (2);

        // Background
        g.setColour (juce::Colour (0xff141422));
        g.fillRect (area);

        // Centre divider (0 correlation)
        const float midX = (float) area.getCentreX();
        g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.6f));
        g.drawLine (midX, (float) area.getY(), midX, (float) area.getBottom(), 1.0f);

        // Filled bar
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

        // Frame
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

    // Three per-band strips
    auto stripArea = bounds;
    for (int b = 0; b < kNumBands; ++b)
    {
        auto band = stripArea.removeFromTop (perBandH);
        drawCorrStrip (band, bandCorr[b], kBandLabels[b], false);
        if (b < kNumBands - 1) stripArea.removeFromTop (padding);
    }

    stripArea.removeFromTop (padding);
    auto overallStrip = stripArea.removeFromTop (overallH);
    drawCorrStrip (overallStrip, displayCorrelation, "OVERALL", true);

    // Warning overlay — name the offending band(s)
    if (warnFlash > 0.01f)
    {
        juce::String msg = "MONO-INCOMPATIBLE";
        juce::StringArray names;
        for (int b = 0; b < kNumBands; ++b)
            if (bandCorr[b] < 0.0f) names.add (kBandLabels[b]);
        if (! names.isEmpty())
            msg += ": " + names.joinIntoString (" + ");

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
