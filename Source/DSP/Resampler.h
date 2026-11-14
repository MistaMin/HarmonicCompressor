#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace hcdsp
{

enum class OversampleMode : int { Native = 0, TwoX = 1, FourX = 2, EightX = 3 };
enum class ResamplerFilter : int { MinPhaseIIR = 0, LinPhaseFIR = 1 };

// Multirate oversampling engine (1x / 2x / 4x / 8x) supporting both Minimum Phase (polyphase IIR)
// and Linear Phase (equiripple FIR) half-band filters.
//
// Dry-path alignment: the dry signals (the dry for parallel Mix, and the untouched input used by Delta)
// run through a second, identical up/down filter chain with no processing in between. They therefore get
// exactly the wet path's magnitude and phase response, not just its nominal latency. A delay line cannot
// do this: the latency is fractional (e.g. 4.433 samples at 4x Min Phase), interpolation low-passes the
// dry, and a minimum-phase filter is not a delay at all, so its phase cannot be matched by one.
class MultirateEngine
{
public:
    void prepare (double sampleRate, int samplesPerBlock, int numChannels)
    {
        baseSampleRate = sampleRate;
        baseBlockSize = std::max (1, samplesPerBlock);
        channels = std::max (1, numChannels);

        wet.build (channels, baseBlockSize);
        dry.build (2 * channels, baseBlockSize);              // dry channels, then reference channels
        dryWork.setSize (2 * channels, baseBlockSize, false, true, false);
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
        wet.reset();
        dry.reset();
    }

    juce::dsp::AudioBlock<float> upsample (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        if (auto* os = wet.get (mode, filter))
            return os->processSamplesUp (block);
        return block;
    }

    void downsample (juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        if (auto* os = wet.get (mode, filter))
            os->processSamplesDown (block);
    }

    float getLatencySamplesFor (OversampleMode m, ResamplerFilter flt) const noexcept
    {
        auto* os = wet.get (m, flt);
        return os != nullptr ? os->getLatencyInSamples() : 0.0f;
    }

    float getLatencySamples() const noexcept
    {
        return getLatencySamplesFor (mode, filter);
    }

    // Runs `dryBuffer` and `referenceBuffer` (same channel count as the wet path) in place through the
    // matched filter chain, so each lines up with the oversampled wet signal in magnitude and phase.
    // Must be called every block, with the same number of samples as the wet path, to keep its state
    // in step with the wet chain. At 1x it does nothing.
    void alignDryBuffers (juce::AudioBuffer<float>& dryBuffer, juce::AudioBuffer<float>& referenceBuffer,
                          int numSamples) noexcept
    {
        auto* os = dry.get (mode, filter);
        if (os == nullptr || numSamples <= 0)
            return;

        const int numCh = std::min ({ dryBuffer.getNumChannels(), referenceBuffer.getNumChannels(), channels });
        for (int ch = 0; ch < numCh; ++ch)
        {
            dryWork.copyFrom (ch, 0, dryBuffer, ch, 0, numSamples);
            dryWork.copyFrom (channels + ch, 0, referenceBuffer, ch, 0, numSamples);
        }

        auto block = juce::dsp::AudioBlock<float> (dryWork).getSubsetChannelBlock (0, (size_t) (2 * channels))
                                                           .getSubBlock (0, (size_t) numSamples);
        os->processSamplesUp (block);
        os->processSamplesDown (block);

        for (int ch = 0; ch < numCh; ++ch)
        {
            dryBuffer.copyFrom (ch, 0, dryWork, ch, 0, numSamples);
            referenceBuffer.copyFrom (ch, 0, dryWork, channels + ch, 0, numSamples);
        }
    }

private:
    // One set of oversamplers: {2x, 4x, 8x} x {Min Phase IIR, Lin Phase FIR}.
    struct Chain
    {
        std::unique_ptr<juce::dsp::Oversampling<float>> stages[3][2];

        void build (int numChannels, int blockSize)
        {
            for (size_t f = 0; f < 2; ++f)
            {
                const auto filterType = (f == 0)
                    ? juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
                    : juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple;
                for (size_t order = 1; order <= 3; ++order)
                {
                    auto& os = stages[order - 1][f];
                    os = std::make_unique<juce::dsp::Oversampling<float>> (
                        static_cast<size_t> (numChannels), order, filterType, true);
                    os->initProcessing (static_cast<size_t> (blockSize));
                }
            }
        }

        void reset() noexcept
        {
            for (auto& row : stages)
                for (auto& os : row)
                    if (os) os->reset();
        }

        juce::dsp::Oversampling<float>* get (OversampleMode m, ResamplerFilter flt) const noexcept
        {
            if (m == OversampleMode::Native)
                return nullptr;
            return stages[static_cast<size_t> (m) - 1][static_cast<size_t> (flt)].get();
        }
    };

    double baseSampleRate = 44100.0;
    int baseBlockSize = 512;
    int channels = 2;
    OversampleMode mode = OversampleMode::TwoX;
    ResamplerFilter filter = ResamplerFilter::MinPhaseIIR;

    Chain wet, dry;
    juce::AudioBuffer<float> dryWork;
};

} // namespace hcdsp
