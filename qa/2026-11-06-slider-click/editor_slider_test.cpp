#include "PluginEditor.h"
#include <iostream>

namespace
{
juce::MouseEvent eventAt (Knob& slider, juce::Point<float> position,
                          juce::Point<float> start, bool dragged = false)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), position,
             juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
             0, 0, 0, 0, 0, &slider, &slider, now, start, now, 1, dragged };
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    HarmonicCompressorAudioProcessor processor;
    int checks = 0, failures = 0;
    // Reopening the editor rebuilds every SliderAttachment and track layout.
    for (int reopen = 0; reopen < 2; ++reopen)
    {
        HarmonicCompressorAudioProcessorEditor editor (processor);
        editor.getCompressionModule().setOpen (true, false);
        editor.getHarmonicsModule().setOpen (true, false);
        editor.getSidechainModule().setOpen (true, false);
        editor.getTransientModule().setOpen (true, false);
        for (int theme = 0; theme < 4; ++theme)
        {
            editor.setColourMode (theme);
            // Theme application restores the design positions. Spread the panels
            // afterwards so the snapshots show every fader without overlap.
            editor.getCompressionModule().setTopLeftPosition (20, 20);
            editor.getHarmonicsModule().setTopLeftPosition (20, 150);
            editor.getSidechainModule().setTopLeftPosition (20, 320);
            editor.getTransientModule().setTopLeftPosition (20, 440);
            for (auto* parameter : processor.getParameters())
            {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
                auto* slider = ranged != nullptr ? editor.findControl (ranged->paramID.toStdString()) : nullptr;
                if (slider == nullptr || slider->isRotaryKnob())
                    continue;

                const auto track = slider->getTrackBounds().toFloat().reduced (6.0f, 0.0f);
                const auto point = [&] (float p) {
                    return juce::Point<float> (track.getX() + p * track.getWidth(), track.getCentreY());
                };
                const auto check = [&] (float expected, const char* action) {
                    // Compare the host-normalised parameter value to the snapped
                    // expected value, including skewed and stepped ranges.
                    const auto snapped = ranged->convertTo0to1 (ranged->convertFrom0to1 (expected));
                    const bool pass = std::abs (ranged->getValue() - snapped) < 0.002f;
                    ++checks;
                    failures += pass ? 0 : 1;
                    std::cout << (pass ? "PASS " : "FAIL ") << ranged->paramID
                              << " theme=" << theme << " reopen=" << reopen
                              << " " << action << " host=" << ranged->getValue() << '\n';
                };

                for (float p : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.5f, 0.0f })
                {
                    const auto e = eventAt (*slider, point (p), point (p));
                    slider->mouseDown (e);
                    slider->mouseUp (e);
                    check (p, "click");
                }
                const auto start = point (1.0f);
                slider->mouseDown (eventAt (*slider, start, start));
                for (float p : { 0.75f, 0.5f, 0.25f, 0.0f })
                {
                    slider->mouseDrag (eventAt (*slider, point (p), start, true));
                    check (p, "drag away from maximum");
                }
                slider->mouseUp (eventAt (*slider, point (0.0f), start, true));
            }
            if (argc > 1 && reopen == 0)
            {
                const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
                juce::File file (juce::String (argv[1]) + "/editor-theme-" + juce::String (theme) + ".png");
                auto stream = file.createOutputStream();
                if (stream != nullptr)
                {
                    stream->setPosition (0);
                    stream->truncate();
                    juce::PNGImageFormat().writeImageToStream (image, *stream);
                }
            }
        }
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
