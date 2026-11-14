// Auto Gain and Delta verification against the real processor and editor.
// Usage: AutoGainDeltaTest [snapshotDir]
// Ledger: qa/2026-11-14-autogain-delta/ledger.md (row IDs in brackets in the output).
#if defined (__APPLE__)
 #include <CoreFoundation/CoreFoundation.h> // before JUCE: its 'using namespace juce' makes MacTypes' Point ambiguous
#endif
#include "PluginEditor.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

namespace
{
// Runs the message loop for a while (button clicks and editor timers are delivered as messages).
// This build has JUCE modal loops disabled, so the platform run loop is pumped directly.
constexpr bool canPumpMessages = JUCE_MAC != 0;
void pump (int ms)
{
#if JUCE_MAC
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, ms / 1000.0, false);
#else
    juce::ignoreUnused (ms);
#endif
}

int checks = 0, failures = 0;

void check (bool pass, const juce::String& row, const juce::String& what)
{
    ++checks;
    failures += pass ? 0 : 1;
    std::cout << (pass ? "PASS " : "FAIL ") << "[" << row << "] " << what << '\n';
}

using Stereo = std::array<std::vector<float>, 2>;

//------------------------------------------------------------------------------
// Independent loudness meter. At 48 kHz it uses the BS.1770-4 published coefficient table, not the
// plugin's design code; at other rates it uses the plugin design (verified against the table first).
struct Biquad { double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };

std::pair<Biquad, Biquad> kFilters (double sr)
{
    if (sr == 48000.0)
        return { Biquad { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 },
                 Biquad { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 } };
    const auto c = hcdsp::KWeightingCoefficients::design (sr);
    return { Biquad { c.shelfB[0], c.shelfB[1], c.shelfB[2], c.shelfA[1], c.shelfA[2] },
             Biquad { c.hpB[0], c.hpB[1], c.hpB[2], c.hpA[1], c.hpA[2] } };
}

// Integrated (ungated) K-weighted loudness of [from, to) seconds; filters run from t = 0.
double lufs (const Stereo& x, double sr, double from, double to, int channels = 2)
{
    double sum = 0.0;
    const size_t a = (size_t) (from * sr), b = std::min (x[0].size(), (size_t) (to * sr));
    for (int ch = 0; ch < channels; ++ch)
    {
        auto [s1, s2] = kFilters (sr);
        for (size_t i = 0; i < b; ++i)
        {
            const double y = s2.run (s1.run (x[(size_t) ch][i]));
            if (i >= a) sum += y * y;
        }
    }
    return -0.691 + 10.0 * std::log10 (std::max (sum / double (b - a), 1e-20));
}

double rmsDb (const Stereo& x, double sr, double from, double to)
{
    double sum = 0.0;
    const size_t a = (size_t) (from * sr), b = std::min (x[0].size(), (size_t) (to * sr));
    for (int ch = 0; ch < 2; ++ch)
        for (size_t i = a; i < b; ++i) sum += (double) x[(size_t) ch][i] * x[(size_t) ch][i];
    return 10.0 * std::log10 (std::max (sum / double (2 * (b - a)), 1e-30));
}

// Program-like test signal: decaying noise bursts (drum hits), a 55 Hz bass line and a 440 Hz tone.
Stereo programSignal (double sr, double seconds, float levelDb = -12.0f)
{
    Stereo x;
    const size_t n = (size_t) (seconds * sr);
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    const float g = juce::Decibels::decibelsToGain (levelDb);
    for (int ch = 0; ch < 2; ++ch)
    {
        x[(size_t) ch].resize (n);
        for (size_t i = 0; i < n; ++i)
        {
            const double t = double (i) / sr;
            const double hitT = std::fmod (t, 0.25);
            const double env = std::exp (-hitT / 0.045);
            const double bass = 0.45 * std::sin (2.0 * juce::MathConstants<double>::pi * 55.0 * t);
            const double tone = 0.20 * std::sin (2.0 * juce::MathConstants<double>::pi * (440.0 + ch) * t);
            x[(size_t) ch][i] = g * float (env * noise (rng) + bass + tone);
        }
    }
    return x;
}

