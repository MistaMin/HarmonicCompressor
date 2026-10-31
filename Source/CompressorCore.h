#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include "DSP/Adaa.h"

//==============================================================================
// Parameter IDs (used for both APVTS registration and atomic lookups)
//==============================================================================
namespace ParamIDs
{
    static constexpr auto algorithmMode   = "algorithmMode";   // choice: 0..9, see CompressorMode
    static constexpr auto threshold       = "threshold";       // dB
    static constexpr auto ratio           = "ratio";           // x:1
    static constexpr auto attack          = "attack";          // ms
    static constexpr auto release         = "release";         // ms
    static constexpr auto knee            = "knee";            // dB, 0 = hard knee
    static constexpr auto makeupGain      = "makeupGain";      // dB
    static constexpr auto mix             = "mix";             // 0..1 dry/wet

    static constexpr auto harmonic2       = "harmonic2";       // 0..100 %
    static constexpr auto harmonic3       = "harmonic3";       // 0..100 %
    static constexpr auto harmonic4       = "harmonic4";       // 0..100 %
    static constexpr auto harmonic5       = "harmonic5";       // 0..100 %

    static constexpr auto scEnabled       = "scEnabled";       // bool
    static constexpr auto scHPFreq        = "scHPFreq";        // Hz, 20-500
    static constexpr auto scListen        = "scListen";        // bool, audition sidechain
    static constexpr auto autoRelease     = "autoRelease";     // bool
    static constexpr auto autoAttack      = "autoAttack";      // bool

    // Appended v1.5 parameters (preserving indices 0..16 for existing sessions)
    static constexpr auto inputGain        = "inputGain";        // -24..+24 dB
    static constexpr auto outputGain       = "outputGain";       // -24..+24 dB
    static constexpr auto oversampleMode   = "oversampleMode";   // 1x, 2x, 4x, 8x
    static constexpr auto oversampleFilter = "oversampleFilter"; // Min Phase, Lin Phase
    static constexpr auto transientEnabled = "transientEnabled"; // bool modifier
    static constexpr auto transientAttack  = "transientAttack";  // -100..+100 %
    static constexpr auto transientSustain = "transientSustain"; // -100..+100 %
    static constexpr auto transientSpeed   = "transientSpeed";   // 0.5..50 ms
}

/**
 * Enum for Algorithm Selection. Values map 1:1 to the discrete UI toggle
 * buttons and to the AudioParameterChoice index stored in the APVTS.
 */
enum class CompressorMode : int
{
    Opto = 0,
    VCA,
    FET,
    RedStyle,
    Hybrid,
    Bridge,
    BusVCA,
    FETCircuit,
    OptoLeveler,
    RMSVCADynamics,
    Count
};

/**
 * Plain-old-data struct carrying the *smoothed*, audio-thread-ready
 * parameter values for a single block. Filled once per block from the
 * atomics/APVTS on the message thread's cached values, then passed by
 * const-ref into the hot loop -- no locks, no allocations.
 */
struct CompressorParams
{
    float thresholdDb   = -18.0f;
    float ratio         = 4.0f;
    float attackMs      = 10.0f;
    float releaseMs     = 100.0f;
    float kneeDb         = 6.0f;
    float makeupGainDb  = 0.0f;
    float mix           = 1.0f;
    bool  autoRelease   = false;
    bool  autoAttack    = false;
};

struct AlgorithmDefaults
{
    float attackMs;
    float releaseMs;
    float kneeDb;
    float thd2nd;   // typical 2nd harmonic level (0..1)
    float thd3rd;   // typical 3rd harmonic level (0..1)
    bool  isFeedback;

    static AlgorithmDefaults forMode (CompressorMode m) noexcept
    {
        switch (m)
        {
            case CompressorMode::Opto:     return { 10.0f, 500.0f, 12.0f, 0.15f, 0.05f, true  };
            case CompressorMode::VCA:      return {  5.0f, 100.0f,  0.0f, 0.04f, 0.25f, true  };
            case CompressorMode::FET:      return {  0.2f, 200.0f,  3.0f, 0.10f, 0.12f, true  };
            case CompressorMode::RedStyle: return { 10.0f, 300.0f,  6.0f, 0.02f, 0.02f, false };
            case CompressorMode::Hybrid:   return {  5.0f, 150.0f,  6.0f, 0.05f, 0.05f, false };
            case CompressorMode::Bridge: return { 2.0f, 400.0f, 0.0f, 0.0f, 0.0f, true };
            case CompressorMode::BusVCA: return { 3.0f, 100.0f, 0.0f, 0.0f, 0.0f, true };
            case CompressorMode::FETCircuit: return { 0.2f, 200.0f, 0.0f, 0.0f, 0.0f, true };
            case CompressorMode::OptoLeveler: return { 116.54f, 91.85f, 0.0f, 0.0f, 0.0f, true };
            case CompressorMode::RMSVCADynamics: return { 34.5f, 34.5f, 0.0f, 0.0f, 0.0f, false };
            default:                       return { 10.0f, 100.0f,  6.0f, 0.0f,  0.0f,  false };
        }
    }
};

//==============================================================================
// Slope helper: computes (1 - 1/ratio) safely, handling infinity (100.0)
// and the zero-crossing region.
//==============================================================================
inline float compressorSlope (float ratio) noexcept
{
    if (ratio >= 99.5f) return 1.0f;
    if (std::abs (ratio) < 0.01f) return 1.0f;
    return 1.0f - 1.0f / ratio;
}

