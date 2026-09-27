#include "VuMeterDisplay.h"
#include "PluginProcessor.h"
#include "LookAndFeelCustom.h"

namespace
{
    constexpr float kTimerHz = 30.0f;
    constexpr float kHoldSeconds = 3.0f;
}

//==============================================================================
VuMeterDisplay::VuMeterDisplay (HarmonicCompressorAudioProcessor& p)
    : processor (p)
{
    startTimerHz ((int) kTimerHz);
}

VuMeterDisplay::~VuMeterDisplay()
{
    stopTimer();
}

//==============================================================================
void VuMeterDisplay::timerCallback()
{
    switch (mode)
    {
        case Mode::Input:
            displayedL = processor.vuInputDbL.load (std::memory_order_relaxed);
            displayedR = processor.vuInputDbR.load (std::memory_order_relaxed);
            peakL = processor.peakInputDbL.load (std::memory_order_relaxed);
            peakR = processor.peakInputDbR.load (std::memory_order_relaxed);
            break;
        case Mode::Output:
            displayedL = processor.vuOutputDbL.load (std::memory_order_relaxed);
            displayedR = processor.vuOutputDbR.load (std::memory_order_relaxed);
            peakL = processor.peakOutputDbL.load (std::memory_order_relaxed);
            peakR = processor.peakOutputDbR.load (std::memory_order_relaxed);
            break;
        case Mode::GainReduction:
        {
            const float gr = processor.vuGrDb.load (std::memory_order_relaxed);
            displayedL = displayedR = gr;
            peakL = peakR = gr;
            break;
        }
    }

    const float dt = 1.0f / kTimerHz;
    const float peakNow = juce::jmax (peakL, peakR);
    const float rmsNow  = juce::jmax (displayedL, displayedR);

    if (peakNow >= peakReadoutHold)
    {
        peakReadoutHold = peakNow;
        peakHoldTimer = 0.0f;
    }
    else if ((peakHoldTimer += dt) > kHoldSeconds)
    {
        peakReadoutHold = peakNow;
    }

    if (rmsNow >= rmsReadoutHold)
    {
        rmsReadoutHold = rmsNow;
        rmsHoldTimer = 0.0f;
    }
    else if ((rmsHoldTimer += dt) > kHoldSeconds)
    {
        rmsReadoutHold = rmsNow;
    }

    repaint();
}

//==============================================================================
void VuMeterDisplay::resized()
{
    auto area = getLocalBounds();
    modeButtonBounds = area.removeFromTop (16);
    ledArea = area.removeFromBottom (14).reduced (16, 0).toFloat();
    area.removeFromBottom (4);
    meterArea = area.reduced (10, 2).toFloat();
}

void VuMeterDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (modeButtonBounds.contains (e.getPosition()))
    {
        mode = (mode == Mode::Input)  ? Mode::Output
             : (mode == Mode::Output) ? Mode::GainReduction
                                       : Mode::Input;
        peakReadoutHold = -60.0f;
        rmsReadoutHold = -60.0f;
        repaint();
    }
}

//==============================================================================
juce::String VuMeterDisplay::formatDb (float db, bool isGr)
{
    return isGr ? (juce::String (-db, 1) + " dB") : (juce::String (db, 1) + " dB");
}

//==============================================================================
void VuMeterDisplay::drawGauge (juce::Graphics& g, juce::Rectangle<float> bounds, float valueDb,
                                 bool isGr, juce::Colour needleColour, const juce::String& channelLabel)
{
    // --- Meter face ---
    g.setColour (juce::Colour (0xfff0e8d6));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (LookAndFeelCustom::outlineColour);
    g.drawRoundedRectangle (bounds, 6.0f, 1.2f);

    const float pivotX = bounds.getCentreX();
    const float pivotY = bounds.getBottom() - bounds.getHeight() * 0.14f;
    const float radius = juce::jmin (bounds.getWidth() * 0.44f, bounds.getHeight() * 0.86f);

    float rangeMin, rangeMax;
    struct Tick { float value; const char* label; };
    std::vector<Tick> ticks;
    bool hasRedZone = false;

    if (isGr)
    {
        rangeMin = -24.0f; rangeMax = 0.0f;
        ticks = { { 0.0f, "0" }, { -6.0f, "6" }, { -12.0f, "12" }, { -18.0f, "18" }, { -24.0f, "24" } };
    }
    else
    {
        rangeMin = -24.0f; rangeMax = 3.0f;
        ticks = { { -24.0f, "-24" }, { -12.0f, "-12" }, { -6.0f, "-6" }, { -3.0f, "-3" }, { 0.0f, "0" }, { 3.0f, "+3" } };
        hasRedZone = true;
    }

    auto angleForValue = [&] (float v) -> float
    {
        const float t = juce::jlimit (0.0f, 1.0f, (v - rangeMin) / (rangeMax - rangeMin));
        return juce::jmap (t, 0.0f, 1.0f, juce::degreesToRadians (-50.0f), juce::degreesToRadians (50.0f));
    };

    juce::Path scaleArc;
    scaleArc.addCentredArc (pivotX, pivotY, radius, radius, 0.0f,
                             angleForValue (rangeMin), angleForValue (rangeMax), true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (scaleArc, juce::PathStrokeType (2.0f));

    if (hasRedZone)
    {
        juce::Path redArc;
        redArc.addCentredArc (pivotX, pivotY, radius, radius, 0.0f,
                               angleForValue (0.0f), angleForValue (rangeMax), true);
        g.setColour (juce::Colours::firebrick.withAlpha (0.85f));
        g.strokePath (redArc, juce::PathStrokeType (3.0f));
    }

    g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::plain)));
    for (auto& tick : ticks)
    {
        const float angle = angleForValue (tick.value);
        const float sinA = std::sin (angle), cosA = std::cos (angle);

        juce::Point<float> outer (pivotX + radius * sinA, pivotY - radius * cosA);
        juce::Point<float> inner (pivotX + (radius - 6.0f) * sinA, pivotY - (radius - 6.0f) * cosA);

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (inner.x, inner.y, outer.x, outer.y, 1.3f);

        juce::Point<float> labelPos (pivotX + (radius - 16.0f) * sinA, pivotY - (radius - 16.0f) * cosA);
        g.drawText (tick.label, juce::Rectangle<float> (20.0f, 10.0f).withCentre (labelPos),
                    juce::Justification::centred);
    }

    // --- Needle ---
    const float needleAngle = angleForValue (valueDb);
    const float tipX = pivotX + radius * 0.92f * std::sin (needleAngle);
    const float tipY = pivotY - radius * 0.92f * std::cos (needleAngle);
    g.setColour (needleColour);
    g.drawLine (pivotX, pivotY, tipX, tipY, 2.2f);
    g.setColour (LookAndFeelCustom::accentColour);
    g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f).withCentre ({ pivotX, pivotY }));

    // --- Channel label ---
    g.setColour (needleColour);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText (channelLabel, juce::Rectangle<float> (18.0f, 12.0f).withPosition (bounds.getX() + 4.0f, bounds.getY() + 3.0f),
                juce::Justification::centred);
}

