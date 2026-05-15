#include "WidthEngine.h"

void WidthEngine::prepare (double sampleRate, int blockSize)
{
    sr = sampleRate;
    blockSz = blockSize;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };

    for (auto* bank : { &midBank, &sideBank })
    {
        bank->lpLow .prepare (spec);
        bank->hpLow .prepare (spec);
        bank->lpHigh.prepare (spec);
        bank->hpHigh.prepare (spec);
        bank->tiltLow.prepare (spec);
        bank->tiltHigh.prepare (spec);
        bank->monoHP_a.prepare (spec);
        bank->monoHP_b.prepare (spec);
    }

    coeffsDirty.store (true);
    updateCoefficientsIfNeeded();
}

void WidthEngine::reset()
{
    for (auto* bank : { &midBank, &sideBank })
    {
        bank->lpLow.reset();  bank->hpLow.reset();
        bank->lpHigh.reset(); bank->hpHigh.reset();
        bank->tiltLow.reset(); bank->tiltHigh.reset();
        bank->monoHP_a.reset(); bank->monoHP_b.reset();
    }
}

void WidthEngine::setCrossovers (float fl, float fh)
{
    pendingFLow.store  (juce::jlimit (40.0f, 1000.0f, fl));
    pendingFHigh.store (juce::jlimit (500.0f, 18000.0f, std::max (fh, fl + 50.0f)));
    coeffsDirty.store (true);
}

void WidthEngine::setWidths (float wl, float wm, float wh)
{
    pendingWLow.store  (juce::jlimit (0.0f, 2.0f, wl));
    pendingWMid.store  (juce::jlimit (0.0f, 2.0f, wm));
    pendingWHigh.store (juce::jlimit (0.0f, 2.0f, wh));
}

void WidthEngine::setSideTilt (float t)
{
    pendingTilt.store (juce::jlimit (-6.0f, 6.0f, t));
    coeffsDirty.store (true);
}

void WidthEngine::setMonomaker (bool on, float cutoff)
{
    pendingMonoOn.store (on);
    pendingMonoFreq.store (juce::jlimit (20.0f, 500.0f, cutoff));
    coeffsDirty.store (true);
}

void WidthEngine::updateCoefficientsIfNeeded()
{
    if (! coeffsDirty.exchange (false))
        return;

    fLow     = pendingFLow.load();
    fHigh    = pendingFHigh.load();
    tiltDb   = pendingTilt.load();
    monoOn   = pendingMonoOn.load();
    monoFreq = pendingMonoFreq.load();

    // LR4 = two cascaded Butterworth 2nd-order biquads with Q=0.7071
    constexpr float Q = 0.7071067811865476f;
    auto lpLowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, fLow,  Q);
    auto hpLowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, fLow,  Q);
    auto lpHighCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, fHigh, Q);
    auto hpHighCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, fHigh, Q);

    for (auto* bank : { &midBank, &sideBank })
    {
        bank->lpLow .setCoeffs (lpLowCoeffs);
        bank->hpLow .setCoeffs (hpLowCoeffs);
        bank->lpHigh.setCoeffs (lpHighCoeffs);
        bank->hpHigh.setCoeffs (hpHighCoeffs);
    }

    // Side tilt: low shelf at 100 Hz with -tilt/2 dB, high shelf at 10 kHz with +tilt/2 dB
    // (split between two shelves so the pivot stays near 1 kHz)
    const float halfTilt = tiltDb * 0.5f;
    const float lowGain  = juce::Decibels::decibelsToGain (-halfTilt);
    const float highGain = juce::Decibels::decibelsToGain ( halfTilt);
    auto tiltLowCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, 100.0f,   0.7f, lowGain);
    auto tiltHighCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 10000.0f, 0.7f, highGain);
    *sideBank.tiltLow .coefficients = *tiltLowCoeffs;
    *sideBank.tiltHigh.coefficients = *tiltHighCoeffs;

    // Monomaker side HP (LR4 = 2 cascaded Butterworth-2 HP)
    auto monoHPcoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, monoFreq, Q);
    *sideBank.monoHP_a.coefficients = *monoHPcoeffs;
    *sideBank.monoHP_b.coefficients = *monoHPcoeffs;
}

void WidthEngine::processMS (juce::AudioBuffer<float>& buf)
{
    updateCoefficientsIfNeeded();

    wLow  = pendingWLow.load();
    wMid  = pendingWMid.load();
    wHigh = pendingWHigh.load();

    auto* mid  = buf.getWritePointer (0);
    auto* side = buf.getWritePointer (1);
    const int n = buf.getNumSamples();

    const bool monoEnabled = monoOn;

    for (int i = 0; i < n; ++i)
    {
        float s = side[i];

        // 1. Side tilt EQ (low shelf + high shelf with opposite gains)
        s = sideBank.tiltLow .processSample (s);
        s = sideBank.tiltHigh.processSample (s);

        // 2. Multiband split on side (LR4 cascade — split at fLow, then split the highs at fHigh)
        const float sLow  = sideBank.lpLow.processSample (s);
        const float sHigh1 = sideBank.hpLow.processSample (s);
        const float sMid  = sideBank.lpHigh.processSample (sHigh1);
        const float sHigh = sideBank.hpHigh.processSample (sHigh1);

        // Per-band width gain on side
        float sOut = wLow * sLow + wMid * sMid + wHigh * sHigh;

        // 3. Monomaker — high-pass on side below cutoff
        if (monoEnabled)
        {
            sOut = sideBank.monoHP_a.processSample (sOut);
            sOut = sideBank.monoHP_b.processSample (sOut);
        }

        side[i] = sOut;

        // Mid: matching LR4 split-and-sum so the allpass phase of the side bank
        // is also applied to the mid path. This keeps the stereo image stable at unity width.
        float m = mid[i];
        const float mLow  = midBank.lpLow.processSample (m);
        const float mHigh1 = midBank.hpLow.processSample (m);
        const float mMid  = midBank.lpHigh.processSample (mHigh1);
        const float mHigh = midBank.hpHigh.processSample (mHigh1);
        mid[i] = mLow + mMid + mHigh;
    }
}