inline float computeSoftKneeGr (float inputDb, const CompressorParams& p) noexcept
{
    const float slope  = compressorSlope (p.ratio);
    const float knee   = juce::jmax (0.0f, p.kneeDb);
    const float overDb = inputDb - p.thresholdDb;

    if (knee <= 0.0001f)
        return overDb > 0.0f ? -overDb * slope : 0.0f;

    const float halfKnee = knee * 0.5f;
    if (overDb <= -halfKnee)
        return 0.0f;
    if (overDb >= halfKnee)
        return -overDb * slope;

    const float x = overDb + halfKnee;
    return -(slope * x * x) / (2.0f * knee);
}

//==============================================================================
// Abstract algorithm interface
//==============================================================================
class CompressorAlgorithm
{
public:
    virtual ~CompressorAlgorithm() = default;

    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;

    // Updates the algorithm's internal coefficients. Called once per block
    // (or once per parameter change) from the audio thread -- must not
    // allocate or lock.
    virtual void updateParameters(const CompressorParams& params) = 0;

    // Computes the linear gain-reduction multiplier for a single sidechain
    // sample. Implementations own their own envelope-follower state.
    virtual float processSidechain(float sidechainSample) = 0;
    virtual float processAudio(float x, float gain, int) { return x * gain; }

    // Exposes the last computed gain reduction in dB, purely for the UI
    // meter/curve display. Safe to read from the message thread as a
    // relaxed load.
    float getLastGainReductionDb() const noexcept { return lastGrDb_.load(std::memory_order_relaxed); }

protected:
    std::atomic<float> lastGrDb_{0.0f};
    double sampleRate_ = 44100.0;
};

//==============================================================================
// 1. Optical (Opto) -- dual-stage program-dependent attack/release,
//    modelling the LA-2A "memory" behaviour where release time lengthens
//    the longer/harder the gain reduction has been engaged.
//==============================================================================
class OptoAlgorithm final : public CompressorAlgorithm
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate_ = spec.sampleRate;
        reset();
    }

    void reset() override
    {
        envelopeDb_ = 0.0f;
        memoryState_ = 0.0f;
        feedbackGrDb_ = 0.0f;
        autoSlowEnv_ = 0.0f;
        lastGrDb_.store(0.0f, std::memory_order_relaxed);
    }

    void updateParameters(const CompressorParams& p) override
    {
        params_ = p;
        fastAttackCoeff_  = timeToCoeff(p.attackMs * 0.3f);
        slowReleaseCoeff_ = timeToCoeff(p.releaseMs);
        fastReleaseCoeff_ = timeToCoeff(p.releaseMs * 0.15f);
    }

    float processSidechain(float x) override
    {
        // LA-2A feedback topology: detector sees gain-reduced signal
        const float feedbackSample = x * juce::Decibels::decibelsToGain(feedbackGrDb_);
        const float inputDb = juce::Decibels::gainToDecibels(std::abs(feedbackSample) + 1.0e-12f);
        const float targetGrDb = computeSoftKneeGr(inputDb, params_);

        const bool rising = targetGrDb < envelopeDb_;

        float releaseCoeff;
        float attackCoeff = fastAttackCoeff_;
        if (params_.autoRelease || params_.autoAttack)
        {
            autoSlowEnv_ = 0.9998f * autoSlowEnv_ + 0.0002f * std::abs(envelopeDb_);
            float adaptFactor = juce::jlimit(0.0f, 1.0f, autoSlowEnv_ / 6.0f);
            if (params_.autoRelease)
                releaseCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.releaseMs * 0.15f, params_.releaseMs * 3.0f));
            else
                releaseCoeff = juce::jmap(memoryState_, slowReleaseCoeff_, fastReleaseCoeff_);
            if (params_.autoAttack)
                attackCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.attackMs * 0.15f, params_.attackMs * 0.5f));
        }
        else
        {
            releaseCoeff = juce::jmap(memoryState_, slowReleaseCoeff_, fastReleaseCoeff_);
        }
        const float coeff = rising ? attackCoeff : releaseCoeff;

        envelopeDb_ = coeff * envelopeDb_ + (1.0f - coeff) * targetGrDb;
        memoryState_ = 0.995f * memoryState_ + 0.005f * (rising ? 1.0f : 0.0f);
        feedbackGrDb_ = envelopeDb_;

        lastGrDb_.store(envelopeDb_, std::memory_order_relaxed);
        return juce::Decibels::decibelsToGain(envelopeDb_);
    }

private:
    float timeToCoeff(float ms) const noexcept
    {
        return std::exp(-1.0f / (0.001f * juce::jmax(0.1f, ms) * (float)sampleRate_));
    }

    CompressorParams params_;
    float envelopeDb_ = 0.0f;
    float memoryState_ = 0.0f;
    float feedbackGrDb_ = 0.0f;
    float autoSlowEnv_ = 0.0f;
    float fastAttackCoeff_ = 0.0f, slowReleaseCoeff_ = 0.0f, fastReleaseCoeff_ = 0.0f;
};

