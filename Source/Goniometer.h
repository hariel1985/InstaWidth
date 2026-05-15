#pragma once
#include <JuceHeader.h>

// Phosphor-style Lissajous goniometer.
//   Audio thread pushes samples into a lock-free fifo via pushSample().
//   Editor timer pulls samples and renders them as bright points with persistence.
//
// The display is rotated 45° so vertical = mono (mid) and horizontal = side, which is
// the conventional audio-engineer orientation (vs. mathematical L vs R).
class Goniometer : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::Timer
{
public:
    Goniometer();
    ~Goniometer() override;

    void paint (juce::Graphics& g) override;

    // Audio thread — call once per output sample
    void pushSample (float left, float right);

private:
    void timerCallback() override;

    static constexpr int fifoSize = 8192;
    std::array<float, fifoSize> fifoL {};
    std::array<float, fifoSize> fifoR {};
    std::atomic<int> writeIndex { 0 };

    juce::Image phosphor;       // accumulation buffer for fading trail
    int readIndex = 0;
};
