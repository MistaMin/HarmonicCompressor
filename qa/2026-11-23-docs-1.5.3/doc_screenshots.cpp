// Renders the user-manual pictures that show the Optical model, with the settings read from the 1.5.2
// pictures, so they show the 1.5.3 transfer curve (hard knee outside Red). Each shot runs decaying tone
// bursts through the processor in real time so the meters and the 6 s history fill as in a host.
// Usage: DocScreenshotRender <outDir>
#if defined (__APPLE__)
 #include <CoreFoundation/CoreFoundation.h> // before JUCE: its 'using namespace juce' makes MacTypes' Point ambiguous
#endif
#include "PluginEditor.h"
#include "PluginProcessor.h"
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
    int oversample, filter;
    bool modules; // the "modules" picture: all four open, harmonics, sidechain and transient settings
    bool audio;
    float scale;
};

void run (HarmonicCompressorAudioProcessor& p, double seconds)
{
    constexpr int block = 512;
    const double sr = 48000.0, period = 0.85 * sr;
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    long long n = 0;
    for (int i = 0; i < (int) (seconds * sr / block); ++i)
    {
        for (int s = 0; s < block; ++s, ++n)
        {
            const double t = std::fmod ((double) n, period) / sr;
            const double env = 0.035 + 0.085 * std::exp (-t / 0.15); // peaks near 0 dBu, as in the 1.5.2 pictures
            const float v = (float) (env * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) n / sr));
            buffer.setSample (0, s, v);
            buffer.setSample (1, s, v);
        }
        p.processBlock (buffer, midi);
        pump ((int) (1000.0 * block / sr));
    }
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: DocScreenshotRender <outDir>\n";
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    outDir.createDirectory();

    const Shot shots[] = {
        { "soft-white", 1, -22.0f, 5.0f, 0.0f, 1.00f, 1, 0, false, true,  2.0f },
        { "grid-0",     0, -22.0f, 5.0f, 0.0f, 1.00f, 1, 0, false, true,  2.0f },
        { "grid-1",     1, -22.0f, 5.0f, 0.0f, 1.00f, 1, 0, false, true,  2.0f },
        { "grid-2",     2, -22.0f, 5.0f, 0.0f, 1.00f, 1, 0, false, true,  2.0f },
        { "grid-3",     3, -22.0f, 5.0f, 0.0f, 1.00f, 1, 0, false, true,  2.0f },
        { "modules",    1, -20.0f, 6.0f, 4.0f, 0.85f, 2, 1, true,  true,  2.0f },
        { "reaper",     0, -18.0f, 4.0f, 0.0f, 1.00f, 1, 0, false, false, 1.0f },
    };

    for (const auto& s : shots)
    {
        HarmonicCompressorAudioProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        HarmonicCompressorAudioProcessorEditor editor (p);
        set (p, ParamIDs::algorithmMode, 5.0f); // Optical
        set (p, ParamIDs::threshold, s.threshold);
        set (p, ParamIDs::ratio, s.ratio);
        set (p, ParamIDs::makeupGain, s.makeup);
        set (p, ParamIDs::mix, s.mix);
        set (p, ParamIDs::oversampleMode, (float) s.oversample);
        set (p, ParamIDs::oversampleFilter, (float) s.filter);
        if (s.modules)
        {
            set (p, ParamIDs::inputGain, 1.5f);
            set (p, ParamIDs::outputGain, -1.0f);
            set (p, ParamIDs::harmonic2, 35.0f);
            set (p, ParamIDs::harmonic4, 15.0f);
            set (p, ParamIDs::scEnabled, 1.0f);
            set (p, ParamIDs::scHPFreq, 150.0f);
            set (p, ParamIDs::transientEnabled, 1.0f);
            set (p, ParamIDs::transientAttack, 35.0f);
            set (p, ParamIDs::transientSustain, -15.0f);
            set (p, ParamIDs::transientSpeed, 10.0f);
            editor.getCompressionModule().setTopLeftPosition (16, 13);
            editor.getHarmonicsModule().setTopLeftPosition (436, 13);
            editor.getSidechainModule().setTopLeftPosition (16, 521);
            editor.getTransientModule().setTopLeftPosition (419, 518);
            for (auto* m : { &editor.getCompressionModule(), &editor.getHarmonicsModule(),
                             &editor.getSidechainModule(), &editor.getTransientModule() })
                m->setOpen (true);
        }
        editor.setColourMode (s.theme);
        pump (200);
        if (s.audio)
            run (p, 8.0);
        else
            pump (500);
        const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, s.scale);
        juce::File file (outDir.getChildFile (juce::String ("editor-") + s.name + ".png"));
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::cout << s.name << " " << image.getWidth() << "x" << image.getHeight() << '\n';
    }
    return 0;
}
