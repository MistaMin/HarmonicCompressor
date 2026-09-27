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
    ledArea = area.removeFromBottom (30).reduced (10, 0).toFloat();
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
    // Never let the scale arc, ticks, or needle spill outside this gauge's
    // own housing rectangle, regardless of the current aspect ratio.
    juce::Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion (bounds.getSmallestIntegerContainer());

    // --- Outer housing: brushed dark metal with corner screws ---
    juce::ColourGradient housingGrad (juce::Colour (0xff3a3a3c), bounds.getTopLeft(),
                                       juce::Colour (0xff141415), bounds.getBottomRight(), false);
    housingGrad.addColour (0.4, juce::Colour (0xff2a2a2c));
    g.setGradientFill (housingGrad);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    const float screwR = 2.4f;
    for (auto corner : { juce::Point<float> (bounds.getX() + 6.0f, bounds.getY() + 6.0f),
                          juce::Point<float> (bounds.getRight() - 6.0f, bounds.getY() + 6.0f),
                          juce::Point<float> (bounds.getX() + 6.0f, bounds.getBottom() - 6.0f),
                          juce::Point<float> (bounds.getRight() - 6.0f, bounds.getBottom() - 6.0f) })
    {
        g.setColour (juce::Colour (0xff0d0d0e));
        g.fillEllipse (juce::Rectangle<float> (screwR * 2.0f, screwR * 2.0f).withCentre (corner));
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawLine (corner.x - screwR * 0.6f, corner.y, corner.x + screwR * 0.6f, corner.y, 0.7f);
    }

    // --- Chrome bezel ring ---
    auto bezel = bounds.reduced (9.0f);
    juce::ColourGradient bezelGrad (juce::Colour (0xffcfcfd2), bezel.getTopLeft(),
                                     juce::Colour (0xff4a4a4d), bezel.getBottomRight(), false);
    bezelGrad.addColour (0.5, juce::Colour (0xff9a9a9e));
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (bezel, 3.0f);

    auto face = bezel.reduced (3.0f);
    const auto faceForClip = face;

    // --- Illuminated amber glass face, brighter top-centre like backlit glass ---
    juce::ColourGradient faceGrad (juce::Colour (0xfffff2c8), face.getCentreX(), face.getY() + face.getHeight() * 0.05f,
                                    juce::Colour (0xffcf9a3e), face.getX(), face.getBottom(), true);
    faceGrad.addColour (0.45, juce::Colour (0xfff5d98a));
    g.setGradientFill (faceGrad);
    g.fillRect (face);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRect (face, 1.0f);

    const float pivotX = face.getCentreX();
    // Use an elliptical sweep (wider than it is tall) so the scale reads like
    // a classic VU meter: a wide, flat arc that fills most of the face's
    // width without growing taller.
    const float radiusY = juce::jmin (face.getHeight() * 0.62f, face.getWidth() * 0.60f);
    const float radiusX = juce::jmax (radiusY, face.getWidth() * 0.50f);
    const float radius = radiusY; // kept for code below that still needs a single scalar (VU caption font size)
    // Keep the pivot -- and the needle's short counterweight tail below it --
    // fully inside the amber face, clear of the metal bezel underneath.
    const float pivotY = face.getBottom() - juce::jmax (12.0f, radiusY * 0.16f);

    float rangeMin, rangeMax;
    struct Tick { float value; const char* label; bool red; };
    std::vector<Tick> ticks;
    bool hasRedZone = false;

    if (isGr)
    {
        rangeMin = -24.0f; rangeMax = 0.0f;
        ticks = { { 0.0f, "0", false }, { -6.0f, "6", false }, { -12.0f, "12", false },
                  { -18.0f, "18", false }, { -24.0f, "24", false } };
    }
    else
    {
        rangeMin = -20.0f; rangeMax = 3.0f;
        ticks = { { -20.0f, "-20", false }, { -10.0f, "-10", false }, { -7.0f, "-7", false },
                  { -5.0f, "-5", false }, { -3.0f, "-3", false }, { -1.0f, "", false },
                  { 0.0f, "0", false }, { 1.0f, "", true }, { 2.0f, "", true }, { 3.0f, "+3", true } };
        hasRedZone = true;
    }

    auto angleForValue = [&] (float v) -> float
    {
        const float t = juce::jlimit (0.0f, 1.0f, (v - rangeMin) / (rangeMax - rangeMin));
        return juce::jmap (t, 0.0f, 1.0f, juce::degreesToRadians (-50.0f), juce::degreesToRadians (50.0f));
    };

    // Elliptical point helper: an absolute pixel inset applied independently
    // on each axis, so ticks stay short and radial-looking even though the
    // sweep itself is a wide ellipse rather than a circle.
    auto ellipsePoint = [&] (float angle, float insetX, float insetY)
    {
        const float sinA = std::sin (angle), cosA = std::cos (angle);
        return juce::Point<float> (pivotX + (radiusX - insetX) * sinA, pivotY - (radiusY - insetY) * cosA);
    };

    juce::Path scaleArc;
    scaleArc.addCentredArc (pivotX, pivotY, radiusX, radiusY, 0.0f,
                             angleForValue (rangeMin), angleForValue (rangeMax), true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (scaleArc, juce::PathStrokeType (2.0f));

    if (hasRedZone)
    {
        juce::Path redArc;
        redArc.addCentredArc (pivotX, pivotY, radiusX, radiusY, 0.0f,
                               angleForValue (0.0f), angleForValue (rangeMax), true);
        g.setColour (juce::Colours::firebrick.withAlpha (0.85f));
        g.strokePath (redArc, juce::PathStrokeType (3.0f));
    }

    // Minor (unlabeled) ticks halfway between each pair of major ticks.
    for (size_t i = 0; i + 1 < ticks.size(); ++i)
    {
        const float midVal = 0.5f * (ticks[i].value + ticks[i + 1].value);
        const float angle = angleForValue (midVal);
        auto outer = ellipsePoint (angle, 0.0f, 0.0f);
        auto inner = ellipsePoint (angle, 3.5f, 3.5f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine (inner.x, inner.y, outer.x, outer.y, 1.0f);
    }

    g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
    for (auto& tick : ticks)
    {
        const float angle = angleForValue (tick.value);
        auto outer = ellipsePoint (angle, 0.0f, 0.0f);
        auto inner = ellipsePoint (angle, 6.5f, 6.5f);

        g.setColour (tick.red ? juce::Colours::firebrick : juce::Colours::black.withAlpha (0.85f));
        g.drawLine (inner.x, inner.y, outer.x, outer.y, 1.6f);

        if (tick.label[0] == '\0')
            continue;

        auto labelPos = ellipsePoint (angle, 17.0f, 17.0f);
        g.setColour (tick.red ? juce::Colours::firebrick : juce::Colours::black);
        g.drawText (tick.label, juce::Rectangle<float> (24.0f, 11.0f).withCentre (labelPos),
                    juce::Justification::centred);
    }

    // --- "VU" caption and channel tag, like the silkscreened text on a
    // real meter face. ---
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (7.0f, radius * 0.16f), juce::Font::bold)));
    g.drawText ("VU", face.removeFromBottom (face.getHeight() * 0.18f).reduced (4.0f, 0.0f),
                juce::Justification::bottomRight);

    g.setColour (needleColour.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText (channelLabel, juce::Rectangle<float> (16.0f, 12.0f).withPosition (bezel.getX() + 3.0f, bezel.getY() + 2.0f),
                juce::Justification::centred);

    // --- Needle: tapered black polygon with a short counterweight tail and
    // a drop shadow, like a real analog meter movement. ---
    juce::Graphics::ScopedSaveState needleClip (g);
    g.reduceClipRegion (faceForClip.getSmallestIntegerContainer());

    const float needleAngle = angleForValue (valueDb);
    const float sinN = std::sin (needleAngle), cosN = std::cos (needleAngle);
    // The needle's tip must trace the same wide ellipse as the scale arc, so
    // scale its horizontal reach by radiusX and its vertical reach by radiusY
    // (a plain circular tip length would undershoot the arc at the sides).
    const float tipLenX = radiusX * 0.95f;
    const float tipLenY = radiusY * 0.95f;
    const float tailLenX = radiusX * 0.10f;
    const float tailLenY = radiusY * 0.10f;
    const float baseHalfWidth = 3.0f;

    auto buildNeedlePath = [&] (float ox, float oy)
    {
        juce::Path p;
        juce::Point<float> tip  (pivotX + ox + tipLenX  * sinN, pivotY + oy - tipLenY  * cosN);
        juce::Point<float> tail (pivotX + ox - tailLenX * sinN, pivotY + oy + tailLenY * cosN);
        juce::Point<float> left  (pivotX + ox - baseHalfWidth * cosN, pivotY + oy - baseHalfWidth * sinN);
        juce::Point<float> right (pivotX + ox + baseHalfWidth * cosN, pivotY + oy + baseHalfWidth * sinN);
        p.addTriangle (tip, left, right);
        p.addTriangle (tail, left, right);
        return p;
    };

    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillPath (buildNeedlePath (0.7f, 0.9f));

    g.setColour (juce::Colour (0xff141414));
    g.fillPath (buildNeedlePath (0.0f, 0.0f));

    // Raised black pivot dome with a small specular highlight, like the
    // riveted centre boss on a real meter movement.
    auto domeBounds = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ pivotX, pivotY });
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (domeBounds.translated (0.6f, 0.8f));
    juce::ColourGradient domeGrad (juce::Colour (0xff5a5a5c), pivotX - 2.2f, pivotY - 2.2f,
                                    juce::Colour (0xff0a0a0a), pivotX + 3.0f, pivotY + 3.0f, true);
    g.setGradientFill (domeGrad);
    g.fillEllipse (domeBounds);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre ({ pivotX - 1.6f, pivotY - 1.6f }));
}

