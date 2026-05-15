#include "SideDeEsser.h"

void SideDeEsser::prepare (double sampleRate, int blockSize)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 1 };
    bp.prepare (spec);

    // 2 ms attack, 80 ms release
    attackCoeff  = (float) std::exp (-1.0 / (0.002 * sampleRate));
    releaseCoeff = (float) std::exp (-1.0 / (0.080 * sampleRate));

    dirty.store (true);
    updateCoeffsIfNeeded();
}

void SideDeEsser::reset()
{
    bp.reset();
    envelope = 0.0f;
    currentGRdb.store (0.0f);
}

void SideDeEsser::updateCoeffsIfNeeded()
{
    if (! dirty.exchange (false))
        return;
    centerFreq = pendingFreq.load();
    // Q ~ 1.4: about 0.6 octave wide. Tight enough to focus on sibilance, broad enough for musical de-essing.
    auto c = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, centerFreq, 1.4f);
    *bp.coefficients = *c;
}

float SideDeEsser::processSide (float* side, int n)
{
    updateCoeffsIfNeeded();

    if (! enabled.load())
    {
        currentGRdb.store (0.0f);
        return 0.0f;
    }

    const float thrLin   = juce::Decibels::decibelsToGain (thresholdDb.load());
    const float maxRange = rangeDb.load();
    float maxGR = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float s = side[i];
        const float det = std::abs (bp.processSample (s));

        // Envelope follower (peak)
        const float coeff = (det > envelope) ? attackCoeff : releaseCoeff;
        envelope = coeff * envelope + (1.0f - coeff) * det;

        float grDb = 0.0f;
        if (envelope > thrLin && thrLin > 0.0f)
        {
            // soft compression: 4:1 above threshold, capped at -range dB
            const float overDb = juce::Decibels::gainToDecibels (envelope / thrLin);
            grDb = overDb * 0.75f;   // 4:1 ratio → 75% of over-threshold gets attenuated
            grDb = std::min (grDb, maxRange);
            maxGR = std::max (maxGR, grDb);
        }

        const float gain = juce::Decibels::decibelsToGain (-grDb);
        side[i] = s * gain;
    }

    currentGRdb.store (maxGR);
    return maxGR;
}
