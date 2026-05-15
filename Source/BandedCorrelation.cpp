#include "BandedCorrelation.h"

void BandedCorrelation::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    alpha = (float) std::exp (-1.0 / (0.200 * sampleRate));  // ~200 ms integration

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    for (auto* f : { &lpFLowL, &hpFLowL, &lpFHighL, &hpFHighL,
                     &lpFLowR, &hpFLowR, &lpFHighR, &hpFHighR })
        f->prepare (spec);

    overall = {};
    for (auto& b : perBand) b = {};

    dirty.store (true);
    updateCoefficientsIfNeeded();
}

void BandedCorrelation::setCrossovers (float fl, float fh)
{
    const float clFL = juce::jlimit (40.0f, 1000.0f, fl);
    const float clFH = juce::jlimit (500.0f, 18000.0f, std::max (fh, fl + 50.0f));
    bool changed = false;
    if (pendingFLow.load()  != clFL) { pendingFLow.store  (clFL); changed = true; }
    if (pendingFHigh.load() != clFH) { pendingFHigh.store (clFH); changed = true; }
    if (changed) dirty.store (true);
}

void BandedCorrelation::updateCoefficientsIfNeeded()
{
    if (! dirty.exchange (false)) return;

    const float fLow  = pendingFLow.load();
    const float fHigh = pendingFHigh.load();
    constexpr float Q = 0.7071067811865476f;

    auto lpLow  = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, fLow,  Q);
    auto hpLow  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, fLow,  Q);
    auto lpHigh = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, fHigh, Q);
    auto hpHigh = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, fHigh, Q);

    *lpFLowL .coefficients = *lpLow;  *hpFLowL .coefficients = *hpLow;
    *lpFHighL.coefficients = *lpHigh; *hpFHighL.coefficients = *hpHigh;
    *lpFLowR .coefficients = *lpLow;  *hpFLowR .coefficients = *hpLow;
    *lpFHighR.coefficients = *lpHigh; *hpFHighR.coefficients = *hpHigh;
}

void BandedCorrelation::processBlock (const float* L, const float* R, int n)
{
    updateCoefficientsIfNeeded();

    const float a = alpha;
    const float oneMinusA = 1.0f - a;

    for (int i = 0; i < n; ++i)
    {
        const float l = L[i];
        const float r = R[i];

        const float lLow  = lpFLowL .processSample (l);
        const float lTemp = hpFLowL .processSample (l);
        const float lMid  = lpFHighL.processSample (lTemp);
        const float lHigh = hpFHighL.processSample (lTemp);

        const float rLow  = lpFLowR .processSample (r);
        const float rTemp = hpFLowR .processSample (r);
        const float rMid  = lpFHighR.processSample (rTemp);
        const float rHigh = hpFHighR.processSample (rTemp);

        overall.lr = a * overall.lr + oneMinusA * (l * r);
        overall.ll = a * overall.ll + oneMinusA * (l * l);
        overall.rr = a * overall.rr + oneMinusA * (r * r);

        perBand[0].lr = a * perBand[0].lr + oneMinusA * (lLow  * rLow);
        perBand[0].ll = a * perBand[0].ll + oneMinusA * (lLow  * lLow);
        perBand[0].rr = a * perBand[0].rr + oneMinusA * (rLow  * rLow);

        perBand[1].lr = a * perBand[1].lr + oneMinusA * (lMid  * rMid);
        perBand[1].ll = a * perBand[1].ll + oneMinusA * (lMid  * lMid);
        perBand[1].rr = a * perBand[1].rr + oneMinusA * (rMid  * rMid);

        perBand[2].lr = a * perBand[2].lr + oneMinusA * (lHigh * rHigh);
        perBand[2].ll = a * perBand[2].ll + oneMinusA * (lHigh * lHigh);
        perBand[2].rr = a * perBand[2].rr + oneMinusA * (rHigh * rHigh);
    }

    // Publish at the end of the block (one snapshot per block — cheap and tear-free)
    auto compute = [] (const Accum& a) -> float
    {
        const float denom = std::sqrt (std::max (a.ll * a.rr, 1e-12f));
        return (denom > 1e-9f) ? juce::jlimit (-1.0f, 1.0f, a.lr / denom) : 1.0f;
    };
    overallCorr.store (compute (overall));
    for (int b = 0; b < numBands; ++b)
        bandCorr[b].store (compute (perBand[b]));
}
