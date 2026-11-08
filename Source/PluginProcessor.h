#pragma once

#include <JuceHeader.h>
#include "CompressorCore.h"
#include "DSP/Resampler.h"
#include "DSP/TransientDesigner.h"

//==============================================================================
/** AAX client extensions: names the control-surface page table shipped in Contents/Resources.
    Without it the AAX validator reports "Failed to load page tables library". */
struct HarmonicCompressorAAXExtensions : juce::AAXClientExtensions
{
    juce::String getPageFileName() const override { return "HarmonicCompressor.xml"; }
};

//==============================================================================
/**
    HarmonicCompressor audio processor.

    Wires the existing CompressorCore.h DSP engine (CompressorEngine) into a
    JUCE AudioProcessor with a main stereo in/out bus plus an optional stereo
    sidechain input bus. All parameter access in the audio callback goes
    through cached raw atomic pointers; the sample loop is allocation-free and
    lock-free.
*/
class HarmonicCompressorAudioProcessor : public juce::AudioProcessor,
                                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    HarmonicCompressorAudioProcessor();
    ~HarmonicCompressorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    juce::AAXClientExtensions& getAAXClientExtensions() override { return aaxExtensions; }
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Single audio producer / single editor consumer, bounded telemetry only.
    struct HistoryFrame { float input, output, reduction, seconds; };
    juce::AbstractFifo historyFifo { 2048 };
    std::array<HistoryFrame, 2048> historyFrames {};
    std::atomic<unsigned> historyDrops { 0 };

    // UI-facing real-time-safe state: written once per block from the audio
    // thread, read from the message thread by the editor/display.
    std::atomic<float> lastGrDbForUI { 0.0f };
    std::atomic<float> lastInputPeakDb { -60.0f };
    std::atomic<float> lastOutputPeakDb { -60.0f };

    // VU-style (analog RMS ballistics) meter readouts, smoothed on the audio
    // thread; the UI just reads these atomics. Separate per-channel values
    // drive the meter's independent left/right needles; the "peak" atomics
    // use fast-attack/slow-release ballistics for the peak LEDs/readout.
    std::atomic<float> vuInputDbL  { -60.0f }, vuInputDbR  { -60.0f };
    std::atomic<float> vuOutputDbL { -60.0f }, vuOutputDbR { -60.0f };
    std::atomic<float> peakInputDbL  { -60.0f }, peakInputDbR  { -60.0f };
    std::atomic<float> peakOutputDbL { -60.0f }, peakOutputDbR { -60.0f };
    std::atomic<float> vuGrDb     { 0.0f };
    std::atomic<float> lastTransientGainDb { 0.0f };

private:
    //==============================================================================
    HarmonicCompressorAAXExtensions aaxExtensions;
    CompressorEngine engine;
    hcdsp::MultirateEngine multirate;
    hcdsp::TransientDesigner transientDesigner;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGainSmoothed;
    void updateReportedLatency();

    // --- Per-algorithm-mode parameter memory ---
    // Every parameter except algorithmMode itself is remembered per mode, so
    // switching between Opto/VCA/FET/Red/Hybrid and back restores whatever
    // values were last used with that mode instead of resetting them.
    struct ModeSnapshot
    {
        float thresholdDb, ratio, attackMs, releaseMs, kneeDb, makeupGainDb, mix;
        float harmonic2, harmonic3, harmonic4, harmonic5;
        float scHPFreq;
        bool  scEnabled, scListen, autoRelease, autoAttack;
    };

    std::array<ModeSnapshot, (size_t) CompressorMode::Count> modeSnapshots;
    int currentModeIndex = 0;
    std::atomic<bool> restoringState { false };
    // Bumped on every state restore. A mode switch queued before a restore must not apply its
    // remembered snapshot afterwards, or it would overwrite the values the host just restored.
    std::atomic<uint32_t> stateRestoreGeneration { 0 };

    // --- VU meter ballistics state (audio thread only) ---
    // Two cascaded one-pole smoothers approximate the ~300 ms, slightly
    // S-curved rise/fall of an analog VU/RMS meter movement.
    struct VuBallistics
    {
        float env1, env2;
        void reset (float value) noexcept { env1 = env2 = value; }
        float process (float targetDb, float coeff) noexcept
        {
            env1 += (targetDb - env1) * coeff;
            env2 += (env1 - env2) * coeff;
            return env2;
        }
    };

    VuBallistics vuInputBallisticsL  { -60.0f, -60.0f }, vuInputBallisticsR  { -60.0f, -60.0f };
    VuBallistics vuOutputBallisticsL { -60.0f, -60.0f }, vuOutputBallisticsR { -60.0f, -60.0f };
    VuBallistics vuGrBallistics      { 0.0f, 0.0f };

    // Peak ballistics: instant attack, smooth exponential release -- the
    // classic PPM/peak-LED response.
    struct PeakBallistics
    {
        float held = -60.0f;
        float process (float instantDb, float releaseCoeff) noexcept
        {
            held = (instantDb > held) ? instantDb : (releaseCoeff * held + (1.0f - releaseCoeff) * instantDb);
            return held;
        }
    };

    PeakBallistics peakInputL, peakInputR, peakOutputL, peakOutputR;
    double hostSampleRate = 44100.0;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void saveSnapshot (int modeIndex);
    void loadSnapshot (int modeIndex);

    // Cached raw parameter pointers (RT-safe atomic loads, no string lookups
    // inside the audio callback).
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* kneeParam = nullptr;
    std::atomic<float>* makeupGainParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* harmonic2Param = nullptr;
    std::atomic<float>* harmonic3Param = nullptr;
    std::atomic<float>* harmonic4Param = nullptr;
    std::atomic<float>* harmonic5Param = nullptr;
    std::atomic<float>* scEnabledParam = nullptr;
    std::atomic<float>* scListenParam = nullptr;
    std::atomic<float>* scHPFreqParam = nullptr;
    std::atomic<float>* autoReleaseParam = nullptr;
    std::atomic<float>* autoAttackParam = nullptr;
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* oversampleModeParam = nullptr;
    std::atomic<float>* oversampleFilterParam = nullptr;
    std::atomic<float>* transientEnabledParam = nullptr;
    std::atomic<float>* transientAttackParam = nullptr;
    std::atomic<float>* transientSustainParam = nullptr;
    std::atomic<float>* transientSpeedParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicCompressorAudioProcessor)
};
