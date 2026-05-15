#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr float invSqrt2 = 0.7071067811865475f;
}

juce::AudioProcessorValueTreeState::ParameterLayout InstaWidthProcessor::createLayout()
{
    using P = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto pct = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "mode", 1 }, "Mode",
        juce::StringArray { "Minimum Phase", "Linear Phase" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "fLow", 1 }, "Low/Mid Xover",
        juce::NormalisableRange<float> (40.0f, 1000.0f, 1.0f, 0.3f), 150.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v)) + " Hz"; })));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "fHigh", 1 }, "Mid/High Xover",
        juce::NormalisableRange<float> (500.0f, 18000.0f, 1.0f, 0.3f), 2000.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v)) + " Hz"; })));

    auto widthAttribs = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [pct] (float v, int) { return pct (v); });
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wLow", 1 },  "Low Width",  juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, widthAttribs));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wMid", 1 },  "Mid Width",  juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, widthAttribs));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wHigh", 1 }, "High Width", juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, widthAttribs));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "tilt", 1 }, "Side Tilt",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " dB"; })));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "monoOn", 1 }, "Monomaker", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "monoFreq", 1 }, "Mono Below",
        juce::NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.4f), 120.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v)) + " Hz"; })));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "deessOn", 1 }, "Side De-esser", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "deessFreq", 1 }, "De-ess Freq",
        juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.4f), 6500.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v)) + " Hz"; })));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "deessThresh", 1 }, "De-ess Threshold",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -18.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " dB"; })));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "deessRange", 1 }, "De-ess Range",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " dB"; })));

    // FFT order 9..14 → 512..16384 taps. Stored as float for APVTS.
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "firQuality", 1 }, "FIR Quality",
        juce::StringArray { "512", "1024", "2048", "4096", "8192", "16384" }, 2));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " dB"; })));

    return { params.begin(), params.end() };
}

InstaWidthProcessor::InstaWidthProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pMode        = apvts.getRawParameterValue ("mode");
    pBypass      = apvts.getRawParameterValue ("bypass");
    pFLow        = apvts.getRawParameterValue ("fLow");
    pFHigh       = apvts.getRawParameterValue ("fHigh");
    pWLow        = apvts.getRawParameterValue ("wLow");
    pWMid        = apvts.getRawParameterValue ("wMid");
    pWHigh       = apvts.getRawParameterValue ("wHigh");
    pTilt        = apvts.getRawParameterValue ("tilt");
    pMonoOn      = apvts.getRawParameterValue ("monoOn");
    pMonoFreq    = apvts.getRawParameterValue ("monoFreq");
    pDeessOn     = apvts.getRawParameterValue ("deessOn");
    pDeessFreq   = apvts.getRawParameterValue ("deessFreq");
    pDeessThresh = apvts.getRawParameterValue ("deessThresh");
    pDeessRange  = apvts.getRawParameterValue ("deessRange");
    pFIRQuality  = apvts.getRawParameterValue ("firQuality");
    pOutputDb    = apvts.getRawParameterValue ("output");
}

InstaWidthProcessor::~InstaWidthProcessor()
{
    firBuilder.stop();
}

bool InstaWidthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void InstaWidthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    widthEngine.prepare (sampleRate, samplesPerBlock);
    deEsser.prepare (sampleRate, samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    sideConvolution.prepare (spec);
    sideConvolution.reset();
    sideFIRLoaded = false;

    msBuffer.setSize (2, samplesPerBlock, false, false, true);

    firBuilder.start (sampleRate);
    updateParametersFromAPVTS();

    // Initial latency based on current mode
    const int latency = (getMode() == LinearPhase) ? firBuilder.getLatencySamples() : 0;
    applyLatency (latency);
}

void InstaWidthProcessor::releaseResources()
{
    sideConvolution.reset();
    widthEngine.reset();
    deEsser.reset();
}

void InstaWidthProcessor::applyLatency (int newLatency)
{
    if (newLatency == currentLatency && midDelayLen == newLatency)
        return;

    currentLatency = newLatency;
    midDelayLen = newLatency;

    if (midDelayLen > 0)
    {
        midDelayBuffer.setSize (1, midDelayLen, false, true, true);
        midDelayBuffer.clear();
    }
    else
    {
        midDelayBuffer.setSize (1, 0);
    }
    midDelayWrite = 0;

    setLatencySamples (currentLatency);
}

