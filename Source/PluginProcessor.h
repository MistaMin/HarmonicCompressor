#pragma once

#include <JuceHeader.h>
#include "CompressorCore.h"
#include "DSP/Resampler.h"
#include "DSP/TransientDesigner.h"
#include "DSP/LoudnessMatch.h"

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
                                          private juce::AudioProcessorValueTreeState::Listener,
                                          private juce::AsyncUpdater
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

    // Message thread, for the editor's model selector: remembers the current model's settings and recalls
    // the new one's. A host or automation change of the model parameter only changes the model, so renders
    // never depend on when the message thread runs.
    void selectMode (int newIndex);

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
    // Gain Auto Gain is applying (smoothed, dB); 0 when Auto Gain is off and settled.
    std::atomic<float> autoGainAppliedDb { 0.0f };

private:
    //==============================================================================
    HarmonicCompressorAAXExtensions aaxExtensions;
    CompressorEngine engine;
    hcdsp::MultirateEngine multirate;
    hcdsp::TransientDesigner transientDesigner;
    juce::AudioBuffer<float> dryBuffer;
    // Untouched plugin input (before Input Gain), aligned with the wet path through the matched resampler
    // chain: Delta's dry signal while Auto Gain is on.
    juce::AudioBuffer<float> referenceBuffer;
    // Unfiltered copy of the same input: Auto Gain's loudness reference. It is what bypass sounds like,
    // so the match is to bypass, not to the band-limited signal the resampler filters produce.
    juce::AudioBuffer<float> loudnessInputBuffer;
    hcdsp::LoudnessMatcher loudnessMatcher;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> autoGainSmoothed;
    juce::SmoothedValue<float> deltaSmoothed;          // 0 = normal output, 1 = output minus dry
    juce::SmoothedValue<float> deltaReferenceSmoothed; // 0 = dry after Input Gain, 1 = untouched input
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGainSmoothed;
    void updateReportedLatency();

    // --- Per-algorithm-mode parameter memory ---
    // These parameters are remembered per mode and saved with the session, so switching between models and
    // back restores whatever values were last used with that model instead of resetting them.
    static constexpr std::array<const char*, 16> snapshotIds {
        ParamIDs::threshold, ParamIDs::ratio, ParamIDs::attack, ParamIDs::release, ParamIDs::knee,
        ParamIDs::makeupGain, ParamIDs::mix, ParamIDs::harmonic2, ParamIDs::harmonic3, ParamIDs::harmonic4,
        ParamIDs::harmonic5, ParamIDs::scHPFreq, ParamIDs::scEnabled, ParamIDs::scListen,
        ParamIDs::autoRelease, ParamIDs::autoAttack };
    static constexpr const char* modeSnapshotsType = "ModeSnapshots";
    std::array<std::array<float, snapshotIds.size()>, (size_t) CompressorMode::Count> modeSnapshots {};
    void saveSnapshot (int modeIndex);
    void loadSnapshot (int modeIndex);

    bool externalSidechainSeen = false; // audio thread: the sidechain bus has carried signal since prepare
    bool referenceRunning = false;      // audio thread: Delta's reference filters are in use

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

    // Latency report after an oversampling change, on the message thread. An AsyncUpdater owned by the
    // processor is cancelled when the processor is destroyed, so a queued update can never run on a freed
    // object (MessageManager::callAsync with a captured `this` could), and triggering it does not allocate.
    void handleAsyncUpdate() override;

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
    std::atomic<float>* autoGainParam = nullptr;
    std::atomic<float>* deltaParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicCompressorAudioProcessor)
};
