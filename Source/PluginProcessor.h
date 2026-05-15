#pragma once
#include <JuceHeader.h>
#include "WidthEngine.h"
#include "SideDeEsser.h"
#include "SideFIRBuilder.h"

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

    // Goniometer + correlation taps — written by the audio thread, read by the editor.
    // The editor connects them via small lock-free fifos held inside the meter components.
    using SampleCallback = std::function<void (float l, float r)>;
    void setMeterCallbacks (SampleCallback gonio, SampleCallback corr);

    int getCurrentLatency() const { return currentLatency; }
    ProcessingMode getMode() const { return (ProcessingMode) (int) *apvts.getRawParameterValue ("mode"); }

private:
    void updateParametersFromAPVTS();
    void applyLatency (int newLatency);

    // DSP blocks
    WidthEngine      widthEngine;      // minimum-phase chain
    SideFIRBuilder   firBuilder;       // linear-phase FIR (background)
    juce::dsp::Convolution sideConvolution;
    SideDeEsser      deEsser;
    bool sideFIRLoaded = false;

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

    // Meter callbacks (set by editor)
    SampleCallback gonioCallback;
    SampleCallback corrCallback;
    juce::SpinLock callbackLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstaWidthProcessor)
};
