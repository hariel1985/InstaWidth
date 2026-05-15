#include "CorrelationMeter.h"
#include "LookAndFeel.h"

CorrelationMeter::CorrelationMeter()
{
    setOpaque (true);
    startTimerHz (30);
}

CorrelationMeter::~CorrelationMeter() { stopTimer(); }

void CorrelationMeter::prepare (double sampleRate)
{
    // 200 ms integration window
    alpha = (float) std::exp (-1.0 / (0.200 * sampleRate));
    sLR.store (0.0f);
    sLL.store (0.0f);
    sRR.store (0.0f);
}

void CorrelationMeter::pushSample (float l, float r)
{
    // Atomic loads + stores in single-producer scenario — fine without locking
    float a = alpha;
    sLR.store (a * sLR.load (std::memory_order_relaxed) + (1.0f - a) * (l * r));
    sLL.store (a * sLL.load (std::memory_order_relaxed) + (1.0f - a) * (l * l));
    sRR.store (a * sRR.load (std::memory_order_relaxed) + (1.0f - a) * (r * r));
}

void CorrelationMeter::timerCallback()
{
    float lr = sLR.load();
    float ll = sLL.load();
    float rr = sRR.load();

    float denom = std::sqrt (std::max (ll * rr, 1e-12f));
    float corr  = (denom > 1e-9f) ? juce::jlimit (-1.0f, 1.0f, lr / denom) : 1.0f;

    // smooth display value
    displayCorrelation += (corr - displayCorrelation) * 0.3f;

    // flash counter for negative-correlation warning
    if (displayCorrelation < 0.0f)
        warnFlash = std::min (1.0f, warnFlash + 0.12f);
    else
        warnFlash = std::max (0.0f, warnFlash - 0.04f);

    repaint();
}

void CorrelationMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour (juce::Colour (0xff0a0a14));
    g.fillRect (bounds);

    // Centre tick (0 correlation)
    const float midX = bounds.getCentreX();
    g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.6f));
    g.drawLine (midX, bounds.getY(), midX, bounds.getBottom(), 1.0f);

    // Tick labels: -1, 0, +1
    g.setColour (InstaWidthLookAndFeel::textSecondary);
    g.setFont (juce::FontOptions().withHeight (9.0f));
    g.drawText ("-1",  juce::Rectangle<float> (bounds.getX(),       bounds.getY(), 18, bounds.getHeight()), juce::Justification::centredLeft);
    g.drawText ("0",   juce::Rectangle<float> (midX - 8,            bounds.getY(), 16, bounds.getHeight()), juce::Justification::centred);
    g.drawText ("+1",  juce::Rectangle<float> (bounds.getRight()-20, bounds.getY(), 18, bounds.getHeight()), juce::Justification::centredRight);

    // Bar from centre toward the current correlation value
    float c = juce::jlimit (-1.0f, 1.0f, displayCorrelation);
    float halfWidth = (bounds.getWidth() - 4.0f) * 0.5f;
    float endX = midX + c * halfWidth;

    juce::Colour barCol;
    if (c >= 0.5f)       barCol = juce::Colour (0xff00ff88);
    else if (c >= 0.0f)  barCol = juce::Colour (0xffeedd44);
    else                 barCol = juce::Colour (0xffff4444);

    auto bar = (c >= 0)
                 ? juce::Rectangle<float> (midX, bounds.getY() + 4, endX - midX, bounds.getHeight() - 8)
                 : juce::Rectangle<float> (endX, bounds.getY() + 4, midX - endX, bounds.getHeight() - 8);
    g.setColour (barCol);
    g.fillRect (bar);

    // Negative-correlation warning flash overlay
    if (warnFlash > 0.01f)
    {
        g.setColour (InstaWidthLookAndFeel::warningCol.withAlpha (warnFlash * 0.35f));
        g.fillRect (bounds);

        g.setColour (juce::Colours::white.withAlpha (warnFlash));
        g.setFont (juce::FontOptions().withHeight (bounds.getHeight() * 0.55f).withStyle ("Bold"));
        g.drawText ("MONO-INCOMPATIBLE", bounds, juce::Justification::centred);
    }

    g.setColour (InstaWidthLookAndFeel::bgLight);
    g.drawRect (bounds, 1.0f);
}