struct Rig
{
    explicit Rig (double sampleRate = 48000.0, int block = 512) : sr (sampleRate), blockSize (block)
    {
        p = std::make_unique<HarmonicCompressorAudioProcessor>();
        p->setRateAndBufferSizeDetails (sr, blockSize);
        p->prepareToPlay (sr, blockSize);
    }
    void set (const char* id, float value)
    {
        auto* param = p->apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
    Stereo run (const Stereo& in, std::vector<float>* autoGainDbPerBlock = nullptr,
                std::function<void (size_t)> beforeBlock = {})
    {
        Stereo out { std::vector<float> (in[0].size()), std::vector<float> (in[0].size()) };
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        for (size_t pos = 0; pos < in[0].size(); pos += (size_t) blockSize)
        {
            if (beforeBlock) beforeBlock (pos);
            const int n = (int) std::min ((size_t) blockSize, in[0].size() - pos);
            buffer.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                buffer.copyFrom (ch, 0, in[(size_t) ch].data() + pos, n);
            p->processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                std::copy (buffer.getReadPointer (ch), buffer.getReadPointer (ch) + n, out[(size_t) ch].data() + pos);
            if (autoGainDbPerBlock != nullptr)
                autoGainDbPerBlock->push_back (p->autoGainAppliedDb.load());
        }
        return out;
    }
    double sr;
    int blockSize;
    std::unique_ptr<HarmonicCompressorAudioProcessor> p;
};

double magnitudeDb (const double* b, const double* a, double f, double sr)
{
    const std::complex<double> z = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * f / sr);
    const auto h = (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z);
    return 20.0 * std::log10 (std::abs (h));
}

double kResponseDb (double f, double sr)
{
    const auto c = hcdsp::KWeightingCoefficients::design (sr);
    return magnitudeDb (c.shelfB, c.shelfA, f, sr) + magnitudeDb (c.hpB, c.hpA, f, sr);
}

double maxStep (const Stereo& y, double sr, double from, double to)
{
    double m = 0.0;
    for (size_t i = (size_t) (from * sr) + 1; i < (size_t) (to * sr); ++i)
        m = std::max (m, (double) std::abs (y[0][i] - y[0][i - 1]));
    return m;
}

juce::String db (double v) { return juce::String (v, 2) + " dB"; }

