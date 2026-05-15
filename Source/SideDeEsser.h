#pragma once
#include <JuceHeader.h>

// Dynamic side-channel de-esser.
//   - Detector path:  bandpass (always IIR — by design) → fast envelope follower
//   - Action:         compute gain reduction from envelope/threshold/range, multiply the side sample
//
// Phase impact: the detector is invisible to the audio path. The side sample is only multiplied by
// a time-varying scalar, which does not add phase distortion — safe to use in Linear Phase mode.
class SideDeEsser
{
public:
    void prepare (double sampleRate, int blockSize);
    void reset();

    void setEnabled    (bool e)         { enabled.store (e); }
    void setCenterFreq (float hz)       { pendingFreq.store (juce::jlimit (1000.0f, 12000.0f, hz)); dirty.store (true); }
    void setThresholdDb(float db)       { thresholdDb.store (juce::jlimit (-40.0f, 0.0f, db)); }
    void setRangeDb    (float db)       { rangeDb.store (juce::jlimit (0.0f, 24.0f, db)); }

    // Process side channel only (in-place). Returns current GR in dB (positive number).
    float processSide (float* side, int numSamples);

    float getCurrentGRdb() const { return currentGRdb.load(); }

private:
    void updateCoeffsIfNeeded();

    double sr { 44100.0 };
    std::atomic<bool>  enabled       { false };
    std::atomic<float> pendingFreq   { 6000.0f };
    std::atomic<float> thresholdDb   { -18.0f };
    std::atomic<float> rangeDb       { 6.0f };
    std::atomic<bool>  dirty         { true };
    std::atomic<float> currentGRdb   { 0.0f };

    float centerFreq = 6000.0f;

    juce::dsp::IIR::Filter<float> bp;        // bandpass detector
    float envelope = 0.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
};
