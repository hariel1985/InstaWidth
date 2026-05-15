#pragma once
#include <JuceHeader.h>

// Pearson correlation between L and R, integrated over ~200 ms.
//   Audio thread: pushSample(l, r) accumulates ΣLR, ΣL², ΣR² in a sliding window.
//   GUI thread:   reads current correlation in [-1..+1] and renders a horizontal strip.
class CorrelationMeter : public juce::Component, private juce::Timer
{
public:
    CorrelationMeter();
    ~CorrelationMeter() override;

    void prepare (double sampleRate);
    void pushSample (float l, float r);   // audio thread

    void paint (juce::Graphics& g) override;

    float getCorrelation()  const { return displayCorrelation; }
    bool  isNegative()      const { return displayCorrelation < 0.0f; }

private:
    void timerCallback() override;

    // One-pole smoothing for the three running products
    float alpha = 0.0f;   // depends on sample rate (~200 ms window)
    std::atomic<float> sLR { 0.0f };
    std::atomic<float> sLL { 0.0f };
    std::atomic<float> sRR { 0.0f };
    float displayCorrelation = 1.0f;

    float warnFlash = 0.0f;
};
