// Regression checks for two fixes, using only the 1.5.1 parameter set so the same file also builds
// against the previous source to measure the "before" state.
//  F1  queued message-thread work must not outlive the processor (was: callAsync capturing `this`)
//  F2  the dry path must match the wet path's resampler response (was: interpolated delay line)
// Usage: ProcessorFixesTest [--lifetime-only]
#if defined (__APPLE__)
 #include <CoreFoundation/CoreFoundation.h> // before JUCE: its 'using namespace juce' makes MacTypes' Point ambiguous
#endif
#include "PluginProcessor.h"
#include <cstring>
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
    set (p, ParamIDs::algorithmMode, 1.0f);
    pump (20);
    check (std::abs (get (p, ParamIDs::threshold) + 18.0f) < 0.05f, "F1",
           "switch to Hybrid loads its remembered threshold (-18): " + juce::String (get (p, ParamIDs::threshold), 1));
    set (p, ParamIDs::threshold, -40.0f);
    set (p, ParamIDs::algorithmMode, 0.0f);
    pump (20);
    check (std::abs (get (p, ParamIDs::threshold) + 30.0f) < 0.05f, "F1",
           "switch back to Red restores -30: " + juce::String (get (p, ParamIDs::threshold), 1));
    set (p, ParamIDs::algorithmMode, 1.0f);
    pump (20);
    check (std::abs (get (p, ParamIDs::threshold) + 40.0f) < 0.05f, "F1",
           "Hybrid remembers -40: " + juce::String (get (p, ParamIDs::threshold), 1));

    // Several switches before the message thread runs: the end state is the last one.
    set (p, ParamIDs::algorithmMode, 2.0f);
    set (p, ParamIDs::algorithmMode, 3.0f);
    set (p, ParamIDs::algorithmMode, 0.0f);
    pump (20);
    check (std::abs (get (p, ParamIDs::threshold) + 30.0f) < 0.05f, "F1",
           "Bridge, Bus VCA, Red queued together end on Red's -30: " + juce::String (get (p, ParamIDs::threshold), 1));
    set (p, ParamIDs::algorithmMode, 1.0f);
    pump (20);
    check (std::abs (get (p, ParamIDs::threshold) + 40.0f) < 0.05f, "F1",
           "and Hybrid's -40 survived the queued switches: " + juce::String (get (p, ParamIDs::threshold), 1));
}

void testRestoreRace()
{
    HarmonicCompressorAudioProcessor source;
    set (source, ParamIDs::threshold, -12.0f);
    juce::MemoryBlock state;
    source.getStateInformation (state);   // Red, threshold -12

    HarmonicCompressorAudioProcessor p;
    set (p, ParamIDs::algorithmMode, 4.0f);                       // queued, not yet delivered
    p.setStateInformation (state.getData(), (int) state.getSize());
    pump (20);
    check (std::abs (get (p, ParamIDs::threshold) + 12.0f) < 0.05f && juce::roundToInt (get (p, ParamIDs::algorithmMode)) == 0,
           "F1", "mode switch queued before a restore does not overwrite it: threshold "
                 + juce::String (get (p, ParamIDs::threshold), 1) + ", mode " + juce::String (get (p, ParamIDs::algorithmMode), 0));
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

// Last: on the old code this is a use-after-free, which crashes or corrupts the heap.
void testLifetime()
{
    for (int i = 0; i < 50; ++i)
    {
        auto p = std::make_unique<HarmonicCompressorAudioProcessor>();
        set (*p, ParamIDs::oversampleMode, (float) (i % 4));
        set (*p, ParamIDs::oversampleFilter, (float) (i % 2));
        set (*p, ParamIDs::algorithmMode, (float) (1 + i % 6));
        p.reset();   // destroy it now, with that work still queued
    }
    pump (50);       // delivers whatever is still queued
    check (true, "F1", "50 processors destroyed with model and oversampling changes queued; message loop ran without a crash");
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool lifetimeOnly = argc > 1 && std::strcmp (argv[1], "--lifetime-only") == 0;
    if (! lifetimeOnly)
    {
        testModeMemory();
        testRestoreRace();
        testLatencyReport();
        testDryPath();
    }
    testLifetime();
    std::cout << checks << " checks, " << failures << " failures" << std::endl;
    return failures == 0 ? 0 : 1;
}
