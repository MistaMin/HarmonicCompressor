#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

namespace hcdsp
{

// Level-independent stereo-linked differential-envelope Transient Designer.
// Placed at the end of the signal chain (post-mix, pre-output-gain).
// - Attack (-100..+100 %): boosts or softens transient onsets via fast-vs-reference envelope crest.
// - Sustain (-100..+100 %): extends or tightens decay/room tails via slow-vs-fast envelope difference.
// - Speed (0.5..50 ms): scales detector integration time constants from snappy percussion to broad program material.
class TransientDesigner
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = std::max (8000.0, newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        fastEnv = 0.0f;
        medEnv = 0.0f;
        slowEnv = 0.0f;
        smoothGainDb = 0.0f;
        lastAppliedGainDb = 0.0f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, int numSamples,
                       bool enabled, float attackPct, float sustainPct, float speedMs) noexcept
    {
        const int numCh = buffer.getNumChannels();
        if (numCh <= 0 || numSamples <= 0)
            return;

        const float speedScale = std::clamp (speedMs / 10.0f, 0.05f, 5.0f);
        const float fastAtt = timeToCoeff (0.35f * speedScale);
        const float fastRel = timeToCoeff (11.0f * speedScale);
        const float medAtt  = timeToCoeff (14.0f * speedScale);
        const float medRel  = timeToCoeff (58.0f * speedScale);
        const float slowAtt = timeToCoeff (24.0f * speedScale);
        const float slowRel = timeToCoeff (320.0f * speedScale);
        const float gainSmooth = timeToCoeff (0.6f);

        const float attAmount = std::clamp (attackPct * 0.01f, -1.0f, 1.0f);
        const float susAmount = std::clamp (sustainPct * 0.01f, -1.0f, 1.0f);

        float* ch0 = buffer.getWritePointer (0);
        float* ch1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        float peakGainMag = 0.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float det = std::abs (ch0[s]);
            if (ch1 != nullptr)
                det = std::max (det, std::abs (ch1[s]));
            for (int c = 2; c < numCh; ++c)
                det = std::max (det, std::abs (buffer.getReadPointer (c)[s]));

            fastEnv = stepEnv (fastEnv, det, fastAtt, fastRel);
            medEnv  = stepEnv (medEnv,  det, medAtt,  medRel);
            slowEnv = stepEnv (slowEnv, det, slowAtt, slowRel);

            float targetDb = 0.0f;
            if (enabled)
            {
                const float fastDb = 20.0f * std::log10 (fastEnv + 1.0e-6f);
                const float medDb  = 20.0f * std::log10 (medEnv  + 1.0e-6f);
                const float slowDb = 20.0f * std::log10 (slowEnv + 1.0e-6f);

                // Smooth floor gate (-66..-50 dBFS) so silence / low hiss is untouched
                const float gate = std::clamp ((slowDb + 66.0f) / 16.0f, 0.0f, 1.0f);

                const float attDiffDb = std::max (0.0f, fastDb - medDb);
                const float susDiffDb = std::max (0.0f, slowDb - fastDb);

                const float attGainDb = attAmount * std::min (15.0f, attDiffDb * 1.65f);
                const float susGainDb = susAmount * std::min (15.0f, susDiffDb * 1.85f);
                targetDb = std::clamp (gate * (attGainDb + susGainDb), -18.0f, 18.0f);
            }

            smoothGainDb = gainSmooth * smoothGainDb + (1.0f - gainSmooth) * targetDb;
            if (std::abs (smoothGainDb) > std::abs (peakGainMag))
                peakGainMag = smoothGainDb;

            if (enabled || std::abs (smoothGainDb) > 1.0e-4f)
            {
                const float g = juce::Decibels::decibelsToGain (smoothGainDb);
                ch0[s] *= g;
                if (ch1 != nullptr)
                    ch1[s] *= g;
                for (int c = 2; c < numCh; ++c)
                    buffer.getWritePointer (c)[s] *= g;
            }
        }

        lastAppliedGainDb = peakGainMag;
    }

    float getLastAppliedGainDb() const noexcept { return lastAppliedGainDb; }

private:
    float timeToCoeff (float ms) const noexcept
    {
        return std::exp (-1.0f / (0.001f * std::max (0.05f, ms) * static_cast<float> (sampleRate)));
    }

    static float stepEnv (float env, float x, float att, float rel) noexcept
    {
        const float c = (x > env) ? att : rel;
        return c * env + (1.0f - c) * x;
    }

    double sampleRate = 48000.0;
    float fastEnv = 0.0f;
    float medEnv = 0.0f;
    float slowEnv = 0.0f;
    float smoothGainDb = 0.0f;
    float lastAppliedGainDb = 0.0f;
};

} // namespace hcdsp
