// Regression checks, using only the 1.5.1 parameter set.
//  F1  queued message-thread work must not outlive the processor (was: callAsync capturing `this`)
//  F2  the dry path must match the wet path's resampler response (was: interpolated delay line)
//  A1  output independent of the host buffer size (was: Red/Hybrid shared one envelope across channels)
//  A2  Hybrid never boosts above the input at ratios between 0 and 1 (was: unbounded gain)
//  A4  a sidechain that falls silent means no compression (was: fell back to the main input)
//  A5  per-model memory is recalled only by the editor's selector and saved with the session
// Usage: ProcessorFixesTest [--lifetime-only]
#if defined (__APPLE__)
 #include <CoreFoundation/CoreFoundation.h> // before JUCE: its 'using namespace juce' makes MacTypes' Point ambiguous
#endif
#include "PluginProcessor.h"
#include <cstring>
#include <functional>
#include <vector>
#include <iostream>

namespace
{
int checks = 0, failures = 0;

void check (bool pass, const char* row, const juce::String& what)
{
    ++checks;
    failures += pass ? 0 : 1;
    std::cout << (pass ? "PASS " : "FAIL ") << "[" << row << "] " << what << std::endl;
}

void pump (int ms)
{
#if defined (__APPLE__)
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, ms / 1000.0, false);
#else
    juce::ignoreUnused (ms);
#endif
}

void set (HarmonicCompressorAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float get (HarmonicCompressorAudioProcessor& p, const char* id)
{
    return p.apvts.getRawParameterValue (id)->load();
}

//------------------------------------------------------------------------------
// F2: gain of the whole plugin at one frequency, with neutral compression (the wet path is the
// resampler chain alone), so any difference between Mix settings is the dry path's own error.
double sineGainDb (int os, int filter, float mix, double freq)
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    HarmonicCompressorAudioProcessor p;
    set (p, ParamIDs::oversampleMode, (float) os);
    set (p, ParamIDs::oversampleFilter, (float) filter);
    set (p, ParamIDs::threshold, 0.0f);
    set (p, ParamIDs::ratio, 1.0f);
    set (p, ParamIDs::mix, mix);
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);

    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    double inSum = 0.0, outSum = 0.0;
    const int total = (int) sr; // 1 s, measure the last half
    for (int pos = 0; pos < total; pos += block)
    {
        for (int s = 0; s < block; ++s)
        {
            const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * double (pos + s) / sr);
            buffer.setSample (0, s, x);
            buffer.setSample (1, s, x);
            if (pos >= total / 2) inSum += (double) x * x;
        }
        p.processBlock (buffer, midi);
        if (pos >= total / 2)
            for (int s = 0; s < block; ++s)
                outSum += (double) buffer.getSample (0, s) * buffer.getSample (0, s);
    }
    pump (5);
    return 10.0 * std::log10 (outSum / inSum);
}

void testDryPath()
{
    const char* names[] = { "1x", "2x Min Phase", "2x Lin Phase", "4x Min Phase", "4x Lin Phase", "8x Min Phase", "8x Lin Phase" };
    const int modes[][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 2, 1 }, { 3, 0 }, { 3, 1 } };
    const double freqs[] = { 100.0, 1000.0, 5000.0, 10000.0, 15000.0, 18000.0 };

    for (int m = 0; m < 7; ++m)
    {
        // Parallel Mix: 50 % and 0 % against 100 % wet. A dry path matched to the wet path makes the three
        // identical at every frequency (wet = resampler chain here). A mismatched one shows loss or combing.
        double worstHalf = 0.0, worstDry = 0.0;
        juce::String detail;
        for (double f : freqs)
        {
            const double wet  = sineGainDb (modes[m][0], modes[m][1], 1.0f, f);
            const double half = sineGainDb (modes[m][0], modes[m][1], 0.5f, f) - wet;
            const double dry  = sineGainDb (modes[m][0], modes[m][1], 0.0f, f) - wet;
            worstHalf = std::max (worstHalf, std::abs (half));
            worstDry  = std::max (worstDry,  std::abs (dry));
            detail << " " << juce::String (f / 1000.0, 1) << "k:" << juce::String (half, 2) << "/" << juce::String (dry, 2);
        }
        check (worstHalf < 0.01 && worstDry < 0.01, "F2",
               juce::String (names[m]) + ": Mix 50%/0% vs 100%, worst " + juce::String (worstHalf, 3) + " / "
               + juce::String (worstDry, 3) + " dB  (per freq, dB:" + detail + ")");
    }
}