//==============================================================================
// 2. VCA / Digital -- classic feed-forward peak/RMS detector with a fully
//    adjustable, continuously variable hard->soft knee.
//==============================================================================
class VCAAlgorithm final : public CompressorAlgorithm
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate_ = spec.sampleRate;
        reset();
    }

    void reset() override
    {
        envelopeDb_ = 0.0f;
        rmsSquared_ = 0.0f;
        peakEnv_ = 0.0f;
        autoSlowEnv_ = 0.0f;
        feedbackGrDb_ = 0.0f;
        lastGrDb_.store(0.0f, std::memory_order_relaxed);
    }

    void updateParameters(const CompressorParams& p) override
    {
        params_ = p;
        attackCoeff_  = timeToCoeff(p.attackMs);
        releaseCoeff_ = timeToCoeff(p.releaseMs);
        rmsCoeff_ = timeToCoeff(5.0f);
    }

    float processSidechain(float x) override
    {
        // dbx 160 feedforward: detector sees input directly
        // But also incorporates a feedback path for musical self-correction
        const float defaults = AlgorithmDefaults::forMode(CompressorMode::VCA).isFeedback ? 1.0f : 0.0f;
        const float detectSample = defaults > 0.5f
            ? x * juce::Decibels::decibelsToGain(feedbackGrDb_ * 0.5f)
            : x;

        // RMS detection (dbx Blackmer style)
        const float sq = detectSample * detectSample;
        rmsSquared_ = rmsCoeff_ * rmsSquared_ + (1.0f - rmsCoeff_) * sq;
        const float rmsDb = juce::Decibels::gainToDecibels(std::sqrt(rmsSquared_) + 1.0e-12f);

        // Peak detection for transient capture
        const float rectified = std::abs(detectSample);
        peakEnv_ = rectified > peakEnv_ ? rectified : timeToCoeff(0.1f) * peakEnv_;
        const float peakDb = juce::Decibels::gainToDecibels(peakEnv_ + 1.0e-12f);

        // Blend peak and RMS (70% RMS, 30% peak for dbx-style punch)
        const float inputDb = rmsDb * 0.7f + peakDb * 0.3f;
        const float targetGrDb = computeKneeGr(inputDb);

        float relCoeff = releaseCoeff_;
        float attCoeff = attackCoeff_;
        if (params_.autoRelease || params_.autoAttack)
        {
            autoSlowEnv_ = 0.9997f * autoSlowEnv_ + 0.0003f * std::abs(envelopeDb_);
            float adaptFactor = juce::jlimit(0.0f, 1.0f, autoSlowEnv_ / 8.0f);
            if (params_.autoRelease)
                relCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.releaseMs * 0.5f, params_.releaseMs * 2.5f));
            if (params_.autoAttack)
                attCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.attackMs * 0.3f, params_.attackMs * 1.2f));
        }

        const bool rising = targetGrDb < envelopeDb_;
        const float coeff = rising ? attCoeff : relCoeff;
        envelopeDb_ = coeff * envelopeDb_ + (1.0f - coeff) * targetGrDb;
        feedbackGrDb_ = envelopeDb_;

        lastGrDb_.store(envelopeDb_, std::memory_order_relaxed);
        return juce::Decibels::decibelsToGain(envelopeDb_);
    }

private:
    float computeKneeGr(float inputDb) const noexcept
    {
        const float knee = juce::jmax(0.0f, params_.kneeDb);
        const float overDb = inputDb - params_.thresholdDb;

        if (knee <= 0.0001f)
            return overDb > 0.0f ? -overDb * compressorSlope(params_.ratio) : 0.0f;

        const float lo = -knee * 0.5f, hi = knee * 0.5f;
        if (overDb <= lo) return 0.0f;
        if (overDb >= hi) return -(overDb - hi) * compressorSlope(params_.ratio) - kneeCompression(hi, knee);
        return -kneeCompression(overDb, knee);
    }

    float kneeCompression(float overDb, float knee) const noexcept
    {
        const float x = overDb + knee * 0.5f;
        return (compressorSlope(params_.ratio) * x * x) / (2.0f * knee);
    }

    float timeToCoeff(float ms) const noexcept
    {
        return std::exp(-1.0f / (0.001f * juce::jmax(0.1f, ms) * (float)sampleRate_));
    }

    CompressorParams params_;
    float envelopeDb_ = 0.0f;
    float rmsSquared_ = 0.0f;
    float peakEnv_ = 0.0f;
    float autoSlowEnv_ = 0.0f;
    float feedbackGrDb_ = 0.0f;
    float attackCoeff_ = 0.0f, releaseCoeff_ = 0.0f, rmsCoeff_ = 0.0f;
};

//==============================================================================
// 3. FET / Distressor Style -- ultra-fast, near-instant attack with an
//    aggressive knee and hard peak clamping above the ratio curve.
//==============================================================================
class FETAlgorithm final : public CompressorAlgorithm
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate_ = spec.sampleRate;
        reset();
    }

    void reset() override
    {
        envelopeDb_ = 0.0f;
        feedbackGrDb_ = 0.0f;
        programDepState_ = 0.0f;
        autoSlowEnv_ = 0.0f;
        lastGrDb_.store(0.0f, std::memory_order_relaxed);
    }

    void updateParameters(const CompressorParams& p) override
    {
        params_ = p;
        attackCoeff_      = timeToCoeff(juce::jmin(p.attackMs, 1.0f));
        releaseCoeff_     = timeToCoeff(p.releaseMs);
        fastReleaseCoeff_ = timeToCoeff(p.releaseMs * 0.25f);
    }

    float processSidechain(float x) override
    {
        // 1176 feedback topology: detector sees gain-reduced signal
        const float fbSample = x * juce::Decibels::decibelsToGain(feedbackGrDb_);
        const float inputDb = juce::Decibels::gainToDecibels(std::abs(fbSample) + 1.0e-12f);
        const float overDb  = inputDb - params_.thresholdDb;

        float targetGrDb = 0.0f;
        if (overDb > 0.0f)
        {
            targetGrDb = -overDb * compressorSlope(params_.ratio);
            if (overDb > peakClampThresholdDb_)
                targetGrDb -= (overDb - peakClampThresholdDb_) * 0.5f;
        }

        const bool rising = targetGrDb < envelopeDb_;

        // 1176 program-dependent release: fast after transients, slows down
        // during sustained heavy compression to reduce pumping
        programDepState_ = 0.9995f * programDepState_ + 0.0005f * (rising ? 1.0f : 0.0f);

        float relCoeff;
        float attCoeff = attackCoeff_;
        if (params_.autoRelease || params_.autoAttack)
        {
            autoSlowEnv_ = 0.9996f * autoSlowEnv_ + 0.0004f * std::abs(envelopeDb_);
            float adaptFactor = juce::jlimit(0.0f, 1.0f, autoSlowEnv_ / 6.0f);
            if (params_.autoRelease)
                relCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.releaseMs * 0.2f, params_.releaseMs * 2.0f));
            else
                relCoeff = juce::jmap(programDepState_, fastReleaseCoeff_, releaseCoeff_);
            if (params_.autoAttack)
                attCoeff = timeToCoeff(juce::jmap(adaptFactor,
                                                   juce::jmin(params_.attackMs, 1.0f) * 0.5f,
                                                   juce::jmin(params_.attackMs, 1.0f) * 1.5f));
        }
        else
        {
            relCoeff = juce::jmap(programDepState_, fastReleaseCoeff_, releaseCoeff_);
        }

        const float coeff = rising ? attCoeff : relCoeff;
        envelopeDb_ = coeff * envelopeDb_ + (1.0f - coeff) * targetGrDb;
        feedbackGrDb_ = envelopeDb_;

        lastGrDb_.store(envelopeDb_, std::memory_order_relaxed);
        return juce::Decibels::decibelsToGain(envelopeDb_);
    }

