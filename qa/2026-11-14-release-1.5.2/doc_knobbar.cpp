// Renders the editor's knob bar (Threshold, Ratio, Makeup, Mix, AUTO GAIN, DELTA) with the settings and
// colour theme shown in each user-manual screenshot, so the screenshots can be updated to the 1.5.2
// knob bar without re-recording their live-audio scope and meters. The knob bar depends only on these
// parameters and the theme. compose_doc_screenshots.py pastes the crops into Docs/*.png.
// Usage: DocKnobBarRender <outDir>
#if defined (__APPLE__)
 #include <CoreFoundation/CoreFoundation.h> // before JUCE: its 'using namespace juce' makes MacTypes' Point ambiguous
#endif
#include "PluginEditor.h"
#include <iostream>

namespace
{
void pump (int ms)
{
#if JUCE_MAC
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

struct Shot
{
    const char* name;
    int theme;
    float threshold, ratio, makeup, mix;
    float scale;
};
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: DocKnobBarRender <outDir>\n";
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    outDir.createDirectory();

    // Values read from the existing screenshots (Docs/*.png).
    const Shot shots[] = {
        { "soft-white",       1, -22.0f, 5.0f, 0.0f, 1.00f, 2.0f },
        { "modules",          1, -20.0f, 6.0f, 4.0f, 0.85f, 2.0f },
        { "grid-0",           0, -22.0f, 5.0f, 0.0f, 1.00f, 2.0f },
        { "grid-1",           1, -22.0f, 5.0f, 0.0f, 1.00f, 2.0f },
        { "grid-2",           2, -22.0f, 5.0f, 0.0f, 1.00f, 2.0f },
        { "grid-3",           3, -22.0f, 5.0f, 0.0f, 1.00f, 2.0f },
        { "reaper",           0, -18.0f, 4.0f, 0.0f, 1.00f, 1.0f },
    };

    HarmonicCompressorAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    HarmonicCompressorAudioProcessorEditor editor (p);
    pump (100);
    for (const auto& s : shots)
    {
        editor.setColourMode (s.theme);
        set (p, ParamIDs::threshold, s.threshold);
        set (p, ParamIDs::ratio, s.ratio);
        set (p, ParamIDs::makeupGain, s.makeup);
        set (p, ParamIDs::mix, s.mix);
        set (p, ParamIDs::autoGain, 0.0f);
        set (p, ParamIDs::delta, 0.0f);
        pump (150);
        const auto bar = editor.getAutoGainKey().getParentComponent()->getBoundsInParent();
        const auto image = editor.createComponentSnapshot (bar, true, s.scale);
        juce::File file (outDir.getChildFile (juce::String ("knobbar-") + s.name + ".png"));
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::cout << s.name << " bar=" << bar.toString() << " image=" << image.getWidth() << "x" << image.getHeight() << '\n';
    }

    // Full editor with audio running, for the metering screenshot: Soft White, Auto Gain on, decaying
    // tone bursts every 0.75 s fed in real time so the meters and the 6 s history fill as in a host.
    editor.setColourMode (1);
    set (p, ParamIDs::threshold, -14.0f);
    set (p, ParamIDs::ratio, 5.0f);
    set (p, ParamIDs::makeupGain, 0.0f);
    set (p, ParamIDs::mix, 1.0f);
    set (p, ParamIDs::autoGain, 1.0f);
    pump (100);
    constexpr int block = 512;
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    long long n = 0;
    const double sr = 48000.0, period = 0.75 * sr;
    for (int i = 0; i < (int) (8.0 * sr / block); ++i)
    {
        for (int s = 0; s < block; ++s, ++n)
        {
            const double t = std::fmod ((double) n, period) / sr;
            const double env = 0.08 + 0.55 * std::exp (-t / 0.12);
            const float v = (float) (env * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) n / sr));
            buffer.setSample (0, s, v);
            buffer.setSample (1, s, v);
        }
        p.processBlock (buffer, midi);
        pump ((int) (1000.0 * block / sr));
    }
    const auto full = editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f);
    juce::File file (outDir.getChildFile ("editor-metering-autogain.png"));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat().writeImageToStream (full, stream);
    std::cout << "metering applied=" << p.autoGainAppliedDb.load() << " dB\n";
    return 0;
}
