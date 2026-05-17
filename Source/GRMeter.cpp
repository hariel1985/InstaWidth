#include "GRMeter.h"
#include "LookAndFeel.h"

GRMeter::GRMeter()
{
    setOpaque (false);
    startTimerHz (30);
}

GRMeter::~GRMeter() { stopTimer(); }

void GRMeter::timerCallback()
{
    const float current = provider ? std::max (0.0f, provider()) : 0.0f;

    // Smooth attack (snap up), slower release (decay)
    if (current > displayDb)
        displayDb = current;                          // instant on attack
    else
        displayDb += (current - displayDb) * 0.18f;   // gentle release

    if (current >= peakHold)
    {
        peakHold = current;
        peakHoldTicks = 30;   // ~1 s hold
    }
    else if (--peakHoldTicks <= 0)
    {
        peakHold = std::max (current, peakHold - maxDb * 0.02f);  // slow fall-off
        peakHoldTicks = 0;
    }

    repaint();
}

void GRMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Background trough
    g.setColour (juce::Colour (0xff141422));
    g.fillRoundedRectangle (bounds, 2.0f);

    // 0-dB tick on the right edge — GR grows leftward from there
    const float rightX = bounds.getRight() - 1.0f;
    g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.7f));
    g.drawLine (rightX, bounds.getY(), rightX, bounds.getBottom(), 1.0f);

    const float usableW = bounds.getWidth() - 4.0f;
    const float frac    = juce::jlimit (0.0f, 1.0f, displayDb / maxDb);
    const float barW    = usableW * frac;

    if (barW > 0.5f)
    {
        const auto barBounds = juce::Rectangle<float> (rightX - barW, bounds.getY() + 1.0f,
                                                        barW, bounds.getHeight() - 2.0f);
        // Red-orange gradient: brighter at the leading edge (recent GR)
        juce::ColourGradient grad (juce::Colour (0xffff5544), barBounds.getRight(), 0,
                                    juce::Colour (0xffcc2222), barBounds.getX(),     0, false);
        g.setGradientFill (grad);
        g.fillRect (barBounds);
    }

    // Peak-hold marker
    if (peakHold > 0.1f && peakHold > displayDb + 0.5f)
    {
        const float peakX = rightX - usableW * juce::jlimit (0.0f, 1.0f, peakHold / maxDb);
        g.setColour (juce::Colour (0xffffcc44));
        g.drawLine (peakX, bounds.getY() + 1.0f, peakX, bounds.getBottom() - 1.0f, 1.5f);
    }

    // Small scale ticks (every 6 dB)
    g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.4f));
    for (float dB = 6.0f; dB < maxDb; dB += 6.0f)
    {
        const float x = rightX - usableW * (dB / maxDb);
        g.drawLine (x, bounds.getY() + bounds.getHeight() * 0.55f,
                    x, bounds.getBottom() - 1.0f, 0.8f);
    }

    // Frame
    g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);
}
