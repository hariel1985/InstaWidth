#include "Goniometer.h"
#include "LookAndFeel.h"

Goniometer::Goniometer()
{
    setOpaque (true);
    startTimerHz (30);
}

Goniometer::~Goniometer() { stopTimer(); }

void Goniometer::pushSample (float l, float r)
{
    int i = writeIndex.load (std::memory_order_relaxed);
    fifoL[i] = l;
    fifoR[i] = r;
    writeIndex.store ((i + 1) & (fifoSize - 1), std::memory_order_release);
}

void Goniometer::timerCallback()
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty()) return;

    if (! phosphor.isValid() || phosphor.getWidth() != bounds.getWidth() || phosphor.getHeight() != bounds.getHeight())
        phosphor = juce::Image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);

    // Fade existing phosphor by drawing a translucent black rect on top
    {
        juce::Graphics g (phosphor);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillAll();
    }

    // Read available new samples
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float scale = std::min (w, h) * 0.45f;
    const float invSqrt2 = 0.7071067811865475f;

    juce::Graphics g (phosphor);
    g.setColour (InstaWidthLookAndFeel::accent.withAlpha (0.7f));

    int writeIdx = writeIndex.load (std::memory_order_acquire);
    int idx = readIndex;
    while (idx != writeIdx)
    {
        const float l = fifoL[idx];
        const float r = fifoR[idx];

        // Rotate 45°: x = (l - r) / sqrt(2) = side, y = -(l + r) / sqrt(2) = -mid (so mid points up)
        const float x = (l - r) * invSqrt2;
        const float y = (l + r) * invSqrt2;

        const float px = cx + x * scale;
        const float py = cy - y * scale;
        if (px >= 0 && px < w && py >= 0 && py < h)
            g.fillRect (px - 0.5f, py - 0.5f, 1.5f, 1.5f);

        idx = (idx + 1) & (fifoSize - 1);
    }
    readIndex = writeIdx;

    repaint();
}

void Goniometer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour (juce::Colour (0xff0a0a14));
    g.fillRect (bounds);

    // Subtle grid (M/S axes + box)
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.45f;

    g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.45f));
    g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.0f);
    g.drawLine (cx, cy - r, cx, cy + r, 0.8f);   // vertical = mono
    g.drawLine (cx - r, cy, cx + r, cy, 0.8f);   // horizontal = side

    // Axis labels
    g.setColour (InstaWidthLookAndFeel::textSecondary.withAlpha (0.7f));
    g.setFont (juce::FontOptions().withHeight (12.5f));
    g.drawText ("M",  juce::Rectangle<float> (cx - 12, cy - r - 18, 24, 16), juce::Justification::centred);
    g.drawText ("-M", juce::Rectangle<float> (cx - 14, cy + r + 2,  28, 16), juce::Justification::centred);
    g.drawText ("+S", juce::Rectangle<float> (cx + r - 4, cy - 8, 28, 16), juce::Justification::centredLeft);
    g.drawText ("-S", juce::Rectangle<float> (cx - r - 24, cy - 8, 28, 16), juce::Justification::centredRight);

    // Phosphor trace
    if (phosphor.isValid())
        g.drawImageAt (phosphor, 0, 0);

    // Frame
    g.setColour (InstaWidthLookAndFeel::bgLight);
    g.drawRect (bounds, 1.0f);
}
