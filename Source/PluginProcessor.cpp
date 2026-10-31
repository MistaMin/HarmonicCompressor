#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
HarmonicCompressorAudioProcessor::HarmonicCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Cache raw parameter pointers once; from here on the audio thread only
    // performs relaxed atomic loads.
    modeParam        = apvts.getRawParameterValue (ParamIDs::algorithmMode);
    thresholdParam   = apvts.getRawParameterValue (ParamIDs::threshold);
    ratioParam       = apvts.getRawParameterValue (ParamIDs::ratio);
    attackParam      = apvts.getRawParameterValue (ParamIDs::attack);
    releaseParam     = apvts.getRawParameterValue (ParamIDs::release);
    kneeParam        = apvts.getRawParameterValue (ParamIDs::knee);
    makeupGainParam  = apvts.getRawParameterValue (ParamIDs::makeupGain);
    mixParam         = apvts.getRawParameterValue (ParamIDs::mix);
    harmonic2Param   = apvts.getRawParameterValue (ParamIDs::harmonic2);
    harmonic3Param   = apvts.getRawParameterValue (ParamIDs::harmonic3);
    harmonic4Param   = apvts.getRawParameterValue (ParamIDs::harmonic4);
    harmonic5Param   = apvts.getRawParameterValue (ParamIDs::harmonic5);
    scEnabledParam   = apvts.getRawParameterValue (ParamIDs::scEnabled);
    scListenParam    = apvts.getRawParameterValue (ParamIDs::scListen);
    scHPFreqParam    = apvts.getRawParameterValue (ParamIDs::scHPFreq);
    autoReleaseParam = apvts.getRawParameterValue (ParamIDs::autoRelease);
    autoAttackParam  = apvts.getRawParameterValue (ParamIDs::autoAttack);

    // Seed each mode's remembered parameters with that mode's hardware-style
    // defaults for attack/release/knee, and sensible shared defaults for
    // everything else, so the very first switch to a mode still lands on its
    // characteristic ballistics.
    for (int i = 0; i < (int) CompressorMode::Count; ++i)
    {
        auto defaults = AlgorithmDefaults::forMode (static_cast<CompressorMode> (i));
        auto& snap = modeSnapshots[(size_t) i];
        snap.thresholdDb  = -18.0f;
        snap.ratio        = 4.0f;
        snap.attackMs     = defaults.attackMs;
        snap.releaseMs    = defaults.releaseMs;
        snap.kneeDb       = defaults.kneeDb;
        snap.makeupGainDb = 0.0f;
        snap.mix          = 1.0f;
        snap.harmonic2 = snap.harmonic3 = snap.harmonic4 = snap.harmonic5 = 0.0f;
        snap.scHPFreq   = 150.0f;
        snap.scEnabled  = false;
        snap.scListen   = false;
        snap.autoRelease = false;
        snap.autoAttack  = false;
    }

    apvts.addParameterListener (ParamIDs::algorithmMode, this);
}

HarmonicCompressorAudioProcessor::~HarmonicCompressorAudioProcessor()
{
    apvts.removeParameterListener (ParamIDs::algorithmMode, this);
}

//==============================================================================
void HarmonicCompressorAudioProcessor::saveSnapshot (int modeIndex)
{
    auto& snap = modeSnapshots[(size_t) modeIndex];
    snap.thresholdDb  = thresholdParam->load (std::memory_order_relaxed);
    snap.ratio        = ratioParam->load (std::memory_order_relaxed);
    snap.attackMs     = attackParam->load (std::memory_order_relaxed);
    snap.releaseMs    = releaseParam->load (std::memory_order_relaxed);
    snap.kneeDb       = kneeParam->load (std::memory_order_relaxed);
    snap.makeupGainDb = makeupGainParam->load (std::memory_order_relaxed);
    snap.mix          = mixParam->load (std::memory_order_relaxed);
    snap.harmonic2    = harmonic2Param->load (std::memory_order_relaxed);
    snap.harmonic3    = harmonic3Param->load (std::memory_order_relaxed);
    snap.harmonic4    = harmonic4Param->load (std::memory_order_relaxed);
    snap.harmonic5    = harmonic5Param->load (std::memory_order_relaxed);
    snap.scHPFreq     = scHPFreqParam->load (std::memory_order_relaxed);
    snap.scEnabled    = scEnabledParam->load (std::memory_order_relaxed) > 0.5f;
    snap.scListen     = scListenParam->load (std::memory_order_relaxed) > 0.5f;
    snap.autoRelease  = autoReleaseParam->load (std::memory_order_relaxed) > 0.5f;
    snap.autoAttack   = autoAttackParam->load (std::memory_order_relaxed) > 0.5f;
}

