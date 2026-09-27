#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Modern flat dark look and feel for the HarmonicCompressor UI.

    Vector-drawn rotary knobs (arc + pointer), flat toggle-style buttons with
    a bright accent fill for the selected state, and a matching linear slider
    and toggle-button look. No dropdowns anywhere.
*/
class LookAndFeelCustom : public juce::LookAndFeel_V4
{
public:
    LookAndFeelCustom();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    // Colour palette
    static const juce::Colour bgColour;         // near-black background
    static const juce::Colour panelColour;      // slightly lighter panel
    static const juce::Colour panelHeaderColour;// panel header strip
    static const juce::Colour knobBodyColour;   // knob body (dark metal)
    static const juce::Colour knobRimColour;    // knob rim highlight
    static const juce::Colour accentColour;     // amber accent (value arcs)
    static const juce::Colour accentDimColour;  // translucent accent
    static const juce::Colour textColour;       // light text
    static const juce::Colour mutedTextColour;  // dimmed text
    static const juce::Colour outlineColour;    // subtle outlines

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LookAndFeelCustom)
};
