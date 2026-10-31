#include "TransferCurveDisplay.h"
#include "PluginProcessor.h"

namespace
{
    constexpr float kParamMinDb = -60.0f;
    constexpr float kParamMaxDb = 0.0f;
    constexpr int   kDbuOffset  = 18; // -18 dBFS = 0 dBu
}

//==============================================================================
TransferCurveDisplay::TransferCurveDisplay (HarmonicCompressorAudioProcessor& p, Skin& s)
    : processor (p), skin (s)
{
    setOpaque (false);

    thresholdParam = processor.apvts.getRawParameterValue (ParamIDs::threshold);
    ratioParam     = processor.apvts.getRawParameterValue (ParamIDs::ratio);
    kneeParam      = processor.apvts.getRawParameterValue (ParamIDs::knee);

    thresholdParamObj = processor.apvts.getParameter (ParamIDs::threshold);
    ratioParamObj     = processor.apvts.getParameter (ParamIDs::ratio);

    makeupParam    = processor.apvts.getRawParameterValue (ParamIDs::makeupGain);
    makeupParamObj = processor.apvts.getParameter (ParamIDs::makeupGain);

    setInterceptsMouseClicks (true, false);
    startTimerHz (30);
}

TransferCurveDisplay::~TransferCurveDisplay()
{
    endDrag();
    stopTimer();
}

void TransferCurveDisplay::setDisplayRange (float minDb, float maxDb) noexcept
{
    const float clampedMax = juce::jlimit (-30.0f, 12.0f, maxDb);
    const float clampedMin = juce::jlimit (-78.0f, clampedMax - 18.0f, minDb);
    if (std::abs (clampedMin - displayMinDb) > 0.02f || std::abs (clampedMax - displayMaxDb) > 0.02f)
    {
        displayMinDb = clampedMin;
        displayMaxDb = clampedMax;
        repaint();
    }
}

//==============================================================================
void TransferCurveDisplay::timerCallback()
{
    if (isShowing())
        repaint();
}

//==============================================================================
void TransferCurveDisplay::resized()
{
    curveArea = getLocalBounds().toFloat()
                    .withTrimmedLeft (kPadLeft)
                    .withTrimmedTop (kPadTop)
                    .withTrimmedRight (kPadRight)
                    .withTrimmedBottom (kPadBottom);
}

bool TransferCurveDisplay::hitTest (int x, int y)
{
    const juce::Point<float> pt { static_cast<float> (x), static_cast<float> (y) };
    // Only capture mouse events directly on the draggable handles so floating modules
    // and background areas remain effortless to interact with.
    return hitTestHandle (pt) != Handle::none;
}

//==============================================================================
float TransferCurveDisplay::computeOutputDb (float inputDb) const
{
    const float threshold = thresholdParam->load (std::memory_order_relaxed);
    const float ratio     = ratioParam->load (std::memory_order_relaxed);
    const float knee      = kneeParam->load (std::memory_order_relaxed);
    const float slope     = compressorSlope (ratio);
    const float makeup    = makeupParam->load (std::memory_order_relaxed);

    const float overDb = inputDb - threshold;

    if (knee > 0.01f && std::abs (overDb) <= knee * 0.5f)
    {
        const float x = overDb + knee * 0.5f;
        const float reduction = slope * (x * x) / (2.0f * knee);
        return inputDb - reduction + makeup;
    }

    if (overDb <= 0.0f)
        return inputDb + makeup;

    return inputDb - slope * overDb + makeup;
}

juce::Point<float> TransferCurveDisplay::pointForDb (float inputDb, float outputDb) const
{
    const float x = juce::jmap (inputDb,  displayMinDb, displayMaxDb, curveArea.getX(), curveArea.getRight());
    const float y = juce::jmap (outputDb, displayMinDb, displayMaxDb, curveArea.getBottom(), curveArea.getY());
    return { x, y };
}

