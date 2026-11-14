#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace hcdsp
{

//==============================================================================
// ITU-R BS.1770 K-weighting: stage 1 high shelf (+4 dB above ~1.7 kHz, head acoustics) followed by
// stage 2 "RLB" high-pass (~38 Hz). Coefficients are designed for any sample rate from the analogue
// prototype; at 48 kHz they reproduce the table published in BS.1770-4.
struct KWeightingCoefficients
{
    double shelfB[3] {}, shelfA[3] {};
    double hpB[3] {}, hpA[3] {};

    static KWeightingCoefficients design (double sampleRate) noexcept
    {
        KWeightingCoefficients c;
        const double pi = 3.14159265358979323846;

        {
            const double f0 = 1681.974450955533;
            const double gainDb = 3.999843853973347;
            const double q = 0.7071752369554196;
            const double k = std::tan (pi * f0 / sampleRate);
            const double vh = std::pow (10.0, gainDb / 20.0);
            const double vb = std::pow (vh, 0.4996667741545416);
            const double a0 = 1.0 + k / q + k * k;
            c.shelfB[0] = (vh + vb * k / q + k * k) / a0;
            c.shelfB[1] = 2.0 * (k * k - vh) / a0;
            c.shelfB[2] = (vh - vb * k / q + k * k) / a0;
            c.shelfA[0] = 1.0;
            c.shelfA[1] = 2.0 * (k * k - 1.0) / a0;
            c.shelfA[2] = (1.0 - k / q + k * k) / a0;
        }
        {
            const double f0 = 38.13547087602444;
            const double q = 0.5003270373238773;
            const double k = std::tan (pi * f0 / sampleRate);
            const double a0 = 1.0 + k / q + k * k;
            c.hpB[0] = 1.0;
            c.hpB[1] = -2.0;
            c.hpB[2] = 1.0;
            c.hpA[0] = 1.0;
            c.hpA[1] = 2.0 * (k * k - 1.0) / a0;
            c.hpA[2] = (1.0 - k / q + k * k) / a0;
        }
        return c;
    }
};

//==============================================================================
// Loudness-matching automatic gain ("Auto Gain").
//
// Measures the short-term loudness (3 s window, as EBU R128 "S") of a reference signal (the untouched
// plugin input) and of the processed signal, both K-weighted, and returns the gain that makes the
// processed signal as loud as the reference. Loudness rather than RMS is used because compression and
// harmonic saturation change the spectrum: plain RMS is dominated by low-frequency energy the ear is
// least sensitive to, so an RMS match leaves added upper harmonics sounding louder than the input.
//
// Feed-forward: the processed signal is measured before compensation, so the gain cannot oscillate.
// Measurement runs in 100 ms chunks. A chunk whose reference loudness is below the BS.1770 absolute gate
// (-70 LUFS) is discarded, so silence and fades hold the last gain instead of ramping it to the clamp.
// The 3 s window is long compared with compressor release times, so the match follows average level and
// does not undo the compression itself.
//
// Real-time safe: no allocation after prepare(), fixed-size state.
class LoudnessMatcher
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int windowChunks = 30;          // 30 x 100 ms = 3 s short-term window
    static constexpr double chunkSeconds = 0.1;
    static constexpr double absoluteGateLufs = -70.0;
    static constexpr float maxGainDb = 24.0f;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = std::max (8000.0, newSampleRate);
        coeffs = KWeightingCoefficients::design (sampleRate);
        chunkLength = std::max (1, (int) std::lround (chunkSeconds * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : refFilters)  f = {};
        for (auto& f : procFilters) f = {};
        refRing.fill (0.0);
        procRing.fill (0.0);
        ringCount = 0;
        ringPos = 0;
        chunkRef = chunkProc = 0.0;
        chunkFill = 0;
        targetGainDb = 0.0f;
        lastRefLufs = lastProcLufs = -100.0f;
    }

    // Feeds one block of the reference and of the processed (uncompensated) signal. Both buffers must
    // be time-aligned and hold the same channel count. Updates the target gain at every completed chunk.
    void process (const juce::AudioBuffer<float>& reference, const juce::AudioBuffer<float>& processed,
                  int numSamples) noexcept
    {
        const int numCh = std::min ({ reference.getNumChannels(), processed.getNumChannels(), maxChannels });
        if (numCh <= 0)
            return;

        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                // BS.1770: channel mean squares are summed with weight 1.0 for left and right.
                const double r = refFilters[(size_t) ch].process (reference.getReadPointer (ch)[s], coeffs);
                const double p = procFilters[(size_t) ch].process (processed.getReadPointer (ch)[s], coeffs);
                chunkRef  += r * r;
                chunkProc += p * p;
            }

            if (++chunkFill >= chunkLength)
                finishChunk();
        }
    }

    // Gain (dB) that matches the processed loudness to the reference over the last 3 s of gated audio.
    float getTargetGainDb() const noexcept { return targetGainDb; }
    float getReferenceLufs() const noexcept { return lastRefLufs; }
    float getProcessedLufs() const noexcept { return lastProcLufs; }

    static double meanSquareToLufs (double meanSquare) noexcept
    {
        return -0.691 + 10.0 * std::log10 (std::max (meanSquare, 1.0e-20));
    }

private:
    struct Biquad
    {
        double z1 = 0.0, z2 = 0.0;
        double run (double x, const double* b, const double* a) noexcept
        {
            // Transposed direct form II, double precision (the RLB pole sits very close to z = 1).
            const double y = b[0] * x + z1;
            z1 = b[1] * x - a[1] * y + z2;
            z2 = b[2] * x - a[2] * y;
            return y;
        }
    };

    struct KFilter
    {
        Biquad shelf, highPass;
        double process (float x, const KWeightingCoefficients& c) noexcept
        {
            return highPass.run (shelf.run ((double) x, c.shelfB, c.shelfA), c.hpB, c.hpA);
        }
    };

    void finishChunk() noexcept
    {
        const double refMs  = chunkRef  / (double) chunkFill;
        const double procMs = chunkProc / (double) chunkFill;
        chunkRef = chunkProc = 0.0;
        chunkFill = 0;

        // Absolute gate on the reference: silence carries no information about the processing gain.
        if (meanSquareToLufs (refMs) < absoluteGateLufs)
            return;

        refRing[(size_t) ringPos]  = refMs;
        procRing[(size_t) ringPos] = procMs;
        ringPos = (ringPos + 1) % windowChunks;
        ringCount = std::min (ringCount + 1, windowChunks);

        // Re-summing 30 values each chunk avoids running-sum drift.
        double refSum = 0.0, procSum = 0.0;
        for (int i = 0; i < ringCount; ++i)
        {
            refSum  += refRing[(size_t) i];
            procSum += procRing[(size_t) i];
        }

        lastRefLufs  = (float) meanSquareToLufs (refSum  / ringCount);
        lastProcLufs = (float) meanSquareToLufs (procSum / ringCount);
        targetGainDb = std::clamp (lastRefLufs - lastProcLufs, -maxGainDb, maxGainDb);
    }

    double sampleRate = 48000.0;
    KWeightingCoefficients coeffs = KWeightingCoefficients::design (48000.0);
    int chunkLength = 4800;

    std::array<KFilter, maxChannels> refFilters {}, procFilters {};
    std::array<double, windowChunks> refRing {}, procRing {};
    int ringCount = 0, ringPos = 0;
    double chunkRef = 0.0, chunkProc = 0.0;
    int chunkFill = 0;

    float targetGainDb = 0.0f;
    float lastRefLufs = -100.0f, lastProcLufs = -100.0f;
};

} // namespace hcdsp