private:
    float timeToCoeff(float ms) const noexcept
    {
        return std::exp(-1.0f / (0.001f * juce::jmax(0.02f, ms) * (float)sampleRate_));
    }

    CompressorParams params_;
    float envelopeDb_ = 0.0f;
    float feedbackGrDb_ = 0.0f;
    float programDepState_ = 0.0f;
    float autoSlowEnv_ = 0.0f;
    float attackCoeff_ = 0.0f, releaseCoeff_ = 0.0f, fastReleaseCoeff_ = 0.0f;
    static constexpr float peakClampThresholdDb_ = 10.0f;
};

//==============================================================================
// 4. Focusrite Red Style -- musical feed-forward VCA with a transient
//    preservation path (fast-tracking a short lookahead-free peak term to
//    protect the initial attack from being clamped).
//==============================================================================
class RedStyleAlgorithm final : public CompressorAlgorithm
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate_ = spec.sampleRate;
        reset();
    }

    void reset() override
    {
        envelopeDb_ = 0.0f;
        transientTracker_ = 0.0f;
        lastGrDb_.store(0.0f, std::memory_order_relaxed);
    }

    void updateParameters(const CompressorParams& p) override
    {
        params_ = p;
        attackCoeff_  = timeToCoeff(p.attackMs);
        releaseCoeff_ = timeToCoeff(p.releaseMs);
        transientCoeff_ = timeToCoeff(1.5f);
    }

    float processSidechain(float x) override
    {
        // Focusrite Red 3: feedforward, Class-A peak detection, soft knee
        const float rectified = std::abs(x);
        transientTracker_ = transientCoeff_ * transientTracker_ + (1.0f - transientCoeff_) * rectified;
        const float punchRelief = juce::jlimit(0.0f, 3.0f, (rectified - transientTracker_) * 6.0f);

        const float inputDb = juce::Decibels::gainToDecibels(rectified + 1.0e-12f);
        const float overDb  = inputDb - params_.thresholdDb;
        const float targetGrDb = overDb > 0.0f
            ? -overDb * compressorSlope(params_.ratio) + punchRelief
            : 0.0f;

        const bool rising = targetGrDb < envelopeDb_;

        float relCoeff = releaseCoeff_;
        float attCoeff = attackCoeff_;
        if (params_.autoRelease || params_.autoAttack)
        {
            autoSlowEnv_ = 0.9997f * autoSlowEnv_ + 0.0003f * std::abs(envelopeDb_);
            float adaptFactor = juce::jlimit(0.0f, 1.0f, autoSlowEnv_ / 6.0f);
            if (params_.autoRelease)
                relCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.releaseMs * 0.4f, params_.releaseMs * 2.0f));
            if (params_.autoAttack)
                attCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.attackMs * 0.3f, params_.attackMs * 1.2f));
        }

        const float coeff = rising ? attCoeff : relCoeff;
        envelopeDb_ = coeff * envelopeDb_ + (1.0f - coeff) * juce::jmin(0.0f, targetGrDb);

        lastGrDb_.store(envelopeDb_, std::memory_order_relaxed);
        return juce::Decibels::decibelsToGain(envelopeDb_);
    }

private:
    float timeToCoeff(float ms) const noexcept
    {
        return std::exp(-1.0f / (0.001f * juce::jmax(0.1f, ms) * (float)sampleRate_));
    }

    CompressorParams params_;
    float envelopeDb_ = 0.0f;
    float transientTracker_ = 0.0f;
    float autoSlowEnv_ = 0.0f;
    float attackCoeff_ = 0.0f, releaseCoeff_ = 0.0f, transientCoeff_ = 0.0f;
};

