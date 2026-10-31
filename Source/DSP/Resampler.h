#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace hcdsp
{

enum class OversampleMode : int { Native = 0, TwoX = 1, FourX = 2, EightX = 3 };
enum class ResamplerFilter : int { MinPhaseIIR = 0, LinPhaseFIR = 1 };

// Multirate oversampling engine (1x / 2x / 4x / 8x) supporting both Minimum Phase (polyphase IIR)
// and Linear Phase (equiripple FIR) half-band filters, plus a pre-allocated fractional-delay
// compensator so parallel dry/wet Mix stays phase-aligned with the oversampled wet path.
class MultirateEngine
{
public:
    void prepare (double sampleRate, int samplesPerBlock, int numChannels)
    {
        baseSampleRate = sampleRate;
        baseBlockSize = std::max (1, samplesPerBlock);
        channels = std::max (1, numChannels);

        for (size_t f = 0; f < 2; ++f)
        {
            const auto filterType = (f == 0)
                ? juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
                : juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple;

            oversamplers2x[f] = std::make_unique<juce::dsp::Oversampling<float>> (
                static_cast<size_t> (channels), 1, filterType, true);
            oversamplers4x[f] = std::make_unique<juce::dsp::Oversampling<float>> (
                static_cast<size_t> (channels), 2, filterType, true);
            oversamplers8x[f] = std::make_unique<juce::dsp::Oversampling<float>> (
                static_cast<size_t> (channels), 3, filterType, true);

            oversamplers2x[f]->initProcessing (static_cast<size_t> (baseBlockSize));
            oversamplers4x[f]->initProcessing (static_cast<size_t> (baseBlockSize));
            oversamplers8x[f]->initProcessing (static_cast<size_t> (baseBlockSize));
        }

        // Pre-allocate dry delay ring buffer (max 8x FIR latency < 512 samples; 2048 gives ample margin).
        delayRingSize = 2048;
        delayRing.assign (static_cast<size_t> (channels * delayRingSize), 0.0f);
        delayWritePos = 0;
        reset();
    }

    void ensureBlockSize (int numSamples)
    {
        if (numSamples <= baseBlockSize)
            return;
        prepare (baseSampleRate, numSamples, channels);
    }

    bool setConfig (OversampleMode newMode, ResamplerFilter newFilter) noexcept
    {
        if (newMode == mode && newFilter == filter)
            return false;
        mode = newMode;
        filter = newFilter;
        reset();
        return true;
    }

    OversampleMode getMode() const noexcept { return mode; }
    ResamplerFilter getFilter() const noexcept { return filter; }

    int getFactor() const noexcept
    {
        switch (mode)
        {
            case OversampleMode::Native: return 1;
            case OversampleMode::TwoX:   return 2;
            case OversampleMode::FourX:  return 4;
            case OversampleMode::EightX: return 8;
        }
        return 1;
    }

    double getEffectiveSampleRate() const noexcept
    {
        return baseSampleRate * static_cast<double> (getFactor());
    }

    void reset() noexcept
    {
        for (size_t f = 0; f < 2; ++f)
        {
            if (oversamplers2x[f]) oversamplers2x[f]->reset();
            if (oversamplers4x[f]) oversamplers4x[f]->reset();
            if (oversamplers8x[f]) oversamplers8x[f]->reset();
        }
        std::fill (delayRing.begin(), delayRing.end(), 0.0f);
        delayWritePos = 0;
    }

    juce::dsp::AudioBlock<float> upsample (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        const size_t f = static_cast<size_t> (filter);
        switch (mode)
        {
            case OversampleMode::TwoX:   return oversamplers2x[f]->processSamplesUp (block);
            case OversampleMode::FourX:  return oversamplers4x[f]->processSamplesUp (block);
            case OversampleMode::EightX: return oversamplers8x[f]->processSamplesUp (block);
            case OversampleMode::Native: return block;
        }
        return block;
    }

    void downsample (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        const size_t f = static_cast<size_t> (filter);
        switch (mode)
        {
            case OversampleMode::TwoX:   oversamplers2x[f]->processSamplesDown (block); break;
            case OversampleMode::FourX:  oversamplers4x[f]->processSamplesDown (block); break;
            case OversampleMode::EightX: oversamplers8x[f]->processSamplesDown (block); break;
            case OversampleMode::Native: break;
        }
    }

    float getLatencySamplesFor (OversampleMode m, ResamplerFilter flt) const noexcept
    {
        const size_t f = static_cast<size_t> (flt);
        switch (m)
        {
            case OversampleMode::TwoX:   return oversamplers2x[f] ? oversamplers2x[f]->getLatencyInSamples() : 0.0f;
            case OversampleMode::FourX:  return oversamplers4x[f] ? oversamplers4x[f]->getLatencyInSamples() : 0.0f;
            case OversampleMode::EightX: return oversamplers8x[f] ? oversamplers8x[f]->getLatencyInSamples() : 0.0f;
            case OversampleMode::Native: return 0.0f;
        }
        return 0.0f;
    }

    float getLatencySamples() const noexcept
    {
        return getLatencySamplesFor (mode, filter);
    }

    // Delays `dryBuffer` in-place by `getLatencySamples()` so parallel Mix aligns with oversampled wet signal.
    // Must be called every block so the delay ring history stays warm across Mix automation.
    void delayDryBuffer (juce::AudioBuffer<float>& dryBuffer, int numSamples) noexcept
    {
        if (delayRing.empty() || delayRingSize <= 4)
            return;

        const float delay = std::clamp (getLatencySamples(), 0.0f, static_cast<float> (delayRingSize - 4));
        const int numCh = std::min (dryBuffer.getNumChannels(), channels);
        const int dInt = static_cast<int> ( std::floor (delay));
        const float frac = delay - static_cast<float> (dInt);
        const int mask = delayRingSize - 1; // delayRingSize == 2048 (power of two)

        int wp = delayWritePos;
        for (int s = 0; s < numSamples; ++s)
        {
            const int r0 = (wp - dInt + delayRingSize) & mask;
            const int r1 = (r0 - 1 + delayRingSize) & mask;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* ring = delayRing.data() + static_cast<size_t> (ch * delayRingSize);
                float* data = dryBuffer.getWritePointer (ch);
                ring[wp] = data[s];
                data[s] = (frac <= 1.0e-5f) ? ring[r0] : (ring[r0] + frac * (ring[r1] - ring[r0]));
            }
            wp = (wp + 1) & mask;
        }
        delayWritePos = wp;
    }

private:
    double baseSampleRate = 44100.0;
    int baseBlockSize = 512;
    int channels = 2;
    OversampleMode mode = OversampleMode::TwoX;
    ResamplerFilter filter = ResamplerFilter::MinPhaseIIR;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers2x[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers4x[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers8x[2];

    int delayRingSize = 2048;
    int delayWritePos = 0;
    std::vector<float> delayRing;
};

} // namespace hcdsp