//==============================================================================
void VuMeterDisplay::drawPeakLed (juce::Graphics& g, juce::Rectangle<float> bounds, float peakDb, const juce::String& label)
{
    // Dark bezel so the LED is visible as a shape even when "off".
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (bounds, 2.0f);

    const float t = juce::jlimit (0.0f, 1.0f, (peakDb + 24.0f) / 27.0f); // -24..+3 dB range
    juce::Colour ledColour = juce::Colours::darkgreen.interpolatedWith (juce::Colours::lime, juce::jlimit (0.0f, 1.0f, t * 1.4f));
    if (peakDb > -3.0f)  ledColour = juce::Colours::yellow;
    if (peakDb > 0.0f)   ledColour = juce::Colours::red;

    const float brightness = juce::jmap (t, 0.35f, 1.0f);
    g.setColour (ledColour.withAlpha (brightness));
    g.fillRoundedRectangle (bounds.reduced (1.5f), 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

    g.setColour (LookAndFeelCustom::mutedTextColour);
    g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
    g.drawText (label, bounds.translated (0.0f, -11.0f), juce::Justification::centred);
}

//==============================================================================
void VuMeterDisplay::paint (juce::Graphics& g)
{
    const bool isGr = mode == Mode::GainReduction;
    const juce::String modeText = mode == Mode::Input ? "INPUT"
                                 : mode == Mode::Output ? "OUTPUT"
                                                          : "GAIN REDUCTION";

    g.setColour (LookAndFeelCustom::accentColour);
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.drawText (modeText + juce::String (juce::CharPointer_UTF8 ("  \xe2\x97\x82 click to switch")),
                modeButtonBounds, juce::Justification::centred);

    if (meterArea.isEmpty())
        return;

    auto gaugesArea = meterArea.withTrimmedBottom (28.0f);
    const float gap = 6.0f;
    auto faceL = gaugesArea.removeFromLeft ((gaugesArea.getWidth() - gap) * 0.5f);
    gaugesArea.removeFromLeft (gap);
    auto faceR = gaugesArea;
    auto face = faceL.getUnion (faceR);

    drawGauge (g, faceL, displayedL, isGr, juce::Colours::black, "L");
    drawGauge (g, faceR, displayedR, isGr, juce::Colours::teal, "R");

    // --- Peak LEDs (L/R) under the meter face ---
    if (! ledArea.isEmpty())
    {
        auto ledRow = ledArea;
        const float ledWidth = juce::jmin (36.0f, ledRow.getWidth() * 0.3f);
        auto ledL = ledRow.removeFromLeft (ledWidth);
        auto ledR = ledRow.removeFromRight (ledWidth);
        drawPeakLed (g, ledL, peakL, "L PK");
        drawPeakLed (g, ledR, peakR, "R PK");
    }

    // --- Numeric readouts: peak (bottom-left), RMS (bottom-right).
    // Each shows a black instantaneous value and, below it, a green 3-second
    // peak-hold value.
    auto readoutArea = juce::Rectangle<float> (meterArea.getX(), face.getBottom() + 1.0f, meterArea.getWidth(), 26.0f);
    auto peakColumn = readoutArea.removeFromLeft (readoutArea.getWidth() * 0.5f);
    auto rmsColumn  = readoutArea;

    auto peakBlackRow = peakColumn.removeFromTop (13.0f);
    auto peakGreenRow  = peakColumn;
    auto rmsBlackRow  = rmsColumn.removeFromTop (13.0f);
    auto rmsGreenRow   = rmsColumn;

    const float peakNow = juce::jmax (peakL, peakR);
    const float rmsNow  = juce::jmax (displayedL, displayedR);

    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.setColour (juce::Colours::black);
    g.drawText ("PK " + formatDb (peakNow, isGr), peakBlackRow, juce::Justification::centredLeft);
    g.setColour (juce::Colours::limegreen);
    g.drawText (formatDb (peakReadoutHold, isGr), peakGreenRow, juce::Justification::centredLeft);

    g.setColour (juce::Colours::black);
    g.drawText ("RMS " + formatDb (rmsNow, isGr), rmsBlackRow, juce::Justification::centredRight);
    g.setColour (juce::Colours::limegreen);
    g.drawText (formatDb (rmsReadoutHold, isGr), rmsGreenRow, juce::Justification::centredRight);
}