//==============================================================================
// 5. Custom Hybrid -- clean digital feed-forward detector whose output is
//    blended with a feedback path (detector fed from the *output* signal),
//    giving a dynamic, program-dependent character controllable at runtime.
//==============================================================================
class HybridAlgorithm final : public CompressorAlgorithm
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate_ = spec.sampleRate;
        reset();
    }

    void reset() override
    {
        feedforwardEnvDb_ = 0.0f;
        feedbackEnvDb_ = 0.0f;
        autoSlowEnv_ = 0.0f;
        lastGrDb_.store(0.0f, std::memory_order_relaxed);
    }

    void updateParameters(const CompressorParams& p) override
    {
        params_ = p;
        attackCoeff_  = timeToCoeff(p.attackMs);
        releaseCoeff_ = timeToCoeff(p.releaseMs);
    }

    void setFeedbackAmount(float amount01) noexcept { feedbackAmount_.store(amount01, std::memory_order_relaxed); }

    float processSidechain(float x) override
    {
        const float inputDb = juce::Decibels::gainToDecibels(std::abs(x) + 1.0e-12f);
        const float overDb  = inputDb - params_.thresholdDb;
        const float targetGrDb = overDb > 0.0f ? -overDb * compressorSlope(params_.ratio) : 0.0f;

        float relCoeff = releaseCoeff_;
        float attCoeff = attackCoeff_;
        if (params_.autoRelease || params_.autoAttack)
        {
            autoSlowEnv_ = 0.9997f * autoSlowEnv_ + 0.0003f * std::abs(feedforwardEnvDb_);
            float adaptFactor = juce::jlimit(0.0f, 1.0f, autoSlowEnv_ / 6.0f);
            if (params_.autoRelease)
                relCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.releaseMs * 0.3f, params_.releaseMs * 2.5f));
            if (params_.autoAttack)
                attCoeff = timeToCoeff(juce::jmap(adaptFactor, params_.attackMs * 0.3f, params_.attackMs * 1.2f));
        }

        const bool rising = targetGrDb < feedforwardEnvDb_;
        const float coeff = rising ? attCoeff : relCoeff;
        feedforwardEnvDb_ = coeff * feedforwardEnvDb_ + (1.0f - coeff) * targetGrDb;

        const float amount = feedbackAmount_.load(std::memory_order_relaxed);
        const float blendedGrDb = feedforwardEnvDb_ * (1.0f - amount) + feedbackEnvDb_ * amount;

        lastGrDb_.store(blendedGrDb, std::memory_order_relaxed);
        return juce::Decibels::decibelsToGain(blendedGrDb);
    }

    // Called by the engine after the gain has been applied, feeding the
    // already-compressed output sample back into a slower envelope used to
    // stabilise the loop (real feedback-topology emulation without recursion
    // inside a single sample pass).
    void updateFeedbackEnvelope(float outputSample) noexcept
    {
        const float outDb = juce::Decibels::gainToDecibels(std::abs(outputSample) + 1.0e-12f);
        const float overDb = outDb - params_.thresholdDb;
        const float targetDb = overDb > 0.0f ? -overDb * compressorSlope(params_.ratio) : 0.0f;
        feedbackEnvDb_ = releaseCoeff_ * feedbackEnvDb_ + (1.0f - releaseCoeff_) * targetDb;
    }

private:
    float timeToCoeff(float ms) const noexcept
    {
        return std::exp(-1.0f / (0.001f * juce::jmax(0.1f, ms) * (float)sampleRate_));
    }

    CompressorParams params_;
    float feedforwardEnvDb_ = 0.0f;
    float feedbackEnvDb_ = 0.0f;
    float autoSlowEnv_ = 0.0f;
    float attackCoeff_ = 0.0f, releaseCoeff_ = 0.0f;
    std::atomic<float> feedbackAmount_{0.25f};
};

// Reduced reference-derived candidates, not complete circuit solvers.
// Constants and omitted stages are recorded in dsp/CIRCUIT_INTEGRATION.md.
class OptoLevelerAlgorithm final : public CompressorAlgorithm
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override {
        sampleRate_=spec.sampleRate;
        lightCoefficient_=coefficient(0.001);
        attack_={coefficient(0.11654),coefficient(0.11654),coefficient(1.0)};
        release_={coefficient(0.01333),coefficient(0.09185),coefficient(0.69596)};
        reset();
    }
    void reset() override { cell_={};light_=0;gain_=1;lastGrDb_.store(0); }
    void updateParameters(const CompressorParams& p) override {
        drive_=juce::Decibels::decibelsToGain(-p.thresholdDb);
        // Shared ratio knob selects the source Compress / Limit configurations.
        limit_=p.ratio>=10.0f;
    }
    float processSidechain(float x) override {
        // Source junction J: before R7, after R6, coupled to cell loading.
        const float r7=limit_ ? 2700.0f : 0.0f;
        const float junction=gain_+(1-gain_)*r7/(68000.0f+r7);
        // Normalized panel drive is provisional until tube driver calibration.
        const float raw=std::pow(juce::jlimit(0.0f,10.0f,std::abs(x)*drive_*junction),5.1f);
        light_=lightCoefficient_*light_+(1-lightCoefficient_)*raw;
        for(size_t i=0;i<3;++i) {
            const float c=light_>cell_[i]?attack_[i]:release_[i];
            cell_[i]=c*cell_[i]+(1-c)*light_;
        }
        const float state=0.65467f*cell_[0]+0.2131f*cell_[1]+0.13223f*cell_[2];
        const float conductance=2.0e-7f+0.001f*std::pow(juce::jmax(0.0f,state),0.9f);
        // Dark-normalized shunt loading; amplifier/transformer loading still omitted.
        gain_=(1+(68000+r7)*2.0e-7f)/(1+(68000+r7)*conductance);
        lastGrDb_.store(juce::Decibels::gainToDecibels(gain_,-120.0f),std::memory_order_relaxed);
        return gain_;
    }
private:
    float coefficient(double seconds) const {return float(std::exp(-1/(sampleRate_*seconds)));}
    std::array<float,3> cell_{},attack_{},release_{};
    float light_=0,gain_=1,drive_=1,lightCoefficient_=0;
    bool limit_=false;
};