void InstaWidthProcessor::updateParametersFromAPVTS()
{
    const float fl = pFLow->load();
    const float fh = pFHigh->load();
    const float wl = pWLow->load();
    const float wm = pWMid->load();
    const float wh = pWHigh->load();
    const float tilt = pTilt->load();
    const bool  monoOn = pMonoOn->load() > 0.5f;
    const float monoFreq = pMonoFreq->load();

    widthEngine.setCrossovers (fl, fh);
    widthEngine.setWidths (wl, wm, wh);
    widthEngine.setSideTilt (tilt);
    widthEngine.setMonomaker (monoOn, monoFreq);

    firBuilder.setCrossovers (fl, fh);
    firBuilder.setWidths (wl, wm, wh);
    firBuilder.setSideTilt (tilt);
    firBuilder.setMonomaker (monoOn, monoFreq);

    const int qualityIdx = (int) pFIRQuality->load();
    firBuilder.setFFTOrder (9 + qualityIdx);   // 9=512 .. 14=16384

    deEsser.setEnabled    (pDeessOn->load() > 0.5f);
    deEsser.setCenterFreq (pDeessFreq->load());
    deEsser.setThresholdDb (pDeessThresh->load());
    deEsser.setRangeDb    (pDeessRange->load());
}

void InstaWidthProcessor::setMeterCallbacks (SampleCallback gonio, SampleCallback corr)
{
    const juce::SpinLock::ScopedLockType lock (callbackLock);
    gonioCallback = std::move (gonio);
    corrCallback  = std::move (corr);
}

void InstaWidthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    updateParametersFromAPVTS();

    const int numSamples = buffer.getNumSamples();
    const int numChans   = buffer.getNumChannels();

    // Pull any new FIR
    if (auto newFIR = firBuilder.getNewFIR())
    {
        sideConvolution.loadImpulseResponse (std::move (*newFIR), currentSampleRate,
                                              juce::dsp::Convolution::Stereo::no,
                                              juce::dsp::Convolution::Trim::no,
                                              juce::dsp::Convolution::Normalise::no);
        sideFIRLoaded = true;
    }

    const bool bypassed = pBypass->load() > 0.5f;
    const ProcessingMode mode = getMode();

    // Adjust latency if mode changed
    const int desiredLatency = (mode == LinearPhase) ? firBuilder.getLatencySamples() : 0;
    if (desiredLatency != currentLatency)
        applyLatency (desiredLatency);

    if (bypassed || numChans < 2)
    {
        // Even in bypass, feed meters with input so the user sees pre/post comparison
        // (here we just feed the bypassed signal so meters reflect what leaves the plugin)
        const juce::SpinLock::ScopedTryLockType lk (callbackLock);
        if (lk.isLocked() && (gonioCallback || corrCallback))
        {
            const float* l = buffer.getReadPointer (0);
            const float* r = buffer.getReadPointer (juce::jmin (1, numChans - 1));
            for (int i = 0; i < numSamples; ++i)
            {
                if (gonioCallback) gonioCallback (l[i], r[i]);
                if (corrCallback)  corrCallback  (l[i], r[i]);
            }
        }
        return;
    }

    msBuffer.setSize (2, numSamples, false, false, true);
    auto* mid  = msBuffer.getWritePointer (0);
    auto* side = msBuffer.getWritePointer (1);
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (1);

    // M/S encode (energy-preserving scaling: 1/sqrt(2))
    for (int i = 0; i < numSamples; ++i)
    {
        mid[i]  = (l[i] + r[i]) * invSqrt2;
        side[i] = (l[i] - r[i]) * invSqrt2;
    }

    if (mode == LinearPhase && sideFIRLoaded)
    {
        // Side: linear-phase convolution
        juce::dsp::AudioBlock<float> sideBlock (&side, 1, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (sideBlock);
        sideConvolution.process (ctx);

        // De-esser after convolution
        deEsser.processSide (side, numSamples);

        // Mid: delay-line to match the FIR's symmetric latency
        if (midDelayLen > 0)
        {
            auto* dl = midDelayBuffer.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
            {
                const float out = dl[midDelayWrite];
                dl[midDelayWrite] = mid[i];
                mid[i] = out;
                midDelayWrite = (midDelayWrite + 1) % midDelayLen;
            }
        }
    }
    else
    {
        // Minimum phase: full IIR chain
        // de-esser runs first on side (so the LR4 + monomaker sees the de-essed signal)
        deEsser.processSide (side, numSamples);
        widthEngine.processMS (msBuffer);
    }

    // M/S decode + output gain
    const float outGain = juce::Decibels::decibelsToGain (pOutputDb->load());
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = (mid[i] + side[i]) * invSqrt2 * outGain;
        outR[i] = (mid[i] - side[i]) * invSqrt2 * outGain;
    }

    // Clear any extra channels (defensive)
    for (int ch = 2; ch < numChans; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Feed meters with the processed output
    {
        const juce::SpinLock::ScopedTryLockType lk (callbackLock);
        if (lk.isLocked() && (gonioCallback || corrCallback))
        {
            for (int i = 0; i < numSamples; ++i)
            {
                if (gonioCallback) gonioCallback (outL[i], outR[i]);
                if (corrCallback)  corrCallback  (outL[i], outR[i]);
            }
        }
    }
}

void InstaWidthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, dest);
}

void InstaWidthProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* InstaWidthProcessor::createEditor()
{
    return new InstaWidthEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InstaWidthProcessor();
}
