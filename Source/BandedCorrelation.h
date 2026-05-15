#pragma once
#include <JuceHeader.h>

// Per-band correlation analyzer.
//   Owned by the audio processor so it runs even when the GUI is closed.
//   Splits L/R into 3 bands (Butterworth 2nd-order) at the user-configured crossovers
//   and tracks Pearson correlation per band plus an overall figure.
//
//   Audio thread: call processBlock(L, R, n) once per buffer.
//   GUI thread:   poll getBandCorrelation(b) / getOverallCorrelation() for display.
class BandedCorrelation
{
public:
    static constexpr int numBands = 3;

    void prepare (double sampleRate, int maxBlockSize);
    void setCrossovers (float fLow, float fHigh);             // any thread
    void processBlock (const float* L, const float* R, int n); // audio thread

    float getBandCorrelation (int band) const { return bandCorr[band].load(); }
    float getOverallCorrelation()       const { return overallCorr.load(); }

private:
    void updateCoefficientsIfNeeded();

    double sr = 44100.0;
    float alpha = 0.0f;

    std::atomic<float> pendingFLow  { 150.0f };
    std::atomic<float> pendingFHigh { 2000.0f };
    std::atomic<bool>  dirty        { true };

    // L and R band-split filters
    juce::dsp::IIR::Filter<float> lpFLowL, hpFLowL, lpFHighL, hpFHighL;
    juce::dsp::IIR::Filter<float> lpFLowR, hpFLowR, lpFHighR, hpFHighR;

    // Smoothed running products (audio thread writes, GUI thread reads)
    struct Accum
    {
        float lr = 0.0f, ll = 0.0f, rr = 0.0f;
    };
    Accum overall;
    Accum perBand[numBands];

    // Published correlation values
    std::atomic<float> overallCorr { 1.0f };
    std::atomic<float> bandCorr[numBands] { {1.0f}, {1.0f}, {1.0f} };
};