//------------------------------------------------------------------------------
void testCoefficients()
{
    const auto c = hcdsp::KWeightingCoefficients::design (48000.0);
    const double table[10] = { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585,
                               1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
    const double ours[10] = { c.shelfB[0], c.shelfB[1], c.shelfB[2], c.shelfA[1], c.shelfA[2],
                              c.hpB[0], c.hpB[1], c.hpB[2], c.hpA[1], c.hpA[2] };
    double worst = 0.0;
    for (int i = 0; i < 10; ++i) worst = std::max (worst, std::abs (table[i] - ours[i]));
    check (worst < 1.0e-8, "S8", "K-weighting design at 48 kHz matches BS.1770-4 table, max |diff| = " + juce::String (worst, 12));

    for (double sr : { 44100.0, 96000.0, 192000.0 })
    {
        double dev = 0.0;
        for (double f : { 20.0, 40.0, 100.0, 1000.0, 2000.0, 4000.0, 10000.0 })
            dev = std::max (dev, std::abs (kResponseDb (f, sr) - kResponseDb (f, 48000.0)));
        check (dev < 0.05, "S8", "K-weighting response at " + juce::String (sr / 1000.0, 1)
                               + " kHz within 0.05 dB of 48 kHz (20 Hz..10 kHz), max dev = " + db (dev));
    }

    // Meter sanity (BS.1770 calibration): 0 dBFS 997 Hz sine on one channel reads -3.01 LUFS.
    Stereo sine { std::vector<float> (48000 * 3), std::vector<float> (48000 * 3) };
    for (size_t i = 0; i < sine[0].size(); ++i)
        sine[0][i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * 997.0 * double (i) / 48000.0);
    const double l = lufs (sine, 48000.0, 1.0, 3.0);
    check (std::abs (l + 3.01) < 0.05, "S8", "test meter: 0 dBFS 997 Hz one channel = " + juce::String (l, 3) + " LUFS (expected -3.01)");
}

struct Scenario { const char* name; std::vector<std::pair<const char*, float>> params; };

void testLoudnessMatch()
{
    const std::vector<Scenario> scenarios {
        { "heavy compression (thr -30, 8:1)",          { { ParamIDs::threshold, -30.0f }, { ParamIDs::ratio, 8.0f } } },
        { "input drive +12 dB (thr -18, 4:1)",         { { ParamIDs::inputGain, 12.0f }, { ParamIDs::threshold, -18.0f } } },
        { "makeup +12 dB (thr -24, 4:1)",              { { ParamIDs::makeupGain, 12.0f }, { ParamIDs::threshold, -24.0f } } },
        { "saturation (3rd 100%, 5th 60%, makeup +6)", { { ParamIDs::harmonic3, 100.0f }, { ParamIDs::harmonic5, 60.0f }, { ParamIDs::makeupGain, 6.0f } } },
        { "transient attack +100%, mix 50%",           { { ParamIDs::transientEnabled, 1.0f }, { ParamIDs::transientAttack, 100.0f }, { ParamIDs::mix, 0.5f }, { ParamIDs::threshold, -30.0f } } },
    };

    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        const auto in = programSignal (sr, 8.0);
        const double inL = lufs (in, sr, 5.0, 8.0);
        for (const auto& sc : scenarios)
        {
            if (sr != 48000.0 && std::string (sc.name).find ("heavy") == std::string::npos
                              && std::string (sc.name).find ("saturation") == std::string::npos)
                continue;
            double offDiff = 0.0, onDiff = 0.0, onRmsDiff = 0.0;
            for (bool autoGain : { false, true })
            {
                Rig rig (sr);
                for (auto& [id, v] : sc.params) rig.set (id, v);
                rig.set (ParamIDs::autoGain, autoGain ? 1.0f : 0.0f);
                const auto out = rig.run (in);
                (autoGain ? onDiff : offDiff) = lufs (out, sr, 5.0, 8.0) - inL;
                if (autoGain) onRmsDiff = rmsDb (out, sr, 5.0, 8.0) - rmsDb (in, sr, 5.0, 8.0);
            }
            const auto rowS = std::string (sc.name).find ("heavy") != std::string::npos ? "S3" : "S4";
            check (std::abs (onDiff) <= 0.5, sr == 48000.0 ? rowS : "S8",
                   juce::String (sr / 1000.0, 1) + " kHz " + sc.name + ": output-input loudness off "
                   + juce::String (offDiff, 2) + " LU -> on " + juce::String (onDiff, 2) + " LU (RMS diff while matched "
                   + db (onRmsDiff) + ")");
        }
    }

    // Oversampling latency per mode (the dry path realises fractional latency by linear interpolation).
    {
        const char* names[] = { "1x", "2x Min", "2x Lin", "4x Min", "4x Lin", "8x Min", "8x Lin" };
        const int modes[][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 2, 1 }, { 3, 0 }, { 3, 1 } };
        juce::String line ("INFO latency (samples):");
        for (int i = 0; i < 7; ++i)
        {
            Rig rig;
            rig.set (ParamIDs::oversampleMode, (float) modes[i][0]);
            rig.set (ParamIDs::oversampleFilter, (float) modes[i][1]);
            rig.p->prepareToPlay (48000.0, 512);
            pump (5);
            line << " " << names[i] << "=" << juce::String (rig.p->getLatencySamples());
        }
        std::cout << line << '\n';

        hcdsp::MultirateEngine engine;
        engine.prepare (48000.0, 512, 2);
        juce::String exact ("INFO engine latency (fractional samples):");
        for (int i = 0; i < 7; ++i)
            exact << " " << names[i] << "=" << juce::String (engine.getLatencySamplesFor ((hcdsp::OversampleMode) modes[i][0],
                                                                                        (hcdsp::ResamplerFilter) modes[i][1]), 3);
        std::cout << exact << '\n';
    }

    // Loudness match must not depend on the oversampling mode.
    for (int os = 0; os < 4; ++os)
        for (int f = 0; f < (os == 0 ? 1 : 2); ++f)
        {
            Rig rig;
            rig.set (ParamIDs::oversampleMode, (float) os);
            rig.set (ParamIDs::oversampleFilter, (float) f);
            rig.p->prepareToPlay (48000.0, 512);
            rig.set (ParamIDs::threshold, -30.0f);
            rig.set (ParamIDs::ratio, 8.0f);
            rig.set (ParamIDs::autoGain, 1.0f);
            const auto out = rig.run (programSignal (48000.0, 8.0));
            const double d = lufs (out, 48000.0, 5.0, 8.0) - lufs (programSignal (48000.0, 8.0), 48000.0, 5.0, 8.0);
            check (std::abs (d) <= 0.5, "S3", "oversample mode " + juce::String (os) + " filter " + juce::String (f)
                                              + ": matched within " + juce::String (d, 2) + " LU");
        }

    // Every compressor model, heavy compression.
    const auto in = programSignal (48000.0, 8.0);
    const double inL = lufs (in, 48000.0, 5.0, 8.0);
    const char* models[] = { "Red", "Hybrid", "Bridge", "Bus VCA", "76", "Opto Leveler", "VCA 160" };
    for (int m = 0; m < 7; ++m)
    {
        Rig rig;
        rig.set (ParamIDs::algorithmMode, (float) m);
        rig.set (ParamIDs::threshold, -30.0f);
        rig.set (ParamIDs::ratio, 8.0f);
        rig.set (ParamIDs::autoGain, 1.0f);
        std::vector<float> g;
        const auto out = rig.run (in, &g);
        const double d = lufs (out, 48000.0, 5.0, 8.0) - inL;
        const bool clamped = g.back() >= hcdsp::LoudnessMatcher::maxGainDb - 0.01f;
        Rig offRig;
        offRig.set (ParamIDs::algorithmMode, (float) m);
        offRig.set (ParamIDs::threshold, -30.0f);
        offRig.set (ParamIDs::ratio, 8.0f);
        const double off = lufs (offRig.run (in), 48000.0, 5.0, 8.0) - inL;
        check (std::abs (d) <= 0.5 || (clamped && off < -hcdsp::LoudnessMatcher::maxGainDb), "S3",
               juce::String ("model ") + models[m] + ": off " + juce::String (off, 2) + " LU, on " + juce::String (d, 2)
               + " LU, applied " + db (g.back()) + (clamped ? " (at the +24 dB clamp)" : ""));
    }
}