class RMSVCADynamicsAlgorithm final : public CompressorAlgorithm
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) override {
        sampleRate_=spec.sampleRate;
        // Constant-current log-cap release ~126 dB/s. Transforming
        // C*dV/dt=Icharge-Iin^2/IX*exp(V/Vt) with P=exp(-V/Vt)
        // gives a single mean-square integrator. 10/log(10)/tau=126.
        const double tau=10.0/std::log(10.0)/126.0;
        coefficient_=float(std::exp(-1.0/(sampleRate_*tau)));
        reset();
    }
    void reset() override {power_=1e-12f;lastGrDb_.store(0);}
    void updateParameters(const CompressorParams& p) override {params_=p;}
    float processSidechain(float x) override {
        power_=coefficient_*power_+(1-coefficient_)*x*x;
        power_=juce::jmax(1e-12f,power_);
        const float level=10*std::log10(power_);
        const float over=level-params_.thresholdDb;
        const float slope=1-1/juce::jlimit(1.0f,100.0f,params_.ratio);
        // Paired -6mV/dB detector, 1/2 divider, +2.98mV/dB VCA.
        const float control=over>=0?over:0.01f*over;
        const float gainDb=juce::jlimit(-100.0f,6.0f,-slope*(3.0f/2.98f)*control);
        lastGrDb_.store(gainDb,std::memory_order_relaxed);
        return juce::Decibels::decibelsToGain(gainDb);
    }
private:
    CompressorParams params_;
    float power_=1e-12f,coefficient_=0;
};

// Provisional reduced circuit engines. These are integration candidates, not
// validated equivalents of the source netlists. See dsp/CIRCUIT_INTEGRATION.md.
class CircuitAlgorithm final : public CompressorAlgorithm
{
public:
    explicit CircuitAlgorithm(CompressorMode kind) : kind_(kind) {}
    void prepare(const juce::dsp::ProcessSpec& spec) override { sampleRate_ = spec.sampleRate; reset(); }
    void reset() override { envelope_ = memory_ = 0.0f; gain_ = 1.0f; lastGrDb_.store(0.0f); }
    void updateParameters(const CompressorParams& p) override
    {
        params_ = p;
        attack_ = coefficient(p.autoAttack ? defaultAttack() : p.attackMs);
        release_ = coefficient(p.releaseMs);
        // Bridge C36/R81 recovery memory: 47uF * 120kOhm.
        memoryCoeff_ = coefficient(kind_ == CompressorMode::Bridge ? 5640.0f : 1200.0f);
        drive_ = juce::Decibels::decibelsToGain(-p.thresholdDb);
    }
    float processSidechain(float x) override
    {
        const float ratio = juce::jlimit(1.0f, 100.0f, params_.ratio);
        float detected = std::abs(x) * drive_;
        if (kind_ == CompressorMode::FETCircuit)
        {
            // Source deck: preamp feedback rectifier, ratio-dependent GR gain,
            // -3.5V quiescent bias, 1.6V/V envelope-to-gate drive.
            const float grGain = ratio >= 99 ? 10.15f : ratio >= 20 ? 9.08f : ratio >= 12 ? 5.23f
                               : ratio >= 8 ? 3.23f : ratio >= 4 ? 2.0f : 1.0f;
            detected *= gain_ * grGain;
        }
        else
        {
            // Bus model sidechain/main control tracking = 8.89/9.53.
            detected *= kind_ == CompressorMode::BusVCA ? std::pow(gain_, 8.89f / 9.53f) : gain_;
        }
        memory_ = memoryCoeff_ * memory_ + (1.0f - memoryCoeff_) * envelope_;
        float target = detected;
        if (params_.autoRelease && target < envelope_) target = juce::jmax(target, memory_);
        const float c = target > envelope_ ? attack_ : release_;
        envelope_ = c * envelope_ + (1.0f - c) * target;
        if (kind_ == CompressorMode::FETCircuit)
        {
            // Small-signal BF245B channel conductance, R3 27k and R4 180R.
            // Finite-signal JFET junctions and transformer networks remain to audit.
            const float gate = juce::jlimit(-3.5f, 0.0f, -3.5f + 1.6f * envelope_);
            const float conductance = 2.0f * 0.0012f * juce::jmax(0.0f, gate + 2.5f);
            gain_ = 1.0f / (1.0f + 27000.0f * conductance / (1.0f + 180.0f * conductance));
        }
        else
        {
            // Provisional feedback control law; replace with the audited
            // rectifier/clamp network and calibrated CV conversion.
            const float over = juce::jmax(0.0f, juce::Decibels::gainToDecibels(envelope_, -120.0f));
            const float tracking = kind_ == CompressorMode::BusVCA ? 8.89f / 9.53f : 1.0f;
            const float slope = (ratio - 1.0f) / tracking;
            gain_ = juce::Decibels::decibelsToGain(-juce::jmin(80.0f, over * slope));
        }
        lastGrDb_.store(juce::Decibels::gainToDecibels(gain_, -120.0f), std::memory_order_relaxed);
        return gain_;
    }
private:
    float coefficient(float ms) const { return std::exp(-1.0f / ((float)sampleRate_ * 0.001f * juce::jmax(0.02f, ms))); }
    float defaultAttack() const { return kind_ == CompressorMode::FETCircuit ? 0.2f : kind_ == CompressorMode::Bridge ? 2.0f : 3.0f; }
    CompressorMode kind_;
    CompressorParams params_;
    float envelope_ = 0, memory_ = 0, gain_ = 1, drive_ = 1;
    float attack_ = 0, release_ = 0, memoryCoeff_ = 0;
};

