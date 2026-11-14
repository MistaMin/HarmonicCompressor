// processBlock cost per oversampling mode, 1.5.1 parameter set only, so the same file times the previous
// source and the current one on the same machine. Conditions follow dsp/evidence/bench_matrix.txt:
// 48 kHz, 256-sample blocks, stereo, Red model compressing, ADAA harmonics (20 % H2, 20 % H3), transient
// designer on. Median of 3000 blocks after 300 warm-up blocks; the best of 5 runs is reported.
#include "PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

namespace
{
void set (HarmonicCompressorAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

double medianMs (int os, int filter, float mix)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    HarmonicCompressorAudioProcessor p;
    set (p, ParamIDs::oversampleMode, (float) os);
    set (p, ParamIDs::oversampleFilter, (float) filter);
    set (p, ParamIDs::threshold, -24.0f);
    set (p, ParamIDs::harmonic2, 20.0f);
    set (p, ParamIDs::harmonic3, 20.0f);
    set (p, ParamIDs::transientEnabled, 1.0f);
    set (p, ParamIDs::transientAttack, 30.0f);
    set (p, ParamIDs::mix, mix);
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);

    std::mt19937 rng (3);
    std::uniform_real_distribution<float> u (-0.25f, 0.25f);
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    std::vector<double> times;
    for (int i = 0; i < 3300; ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < block; ++s)
                buffer.setSample (ch, s, u (rng));
        const auto t0 = std::chrono::steady_clock::now();
        p.processBlock (buffer, midi);
        const auto t1 = std::chrono::steady_clock::now();
        if (i >= 300)
            times.push_back (std::chrono::duration<double, std::milli> (t1 - t0).count());
    }
    std::nth_element (times.begin(), times.begin() + (long) times.size() / 2, times.end());
    return times[times.size() / 2];
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const char* names[] = { "1x", "2x Min", "2x Lin", "4x Min", "4x Lin", "8x Min", "8x Lin" };
    const int modes[][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 2, 1 }, { 3, 0 }, { 3, 1 } };
    const double deadlineMs = 256.0 / 48.0;
    for (int m = 0; m < 7; ++m)
    {
        double best = 1.0e9;
        for (int run = 0; run < 5; ++run)
            best = std::min (best, medianMs (modes[m][0], modes[m][1], 0.5f));
        std::cout << "BENCH " << names[m] << " " << juce::String (best, 4) << " ms/block "
                  << juce::String (100.0 * best / deadlineMs, 2) << " % of deadline" << std::endl;
    }
    return 0;
}