void HarmonicCompressorAudioProcessor::loadSnapshot (int modeIndex)
{
    const auto& snap = modeSnapshots[(size_t) modeIndex];

    auto set = [this] (const char* paramID, float value)
    {
        if (auto* p = apvts.getParameter (paramID))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    set (ParamIDs::threshold,   snap.thresholdDb);
    set (ParamIDs::ratio,       snap.ratio);
    set (ParamIDs::attack,      snap.attackMs);
    set (ParamIDs::release,     snap.releaseMs);
    set (ParamIDs::knee,        snap.kneeDb);
    set (ParamIDs::makeupGain,  snap.makeupGainDb);
    set (ParamIDs::mix,         snap.mix);
    set (ParamIDs::harmonic2,   snap.harmonic2);
    set (ParamIDs::harmonic3,   snap.harmonic3);
    set (ParamIDs::harmonic4,   snap.harmonic4);
    set (ParamIDs::harmonic5,   snap.harmonic5);
    set (ParamIDs::scHPFreq,    snap.scHPFreq);
    set (ParamIDs::scEnabled,   snap.scEnabled  ? 1.0f : 0.0f);
    set (ParamIDs::scListen,    snap.scListen   ? 1.0f : 0.0f);
    set (ParamIDs::autoRelease, snap.autoRelease ? 1.0f : 0.0f);
    set (ParamIDs::autoAttack,  snap.autoAttack  ? 1.0f : 0.0f);
}

void HarmonicCompressorAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID != ParamIDs::algorithmMode)
        return;

    const int newIndex = juce::jlimit (0, (int) CompressorMode::Count - 1, juce::roundToInt (newValue));

    if (newIndex == currentModeIndex)
        return;

    // setValueNotifyingHost() below must run on the message thread; automation
    // can deliver this callback from other threads, so hop over if needed.
    juce::MessageManager::callAsync ([this, newIndex]
    {
        if (newIndex == currentModeIndex)
            return;

        saveSnapshot (currentModeIndex);
        currentModeIndex = newIndex;
        loadSnapshot (currentModeIndex);
    });
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HarmonicCompressorAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (ParamIDs::algorithmMode, "Algorithm",
                                                              juce::StringArray { "Opto", "VCA", "FET", "Red", "Hybrid", "Bridge (Provisional)", "Bus VCA (Provisional)", "FET Circuit (Provisional)", "Opto Leveler (Provisional)", "RMS VCA Dynamics (Provisional)" },
                                                              0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::threshold, "Threshold",
                                                             juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::ratio, "Ratio",
                                                             juce::NormalisableRange<float> (-2.0f, 100.0f, 0.1f, 0.5f), 4.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::attack, "Attack",
                                                             juce::NormalisableRange<float> (0.1f, 300.0f, 0.1f, 0.3f), 10.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::release, "Release",
                                                             juce::NormalisableRange<float> (5.0f, 3000.0f, 1.0f, 0.3f), 100.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::knee, "Knee",
                                                             juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::makeupGain, "Makeup Gain",
                                                             juce::NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::mix, "Mix",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::harmonic2, "2nd Harmonic",
                                                             juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::harmonic3, "3rd Harmonic",
                                                             juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::harmonic4, "4th Harmonic",
                                                             juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::harmonic5, "5th Harmonic",
                                                             juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::scEnabled, "Sidechain Enable", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::scListen, "Sidechain Listen", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::scHPFreq, "Sidechain HP Freq",
                                                             juce::NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.3f), 150.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::autoRelease, "Auto Release", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::autoAttack, "Auto Attack", false));

    return layout;
}

//==============================================================================
void HarmonicCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();
    oversamplingMaxBlockSize = samplesPerBlock;

    hostSampleRate = sampleRate;
    vuInputBallisticsL.reset (-60.0f);
    vuInputBallisticsR.reset (-60.0f);
    vuOutputBallisticsL.reset (-60.0f);
    vuOutputBallisticsR.reset (-60.0f);
    vuGrBallistics.reset (0.0f);
    peakInputL = peakInputR = peakOutputL = peakOutputR = PeakBallistics {};

    const int mainNumChannels = getMainBusNumOutputChannels();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate * (double) oversampling.getOversamplingFactor();
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock * (juce::uint32) oversampling.getOversamplingFactor();
    spec.numChannels = (juce::uint32) mainNumChannels;

    engine.prepare (spec);
    engine.reset();

    dryBuffer.setSize (mainNumChannels, samplesPerBlock, false, false, true);
}

