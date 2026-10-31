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
    inputGainParam        = apvts.getRawParameterValue (ParamIDs::inputGain);
    outputGainParam       = apvts.getRawParameterValue (ParamIDs::outputGain);
    oversampleModeParam   = apvts.getRawParameterValue (ParamIDs::oversampleMode);
    oversampleFilterParam = apvts.getRawParameterValue (ParamIDs::oversampleFilter);
    transientEnabledParam = apvts.getRawParameterValue (ParamIDs::transientEnabled);
    transientAttackParam  = apvts.getRawParameterValue (ParamIDs::transientAttack);
    transientSustainParam = apvts.getRawParameterValue (ParamIDs::transientSustain);
    transientSpeedParam   = apvts.getRawParameterValue (ParamIDs::transientSpeed);

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
    apvts.addParameterListener (ParamIDs::oversampleMode, this);
    apvts.addParameterListener (ParamIDs::oversampleFilter, this);
}

HarmonicCompressorAudioProcessor::~HarmonicCompressorAudioProcessor()
{
    apvts.removeParameterListener (ParamIDs::algorithmMode, this);
    apvts.removeParameterListener (ParamIDs::oversampleMode, this);
    apvts.removeParameterListener (ParamIDs::oversampleFilter, this);
}

void HarmonicCompressorAudioProcessor::updateReportedLatency()
{
    const auto osMode = static_cast<hcdsp::OversampleMode> (
        juce::jlimit (0, 3, juce::roundToInt (oversampleModeParam->load (std::memory_order_relaxed))));
    const auto osFilter = static_cast<hcdsp::ResamplerFilter> (
        juce::jlimit (0, 1, juce::roundToInt (oversampleFilterParam->load (std::memory_order_relaxed))));
    const int lat = juce::roundToInt (multirate.getLatencySamplesFor (osMode, osFilter));
    if (lat != getLatencySamples())
        setLatencySamples (lat);
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
    if (parameterID == ParamIDs::oversampleMode || parameterID == ParamIDs::oversampleFilter)
    {
        juce::MessageManager::callAsync ([this] { updateReportedLatency(); });
        return;
    }

    if (parameterID != ParamIDs::algorithmMode || restoringState.load (std::memory_order_acquire))
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
                                                              juce::StringArray { "Red", "Hybrid", "Bridge", "Bus VCA", "76", "Opto Leveler", "VCA 160" },
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

    // Appended v1.5 parameters (preserving indices 0..16 for existing sessions)
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::inputGain, "Input Gain",
                                                             juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::outputGain, "Output Gain",
                                                             juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParamIDs::oversampleMode, "Oversample",
                                                              juce::StringArray { "1x", "2x", "4x", "8x" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParamIDs::oversampleFilter, "Oversample Filter",
                                                              juce::StringArray { "Min Phase", "Lin Phase" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParamIDs::transientEnabled, "Transient Enable", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::transientAttack, "Transient Attack",
                                                             juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::transientSustain, "Transient Sustain",
                                                             juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParamIDs::transientSpeed, "Transient Speed",
                                                             juce::NormalisableRange<float> (0.5f, 50.0f, 0.1f, 0.4f), 10.0f));

    return layout;
}

//==============================================================================
void HarmonicCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    vuInputBallisticsL.reset (-60.0f);
    vuInputBallisticsR.reset (-60.0f);
    vuOutputBallisticsL.reset (-60.0f);
    vuOutputBallisticsR.reset (-60.0f);
    vuGrBallistics.reset (0.0f);
    peakInputL = peakInputR = peakOutputL = peakOutputR = PeakBallistics {};

    const int mainNumChannels = juce::jmax (1, getMainBusNumOutputChannels());

    multirate.prepare (sampleRate, samplesPerBlock, mainNumChannels);
    const auto osMode = static_cast<hcdsp::OversampleMode> (
        juce::jlimit (0, 3, juce::roundToInt (oversampleModeParam->load (std::memory_order_relaxed))));
    const auto osFilter = static_cast<hcdsp::ResamplerFilter> (
        juce::jlimit (0, 1, juce::roundToInt (oversampleFilterParam->load (std::memory_order_relaxed))));
    multirate.setConfig (osMode, osFilter);

    transientDesigner.prepare (sampleRate);

    inputGainSmoothed.reset (sampleRate, 0.010);
    inputGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (inputGainParam->load (std::memory_order_relaxed)));
    outputGainSmoothed.reset (sampleRate, 0.010);
    outputGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (outputGainParam->load (std::memory_order_relaxed)));

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = multirate.getEffectiveSampleRate();
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock * multirate.getFactor());
    spec.numChannels = static_cast<juce::uint32> (mainNumChannels);

    engine.prepare (spec);
    engine.reset();

    dryBuffer.setSize (mainNumChannels, samplesPerBlock, false, false, true);
    updateReportedLatency();
}

void HarmonicCompressorAudioProcessor::releaseResources()
{
    reset();
}

