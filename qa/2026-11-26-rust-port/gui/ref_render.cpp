// Renders 1.5.3's editor in each state of scenarios.txt to <outDir>/<name>.png, the reference the
// OpenPluginSDK port's editor is compared with (compare.py).
// Usage: GuiReferenceRender <scenarios.txt> <outDir>
#if defined (__APPLE__)
 #include <CoreFoundation/CoreFoundation.h> // before JUCE: its 'using namespace juce' makes MacTypes' Point ambiguous
#endif
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>

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
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: GuiReferenceRender <scenarios.txt> <outDir>\n";
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI gui;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const juce::File outDir (cwd.getChildFile (argv[2]));
    outDir.createDirectory();

    std::ifstream in (cwd.getChildFile (argv[1]).getFullPathName().toStdString());
    std::string line;
    while (std::getline (in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream words (line);
        std::string name, word;
        words >> name;

        HarmonicCompressorAudioProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        float scale = 2.0f;
        juce::StringArray open;
        while (words >> word)
        {
            const auto eq = word.find ('=');
            const auto key = juce::String (word.substr (0, eq));
            const auto value = juce::String (word.substr (eq + 1));
            if (key == "scale")
                scale = value.getFloatValue();
            else if (key == "open")
                open.addTokens (value, ",", "");
            else if (key.startsWith ("ui."))
                p.apvts.state.setProperty (key.substring (3), value.getIntValue(), nullptr);
            else if (auto* param = p.apvts.getParameter (key))
                param->setValueNotifyingHost (param->convertTo0to1 (value.getFloatValue()));
            else
                std::cerr << name << ": unknown setting " << word << '\n';
        }

        HarmonicCompressorAudioProcessorEditor editor (p);
        if (open.contains ("comp"))  editor.getCompressionModule().setOpen (true);
        if (open.contains ("harm"))  editor.getHarmonicsModule().setOpen (true);
        if (open.contains ("sc"))    editor.getSidechainModule().setOpen (true);
        if (open.contains ("trans")) editor.getTransientModule().setOpen (true);
        pump (100);

        const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, scale);
        juce::File file (outDir.getChildFile (juce::String (name) + ".png"));
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::cout << name << " " << image.getWidth() << "x" << image.getHeight() << '\n';

        // GUI_TIME=1: CPU time of a whole-editor repaint and of the history field's repaint (what its
        // 30 Hz timer redraws), at this state's scale
        if (std::getenv ("GUI_TIME") != nullptr)
        {
            const auto cpu = [] { timespec t; clock_gettime (CLOCK_THREAD_CPUTIME_ID, &t); return (double) t.tv_sec + (double) t.tv_nsec * 1e-9; };
            constexpr int n = 30;
            auto t0 = cpu();
            for (int i = 0; i < n; ++i)
                juce::ignoreUnused (editor.createComponentSnapshot (editor.getLocalBounds(), true, scale));
            const double full = (cpu() - t0) * 1000.0 / n;
            t0 = cpu();
            for (int i = 0; i < n; ++i)
                juce::ignoreUnused (editor.getScope().createComponentSnapshot (editor.getScope().getLocalBounds(), true, scale));
            const double field = (cpu() - t0) * 1000.0 / n;
            std::cout << "  cpu ms per repaint: whole " << full << ", history field " << field << '\n';
        }
    }
    return 0;
}