// Why loudness and not RMS: saturation moves energy to harmonics the ear (and K-weighting) rates
// differently from the fundamental. An RMS match would leave this much loudness error.
void infoRmsVersusLoudness()
{
    const double sr = 48000.0;
    struct Source { const char* name; double freq; };
    for (const Source src : { Source { "55 Hz bass", 55.0 }, Source { "200 Hz", 200.0 }, Source { "1 kHz", 1000.0 }, Source { "program", 0.0 } })
    {
        Stereo in = src.freq > 0.0 ? Stereo { std::vector<float> ((size_t) (sr * 8)), std::vector<float> ((size_t) (sr * 8)) }
                                   : programSignal (sr, 8.0);
        if (src.freq > 0.0)
            for (size_t i = 0; i < in[0].size(); ++i)
                in[0][i] = in[1][i] = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * src.freq * double (i) / sr);
        Rig rig;
        rig.set (ParamIDs::harmonic3, 100.0f);
        rig.set (ParamIDs::harmonic5, 100.0f);
        rig.set (ParamIDs::makeupGain, 12.0f);   // drive the saturation hard
        rig.set (ParamIDs::threshold, -24.0f);
        rig.set (ParamIDs::autoGain, 1.0f);
        const auto out = rig.run (in);
        const double loudErr = lufs (out, sr, 5.0, 8.0) - lufs (in, sr, 5.0, 8.0);
        const double rmsDiff = rmsDb (out, sr, 5.0, 8.0) - rmsDb (in, sr, 5.0, 8.0);
        // An RMS matcher would apply -rmsDiff more gain; its loudness error is therefore loudErr - rmsDiff.
        std::cout << "INFO saturated " << src.name << ": loudness match error " << juce::String (loudErr, 2)
                  << " LU; an RMS match would leave the output " << juce::String (loudErr - rmsDiff, 2) << " LU from the input\n";
    }
}