//------------------------------------------------------------------------------
void testModeMemory()
{
    HarmonicCompressorAudioProcessor p;
    set (p, ParamIDs::threshold, -30.0f);
    p.selectMode (1);
    check (std::abs (get (p, ParamIDs::threshold) + 18.0f) < 0.05f, "A5",
           "selecting Hybrid loads its remembered threshold (-18): " + juce::String (get (p, ParamIDs::threshold), 1));
    set (p, ParamIDs::threshold, -40.0f);
    p.selectMode (0);
    check (std::abs (get (p, ParamIDs::threshold) + 30.0f) < 0.05f, "A5",
           "selecting Red again restores -30: " + juce::String (get (p, ParamIDs::threshold), 1));

    // A host or automation change of the model only changes the model.
    set (p, ParamIDs::algorithmMode, 2.0f);
    pump (20);
    check (std::abs (get (p, ParamIDs::threshold) + 30.0f) < 0.05f && juce::roundToInt (get (p, ParamIDs::algorithmMode)) == 2,
           "A5", "host switch to Bridge keeps threshold -30: " + juce::String (get (p, ParamIDs::threshold), 1));
    p.selectMode (0);

    // Saved with the session: a new instance recalls Hybrid's -40.
    juce::MemoryBlock state;
    p.getStateInformation (state);
    HarmonicCompressorAudioProcessor q;
    q.setStateInformation (state.getData(), (int) state.getSize());
    q.selectMode (1);
    check (std::abs (get (q, ParamIDs::threshold) + 40.0f) < 0.05f, "A5",
           "after save and reload, Hybrid recalls -40: " + juce::String (get (q, ParamIDs::threshold), 1));

    // A session saved before the memory was stored loads, and the other models keep their defaults.
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    xml->removeChildElement (xml->getChildByName ("ModeSnapshots"), true);
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    HarmonicCompressorAudioProcessor r;
    r.setStateInformation (oldState.getData(), (int) oldState.getSize());
    const bool loaded = std::abs (get (r, ParamIDs::threshold) + 30.0f) < 0.05f;
    r.selectMode (1);
    check (loaded && std::abs (get (r, ParamIDs::threshold) + 18.0f) < 0.05f, "A5",
           "old-format session loads (-30) and Hybrid keeps its default: " + juce::String (get (r, ParamIDs::threshold), 1));
}

// Renders `seconds` of a 1 kHz sine at `level` through `p` (1x, 48 kHz) and returns output/input in dB over
// the last quarter. `sidechain(t)` feeds the sidechain bus when the processor has one enabled.
double renderGainDb (HarmonicCompressorAudioProcessor& p, float level, std::function<float (double)> sidechain = {})
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), block);
    juce::MidiBuffer midi;
    const int total = 96 * block;
    double inSum = 0.0, outSum = 0.0;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int s = 0; s < block; ++s)
        {
            const double t = double (pos + s) / sr;
            const float x = level * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * t);
            buffer.setSample (0, s, x);
            buffer.setSample (1, s, x);
            for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, s, sidechain ? sidechain (t) : 0.0f);
            if (pos >= total * 3 / 4) inSum += (double) x * x;
        }
        p.processBlock (buffer, midi);
        if (pos >= total * 3 / 4)
            for (int s = 0; s < block; ++s)
                outSum += (double) buffer.getSample (0, s) * buffer.getSample (0, s);
    }
    return 10.0 * std::log10 (outSum / inSum);
}

void testHybridLowRatio()
{
    for (float ratio : { 0.5f, 0.1f })
    {
        HarmonicCompressorAudioProcessor p;
        set (p, ParamIDs::algorithmMode, 1.0f);
        set (p, ParamIDs::oversampleMode, 0.0f);
        set (p, ParamIDs::threshold, -40.0f);
        set (p, ParamIDs::ratio, ratio);
        const double g = renderGainDb (p, 0.5f);
        check (g <= 0.01, "A2", "Hybrid ratio " + juce::String (ratio, 1) + ":1, threshold -40 dB: gain " + juce::String (g, 2) + " dB");
    }
}