//==============================================================================
// HarmonicsEngine: post-compression waveshaping with independent 2nd/3rd/
// 4th/5th harmonic mix controls (each 0..1, mapped from a 0-100% slider),
// anti-aliased using first-order residual ADAA + smooth Hermite rail limiting
// matching the HybridEQ ADAA architecture.
//==============================================================================
class HarmonicsEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate_ = spec.sampleRate;
        (void) hcdsp::adaa::railTable(); // touch static Simpson/Hermite table during prepare
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : shaperAdaa_) s.reset();
        for (auto& r : railAdaa_)   r.reset();
    }

    // Per-channel ADAA harmonic waveshaper + smooth rail limiter.
    inline float processSample(float x, float h2, float h3, float h4, float h5, int channel = 0) noexcept
    {
        const size_t ch = static_cast<size_t>(juce::jlimit(0, 1, channel));
        const double xd = static_cast<double>(x);

        const double c2 = 0.50 * static_cast<double>(h2);
        const double c3 = 0.50 * static_cast<double>(h3);
        const double c4 = 0.25 * static_cast<double>(h4);
        const double c5 = 0.20 * static_cast<double>(h5);
        const bool active = (c2 + c3 + c4 + c5) > 1.0e-7;

        // C1-continuous zero-at-origin harmonic shaper p(x) = x + g(x) and exact antiderivative P(x).
        // Within [-1, 1], g(x) = c2*x|x| + c3*(x^3 - x) + c4*x^3|x| + c5*(x^5 - x).
        // At x = +-1, t3(+-1) = 0 and t5(+-1) = 0, so g'(+-1) smoothly transitions to constant +-(c2 + c4).
        const double slope = 1.0 - c3 - c5;
        auto g0Antideriv = [&](double u) noexcept -> double
        {
            const double au = std::abs(u);
            const double u2 = u * u;
            const double u4 = u2 * u2;
            return (c2 / 3.0) * (au * u2)
                 + c3 * (0.25 * u4 - 0.5 * u2)
                 + (c4 / 5.0) * (au * u4)
                 + c5 * ((u4 * u2) / 6.0 - 0.5 * u2);
        };

        double gVal = 0.0;
        double gInt = 0.0;
        if (active)
        {
            if (xd > 1.0)
            {
                gVal = c2 + c4;
                gInt = g0Antideriv(1.0) + (c2 + c4) * (xd - 1.0);
            }
            else if (xd < -1.0)
            {
                gVal = -(c2 + c4);
                gInt = g0Antideriv(-1.0) - (c2 + c4) * (xd + 1.0);
            }
            else
            {
                const double ax = std::abs(xd);
                const double x2 = xd * xd;
                const double x3 = x2 * xd;
                gVal = c2 * (xd * ax) + c3 * (x3 - xd) + c4 * (x3 * ax) + c5 * (x3 * x2 - xd);
                gInt = g0Antideriv(xd);
            }
        }

        const double pVal = xd + gVal;
        const double pInt = 0.5 * xd * xd + gInt;
        const double shaped = shaperAdaa_[ch].step(xd, pVal, pInt, slope);

        // Stage 2: HybridEQ smooth rail limiter f(v) = v / (1 + |v|^8)^(1/8) with ADAA.
        // Keep railAdaa_ primed when harmonics are off so automation transitions are click-free,
        // while returning bit-exact unity x when all harmonic sliders are at 0%.
        const double limited = railAdaa_[ch].step(shaped,
                                                 hcdsp::adaa::railLimit(shaped, 1.0),
                                                 hcdsp::adaa::railLimitF(shaped, 1.0),
                                                 1.0);
        return active ? static_cast<float>(limited) : x;
    }

private:
    double sampleRate_ = 44100.0;
    std::array<hcdsp::adaa::ZeroDelayResidual, 2> shaperAdaa_{};
    std::array<hcdsp::adaa::ZeroDelayResidual, 2> railAdaa_{};
};

//==============================================================================
// Sidechain conditioning: internal/external 2nd-order Butterworth HPF (20-500 Hz)
// + listen/audition mode. Zero-allocation per-sample biquad + block wrapper.
//==============================================================================
class SidechainProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate_ = juce::jmax(8000.0, spec.sampleRate);
        lastHz_ = -1.0f;
        lastSr_ = -1.0;
        reset();
        setFrequency(targetHz_);
    }

    void reset() noexcept
    {
        z1_ = { 0.0f, 0.0f };
        z2_ = { 0.0f, 0.0f };
    }

    void setSampleRate(double sr) noexcept
    {
        const double clamped = juce::jmax(8000.0, sr);
        if (std::abs(clamped - sampleRate_) > 0.5)
        {
            sampleRate_ = clamped;
            updateCoeffs();
        }
    }

    void setFrequency(float hz) noexcept
    {
        const float clamped = juce::jlimit(20.0f, 500.0f, hz);
        if (std::abs(clamped - targetHz_) > 0.01f || std::abs(sampleRate_ - lastSr_) > 0.5)
        {
            targetHz_ = clamped;
            updateCoeffs();
        }
    }

    inline float processSample(int channel, float x) noexcept
    {
        const size_t ch = static_cast<size_t>(juce::jlimit(0, 1, channel));
        const float y = b0_ * x + z1_[ch];
        z1_[ch] = b1_ * x - a1_ * y + z2_[ch];
        z2_[ch] = b2_ * x - a2_ * y;
        return y;
    }

    void process(juce::dsp::AudioBlock<float>& block) noexcept
    {
        const int numCh = juce::jmin(2, static_cast<int>(block.getNumChannels()));
        const int numSamp = static_cast<int>(block.getNumSamples());
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = block.getChannelPointer(static_cast<size_t>(ch));
            for (int i = 0; i < numSamp; ++i)
                d[i] = processSample(ch, d[i]);
        }
    }

