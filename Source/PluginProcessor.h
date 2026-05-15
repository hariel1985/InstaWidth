#pragma once
#include <JuceHeader.h>
#include "WidthEngine.h"
#include "SideDeEsser.h"
#include "SideFIRBuilder.h"
#include "BandedCorrelation.h"

class InstaWidthEditor;

class InstaWidthProcessor : public juce::AudioProcessor
{
public:
    enum ProcessingMode { MinimumPhase = 0, LinearPhase = 1 };

    InstaWidthProcessor();
    ~InstaWidthProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Goniometer sample feed (audio thread -> editor). The correlation meter polls the
    // analyser directly (no callback path needed for it).
    using SampleCallback = std::function<void (float l, float r)>;
    void setGoniometerCallback (SampleCallback gonio);

    int getCurrentLatency() const { return currentLatency; }
    ProcessingMode getMode() const { return (ProcessingMode) (int) *apvts.getRawParameterValue ("mode"); }

    // Per-band correlation analyser (audio thread writes, GUI reads)
    const BandedCorrelation& getCorrelationAnalyser() const { return analyser; }

    // Auto Mono Safety state — current per-band safety multiplier in [0..1].
    // 1.0 = user's width is applied as-is. <1 = effective width is attenuated to keep mono compatibility.
    float getBandSafety (int band) const { return bandSafety[band].load(); }

private:
    void updateParametersFromAPVTS();
    void applyLatency (int newLatency);
    void updateAutoSafety (int blockSize);

    // DSP blocks
    WidthEngine      widthEngine;      // minimum-phase chain
    SideFIRBuilder   firBuilder;       // linear-phase FIR (background)
    juce::dsp::Convolution sideConvolution;
    SideDeEsser      deEsser;
    BandedCorrelation analyser;
    bool sideFIRLoaded = false;

    // Auto Mono Safety — sample-block-rate gain envelope per band
    std::array<std::atomic<float>, 3> bandSafety { { {1.0f}, {1.0f}, {1.0f} } };

    juce::AudioBuffer<float> msBuffer;        // 2 channels: 0 = mid, 1 = side
    juce::AudioBuffer<float> midDelayBuffer;  // delay line to match FIR latency on the mid channel
    int midDelayWrite = 0;
    int midDelayLen   = 0;

    double currentSampleRate = 44100.0;
    int currentBlockSize     = 512;
    int currentLatency       = 0;

    // Cached parameter pointers
    std::atomic<float>* pMode        = nullptr;
    std::atomic<float>* pBypass      = nullptr;
    std::atomic<float>* pFLow        = nullptr;
    std::atomic<float>* pFHigh       = nullptr;
    std::atomic<float>* pWLow        = nullptr;
    std::atomic<float>* pWMid        = nullptr;
    std::atomic<float>* pWHigh       = nullptr;
    std::atomic<float>* pTilt        = nullptr;
    std::atomic<float>* pMonoOn      = nullptr;
    std::atomic<float>* pMonoFreq    = nullptr;
    std::atomic<float>* pDeessOn     = nullptr;
    std::atomic<float>* pDeessFreq   = nullptr;
    std::atomic<float>* pDeessThresh = nullptr;
    std::atomic<float>* pDeessRange  = nullptr;
    std::atomic<float>* pFIRQuality  = nullptr;
    std::atomic<float>* pOutputDb    = nullptr;
    std::atomic<float>* pAutoSafe    = nullptr;

    // Meter callback (set by editor)
    SampleCallback gonioCallback;
    juce::SpinLock callbackLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstaWidthProcessor)
};
