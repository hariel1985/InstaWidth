#include "SideFIRBuilder.h"

SideFIRBuilder::SideFIRBuilder() : Thread ("SideFIRBuilder") {}
SideFIRBuilder::~SideFIRBuilder() { stop(); }

void SideFIRBuilder::start (double sr)
{
    sampleRate.store (sr);
    needsUpdate.store (true);
    startThread (juce::Thread::Priority::normal);
}

void SideFIRBuilder::stop()
{
    signalThreadShouldExit();
    notify();
    stopThread (2000);
}

namespace
{
    // Update an atomic only if the new value differs; returns true if it actually changed.
    template <typename T>
    bool storeIfChanged (std::atomic<T>& a, T v)
    {
        T old = a.load (std::memory_order_relaxed);
        if (old == v) return false;
        a.store (v, std::memory_order_relaxed);
        return true;
    }
}

void SideFIRBuilder::setFFTOrder (int order)
{
    if (storeIfChanged (fftOrder, juce::jlimit (minFFTOrder, maxFFTOrder, order)))
    {
        needsUpdate.store (true);
        notify();
    }
}

void SideFIRBuilder::setCrossovers (float fl, float fh)
{
    const float clFL = juce::jlimit (40.0f, 1000.0f, fl);
    const float clFH = juce::jlimit (500.0f, 18000.0f, std::max (fh, fl + 50.0f));
    bool changed = storeIfChanged (fLow, clFL);
    changed |= storeIfChanged (fHigh, clFH);
    if (changed) { needsUpdate.store (true); notify(); }
}

void SideFIRBuilder::setWidths (float wl, float wm, float wh)
{
    bool changed = storeIfChanged (wLow,  juce::jlimit (0.0f, 2.0f, wl));
    changed |= storeIfChanged (wMid,  juce::jlimit (0.0f, 2.0f, wm));
    changed |= storeIfChanged (wHigh, juce::jlimit (0.0f, 2.0f, wh));
    if (changed) { needsUpdate.store (true); notify(); }
}

void SideFIRBuilder::setSideTilt (float t)
{
    if (storeIfChanged (tiltDb, juce::jlimit (-6.0f, 6.0f, t)))
    {
        needsUpdate.store (true);
        notify();
    }
}

void SideFIRBuilder::setMonomaker (bool on, float cutoff)
{
    bool changed = storeIfChanged (monoOn, on);
    changed |= storeIfChanged (monoFreq, juce::jlimit (20.0f, 500.0f, cutoff));
    if (changed) { needsUpdate.store (true); notify(); }
}

std::unique_ptr<juce::AudioBuffer<float>> SideFIRBuilder::getNewFIR()
{
    const juce::SpinLock::ScopedTryLockType lock (firLock);
    if (lock.isLocked() && pendingFIR != nullptr)
        return std::move (pendingFIR);
    return nullptr;
}

void SideFIRBuilder::run()
{
    while (! threadShouldExit())
    {
        if (needsUpdate.exchange (false))
        {
            auto fir = generate (sampleRate.load(), fftOrder.load());
            {
                const juce::SpinLock::ScopedLockType lock (firLock);
                pendingFIR = std::make_unique<juce::AudioBuffer<float>> (std::move (fir));
            }
            // Debounce — give the audio thread + JUCE convolution time to consume this IR
            // and finish its internal partitioning before we possibly queue another one.
            // Critical for large IRs (8192/16384 taps), where the convolution's preparation
            // is slow enough that constant reloads otherwise leave it permanently un-prepared
            // (resulting in silent output, i.e. "full mono" in M/S decode).
            wait (150);
        }
        else
        {
            wait (-1);  // sleep indefinitely until notify()
        }
    }
}

