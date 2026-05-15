#pragma once
#include <JuceHeader.h>

// Pearson correlation between L and R, computed both overall and per-band.
//   Bands match the main DSP crossovers (Low/Mid/High at fLow and fHigh).
//   The display shows a stacked set of bars so the user can see WHICH frequency
//   region is going mono-incompatible — and the warning text names the band(s).
class CorrelationMeter : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    CorrelationMeter();
    ~CorrelationMeter() override;

    void prepare (double sampleRate);

    // Audio thread — call once per output sample.
    void pushSample (float l, float r);

    // GUI thread — tell the meter what frequencies to split on.
    void setCrossovers (float fLow, float fHigh);

    void paint (juce::Graphics& g) override;

    float getCorrelation() const  { return displayCorrelation; }
    bool  isNegative()    const   { return displayCorrelation < 0.0f; }

private:
    void timerCallback() override;
    void updateCoefficientsIfNeeded();   // audio thread

    double sr = 44100.0;
    float alpha = 0.0f;

    // Pending crossover state (set from GUI, applied in audio thread)
    std::atomic<float> pendingFLow   { 150.0f };
    std::atomic<float> pendingFHigh  { 2000.0f };
    std::atomic<bool>  coeffsDirty   { true };

    // Per-channel split filters: LP_fLow + HP_fLow + LP_fHigh + HP_fHigh (Butterworth 2nd order)
    juce::dsp::IIR::Filter<float> lpFLowL, hpFLowL, lpFHighL, hpFHighL;
    juce::dsp::IIR::Filter<float> lpFLowR, hpFLowR, lpFHighR, hpFHighR;

    // Running smoothed products
    struct Accum
    {
        std::atomic<float> sLR { 0.0f };
        std::atomic<float> sLL { 0.0f };
        std::atomic<float> sRR { 0.0f };
    };
    Accum overall;
    Accum perBand[3];  // 0=Low, 1=Mid, 2=High

    // Display state (GUI thread)
    float displayCorrelation = 1.0f;
    std::array<float, 3> bandCorr { 1.0f, 1.0f, 1.0f };
    std::array<int, 3>   negativeHold { 0, 0, 0 };   // ticks below threshold (hysteresis)
    std::array<bool, 3>  bandTriggered { false, false, false };
    float warnFlash = 0.0f;
};