void HarmonicCompressorAudioProcessor::reset()
{
    multirate.reset();
    transientDesigner.reset();
    engine.reset();
    vuInputBallisticsL.reset (-60.0f);
    vuInputBallisticsR.reset (-60.0f);
    vuOutputBallisticsL.reset (-60.0f);
    vuOutputBallisticsR.reset (-60.0f);
    vuGrBallistics.reset (0.0f);
    peakInputL = peakInputR = peakOutputL = peakOutputR = PeakBallistics {};
    if (inputGainParam != nullptr)
        inputGainSmoothed.setCurrentAndTargetValue (
            juce::Decibels::decibelsToGain (inputGainParam->load (std::memory_order_relaxed)));
    if (outputGainParam != nullptr)
        outputGainSmoothed.setCurrentAndTargetValue (
            juce::Decibels::decibelsToGain (outputGainParam->load (std::memory_order_relaxed)));
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

    if (numSamples == 0 || numChannels == 0)
        return;

    // Defensive: some hosts may call processBlock with more samples than were
    // declared in prepareToPlay.
    if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() != numChannels)
        dryBuffer.setSize (numChannels, numSamples, false, false, true);

    multirate.ensureBlockSize (numSamples);

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

    const float inGainDb  = inputGainParam->load (std::memory_order_relaxed);
    const float outGainDb = outputGainParam->load (std::memory_order_relaxed);
    const auto osMode = static_cast<hcdsp::OversampleMode> (
        juce::jlimit (0, 3, juce::roundToInt (oversampleModeParam->load (std::memory_order_relaxed))));
    const auto osFilter = static_cast<hcdsp::ResamplerFilter> (
        juce::jlimit (0, 1, juce::roundToInt (oversampleFilterParam->load (std::memory_order_relaxed))));

    const bool trEnabled  = transientEnabledParam->load (std::memory_order_relaxed) > 0.5f;
    const float trAttack  = transientAttackParam->load (std::memory_order_relaxed);
    const float trSustain = transientSustainParam->load (std::memory_order_relaxed);
    const float trSpeed   = transientSpeedParam->load (std::memory_order_relaxed);

    // Harmonics are stored as 0..100 % in the APVTS; the engine expects 0..1.
    const float h2 = harmonic2Param->load (std::memory_order_relaxed) / 100.0f;
    const float h3 = harmonic3Param->load (std::memory_order_relaxed) / 100.0f;
    const float h4 = harmonic4Param->load (std::memory_order_relaxed) / 100.0f;
    const float h5 = harmonic5Param->load (std::memory_order_relaxed) / 100.0f;

    // --- Input Gain stage (drives input meter, history PRE trace, and compressor threshold) ---
    inputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (inGainDb));
    if (inputGainSmoothed.isSmoothing() || std::abs (inGainDb) > 1.0e-4f)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            const float g = inputGainSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                mainBuffer.getWritePointer (ch)[s] *= g;
        }
    }

    // --- Configure multirate oversampling & propagate effective sample rate ---
    multirate.setConfig (osMode, osFilter);
    engine.ensureSampleRate (multirate.getEffectiveSampleRate());
    engine.updateBlockParameters (mode, params, scHpHz, scEnabled, scListen, h2, h3, h4, h5);

    auto* alg       = engine.getActiveAlgorithm();
    auto& harmonics = engine.getHarmonics();
    auto& sidechain = engine.getSidechain();

    // --- External sidechain detection (falls back to internal main input when unrouted/silent) ---
    juce::AudioBuffer<float> extScBus;
    if (getBusCount (true) > 1)
        extScBus = getBusBuffer (buffer, true, 1);
    const bool hasExternalSc = extScBus.getNumChannels() > 0
                            && extScBus.getNumSamples() >= numSamples
                            && extScBus.getMagnitude (0, numSamples) > 1.0e-7f;

    const float makeupGainLinear = juce::Decibels::decibelsToGain (params.makeupGainDb);
    const float mixWet = params.mix;

    // Always capture and latency-compensate dryBuffer so parallel Mix aligns phase-coherently
    // across 1x/2x/4x/8x Min Phase (IIR) and Lin Phase (FIR) oversampling modes.
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, mainBuffer, ch, 0, numSamples);
    multirate.delayDryBuffer (dryBuffer, numSamples);

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

    // --- Upsample, process compressor + sidechain HPF + ADAA harmonics, downsample ---
    auto oversampledBlock = multirate.upsample (mainBuffer);

    const int osNumSamples  = static_cast<int> (oversampledBlock.getNumSamples());
    const int osNumChannels = static_cast<int> (oversampledBlock.getNumChannels());
    const int osFactor      = multirate.getFactor();

    auto readRawSidechain = [&] (int ch, int osSampleIdx) noexcept -> float
    {
        if (! hasExternalSc)
            return oversampledBlock.getChannelPointer (static_cast<size_t> (ch))[osSampleIdx];

        const int scCh = juce::jmin (ch, extScBus.getNumChannels() - 1);
        const float* scPtr = extScBus.getReadPointer (scCh);
        const float basePos = static_cast<float> (osSampleIdx) / static_cast<float> (osFactor);
        const int i0 = juce::jlimit (0, numSamples - 1, static_cast<int> (basePos));
        const int i1 = juce::jmin (numSamples - 1, i0 + 1);
        const float frac = basePos - static_cast<float> (i0);
        return scPtr[i0] + frac * (scPtr[i1] - scPtr[i0]);
    };

    float blockReduction = 0.0f;
    if (mode >= CompressorMode::Bridge)
    {
        for (int s = 0; s < osNumSamples; ++s)
        {
            float detector = 0.0f;
            float scCh[2] = { 0.0f, 0.0f };
            for (int ch = 0; ch < osNumChannels; ++ch)
            {
                const float rawSc = readRawSidechain (ch, s);
                const float scVal = scEnabled ? sidechain.processSample (ch, rawSc) : rawSc;
                if (ch < 2)
                    scCh[ch] = scVal;
                detector = juce::jmax (detector, std::abs (scVal));
            }
            const float gr = alg->processSidechain (detector);
            blockReduction = juce::jmax (blockReduction, -juce::Decibels::gainToDecibels (gr, -120.0f));
            for (int ch = 0; ch < osNumChannels; ++ch)
            {
                auto* data = oversampledBlock.getChannelPointer (static_cast<size_t> (ch));
                if (scListen)
                    data[s] = (ch < 2) ? scCh[ch] : 0.0f;
                else
                {
                    const float y = alg->processAudio (data[s], gr, ch) * makeupGainLinear;
                    data[s] = harmonics.processSample (y, h2, h3, h4, h5, ch);
                }
            }
        }
    }
    else
    {
        for (int ch = 0; ch < osNumChannels; ++ch)
        {
            auto* data = oversampledBlock.getChannelPointer (static_cast<size_t> (ch));

            for (int s = 0; s < osNumSamples; ++s)
            {
                const float rawSc = readRawSidechain (ch, s);
                const float scSample = scEnabled ? sidechain.processSample (ch, rawSc) : rawSc;
                const float gr = alg->processSidechain (scSample);
                blockReduction = juce::jmax (blockReduction, -juce::Decibels::gainToDecibels (gr, -120.0f));
                if (scListen)
                {
                    data[s] = scSample;
                }
                else
                {
                    float y = alg->processAudio (data[s], gr, ch) * makeupGainLinear;
                    y = harmonics.processSample (y, h2, h3, h4, h5, ch);
                    data[s] = y;
                }
            }
        }
    }

    multirate.downsample (mainBuffer);

    // --- Latency-aligned Dry/Wet mix ---
    if (! scListen && mixWet < 1.0f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = mainBuffer.getWritePointer (ch);
            const float* dryData = dryBuffer.getReadPointer (ch);
            for (int s = 0; s < numSamples; ++s)
                channelData[s] = mixWet * channelData[s] + (1.0f - mixWet) * dryData[s];
        }
    }

    // --- End-of-chain Transient Designer modifier (post-mix, pre-output-gain) ---
    if (! scListen)
        transientDesigner.processBlock (mainBuffer, numSamples, trEnabled, trAttack, trSustain, trSpeed);
    lastTransientGainDb.store (transientDesigner.getLastAppliedGainDb(), std::memory_order_relaxed);

    // --- Output Gain stage ---
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outGainDb));
    if (outputGainSmoothed.isSmoothing() || std::abs (outGainDb) > 1.0e-4f)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            const float g = outputGainSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                mainBuffer.getWritePointer (ch)[s] *= g;
        }
    }

    // --- Measure final output for meters & Compression History ---
    float outputPeak = 0.0f;
    float outputPeakCh[2] = { 0.0f, 0.0f };
    double outputSumSqCh[2] = { 0.0, 0.0 };
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto* channelData = mainBuffer.getReadPointer (ch);
        const int meterCh = juce::jmin (ch, 1);

        for (int s = 0; s < numSamples; ++s)
        {
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
    lastInputPeakDb.store (juce::jmax (-72.0f, juce::Decibels::gainToDecibels (inputPeak, -72.0f)), std::memory_order_relaxed);
    lastOutputPeakDb.store (juce::jmax (-72.0f, juce::Decibels::gainToDecibels (outputPeak, -72.0f)), std::memory_order_relaxed);

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
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HarmonicCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree tree;
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
            tree = juce::ValueTree::fromXml (*xml);
    }
    if (! tree.isValid())
        tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);

    if (tree.isValid())
    {
        restoringState.store (true, std::memory_order_release);
        apvts.replaceState (tree);
        currentModeIndex = juce::jlimit (0, (int) CompressorMode::Count - 1,
                                         juce::roundToInt (modeParam->load (std::memory_order_relaxed)));
        saveSnapshot (currentModeIndex);
        restoringState.store (false, std::memory_order_release);
        updateReportedLatency();
    }
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HarmonicCompressorAudioProcessor();
}