void testSilenceHoldAndClamp()
{
    const double sr = 48000.0;
    auto sig = programSignal (sr, 6.0);
    Stereo in = sig;
    for (auto& ch : in) ch.resize ((size_t) (sr * 12.0), 0.0f);           // 6 s of digital silence
    for (int ch = 0; ch < 2; ++ch)                                       // -80 dBFS hiss for the last 2 s
    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        for (size_t i = (size_t) (sr * 10.0); i < in[(size_t) ch].size(); ++i) in[(size_t) ch][i] = 1.0e-4f * u (rng);
    }
    Rig rig;
    rig.set (ParamIDs::threshold, -30.0f);
    rig.set (ParamIDs::ratio, 8.0f);
    rig.set (ParamIDs::autoGain, 1.0f);
    std::vector<float> g;
    rig.run (in, &g);
    const auto at = [&] (double t) { return g[(size_t) (t * sr / rig.blockSize)]; };
    const double drift = std::max (std::abs (at (11.9) - at (5.9)), std::abs (at (9.0) - at (5.9)));
    check (drift < 0.1, "S5", "gain through 6 s silence then -80 dBFS hiss: " + db (at (5.9)) + " -> " + db (at (11.9))
                               + " (drift " + db (drift) + ")");

    Rig clampRig;
    clampRig.set (ParamIDs::inputGain, -24.0f);
    clampRig.set (ParamIDs::makeupGain, -12.0f);
    clampRig.set (ParamIDs::threshold, 0.0f);
    clampRig.set (ParamIDs::ratio, 1.0f);
    clampRig.set (ParamIDs::autoGain, 1.0f);
    std::vector<float> cg;
    clampRig.run (programSignal (sr, 6.0), &cg);
    check (std::abs (cg.back() - 24.0f) < 0.05f, "S6", "-36 dB of processing loss clamps at +24 dB: applied " + db (cg.back()));
}

void testTransitions()
{
    const double sr = 48000.0;
    Stereo sine { std::vector<float> ((size_t) (sr * 8)), std::vector<float> ((size_t) (sr * 8)) };
    for (size_t i = 0; i < sine[0].size(); ++i)
        sine[0][i] = sine[1][i] = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 80.0 * double (i) / sr);

    for (const char* id : { ParamIDs::autoGain, ParamIDs::delta })
    {
        Rig rig;
        rig.set (ParamIDs::threshold, -30.0f);
        rig.set (ParamIDs::ratio, 8.0f);
        rig.set (ParamIDs::makeupGain, 6.0f);
        const auto out = rig.run (sine, nullptr, [&] (size_t pos)
        {
            if (pos >= (size_t) (sr * 5.0) && pos < (size_t) (sr * 5.0) + (size_t) rig.blockSize)
                rig.set (id, 1.0f);
        });
        // The steepest slope of the louder of the two states bounds a click-free change.
        const double steady = std::max (maxStep (out, sr, 4.0, 4.9), maxStep (out, sr, 6.5, 7.5));
        const double change = maxStep (out, sr, 4.95, 5.3);
        check (change <= steady * 1.05, id == ParamIDs::autoGain ? "S7" : "S9",
               juce::String (id) + " switched on mid-signal: max sample step " + juce::String (change, 5)
               + " vs steady " + juce::String (steady, 5) + " (no click)");
    }
}

