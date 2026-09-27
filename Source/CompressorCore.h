#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// Parameter IDs (used for both APVTS registration and atomic lookups)
//==============================================================================
namespace ParamIDs
{
    static constexpr auto algorithmMode   = "algorithmMode";   // choice: 0..4, see CompressorMode
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
        const float overDb  = inputDb - params_.thresholdDb;
        const float targetGrDb = overDb > 0.0f
            ? -overDb * compressorSlope(params_.ratio)
            : 0.0f;

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

//==============================================================================
// HarmonicsEngine: post-compression waveshaping with independent 2nd/3rd/
// 4th/5th harmonic mix controls (each 0..1, mapped from a 0-100% slider).
//==============================================================================
class HarmonicsEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate_ = spec.sampleRate;
        juce::ignoreUnused(sampleRate_);
    }

    void reset() {}

    // h2/h4 = asymmetric curves (even harmonics), h3/h5 = symmetric curves
    // (odd harmonics). All coefficients pre-normalised so 100% mix stays
    // within +/-1 for a full-scale input.
    inline float processSample(float x, float h2, float h3, float h4, float h5) const noexcept
    {
        float shaped = x;

        // 2nd harmonic: asymmetric soft clip -- adds even-order content by
        // biasing the curve differently for positive/negative half-cycles.
        shaped += h2 * 0.5f * (x * x) * (x >= 0.0f ? 1.0f : -1.0f);

        // 3rd harmonic: symmetric soft clip (classic tanh-style odd curve).
        shaped += h3 * (x * x * x - x) * 0.5f;

        // 4th harmonic: asymmetric higher-order curve.
        shaped += h4 * 0.25f * (x * x * x * x) * (x >= 0.0f ? 1.0f : -1.0f);

        // 5th harmonic: symmetric higher-order curve.
        shaped += h5 * 0.2f * (x * x * x * x * x - x);

        return juce::jlimit(-1.0f, 1.0f, shaped);
    }

private:
    double sampleRate_ = 44100.0;
};

//==============================================================================
// Sidechain conditioning: internal HPF (20-500 Hz) + listen/audition mode.
//==============================================================================
class SidechainProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        spec_ = spec;
        filter_.prepare(spec);
        setFrequency(150.0f);
    }

    void reset() { filter_.reset(); }

    void setFrequency(float hz)
    {
        *filter_.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
            spec_.sampleRate, juce::jlimit(20.0f, 500.0f, hz));
    }

    void process(juce::dsp::AudioBlock<float>& block)
    {
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        filter_.process(ctx);
    }

private:
    juce::dsp::ProcessSpec spec_;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                    juce::dsp::IIR::Coefficients<float>> filter_;
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

    std::array<CompressorAlgorithm*, (size_t)CompressorMode::Count> algorithmPool_{};

    HarmonicsEngine    harmonics_;
    SidechainProcessor sidechain_;

    std::atomic<int> activeModeIndex_{0};
    std::atomic<float> h2Mix_{0.0f}, h3Mix_{0.0f}, h4Mix_{0.0f}, h5Mix_{0.0f};
    bool scEnabled_ = false;
    bool scListen_ = false;

    juce::dsp::ProcessSpec spec_;
};
