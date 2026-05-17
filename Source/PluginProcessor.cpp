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

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "autoSafe", 1 }, "Auto Mono Safe", false));

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
    pAutoSafe    = apvts.getRawParameterValue ("autoSafe");
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
    analyser.prepare (sampleRate, samplesPerBlock);

    for (auto& s : bandSafety) s.store (1.0f);

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

    // Effective widths = user widths * per-band safety multiplier.
    const float s0 = bandSafety[0].load();
    const float s1 = bandSafety[1].load();
    const float s2 = bandSafety[2].load();

    // Min Phase chain: smooth (block-rate) modulation is fine and free.
    widthEngine.setCrossovers (fl, fh);
    widthEngine.setWidths (wl * s0, wm * s1, wh * s2);
    widthEngine.setSideTilt (tilt);
    widthEngine.setMonomaker (monoOn, monoFreq);

    // Linear-phase chain: a smoothly-varying safety would produce a slightly different
    // effective-width value every audio block, which forces the FIR builder to rebuild
    // continuously. For large IRs (8192/16384 taps) the JUCE convolution cannot finish
    // preparing one IR before the next one arrives, and outputs silence ("full mono").
    // Quantising the safety to 10% steps lets the value sit still between transitions
    // so the convolution actually finishes its preparation.
    auto quantise = [] (float v) { return std::round (v * 10.0f) * 0.1f; };
    const float qs0 = quantise (s0);
    const float qs1 = quantise (s1);
    const float qs2 = quantise (s2);

    firBuilder.setCrossovers (fl, fh);
    firBuilder.setWidths (wl * qs0, wm * qs1, wh * qs2);
    firBuilder.setSideTilt (tilt);
    firBuilder.setMonomaker (monoOn, monoFreq);

    analyser.setCrossovers (fl, fh);

    const int qualityIdx = (int) pFIRQuality->load();
    firBuilder.setFFTOrder (9 + qualityIdx);   // 9=512 .. 14=16384

    deEsser.setEnabled    (pDeessOn->load() > 0.5f);
    deEsser.setCenterFreq (pDeessFreq->load());
    deEsser.setThresholdDb (pDeessThresh->load());
    deEsser.setRangeDb    (pDeessRange->load());
}

void InstaWidthProcessor::updateAutoSafety (int blockSize)
{
    const bool enabled = pAutoSafe != nullptr && pAutoSafe->load() > 0.5f;

    if (! enabled)
    {
        // Smoothly return safety multipliers to 1.0 (no attenuation) when auto-safe is off,
        // so disabling the feature does not suddenly jump the effective width.
        for (auto& s : bandSafety)
        {
            float v = s.load();
            if (v < 0.9999f)
                s.store (v + (1.0f - v) * 0.10f);
            else
                s.store (1.0f);
        }
        return;
    }

    // Same per-band thresholds the meter uses for the warning -- the safety system kicks in
    // exactly when the warning would.
    constexpr float kThreshold[3] = { -0.05f, -0.30f, -0.45f };
    // How far below the threshold counts as "fully duck to mono". A correlation that drops
    // an additional 0.30 below the threshold pulls safety down to 0 (full mono).
    constexpr float kDuckRange = 0.30f;

    // Per-block attack/release coefficients. Slower than a peak limiter on purpose --
    // the FIR builder cannot keep up with fast attacks at large tap counts, and even
    // in min-phase mode a slower envelope avoids audible pumping on the stereo image.
    const double blockSec = (double) blockSize / std::max (1.0, currentSampleRate);
    const float attackCoef  = (float) std::exp (-blockSec / 0.200);  // ~200 ms attack
    const float releaseCoef = (float) std::exp (-blockSec / 0.800);  // ~800 ms release

    for (int b = 0; b < 3; ++b)
    {
        const float corr = analyser.getBandCorrelation (b);
        // overshoot is positive when we're below threshold
        const float overshoot = std::max (0.0f, kThreshold[b] - corr);
        const float target = std::max (0.0f, 1.0f - overshoot / kDuckRange);

        float current = bandSafety[b].load();
        const float coef = (target < current) ? attackCoef : releaseCoef;
        current = coef * current + (1.0f - coef) * target;
        bandSafety[b].store (current);
    }
}

void InstaWidthProcessor::setGoniometerCallback (SampleCallback gonio)
{
    const juce::SpinLock::ScopedLockType lock (callbackLock);
    gonioCallback = std::move (gonio);
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
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (juce::jmin (1, numChans - 1));
        analyser.processBlock (l, r, numSamples);
        updateAutoSafety (numSamples);

        const juce::SpinLock::ScopedTryLockType lk (callbackLock);
        if (lk.isLocked() && gonioCallback)
        {
            for (int i = 0; i < numSamples; ++i)
                gonioCallback (l[i], r[i]);
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

    // Run the per-band correlation analyser on the processed output, then the auto-safety
    // controller. These run regardless of whether the editor is open.
    analyser.processBlock (outL, outR, numSamples);
    updateAutoSafety (numSamples);

    // Feed the goniometer (the correlation meter polls the analyser directly).
    {
        const juce::SpinLock::ScopedTryLockType lk (callbackLock);
        if (lk.isLocked() && gonioCallback)
        {
            for (int i = 0; i < numSamples; ++i)
                gonioCallback (outL[i], outR[i]);
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