void testSilentSidechain()
{
    HarmonicCompressorAudioProcessor p;
    p.enableAllBuses();
    set (p, ParamIDs::oversampleMode, 0.0f);
    set (p, ParamIDs::threshold, -30.0f);
    set (p, ParamIDs::ratio, 10.0f);
    set (p, ParamIDs::release, 20.0f);
    // Kick-like sidechain for the first quarter second, then digital silence.
    const double g = renderGainDb (p, 0.5f, [] (double t)
    {
        return t < 0.25 ? 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * t) : 0.0f;
    });
    check (p.getTotalNumInputChannels() == 4 && std::abs (g) < 0.1, "A4",
           "main input uncompressed once the sidechain is silent: " + juce::String (g, 2) + " dB ("
           + juce::String (p.getTotalNumInputChannels()) + " input channels)");
}

void testLatencyReport()
{
    HarmonicCompressorAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    set (p, ParamIDs::oversampleMode, 3.0f);
    set (p, ParamIDs::oversampleFilter, 1.0f);
    pump (20);
    check (p.getLatencySamples() == 64, "F1", "latency report follows 8x Lin Phase: " + juce::String (p.getLatencySamples()));
    set (p, ParamIDs::oversampleMode, 0.0f);
    pump (20);
    check (p.getLatencySamples() == 0, "F1", "and 1x: " + juce::String (p.getLatencySamples()));
}

// A1 (2026-11-17 audit): the output must not depend on the host's buffer size. Different left and right
// signals, so a detector state shared between channels shows up.
std::vector<float> renderStereo (int mode, int block)
{
    constexpr double sr = 48000.0;
    HarmonicCompressorAudioProcessor p;
    set (p, ParamIDs::algorithmMode, (float) mode);
    set (p, ParamIDs::threshold, -30.0f);
    set (p, ParamIDs::ratio, 4.0f);
    set (p, ParamIDs::attack, 5.0f);
    set (p, ParamIDs::release, 80.0f);
    p.setRateAndBufferSizeDetails (sr, 512);
    p.prepareToPlay (sr, 512);

    const int total = 48 * 512;
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int s = 0; s < block; ++s)
        {
            const double t = double (pos + s) / sr;
            const double env = std::exp (-std::fmod (t, 0.13) / 0.03);
            buffer.setSample (0, s, (float) (0.7 * env * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t)));
            buffer.setSample (1, s, (float) (0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 1300.0 * t)));
        }
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < block; ++s)
                out.push_back (buffer.getSample (ch, s));
    }
    return out;
}

void testBlockSizeInvariance()
{
    const char* names[] = { "Red", "Hybrid", "Bridge", "Bus VCA", "76", "Opto Leveler", "VCA 160" };
    for (int mode = 0; mode < 7; ++mode)
    {
        // Compared sample by sample, so reorder the 512-sample render's interleaving to 64-sample blocks.
        const auto small = renderStereo (mode, 64);
        const auto large = renderStereo (mode, 512);
        double worst = 0.0;
        for (size_t blockStart = 0; blockStart < small.size() / 2; blockStart += 512)
            for (int ch = 0; ch < 2; ++ch)
                for (size_t s = 0; s < 512; ++s)
                {
                    const size_t frame = blockStart + s;
                    const float a = small[(frame / 64) * 128 + (size_t) ch * 64 + frame % 64];
                    const float b = large[(frame / 512) * 1024 + (size_t) ch * 512 + frame % 512];
                    worst = std::max (worst, (double) std::abs (a - b));
                }
        check (worst < 1.0e-5, "A1", juce::String (names[mode]) + ": 64- vs 512-sample blocks, max difference "
                                     + juce::String (worst, 8));
    }
}

// Last: on the old code this is a use-after-free, which crashes or corrupts the heap.
void testLifetime()
{
    for (int i = 0; i < 50; ++i)
    {
        auto p = std::make_unique<HarmonicCompressorAudioProcessor>();
        set (*p, ParamIDs::oversampleMode, (float) (i % 4));
        set (*p, ParamIDs::oversampleFilter, (float) (i % 2));
        p.reset();   // destroy it now, with that work still queued
    }
    pump (50);       // delivers whatever is still queued
    check (true, "F1", "50 processors destroyed with oversampling changes queued; message loop ran without a crash");
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool lifetimeOnly = argc > 1 && std::strcmp (argv[1], "--lifetime-only") == 0;
    if (! lifetimeOnly)
    {
        testModeMemory();
        testHybridLowRatio();
        testSilentSidechain();
        testLatencyReport();
        testDryPath();
        testBlockSizeInvariance();
    }
    testLifetime();
    std::cout << checks << " checks, " << failures << " failures" << std::endl;
    return failures == 0 ? 0 : 1;
}
