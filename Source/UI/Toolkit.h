#pragma once
// Everything the plugin takes from GoodLookinUI, with short local names.
#include <goodlookinui/Design.h>
#include <GoodLookinUI.h>
#include <Meters.h>
#include <Switches.h>
#include <Retro.h>
#if GOODLOOKINUI_ENABLE_EDITOR
#include <Studio.h>
#endif

namespace glui = goodlookinui;
namespace gj = goodlookinui::juce_adapter;

// Shared look state: the current faceplate plus a few UI-only switches. Owned by the editor,
// referenced by every component, saved in the plugin state (never touches the DSP).
struct Skin {
    const glui::Faceplate* plate = glui::findFaceplate("Graphite");
    juce::Colour bottom() const { return juce::Colour(plate->panelBottom); }
    juce::Colour text() const { return juce::Colour(plate->text); }
    juce::Colour mid() const { return juce::Colour(plate->textMid); }
    juce::Colour accent() const { return juce::Colour(plate->accent); }
    juce::Colour backdrop() const { return juce::Colour(plate->panelTop).interpolatedWith(juce::Colour(plate->panelBottom), 0.5f); }
    int finish() const { return plate->finish; }
    int meterFace = 0;    // 0 cream VU, 1 black VU, 2 LED ladders
    int pushStyle = 0;    // switches::PushStyle
};