//==============================================================================
// Segmented peak/average LED ladder: a row of discrete segments filled
// solidly up to the (slower) average level -- green/yellow/red by zone,
// like a classic leveling bargraph -- plus a single bright segment riding
// at the (fast) peak level. Because the peak ballistic reacts far quicker
// than the average and releases slowly, it sits visibly ahead of the solid
// fill, showing the peak/average "gap".
void VuMeterDisplay::drawLadderBar (juce::Graphics& g, juce::Rectangle<float> bounds, float avgDb, float peakDb,
                                     bool isGr, const juce::String& channelLabel)
{
    const float rangeMin = -24.0f;
    const float rangeMax = isGr ? 0.0f : 3.0f;
    constexpr int numSegments = 26;

    auto labelArea = bounds.removeFromLeft (14.0f);
    g.setColour (LookAndFeelCustom::mutedTextColour);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText (channelLabel, labelArea, juce::Justification::centred);

    bounds.removeFromLeft (2.0f);
    const float segGap = 1.3f;
    const float segWidth = (bounds.getWidth() - segGap * (numSegments - 1)) / (float) numSegments;

    auto colourForDb = [] (float db) -> juce::Colour
    {
        if (db > 0.0f)   return juce::Colours::red;
        if (db > -6.0f)  return juce::Colours::yellow;
        return juce::Colours::limegreen;
    };

    const int litSegments  = (int) std::round (juce::jlimit (0.0f, 1.0f, (avgDb  - rangeMin) / (rangeMax - rangeMin)) * numSegments);
    const int peakSegment  = (int) std::round (juce::jlimit (0.0f, 1.0f, (peakDb - rangeMin) / (rangeMax - rangeMin)) * numSegments) - 1;

    for (int i = 0; i < numSegments; ++i)
    {
        auto seg = juce::Rectangle<float> (bounds.getX() + i * (segWidth + segGap), bounds.getY(), segWidth, bounds.getHeight());
        const float segDb = juce::jmap ((float) i / (float) (numSegments - 1), rangeMin, rangeMax);
        const juce::Colour zoneColour = colourForDb (segDb);

        const bool isLit  = i < litSegments;
        const bool isPeak = i == peakSegment && ! isLit;

        g.setColour (zoneColour.withAlpha (isLit ? 0.95f : 0.16f));
        g.fillRoundedRectangle (seg, 0.6f);

        if (isPeak)
        {
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (seg, 0.6f);
            g.setColour (zoneColour);
            g.drawRoundedRectangle (seg.reduced (0.3f), 0.6f, 0.8f);
        }
    }
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

    // --- Peak/average LED ladders (L on top, R below) ---
    if (! ledArea.isEmpty())
    {
        auto rows = ledArea;
        auto rowL = rows.removeFromTop ((rows.getHeight() - 3.0f) * 0.5f);
        rows.removeFromTop (3.0f);
        auto rowR = rows;
        drawLadderBar (g, rowL, displayedL, peakL, isGr, "L");
        drawLadderBar (g, rowR, displayedR, peakR, isGr, "R");
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
    g.setColour (juce::Colours::white);
    g.drawText ("PK " + formatDb (peakNow, isGr), peakBlackRow, juce::Justification::centredLeft);
    g.setColour (juce::Colours::limegreen);
    g.drawText (formatDb (peakReadoutHold, isGr), peakGreenRow, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white);
    g.drawText ("RMS " + formatDb (rmsNow, isGr), rmsBlackRow, juce::Justification::centredRight);
    g.setColour (juce::Colours::limegreen);
    g.drawText (formatDb (rmsReadoutHold, isGr), rmsGreenRow, juce::Justification::centredRight);
}