private:
    void updateCoeffs() noexcept
    {
        lastHz_ = targetHz_;
        lastSr_ = sampleRate_;
        const double w0 = 2.0 * juce::MathConstants<double>::pi * static_cast<double>(targetHz_) / sampleRate_;
        const double cosw = std::cos(w0);
        const double alpha = std::sin(w0) / std::sqrt(2.0); // Butterworth Q = 1/sqrt(2)
        const double a0 = 1.0 + alpha;
        b0_ = static_cast<float>(((1.0 + cosw) * 0.5) / a0);
        b1_ = static_cast<float>(-(1.0 + cosw) / a0);
        b2_ = b0_;
        a1_ = static_cast<float>((-2.0 * cosw) / a0);
        a2_ = static_cast<float>((1.0 - alpha) / a0);
    }

    double sampleRate_ = 48000.0;
    double lastSr_ = -1.0;
    float targetHz_ = 150.0f;
    float lastHz_ = -1.0f;
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f, a1_ = 0.0f, a2_ = 0.0f;
    std::array<float, 2> z1_{ 0.0f, 0.0f }, z2_{ 0.0f, 0.0f };
};

//==============================================================================
// CompressorEngine: owns the pre-allocated algorithm pool, the harmonics
// engine and the sidechain conditioning; exposes the real-time-safe
// atomics the UI thread writes to and the processBlock() hot path reads.
//==============================================================================
class CompressorEngine
{
public:
    CompressorEngine()
    {
        algorithmPool_[(size_t)CompressorMode::Opto]     = &opto_;
        algorithmPool_[(size_t)CompressorMode::VCA]      = &vca_;
        algorithmPool_[(size_t)CompressorMode::FET]      = &fet_;
        algorithmPool_[(size_t)CompressorMode::RedStyle] = &red_;
        algorithmPool_[(size_t)CompressorMode::Hybrid]   = &hybrid_;
        algorithmPool_[(size_t)CompressorMode::Bridge] = &bridge_;
        algorithmPool_[(size_t)CompressorMode::BusVCA] = &busVca_;
        algorithmPool_[(size_t)CompressorMode::FETCircuit] = &fetCircuit_;
        algorithmPool_[(size_t)CompressorMode::OptoLeveler] = &optoLeveler_;
        algorithmPool_[(size_t)CompressorMode::RMSVCADynamics] = &rmsVcaDynamics_;
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        spec_ = spec;
        for (auto* alg : algorithmPool_)
            alg->prepare(spec);
        harmonics_.prepare(spec);
        sidechain_.prepare(spec);
    }

    void reset()
    {
        for (auto* alg : algorithmPool_)
            alg->reset();
        harmonics_.reset();
        sidechain_.reset();
    }

    void ensureSampleRate(double effectiveSampleRate)
    {
        if (std::abs(effectiveSampleRate - spec_.sampleRate) > 0.5)
        {
            spec_.sampleRate = effectiveSampleRate;
            for (auto* alg : algorithmPool_)
                alg->prepare(spec_);
            harmonics_.prepare(spec_);
            sidechain_.setSampleRate(effectiveSampleRate);
        }
    }

    // Called once per block from processBlock() with values pulled from the
    // APVTS -- keeps parameter smoothing / lookups out of the sample loop.
    void updateBlockParameters(CompressorMode mode, const CompressorParams& params,
                                float scHpHz, bool scEnabled, bool scListen,
                                float h2, float h3, float h4, float h5)
    {
        activeModeIndex_.store((int)mode, std::memory_order_relaxed);
        algorithmPool_[(size_t)mode]->updateParameters(params);
        sidechain_.setFrequency(scHpHz);
        scEnabled_ = scEnabled;
        scListen_ = scListen;
        h2Mix_.store(h2, std::memory_order_relaxed);
        h3Mix_.store(h3, std::memory_order_relaxed);
        h4Mix_.store(h4, std::memory_order_relaxed);
        h5Mix_.store(h5, std::memory_order_relaxed);
    }

    CompressorAlgorithm* getActiveAlgorithm() const noexcept
    {
        return algorithmPool_[(size_t)activeModeIndex_.load(std::memory_order_relaxed)];
    }

    float getH2() const noexcept { return h2Mix_.load(std::memory_order_relaxed); }
    float getH3() const noexcept { return h3Mix_.load(std::memory_order_relaxed); }
    float getH4() const noexcept { return h4Mix_.load(std::memory_order_relaxed); }
    float getH5() const noexcept { return h5Mix_.load(std::memory_order_relaxed); }

    HarmonicsEngine&    getHarmonics()  noexcept { return harmonics_; }
    SidechainProcessor&  getSidechain() noexcept { return sidechain_; }
    bool isSidechainEnabled() const noexcept { return scEnabled_; }
    bool isSidechainListen() const noexcept { return scListen_; }

private:
    OptoAlgorithm      opto_;
    VCAAlgorithm       vca_;
    FETAlgorithm       fet_;
    RedStyleAlgorithm  red_;
    HybridAlgorithm    hybrid_;
    OptoLevelerAlgorithm optoLeveler_;
    RMSVCADynamicsAlgorithm rmsVcaDynamics_;
    CircuitAlgorithm bridge_{CompressorMode::Bridge};
    CircuitAlgorithm busVca_{CompressorMode::BusVCA};
    CircuitAlgorithm fetCircuit_{CompressorMode::FETCircuit};

    std::array<CompressorAlgorithm*, (size_t)CompressorMode::Count> algorithmPool_{};

    HarmonicsEngine    harmonics_;
    SidechainProcessor sidechain_;

    std::atomic<int> activeModeIndex_{0};
    std::atomic<float> h2Mix_{0.0f}, h3Mix_{0.0f}, h4Mix_{0.0f}, h5Mix_{0.0f};
    bool scEnabled_ = false;
    bool scListen_ = false;

    juce::dsp::ProcessSpec spec_;
};