juce::AudioBuffer<float> SideFIRBuilder::generate (double sr, int order)
{
    const int fftSize = 1 << order;
    const int numBins = fftSize / 2 + 1;

    const float fl  = fLow.load();
    const float fh  = fHigh.load();
    const float wl  = wLow.load();
    const float wm  = wMid.load();
    const float wh  = wHigh.load();
    const float td  = tiltDb.load();
    const bool  mOn = monoOn.load();
    const float mfc = monoFreq.load();

    // Pre-compute tilt shelf coefficients (so we can use JUCE's high-accuracy magnitude evaluator)
    const float halfTilt = td * 0.5f;
    const float lowGain  = juce::Decibels::decibelsToGain (-halfTilt);
    const float highGain = juce::Decibels::decibelsToGain ( halfTilt);
    auto tiltLow  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, 100.0f,   0.7f, lowGain);
    auto tiltHigh = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 10000.0f, 0.7f, highGain);

    std::vector<double> frequencies (numBins);
    for (int i = 0; i < numBins; ++i)
        frequencies[i] = (double) i * sr / (double) fftSize;

    std::vector<double> tiltLowMag (numBins), tiltHighMag (numBins);
    tiltLow ->getMagnitudeForFrequencyArray (frequencies.data(), tiltLowMag.data(),  numBins, sr);
    tiltHigh->getMagnitudeForFrequencyArray (frequencies.data(), tiltHighMag.data(), numBins, sr);

    // Build target magnitude
    std::vector<double> mag (numBins, 1.0);

    for (int i = 0; i < numBins; ++i)
    {
        double f = frequencies[i];

        // LR4 crossover weights — sum to 1 across all bands at every frequency
        auto lr4LowPass = [] (double freq, double fc)
        {
            double r4 = std::pow (freq / fc, 4.0);
            return 1.0 / (1.0 + r4);
        };

        double lowW   = lr4LowPass (f, fl);
        double bandHi = 1.0 - lowW;                 // = LR4_HP@fLow magnitude
        double midW   = bandHi * lr4LowPass (f, fh);
        double highW  = bandHi * (1.0 - lr4LowPass (f, fh));

        double bandSum = wl * lowW + wm * midW + wh * highW;

        double tiltMag = tiltLowMag[i] * tiltHighMag[i];

        double monoMag = 1.0;
        if (mOn)
        {
            // LR4 HP magnitude at monoFreq
            double r4 = std::pow (f / mfc, 4.0);
            monoMag = r4 / (1.0 + r4);
        }

        mag[i] = bandSum * tiltMag * monoMag;
    }

    // Zero-phase complex spectrum -> inverse real FFT
    std::vector<float> fftData (fftSize * 2, 0.0f);
    fftData[0] = (float) mag[0];
    fftData[1] = (float) mag[numBins - 1];
    for (int i = 1; i < numBins - 1; ++i)
    {
        fftData[i * 2]     = (float) mag[i];
        fftData[i * 2 + 1] = 0.0f;
    }

    juce::dsp::FFT fft (order);
    fft.performRealOnlyInverseTransform (fftData.data());

    juce::AudioBuffer<float> firBuffer (1, fftSize);
    float* firData = firBuffer.getWritePointer (0);
    const int half = fftSize / 2;
    for (int i = 0; i < fftSize; ++i)
        firData[i] = fftData[(i + half) % fftSize];

    // Window with Blackman-Harris to keep transition bands clean
    juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);
    window.multiplyWithWindowingTable (firData, fftSize);

    // DC normalization — but only when DC gain isn't intentionally near zero (monomaker ON suppresses DC).
    // We normalize against the magnitude at 1 kHz, which is always present in the side processing.
    {
        std::vector<float> analysisBuf (fftSize * 2, 0.0f);
        std::copy (firData, firData + fftSize, analysisBuf.data());
        juce::dsp::FFT analysisFft (order);
        analysisFft.performRealOnlyForwardTransform (analysisBuf.data());

        const double binRes = sr / (double) fftSize;
        const int refBin = juce::jlimit (1, fftSize / 2 - 1, (int) std::round (1000.0 / binRes));
        const float re = analysisBuf[refBin * 2];
        const float im = analysisBuf[refBin * 2 + 1];
        const float actualMag = std::sqrt (re * re + im * im);

        // Target magnitude at 1 kHz (re-compute analytically — must match the synthesis formula)
        const double f = 1000.0;
        const double r4_lo = std::pow (f / fl, 4.0);
        const double lowW = 1.0 / (1.0 + r4_lo);
        const double bandHi = 1.0 - lowW;
        const double r4_hi = std::pow (f / fh, 4.0);
        const double midW = bandHi * (1.0 / (1.0 + r4_hi));
        const double highW = bandHi * (r4_hi / (1.0 + r4_hi));
        std::vector<double> oneFreq { 1000.0 };
        std::vector<double> tLowMag (1), tHighMag (1);
        tiltLow ->getMagnitudeForFrequencyArray (oneFreq.data(), tLowMag.data(),  1, sr);
        tiltHigh->getMagnitudeForFrequencyArray (oneFreq.data(), tHighMag.data(), 1, sr);
        double targetMag = (wl * lowW + wm * midW + wh * highW) * tLowMag[0] * tHighMag[0];
        if (mOn)
        {
            const double r4 = std::pow (1000.0 / mfc, 4.0);
            targetMag *= r4 / (1.0 + r4);
        }

        if (actualMag > 1e-6f && targetMag > 1e-6)
        {
            float norm = (float) (targetMag / (double) actualMag);
            for (int i = 0; i < fftSize; ++i)
                firData[i] *= norm;
        }
    }

    return firBuffer;
}