void testDelta()
{
    const double sr = 48000.0;
    const auto in = programSignal (sr, 4.0);
    const char* osNames[] = { "1x", "2x", "4x", "8x" };
    const char* filterNames[] = { "Min Phase", "Lin Phase" };

    // Neutral processing: delta should be silent. Null depth relative to the input.
    for (int os = 0; os < 4; ++os)
        for (int f = 0; f < (os == 0 ? 1 : 2); ++f)
        {
            Rig rig;
            rig.set (ParamIDs::oversampleMode, (float) os);
            rig.set (ParamIDs::oversampleFilter, (float) f);
            rig.p->prepareToPlay (sr, rig.blockSize);
            rig.set (ParamIDs::threshold, 0.0f);
            rig.set (ParamIDs::ratio, 1.0f);
            rig.set (ParamIDs::delta, 1.0f);
            const auto out = rig.run (in);
            const double depth = rmsDb (out, sr, 1.0, 4.0) - rmsDb (in, sr, 1.0, 4.0);
            check (depth < -90.0, "S11",
                   juce::String ("neutral null ") + osNames[os] + (os == 0 ? "" : juce::String (" ") + filterNames[f])
                   + ": delta is " + db (depth) + " relative to input");
        }

    // Identity: delta = normal output - reference, at 1x where the dry path has no delay.
    struct Case { const char* name; bool autoGain; float inputGainDb; const char* row; };
    for (const Case c : { Case { "Auto Gain off, input +6 dB: delta = output - dry after input gain", false, 6.0f, "S9" },
                          Case { "Auto Gain on, input +6 dB: delta = level-matched output - untouched input", true, 6.0f, "S10" } })
    {
        Stereo outputs[2];
        for (int d = 0; d < 2; ++d)
        {
            Rig rig;
            rig.set (ParamIDs::oversampleMode, 0.0f);
            rig.p->prepareToPlay (sr, rig.blockSize);
            rig.set (ParamIDs::threshold, -30.0f);
            rig.set (ParamIDs::ratio, 6.0f);
            rig.set (ParamIDs::harmonic2, 40.0f);
            rig.set (ParamIDs::inputGain, c.inputGainDb);
            rig.set (ParamIDs::autoGain, c.autoGain ? 1.0f : 0.0f);
            rig.set (ParamIDs::delta, d == 1 ? 1.0f : 0.0f);
            outputs[d] = rig.run (in);
        }
        const float refGain = c.autoGain ? 1.0f : juce::Decibels::decibelsToGain (c.inputGainDb);
        double worst = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (size_t i = (size_t) (0.5 * sr); i < in[0].size(); ++i)
                worst = std::max (worst, (double) std::abs (outputs[1][(size_t) ch][i]
                                                            - (outputs[0][(size_t) ch][i] - refGain * in[(size_t) ch][i])));
        check (worst < 1.0e-5, c.row, juce::String (c.name) + ", max error " + juce::String (worst, 9));
    }

    // Output Gain applies after Auto Gain and Delta.
    {
        Stereo outs[2];
        for (int k = 0; k < 2; ++k)
        {
            Rig rig;
            rig.set (ParamIDs::oversampleMode, 0.0f);
            rig.p->prepareToPlay (sr, rig.blockSize);
            rig.set (ParamIDs::threshold, -30.0f);
            rig.set (ParamIDs::autoGain, 1.0f);
            rig.set (ParamIDs::delta, 1.0f);
            rig.set (ParamIDs::outputGain, k == 0 ? 0.0f : -6.0f);
            outs[k] = rig.run (in);
        }
        const double d = rmsDb (outs[1], sr, 1.0, 4.0) - rmsDb (outs[0], sr, 1.0, 4.0);
        check (std::abs (d + 6.0) < 0.01, "S13", "Output Gain -6 dB scales the delta + auto-gained output by " + db (d));
    }

    // Sidechain Listen wins over Delta and freezes Auto Gain.
    {
        Stereo outs[2];
        float applied = 1.0f;
        for (int k = 0; k < 2; ++k)
        {
            Rig rig;
            rig.set (ParamIDs::scListen, 1.0f);
            rig.set (ParamIDs::scEnabled, 1.0f);
            rig.set (ParamIDs::makeupGain, 6.0f);
            rig.set (ParamIDs::autoGain, k == 1 ? 1.0f : 0.0f);
            rig.set (ParamIDs::delta, k == 1 ? 1.0f : 0.0f);
            std::vector<float> g;
            outs[k] = rig.run (in, &g);
            if (k == 1) applied = g.back();
        }
        double worst = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (size_t i = 0; i < in[0].size(); ++i)
                worst = std::max (worst, (double) std::abs (outs[1][(size_t) ch][i] - outs[0][(size_t) ch][i]));
        check (worst < 1.0e-7 && std::abs (applied) < 1.0e-4f, "S12",
               "Sidechain Listen output identical with Delta + Auto Gain on (max diff " + juce::String (worst, 9)
               + ", auto gain applied " + db (applied) + ")");
    }
}