//==============================================================================
void TransferCurveDisplay::paint (juce::Graphics& g)
{
    if (curveArea.isEmpty())
        return;

    const auto& t = skin.theme();

    // Smoked dark-glass / frosted soft-white HUD overlay so history traces remain visible underneath
    g.setColour (juce::Colour (t.wellBgTop).withAlpha (t.lightSurface ? 0.62f : 0.48f));
    g.fillRoundedRectangle (curveArea, 3.5f);
    g.setColour (juce::Colour (t.accentPrimary).withAlpha (t.lightSurface ? 0.30f : 0.42f));
    g.drawRoundedRectangle (curveArea, 3.5f, 1.0f);

    // Input-level vertical gridlines anchored at -18 dBFS = 0 dBu (every 12 dB: ..., -24, -12, 0dBu, +12)
    g.setFont (juce::FontOptions (7.8f, juce::Font::bold));
    for (int db = -66; db <= 18; db += 12)
    {
        if (static_cast<float> (db) < displayMinDb + 2.0f || static_cast<float> (db) > displayMaxDb - 1.0f)
            continue;
        const float x = juce::jmap (static_cast<float> (db), displayMinDb, displayMaxDb, curveArea.getX(), curveArea.getRight());
        const bool isZeroDbu = (db == -18);
        g.setColour (isZeroDbu ? juce::Colour (t.zeroDbuRef).withAlpha (0.50f)
                               : juce::Colour (t.textMuted).withAlpha (0.28f));
        g.drawVerticalLine (static_cast<int> (x), curveArea.getY(), curveArea.getBottom());

        const int dbu = db + kDbuOffset;
        const juce::String lbl = (dbu == 0) ? "0dBu" : ((dbu > 0 ? "+" : "") + juce::String (dbu));
        g.setColour (isZeroDbu ? juce::Colour (t.zeroDbuRef) : juce::Colour (t.textPrimary).withAlpha (0.80f));
        g.drawText (lbl, static_cast<int> (x) - 18, static_cast<int> (curveArea.getBottom()) - 13, 36, 11, juce::Justification::centred);
    }

    // Clip unity diagonal & glowing transfer curve to curveArea
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (curveArea.toNearestInt());

        juce::Path unity;
        unity.startNewSubPath (pointForDb (displayMinDb, displayMinDb));
        unity.lineTo (pointForDb (displayMaxDb, displayMaxDb));
        float dashLengths[] = { 4.0f, 3.0f };
        juce::Path dashedUnity;
        juce::PathStrokeType (1.1f).createDashedStroke (dashedUnity, unity, dashLengths, 2);
        g.setColour (juce::Colour (t.textMuted).withAlpha (0.50f));
        g.fillPath (dashedUnity);

        juce::Path curve;
        bool first = true;
        const float step = juce::jmax (0.25f, (displayMaxDb - displayMinDb) / 140.0f);
        for (float inDb = displayMinDb; inDb <= displayMaxDb + 0.01f; inDb += step)
        {
            const float clampedIn = juce::jmin (inDb, displayMaxDb);
            auto pt = pointForDb (clampedIn, computeOutputDb (clampedIn));
            if (first) { curve.startNewSubPath (pt); first = false; }
            else       curve.lineTo (pt);
        }

        // Multi-pass electro-optical / obsidian-ink transfer curve
        g.setColour (juce::Colour (t.postGlow).withAlpha (t.lightSurface ? 0.14f : 0.24f));
        g.strokePath (curve, juce::PathStrokeType (t.lightSurface ? 5.4f : 6.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (t.accentPrimary).withAlpha (t.lightSurface ? 0.32f : 0.50f));
        g.strokePath (curve, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (t.accentPrimary));
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (t.lightSurface ? juce::Colour (0xff07090d)
                                    : juce::Colour (t.accentPrimary).interpolatedWith (juce::Colours::white, 0.58f));
        g.strokePath (curve, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Separate rails keep dedicated handles reachable even as auto-scale zooms
    const float bottomRailY = curveArea.getBottom() + 10.0f;
    const float rightRailX  = curveArea.getRight() + 13.0f;
    g.setColour (juce::Colour (t.accentPrimary).withAlpha (0.45f));
    g.drawHorizontalLine (static_cast<int> (bottomRailY), curveArea.getX(), curveArea.getRight());
    g.setColour (juce::Colour (t.accentSecondary).withAlpha (0.45f));
    g.drawVerticalLine (static_cast<int> (rightRailX), curveArea.getY(), curveArea.getBottom());

    for (auto h : { Handle::combined, Handle::makeup, Handle::threshold })
    {
        auto pos = positionForHandle (h);
        auto colour = h == Handle::combined ? juce::Colour (t.accentPrimary)
                    : h == Handle::makeup   ? juce::Colour (t.accentSecondary)
                                            : juce::Colour (t.thresholdRef);
        const bool activeHandle = (h == hovering || h == dragging);
        g.setColour (colour.withAlpha (activeHandle ? 0.34f : 0.20f));
        g.fillEllipse (juce::Rectangle<float> (16.5f, 16.5f).withCentre (pos));
        g.setColour (juce::Colour (t.lightSurface ? 0xfffaf9f6 : t.oledBg));
        g.fillEllipse (juce::Rectangle<float> (11.5f, 11.5f).withCentre (pos));
        g.setColour (activeHandle ? (t.lightSurface ? colour.darker (0.25f) : colour.brighter (0.40f)) : colour);
        g.drawEllipse (juce::Rectangle<float> (11.5f, 11.5f).withCentre (pos), activeHandle ? 2.1f : 1.6f);
        g.fillEllipse (juce::Rectangle<float> (4.6f, 4.6f).withCentre (pos));
    }

    // Place MAKEUP label at the bottom of the vertical makeup rail so the top lane stays clear for floating modules
    g.setColour (juce::Colour (t.textPrimary).withAlpha (0.85f));
    g.setFont (juce::FontOptions (7.8f, juce::Font::bold));
    g.drawText ("MAKEUP", static_cast<int> (rightRailX) - 21, static_cast<int> (bottomRailY) - 4, 44, 10, juce::Justification::centred);

    // Blazing live input/output operating plasma dot along the transfer curve
    const float liveInputDb  = processor.lastInputPeakDb.load (std::memory_order_relaxed);
    const float liveOutputDb = computeOutputDb (liveInputDb);
    auto livePt = pointForDb (juce::jlimit (displayMinDb, displayMaxDb, liveInputDb),
                              juce::jlimit (displayMinDb, displayMaxDb, liveOutputDb));
    const juce::Colour dotCol (t.lampWarn);
    g.setColour (dotCol.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (livePt));
    g.setColour (dotCol);
    g.fillEllipse (juce::Rectangle<float> (7.5f, 7.5f).withCentre (livePt));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (livePt));
}

//==============================================================================
juce::Point<float> TransferCurveDisplay::positionForHandle (Handle h) const
{
    const float t = thresholdParam->load (std::memory_order_relaxed);
    if (h == Handle::threshold)
    {
        const float rx = juce::jlimit (curveArea.getX(), curveArea.getRight(),
                                       pointForDb (t, displayMinDb).x);
        return { rx, curveArea.getBottom() + 10.0f };
    }
    if (h == Handle::makeup)
        return { curveArea.getRight() + 13.0f,
                 juce::jmap (makeupParamObj->convertTo0to1 (makeupParam->load()), curveArea.getBottom(), curveArea.getY()) };
    auto pos = pointForDb (t, computeOutputDb (t));
    return { juce::jlimit (curveArea.getX() + 6.0f, curveArea.getRight() - 6.0f, pos.x),
             juce::jlimit (curveArea.getY() + 6.0f, curveArea.getBottom() - 6.0f, pos.y) };
}

TransferCurveDisplay::Handle TransferCurveDisplay::hitTestHandle (juce::Point<float> pos) const
{
    for (auto h : { Handle::threshold, Handle::makeup, Handle::combined })
        if (pos.getDistanceFrom (positionForHandle (h)) <= 12.0f) return h;
    return Handle::none;
}

void TransferCurveDisplay::endDrag()
{
    if (dragging == Handle::combined || dragging == Handle::threshold) thresholdParamObj->endChangeGesture();
    if (dragging == Handle::combined) ratioParamObj->endChangeGesture();
    if (dragging == Handle::makeup) makeupParamObj->endChangeGesture();
    dragging = Handle::none;
}

void TransferCurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    endDrag();
    dragging = hitTestHandle (e.position);
    dragStartThreshold = thresholdParam->load();
    dragStartRatio = ratioParam->load();
    dragStartMakeup = makeupParam->load();
    dragStartPos = e.position;
    if (dragging == Handle::combined || dragging == Handle::threshold) thresholdParamObj->beginChangeGesture();
    if (dragging == Handle::combined) ratioParamObj->beginChangeGesture();
    if (dragging == Handle::makeup) makeupParamObj->beginChangeGesture();
}

void TransferCurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == Handle::none || curveArea.isEmpty()) return;
    const float spanDb = juce::jmax (18.0f, displayMaxDb - displayMinDb);
    const float dx = (e.position.x - dragStartPos.x) * spanDb / curveArea.getWidth();
    const float dy = -(e.position.y - dragStartPos.y) * spanDb / curveArea.getHeight();
    auto set = [] (juce::RangedAudioParameter* p, float value)
    { p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->convertTo0to1 (value))); };
    if (dragging == Handle::combined || dragging == Handle::threshold)
        set (thresholdParamObj, juce::jlimit (kParamMinDb, kParamMaxDb, dragStartThreshold + dx));
    if (dragging == Handle::combined)
        set (ratioParamObj, juce::jlimit (1.0f, 20.0f, dragStartRatio + dy * 0.5f));
    if (dragging == Handle::makeup)
    {
        const auto range = makeupParamObj->getNormalisableRange();
        const float delta = -(e.position.y - dragStartPos.y) / curveArea.getHeight() * (range.end - range.start);
        set (makeupParamObj, juce::jlimit (range.start, range.end, dragStartMakeup + delta));
    }
    repaint();
}

void TransferCurveDisplay::mouseUp (const juce::MouseEvent&) { endDrag(); repaint(); }
void TransferCurveDisplay::mouseMove (const juce::MouseEvent& e)
{
    hovering = hitTestHandle (e.position);
    setMouseCursor (hovering == Handle::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::DraggingHandCursor);
    repaint();
}
void TransferCurveDisplay::mouseExit (const juce::MouseEvent&) { hovering = Handle::none; repaint(); }