void HarmonicCompressorAudioProcessor::releaseResources()
{
    oversampling.reset();
    engine.reset();
}

//==============================================================================
bool HarmonicCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Main bus: mono or stereo, input and output must match.
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    if (mainIn.size() > 2)
        return false;

    // Sidechain bus: must be disabled or match the main channel count.
    const auto sidechain = layouts.getChannelSet (true, 1);
    if (! sidechain.isDisabled() && sidechain.size() != mainIn.size())
        return false;

    return true;
}

//==============================================================================
void HarmonicCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Restrict all processing to the main input/output bus. `buffer` may
    // contain extra channels appended for the sidechain input bus when it is
    // enabled, and must not be fed directly into the (fixed 2-channel)
    // oversampler or the compressor engine.
    auto mainBuffer = getBusBuffer (buffer, false, 0);

    const int numChannels = mainBuffer.getNumChannels();
    const int numSamples  = mainBuffer.getNumSamples();

    if (numSamples == 0)
        return;

    // Defensive: some hosts may call processBlock with more samples than were
    // declared in prepareToPlay. Grow the dry-signal scratch buffer rather
    // than let a later copyFrom() write out of bounds and crash the host.
    if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() != numChannels)
        dryBuffer.setSize (numChannels, numSamples, false, false, true);

    if (numSamples > oversamplingMaxBlockSize)
    {
        oversampling.initProcessing ((size_t) numSamples);
        oversampling.reset();
        oversamplingMaxBlockSize = numSamples;
    }

    // --- Read all parameters once per block (relaxed atomic loads) ---
    CompressorParams params;
    params.thresholdDb  = thresholdParam->load (std::memory_order_relaxed);
    params.ratio        = ratioParam->load (std::memory_order_relaxed);
    params.attackMs     = attackParam->load (std::memory_order_relaxed);
    params.releaseMs    = releaseParam->load (std::memory_order_relaxed);
    params.kneeDb       = kneeParam->load (std::memory_order_relaxed);
    params.makeupGainDb = makeupGainParam->load (std::memory_order_relaxed);
    params.mix          = mixParam->load (std::memory_order_relaxed);
    params.autoRelease  = autoReleaseParam->load (std::memory_order_relaxed) > 0.5f;
    params.autoAttack   = autoAttackParam->load (std::memory_order_relaxed) > 0.5f;

    const auto mode      = static_cast<CompressorMode> ((int) modeParam->load (std::memory_order_relaxed));
    const float scHpHz   = scHPFreqParam->load (std::memory_order_relaxed);
    const bool scEnabled = scEnabledParam->load (std::memory_order_relaxed) > 0.5f;
    const bool scListen  = scListenParam->load (std::memory_order_relaxed) > 0.5f;

    // Harmonics are stored as 0..100 % in the APVTS; the engine expects 0..1.
    const float h2 = harmonic2Param->load (std::memory_order_relaxed) / 100.0f;
    const float h3 = harmonic3Param->load (std::memory_order_relaxed) / 100.0f;
    const float h4 = harmonic4Param->load (std::memory_order_relaxed) / 100.0f;
    const float h5 = harmonic5Param->load (std::memory_order_relaxed) / 100.0f;

    engine.updateBlockParameters (mode, params, scHpHz, scEnabled, scListen, h2, h3, h4, h5);

    auto* alg       = engine.getActiveAlgorithm();
    auto& harmonics = engine.getHarmonics();

    // --- Sidechain conditioning (HPF) ---
    juce::AudioBuffer<float> scBus;

    if (scEnabled && getBusCount (true) > 1)
    {
        scBus = getBusBuffer (buffer, true, 1);

        if (scBus.getNumChannels() > 0)
        {
            juce::dsp::AudioBlock<float> scBlock (scBus);
            engine.getSidechain().process (scBlock);
        }
    }

    const bool hasSidechain = scBus.getNumChannels() > 0;
    const float makeupGainLinear = juce::Decibels::decibelsToGain (params.makeupGainDb);
    const float mixWet = params.mix;

    // Capture dry signal for dry/wet mixing after oversampled processing.
    // dryBuffer is pre-sized in prepareToPlay, so this never allocates here.
    const bool needsDry = mixWet < 1.0f || scListen;
    if (needsDry)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom (ch, 0, mainBuffer, ch, 0, numSamples);
    }

    float inputPeak = 0.0f;
    float inputPeakCh[2] = { 0.0f, 0.0f };
    double inputSumSqCh[2] = { 0.0, 0.0 };
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* r = mainBuffer.getReadPointer (ch);
        const int meterCh = juce::jmin (ch, 1);
        for (int s = 0; s < numSamples; ++s)
        {
            const float a = std::abs (r[s]);
            inputPeak = juce::jmax (inputPeak, a);
            inputPeakCh[meterCh] = juce::jmax (inputPeakCh[meterCh], a);
            inputSumSqCh[meterCh] += (double) r[s] * (double) r[s];
        }
    }

    // --- Upsample, process, downsample ---
    juce::dsp::AudioBlock<float> inputBlock (mainBuffer);
    auto oversampledBlock = oversampling.processSamplesUp (inputBlock);

    const int osNumSamples  = (int) oversampledBlock.getNumSamples();
    const int osNumChannels = (int) oversampledBlock.getNumChannels();

    float blockReduction = 0.0f;
    if (mode >= CompressorMode::Bridge)
    {
        // Advance linked circuit state once per sample, regardless of channel count.
        for (int s = 0; s < osNumSamples; ++s)
        {
            float detector = 0.0f;
            for (int ch = 0; ch < osNumChannels; ++ch)
                detector = juce::jmax(detector, std::abs(oversampledBlock.getChannelPointer(ch)[s]));
            const float gr = alg->processSidechain(detector);
            blockReduction = juce::jmax(blockReduction, -juce::Decibels::gainToDecibels(gr, -120.0f));
            for (int ch = 0; ch < osNumChannels; ++ch)
            {
                auto* data = oversampledBlock.getChannelPointer(ch);
                const float y = alg->processAudio(data[s], gr, ch) * makeupGainLinear;
                data[s] = harmonics.processSample(y, h2, h3, h4, h5);
            }
        }
    }
    else for (int ch = 0; ch < osNumChannels; ++ch)
    {
        auto* data = oversampledBlock.getChannelPointer (ch);

        for (int s = 0; s < osNumSamples; ++s)
        {
            const float scSample = data[s];
            const float gr = alg->processSidechain (scSample);
            blockReduction = juce::jmax(blockReduction, -juce::Decibels::gainToDecibels(gr, -120.0f));
            float y = alg->processAudio(data[s], gr, ch) * makeupGainLinear;
            y = harmonics.processSample (y, h2, h3, h4, h5);
            data[s] = y;
        }
    }

    oversampling.processSamplesDown (inputBlock);

    // --- Dry/wet mix and sidechain listen ---
    float outputPeak = 0.0f;
    float outputPeakCh[2] = { 0.0f, 0.0f };
    double outputSumSqCh[2] = { 0.0, 0.0 };
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = mainBuffer.getWritePointer (ch);
        const float* dryData = needsDry ? dryBuffer.getReadPointer (ch) : nullptr;
        const float* scData  = (hasSidechain && ch < scBus.getNumChannels()) ? scBus.getReadPointer (ch) : nullptr;
        const int meterCh = juce::jmin (ch, 1);

        for (int s = 0; s < numSamples; ++s)
        {
            if (scListen && scData != nullptr)
                channelData[s] = scData[s];
            else if (scListen && dryData != nullptr)
                channelData[s] = dryData[s];
            else if (dryData != nullptr && mixWet < 1.0f)
                channelData[s] = mixWet * channelData[s] + (1.0f - mixWet) * dryData[s];

            const float a = std::abs (channelData[s]);
            outputPeak = juce::jmax (outputPeak, a);
            outputPeakCh[meterCh] = juce::jmax (outputPeakCh[meterCh], a);
            outputSumSqCh[meterCh] += (double) channelData[s] * (double) channelData[s];
        }
    }

    // Publish actual block peak envelopes and maximum applied wet-path GR.
    // Dropping telemetry when no editor drains it must never stall audio.
    int start1, size1, start2, size2;
    historyFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        historyFrames[(size_t)start1] = {inputPeak, outputPeak, blockReduction,
                                      (float)numSamples / (float)hostSampleRate};
        historyFifo.finishedWrite(1);
    }
    else historyDrops.fetch_add(1, std::memory_order_relaxed);

    // --- Update UI-facing atomics once per block ---
    lastGrDbForUI.store (alg->getLastGainReductionDb(), std::memory_order_relaxed);
    lastInputPeakDb.store (juce::jmax (-60.0f, juce::Decibels::gainToDecibels (inputPeak)), std::memory_order_relaxed);
    lastOutputPeakDb.store (juce::jmax (-60.0f, juce::Decibels::gainToDecibels (outputPeak)), std::memory_order_relaxed);

    // --- VU meter: per-channel RMS + damped analog ballistics, plus
    //     fast-attack/slow-release peak ballistics for the peak LEDs/readout.
    auto rmsDbFor = [numSamples] (double sumSq) -> float
    {
        return numSamples > 0
            ? juce::jmax (-60.0f, juce::Decibels::gainToDecibels ((float) std::sqrt (sumSq / (double) numSamples)))
            : -60.0f;
    };

    const float inputRmsDbL  = rmsDbFor (inputSumSqCh[0]);
    const float inputRmsDbR  = rmsDbFor (inputSumSqCh[1]);
    const float outputRmsDbL = rmsDbFor (outputSumSqCh[0]);
    const float outputRmsDbR = rmsDbFor (outputSumSqCh[1]);

    const float blockSeconds = (float) numSamples / (float) hostSampleRate;
    const float vuCoeff   = 1.0f - std::exp (-blockSeconds / 0.15f);  // ~300 ms VU ballistics
    const float peakCoeff = std::exp (-blockSeconds / 0.80f);         // ~0.8 s peak-LED release

    vuInputDbL.store  (vuInputBallisticsL.process  (inputRmsDbL,  vuCoeff), std::memory_order_relaxed);
    vuInputDbR.store  (vuInputBallisticsR.process  (inputRmsDbR,  vuCoeff), std::memory_order_relaxed);
    vuOutputDbL.store (vuOutputBallisticsL.process (outputRmsDbL, vuCoeff), std::memory_order_relaxed);
    vuOutputDbR.store (vuOutputBallisticsR.process (outputRmsDbR, vuCoeff), std::memory_order_relaxed);
    vuGrDb.store (vuGrBallistics.process (alg->getLastGainReductionDb(), vuCoeff), std::memory_order_relaxed);

    const float inputPeakDbL  = juce::jmax (-60.0f, juce::Decibels::gainToDecibels (inputPeakCh[0]));
    const float inputPeakDbR  = juce::jmax (-60.0f, juce::Decibels::gainToDecibels (inputPeakCh[1]));
    const float outputPeakDbL = juce::jmax (-60.0f, juce::Decibels::gainToDecibels (outputPeakCh[0]));
    const float outputPeakDbR = juce::jmax (-60.0f, juce::Decibels::gainToDecibels (outputPeakCh[1]));

    peakInputDbL.store  (peakInputL.process  (inputPeakDbL,  peakCoeff), std::memory_order_relaxed);
    peakInputDbR.store  (peakInputR.process  (inputPeakDbR,  peakCoeff), std::memory_order_relaxed);
    peakOutputDbL.store (peakOutputL.process (outputPeakDbL, peakCoeff), std::memory_order_relaxed);
    peakOutputDbR.store (peakOutputR.process (outputPeakDbR, peakCoeff), std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* HarmonicCompressorAudioProcessor::createEditor()
{
    return new HarmonicCompressorAudioProcessorEditor (*this);
}

bool HarmonicCompressorAudioProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
const juce::String HarmonicCompressorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool HarmonicCompressorAudioProcessor::acceptsMidi() const
{
    return false;
}

bool HarmonicCompressorAudioProcessor::producesMidi() const
{
    return false;
}

bool HarmonicCompressorAudioProcessor::isMidiEffect() const
{
    return false;
}

double HarmonicCompressorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================
int HarmonicCompressorAudioProcessor::getNumPrograms()
{
    return 1;
}

int HarmonicCompressorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void HarmonicCompressorAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String HarmonicCompressorAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void HarmonicCompressorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void HarmonicCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void HarmonicCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);

    if (tree.isValid())
        apvts.replaceState (tree);
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HarmonicCompressorAudioProcessor();
}