void testParametersAndState()
{
    HarmonicCompressorAudioProcessor p;
    const juce::StringArray v151 { "algorithmMode", "threshold", "ratio", "attack", "release", "knee", "makeupGain", "mix",
                                   "harmonic2", "harmonic3", "harmonic4", "harmonic5", "scEnabled", "scListen", "scHPFreq",
                                   "autoRelease", "autoAttack", "inputGain", "outputGain", "oversampleMode", "oversampleFilter",
                                   "transientEnabled", "transientAttack", "transientSustain", "transientSpeed" };
    juce::StringArray ids;
    for (auto* param : p.getParameters())
        ids.add (dynamic_cast<juce::RangedAudioParameter*> (param)->paramID);
    bool prefix = ids.size() == v151.size() + 2;
    for (int i = 0; prefix && i < v151.size(); ++i) prefix = ids[i] == v151[i];
    check (prefix, "S18", "1.5.1 parameter IDs keep their order; total " + juce::String (ids.size()));
    check (ids[25] == "autoGain" && ids[26] == "delta", "S1", "autoGain appended at index 25, delta at 26");
    check (p.apvts.getParameter (ParamIDs::autoGain)->getValue() < 0.5f
           && p.apvts.getParameter (ParamIDs::delta)->getValue() < 0.5f, "S2", "both default Off");

    p.apvts.getParameter (ParamIDs::autoGain)->setValueNotifyingHost (1.0f);
    p.apvts.getParameter (ParamIDs::delta)->setValueNotifyingHost (1.0f);
    juce::MemoryBlock state;
    p.getStateInformation (state);

    HarmonicCompressorAudioProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (restored.apvts.getParameter (ParamIDs::autoGain)->getValue() > 0.5f
           && restored.apvts.getParameter (ParamIDs::delta)->getValue() > 0.5f, "S17", "state round-trip restores Auto Gain and Delta On");

    // A 1.5.1 session has no PARAM nodes for the new IDs: both must load Off even on an instance where they were On.
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        if (child->getStringAttribute ("id") == "autoGain" || child->getStringAttribute ("id") == "delta")
            xml->removeChildElement (child, true);
        child = next;
    }
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    restored.setStateInformation (oldState.getData(), (int) oldState.getSize());
    check (restored.apvts.getParameter (ParamIDs::autoGain)->getValue() < 0.5f
           && restored.apvts.getParameter (ParamIDs::delta)->getValue() < 0.5f, "S17", "1.5.1 state (no new IDs) loads both Off");
}

