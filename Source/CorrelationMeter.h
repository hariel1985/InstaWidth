#pragma once
#include <JuceHeader.h>
#include "BandedCorrelation.h"

// Display-only widget that polls a BandedCorrelation analyser running in the audio thread.
// Renders 3 per-band correlation bars plus an overall one, with a sustained-hold warning
// that names the offending band(s).
class CorrelationMeter : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    CorrelationMeter();
    ~CorrelationMeter() override;

    // The analyser must outlive the meter -- it lives in the audio processor.
    void setSource (const BandedCorrelation* src) { source = src; }

    // Provide getters for the auto-safety multipliers so the warning can mention when
    // the safety system is actively pulling a band down.
    void setSafetyProvider (std::function<float (int)> p) { safetyProvider = std::move (p); }
    void setAutoSafetyEnabled (bool e) { autoSafetyOn = e; }

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    const BandedCorrelation* source = nullptr;
    std::function<float (int)> safetyProvider;
    bool autoSafetyOn = false;

    // Smoothed display state
    float displayCorrelation = 1.0f;
    std::array<float, 3> bandCorr { 1.0f, 1.0f, 1.0f };
    std::array<int, 3>   negativeHold  { 0, 0, 0 };
    std::array<bool, 3>  bandTriggered { false, false, false };
    float warnFlash = 0.0f;
};
