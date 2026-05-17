#pragma once
#include <JuceHeader.h>

// Horizontal gain-reduction strip. Polls a provider (e.g. SideDeEsser::getCurrentGRdb)
// at 30 Hz and draws a right-to-left red bar growing from the 0 dB end.
class GRMeter : public juce::Component, private juce::Timer
{
public:
    GRMeter();
    ~GRMeter() override;

    void setProvider (std::function<float()> p) { provider = std::move (p); }
    void setMaxDb    (float dB)                 { maxDb = std::max (1.0f, dB); }

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    std::function<float()> provider;
    float maxDb         = 24.0f;
    float displayDb     = 0.0f;
    float peakHold      = 0.0f;
    int   peakHoldTicks = 0;
};