void testEditor (const juce::String& snapshotDir)
{
    HarmonicCompressorAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    for (int reopen = 0; reopen < 2; ++reopen)
    {
        HarmonicCompressorAudioProcessorEditor editor (p);
        auto& ag = editor.getAutoGainKey();
        auto& dk = editor.getDeltaKey();
        auto* agParam = p.apvts.getParameter (ParamIDs::autoGain);
        auto* dParam = p.apvts.getParameter (ParamIDs::delta);

        check (ag.isShowing() || (ag.isVisible() && ag.getParentComponent()->isVisible()), "S14", "AUTO GAIN key is visible (reopen " + juce::String (reopen) + ")");
        check (dk.isVisible(), "S15", "DELTA key is visible (reopen " + juce::String (reopen) + ")");

        // Keys must not overlap the narrowed rotary knobs.
        bool clear = true;
        for (const char* id : { ParamIDs::threshold, ParamIDs::ratio, ParamIDs::makeupGain, ParamIDs::mix })
            if (auto* k = editor.findControl (id))
                clear = clear && ! k->getBounds().intersects (ag.getBounds()) && ! k->getBounds().intersects (dk.getBounds())
                              && k->getRight() <= ag.getX();
        check (clear, "S14", "keys do not overlap Threshold/Ratio/Makeup/Mix");

        if (! canPumpMessages)
        {
            std::cout << "SKIP [S16] click and readout checks need a message-loop pump on this platform\n";
            continue;
        }

        // Click writes the parameter; host automation moves the key.
        agParam->setValueNotifyingHost (0.0f);
        dParam->setValueNotifyingHost (0.0f);
        pump (20);
        ag.triggerClick();
        dk.triggerClick();
        pump (50);
        check (agParam->getValue() > 0.5f && dParam->getValue() > 0.5f, "S16", "clicking AUTO GAIN and DELTA turns both parameters On");
        agParam->setValueNotifyingHost (0.0f);
        dParam->setValueNotifyingHost (0.0f);
        pump (50);
        check (! ag.getToggleState() && ! dk.getToggleState(), "S16", "host automation to Off releases both keys");
        agParam->setValueNotifyingHost (1.0f);
        dParam->setValueNotifyingHost (1.0f);
        pump (50);
        check (ag.getToggleState() && dk.getToggleState(), "S16", "host automation to On lights both keys");

        // The applied gain readout.
        p.autoGainAppliedDb.store (3.14f);
        pump (120);
        check (ag.getButtonText() == "AUTO +3.1 dB", "S14", "AUTO GAIN key shows applied gain: '" + ag.getButtonText() + "'");
        p.autoGainAppliedDb.store (-7.06f);
        pump (120);
        check (ag.getButtonText() == "AUTO -7.1 dB", "S14", "negative gain readout: '" + ag.getButtonText() + "'");

        if (snapshotDir.isNotEmpty() && reopen == 0)
            for (int theme = 0; theme < 4; ++theme)
            {
                editor.setColourMode (theme);
                for (bool on : { false, true })
                {
                    agParam->setValueNotifyingHost (on ? 1.0f : 0.0f);
                    dParam->setValueNotifyingHost (on ? 1.0f : 0.0f);
                    p.autoGainAppliedDb.store (on ? 4.2f : 0.0f);
                    pump (120);
                    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f);
                    juce::File file (snapshotDir + "/editor-theme-" + juce::String (theme) + (on ? "-on" : "-off") + ".png");
                    file.deleteFile();
                    juce::FileOutputStream stream (file);
                    juce::PNGImageFormat().writeImageToStream (image, stream);
                }
            }
        agParam->setValueNotifyingHost (0.0f);
        dParam->setValueNotifyingHost (0.0f);
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    testCoefficients();
    testLoudnessMatch();
    infoRmsVersusLoudness();
    testSilenceHoldAndClamp();
    testTransitions();
    testDelta();
    testParametersAndState();
    testEditor (argc > 1 ? juce::String (argv[1]) : juce::String());
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
