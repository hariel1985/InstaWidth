#pragma once
#include <JuceHeader.h>

// Linear-phase side-channel FIR builder.
//
// Combines the static side processing — Tilt + per-band width + Monomaker HP —
// into a single zero-phase magnitude response, then IFFTs that into a symmetric FIR.
// The FIR is recomputed on a background thread whenever any parameter changes.
//
// The dynamic side de-esser is intentionally NOT included; it lives outside this builder
// (as a sample-by-sample multiply with an IIR detector) so that linear-phase guarantees
// hold for the convolution stage.
class SideFIRBuilder : private juce::Thread
{
public:
    static constexpr int defaultFFTOrder = 11;  // 2048 taps
    static constexpr int minFFTOrder = 9;       // 512 taps
    static constexpr int maxFFTOrder = 14;      // 16384 taps

    SideFIRBuilder();
    ~SideFIRBuilder() override;

    void start (double sampleRate);
    void stop();

    void setFFTOrder (int order);
    void setCrossovers (float fLow, float fHigh);
    void setWidths (float wLow, float wMid, float wHigh);
    void setSideTilt (float tiltDb);
    void setMonomaker (bool on, float cutoff);

    // Audio thread: returns a new FIR if one is ready, nullptr otherwise.
    std::unique_ptr<juce::AudioBuffer<float>> getNewFIR();

    int getFIRLength()      const { return 1 << fftOrder.load(); }
    int getLatencySamples() const { return getFIRLength() / 2; }

private:
    void run() override;
    juce::AudioBuffer<float> generate (double sampleRate, int order);

    std::atomic<double> sampleRate   { 44100.0 };
    std::atomic<int>    fftOrder     { defaultFFTOrder };
    std::atomic<bool>   needsUpdate  { true };

    std::atomic<float> fLow      { 150.0f };
    std::atomic<float> fHigh     { 2000.0f };
    std::atomic<float> wLow      { 1.0f };
    std::atomic<float> wMid      { 1.0f };
    std::atomic<float> wHigh     { 1.0f };
    std::atomic<float> tiltDb    { 0.0f };
    std::atomic<bool>  monoOn    { true };
    std::atomic<float> monoFreq  { 120.0f };

    std::unique_ptr<juce::AudioBuffer<float>> pendingFIR;
    juce::SpinLock firLock;
};
