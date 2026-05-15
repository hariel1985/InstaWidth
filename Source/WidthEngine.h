#pragma once
#include <JuceHeader.h>

// Minimum-phase processing chain operating on the M/S domain.
//   Side: Tilt (low/high shelf) -> LR4 3-band split -> per-band width gain -> sum -> Monomaker HP
//   Mid:  identical LR4 split-and-sum (no per-band gain) so the phase rotation of the LR4 allpass
//         matches on both M and S — preserves the stereo image at width=1.
//
// The FIR (linear-phase) mode does not use this engine; it lives in SideFIRBuilder + convolution.
class WidthEngine
{
public:
    void prepare (double sampleRate, int blockSize);
    void reset();

    // All setters are safe to call from GUI thread; they update std::atomic<float> values
    // that are picked up at the start of each audio block.
    void setCrossovers (float fLow, float fHigh);
    void setWidths     (float wLow, float wMid, float wHigh);
    void setSideTilt   (float tiltDb);               // pivot 1 kHz, ±6 dB at 20 Hz / 20 kHz
    void setMonomaker  (bool enabled, float cutoff); // side HP cutoff (Hz) when enabled

    // Operates on stereo buffer in-place. Caller is responsible for M/S encode/decode wrappers.
    // Buffer must be 2 channels: channel 0 = Mid, channel 1 = Side.
    void processMS (juce::AudioBuffer<float>& msBuffer);

private:
    void updateCoefficientsIfNeeded();

    double sr { 44100.0 };
    int blockSz { 512 };

    // Pending parameter values (set from GUI)
    std::atomic<float> pendingFLow      { 150.0f };
    std::atomic<float> pendingFHigh     { 2000.0f };
    std::atomic<float> pendingWLow      { 1.0f };
    std::atomic<float> pendingWMid      { 1.0f };
    std::atomic<float> pendingWHigh     { 1.0f };
    std::atomic<float> pendingTilt      { 0.0f };
    std::atomic<bool>  pendingMonoOn    { true };
    std::atomic<float> pendingMonoFreq  { 120.0f };
    std::atomic<bool>  coeffsDirty      { true };

    // Active values (used in the audio thread)
    float fLow = 150.0f, fHigh = 2000.0f;
    float wLow = 1.0f, wMid = 1.0f, wHigh = 1.0f;
    float tiltDb = 0.0f;
    bool  monoOn = true;
    float monoFreq = 120.0f;

    // LR4 = cascade of 2 Butterworth-2 biquads (same fc, Q=0.7071)
    struct LR4
    {
        juce::dsp::IIR::Filter<float> a, b;
        void reset() { a.reset(); b.reset(); }
        void prepare (const juce::dsp::ProcessSpec& spec) { a.prepare (spec); b.prepare (spec); }
        float processSample (float x) { return b.processSample (a.processSample (x)); }
        void setCoeffs (juce::dsp::IIR::Coefficients<float>::Ptr c) { *a.coefficients = *c; *b.coefficients = *c; }
    };

    // Per-channel filter bank (M and S each get their own state)
    struct Bank
    {
        LR4 lpLow,  hpLow;   // split at fLow
        LR4 lpHigh, hpHigh;  // split at fHigh (applied to the high path of the first split)
        juce::dsp::IIR::Filter<float> tiltLow, tiltHigh;     // shelf filters (side only)
        juce::dsp::IIR::Filter<float> monoHP_a, monoHP_b;    // LR4 HP for Monomaker (side only)
    };
    Bank midBank, sideBank;
};
