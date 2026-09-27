#include "LookAndFeelCustom.h"

#include <cmath>

//==============================================================================
const juce::Colour LookAndFeelCustom::bgColour          (10, 10, 11);
const juce::Colour LookAndFeelCustom::panelColour       (36, 36, 38);
const juce::Colour LookAndFeelCustom::panelHeaderColour (24, 25, 29);
const juce::Colour LookAndFeelCustom::knobBodyColour    (42, 43, 48);
const juce::Colour LookAndFeelCustom::knobRimColour     (66, 67, 74);
const juce::Colour LookAndFeelCustom::accentColour      (255, 176, 44);
const juce::Colour LookAndFeelCustom::accentDimColour   (juce::Colour (255, 176, 44).withAlpha (0.30f));
const juce::Colour LookAndFeelCustom::textColour        (232, 232, 234);
const juce::Colour LookAndFeelCustom::mutedTextColour   (140, 141, 148);
const juce::Colour LookAndFeelCustom::outlineColour     (54, 55, 61);

//==============================================================================
LookAndFeelCustom::LookAndFeelCustom()
{
    setColour (juce::Slider::rotarySliderFillColourId,   accentColour);
    setColour (juce::Slider::rotarySliderOutlineColourId, outlineColour);
    setColour (juce::Slider::textBoxTextColourId,        accentColour);
    setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colour (8, 8, 9));
    setColour (juce::Slider::textBoxHighlightColourId,   accentColour);
    setColour (juce::Slider::textBoxOutlineColourId,     outlineColour);
    setColour (juce::Label::textColourId,                textColour);
    setColour (juce::ToggleButton::textColourId,         textColour);
    setColour (juce::TextButton::textColourOffId,        textColour);
    setColour (juce::TextButton::textColourOnId,         bgColour);
    setColour (juce::TextButton::buttonColourId,         panelColour);
}

//==============================================================================
void LookAndFeelCustom::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    // Flux Pure Compressor style: a black glossy disc, a coloured value arc
    // just outside it, a ring of small tick dots further out, and a short
    // pointer line on top of the disc.
    const bool enabled = slider.isEnabled();
    const float diameter = (float) juce::jmin (width, height);
    const float centreX = (float) x + (float) width  * 0.5f;
    const float centreY = (float) y + (float) height * 0.5f;
    const float angle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPosProportional;
    const juce::Colour arcColour = enabled ? accentColour : mutedTextColour;

    const float outerRadius = diameter * 0.5f - 1.0f;
    const float dotRadius   = outerRadius;
    const float arcRadius   = outerRadius - 3.5f;
    const float discRadius  = arcRadius - 4.0f;

    const float halfPi = juce::MathConstants<float>::halfPi;

    // Dotted tick ring (JUCE angles: CW from north, offset by -pi/2 for cos/sin)
    g.setColour (outlineColour.brighter (0.15f));
    constexpr int numDots = 28;
    for (int i = 0; i < numDots; ++i)
    {
        const float t = (float) i / (float) (numDots - 1);
        const float a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * t;
        const float dx = centreX + std::cos (a - halfPi) * dotRadius;
        const float dy = centreY + std::sin (a - halfPi) * dotRadius;
        g.fillEllipse (dx - 1.0f, dy - 1.0f, 2.0f, 2.0f);
    }

    // Value arc track
    const float arcThickness = juce::jmax (2.0f, diameter * 0.07f);
    juce::Path bgArc;
    bgArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (outlineColour);
    g.strokePath (bgArc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    juce::Path fillArc;
    fillArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (arcColour);
    g.strokePath (fillArc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // Glossy black disc body
    juce::ColourGradient discGrad (juce::Colour (54, 55, 58), centreX, centreY - discRadius,
                                    juce::Colour (8, 8, 9), centreX, centreY + discRadius, false);
    g.setGradientFill (discGrad);
    g.fillEllipse (centreX - discRadius, centreY - discRadius, discRadius * 2.0f, discRadius * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (centreX - discRadius, centreY - discRadius, discRadius * 2.0f, discRadius * 2.0f, 1.0f);

    // Pointer
    const float pointerInner = discRadius * 0.25f;
    const float pointerOuter = discRadius - 1.5f;
    const float x1 = centreX + std::cos (angle - halfPi) * pointerInner;
    const float y1 = centreY + std::sin (angle - halfPi) * pointerInner;
    const float x2 = centreX + std::cos (angle - halfPi) * pointerOuter;
    const float y2 = centreY + std::sin (angle - halfPi) * pointerOuter;
    g.setColour (enabled ? juce::Colours::white : mutedTextColour);
    g.drawLine (x1, y1, x2, y2, juce::jmax (1.2f, diameter * 0.045f));
}

//==============================================================================
void LookAndFeelCustom::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, style, slider);

    const float trackY = (float) y + (float) height * 0.5f - 2.0f;
    const float trackHeight = 4.0f;

    // Track
    g.setColour (outlineColour);
    g.fillRoundedRectangle ((float) x, trackY, (float) width, trackHeight, 2.0f);

    // Filled portion
    g.setColour (accentColour);
    g.fillRoundedRectangle ((float) x, trackY, sliderPos - (float) x, trackHeight, 2.0f);

    // Thumb
    g.setColour (textColour);
    g.fillEllipse (sliderPos - 6.0f, trackY - 4.0f, 12.0f, 12.0f);
}

//==============================================================================
void LookAndFeelCustom::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (backgroundColour, shouldDrawButtonAsDown);

    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = 3.0f;

    if (button.getToggleState())
    {
        g.setColour (accentColour);
        g.fillRoundedRectangle (bounds, corner);
    }
    else
    {
        g.setColour (shouldDrawButtonAsHighlighted ? panelColour.brighter (0.08f) : panelColour);
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (outlineColour);
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }
}

//==============================================================================
void LookAndFeelCustom::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.setColour (button.getToggleState() ? bgColour : textColour);
    g.drawText (button.getButtonText(), button.getLocalBounds().toFloat(), juce::Justification::centred);
}

//==============================================================================
void LookAndFeelCustom::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto bounds = button.getLocalBounds().toFloat();
    const float boxSize = 16.0f;
    const juce::Rectangle<float> box (bounds.getX(),
                                      bounds.getY() + (bounds.getHeight() - boxSize) * 0.5f,
                                      boxSize, boxSize);

    if (button.getToggleState())
    {
        g.setColour (accentColour);
        g.fillRoundedRectangle (box, 4.0f);

        g.setColour (bgColour);
        juce::Path check;
        check.startNewSubPath (box.getX() + 3.5f, box.getCentreY());
        check.lineTo (box.getX() + 7.0f, box.getY() + box.getHeight() - 4.0f);
        check.lineTo (box.getX() + box.getWidth() - 3.0f, box.getY() + 3.0f);
        g.strokePath (check, juce::PathStrokeType (2.0f));
    }
    else
    {
        g.setColour (outlineColour);
        g.drawRoundedRectangle (box, 4.0f, 1.5f);
    }

    g.setColour (textColour);
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText (button.getButtonText(), bounds.withLeft (box.getRight() + 8.0f),
                juce::Justification::centredLeft);
}
