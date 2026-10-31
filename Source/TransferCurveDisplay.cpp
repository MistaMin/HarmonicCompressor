#include "TransferCurveDisplay.h"
#include "PluginProcessor.h"

namespace
{
    constexpr float kMinDb = -60.0f;
    constexpr float kMaxDb = 0.0f;
}

//==============================================================================
TransferCurveDisplay::TransferCurveDisplay (HarmonicCompressorAudioProcessor& p, Skin& s)
    : processor (p), skin (s)
{
    thresholdParam = processor.apvts.getRawParameterValue (ParamIDs::threshold);
    ratioParam     = processor.apvts.getRawParameterValue (ParamIDs::ratio);
    kneeParam      = processor.apvts.getRawParameterValue (ParamIDs::knee);

    thresholdParamObj = processor.apvts.getParameter (ParamIDs::threshold);
    ratioParamObj     = processor.apvts.getParameter (ParamIDs::ratio);

    makeupParam = processor.apvts.getRawParameterValue(ParamIDs::makeupGain);
    makeupParamObj = processor.apvts.getParameter(ParamIDs::makeupGain);

    setInterceptsMouseClicks (true, false);
    startTimerHz (30);
}

TransferCurveDisplay::~TransferCurveDisplay()
{
    endDrag();
    stopTimer();
}

//==============================================================================
void TransferCurveDisplay::timerCallback()
{
    repaint();
}

//==============================================================================
void TransferCurveDisplay::resized()
{
    curveArea = getLocalBounds().toFloat().withTrimmedLeft(12).withTrimmedTop(16).withTrimmedRight(36).withTrimmedBottom(32);
}

//==============================================================================
float TransferCurveDisplay::computeOutputDb (float inputDb) const
{
    const float threshold = thresholdParam->load (std::memory_order_relaxed);
    const float ratio     = ratioParam->load (std::memory_order_relaxed);
    const float knee      = kneeParam->load (std::memory_order_relaxed);
    const float slope     = compressorSlope (ratio);
    const float makeup = makeupParam->load(std::memory_order_relaxed);

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
    const float x = juce::jmap (inputDb,  kMinDb, kMaxDb, curveArea.getX(), curveArea.getRight());
    const float y = juce::jmap (outputDb, kMinDb, kMaxDb, curveArea.getBottom(), curveArea.getY());
    return { x, y };
}

//==============================================================================
void TransferCurveDisplay::paint (juce::Graphics& g)
{
    if (curveArea.isEmpty())
        return;

    g.setColour (juce::Colour (0xffe7f0f7));
    g.fillRoundedRectangle (curveArea, 4.0f);
    g.setColour (juce::Colour (0xffb8cddd));
    g.drawRoundedRectangle (curveArea, 4.0f, 1.0f);

    // Grid
    g.setColour (juce::Colour (0xff86a6bf).withAlpha (0.4f));
    for (float db = kMinDb; db <= kMaxDb; db += 12.0f)
    {
        const float x = juce::jmap (db, kMinDb, kMaxDb, curveArea.getX(), curveArea.getRight());
        const float y = juce::jmap (db, kMinDb, kMaxDb, curveArea.getBottom(), curveArea.getY());
        g.drawVerticalLine ((int) x, curveArea.getY(), curveArea.getBottom());
        g.drawHorizontalLine ((int) y, curveArea.getX(), curveArea.getRight());
    }

    // Unity reference line (dashed)
    juce::Path unity;
    unity.startNewSubPath (pointForDb (kMinDb, kMinDb));
    unity.lineTo (pointForDb (kMaxDb, kMaxDb));
    float dashLengths[] = { 3.0f, 3.0f };
    juce::Path dashedUnity;
    juce::PathStrokeType (1.0f).createDashedStroke (dashedUnity, unity, dashLengths, 2);
    g.setColour (skin.mid().withAlpha (0.5f));
    g.fillPath (dashedUnity);

    // Transfer curve
    juce::Path curve;
    bool first = true;
    for (float inDb = kMinDb; inDb <= kMaxDb; inDb += 0.5f)
    {
        auto pt = pointForDb (inDb, juce::jlimit(kMinDb, kMaxDb, computeOutputDb (inDb)));
        if (first) { curve.startNewSubPath (pt); first = false; }
        else       curve.lineTo (pt);
    }
    g.setColour (skin.accent());
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Separate rails keep dedicated handles reachable even at extreme settings.
    g.setColour(juce::Colour(0xff94b1c7));
    g.drawHorizontalLine((int)curveArea.getBottom() + 15, curveArea.getX(), curveArea.getRight());
    g.drawVerticalLine((int)curveArea.getRight() + 17, curveArea.getY(), curveArea.getBottom());
    for (auto h : { Handle::combined, Handle::makeup, Handle::threshold })
    {
        auto pos = positionForHandle(h);
        auto colour = h == Handle::combined ? skin.accent()
                    : h == Handle::makeup ? juce::Colour(0xff7fb3d5) : juce::Colour(0xff70c9ac);
        g.setColour(h == hovering || h == dragging ? juce::Colours::white : colour);
        g.fillEllipse(juce::Rectangle<float>(11, 11).withCentre(pos));
        g.setColour(juce::Colour(0xfff5faff));
        g.drawEllipse(juce::Rectangle<float>(11, 11).withCentre(pos), 1.0f);
    }
    g.setColour(skin.mid());
    g.setFont(juce::FontOptions(8.0f));
    g.drawText("THRESHOLD", curveArea.getX(), curveArea.getBottom() + 22,
               curveArea.getWidth(), 10, juce::Justification::centred);
    g.drawText("OUT", curveArea.getRight() + 3, curveArea.getY() - 12, 29, 10, juce::Justification::centred);

    // Live input/GR dot
    const float liveInputDb = processor.lastInputPeakDb.load (std::memory_order_relaxed);
    const float liveOutputDb = computeOutputDb (liveInputDb);
    auto livePt = pointForDb (juce::jlimit(kMinDb, kMaxDb, liveInputDb), juce::jlimit(kMinDb, kMaxDb, liveOutputDb));
    g.setColour (juce::Colours::orange);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (livePt));

    g.setColour (skin.text());
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    auto grLabelArea = curveArea.withHeight (14.0f).reduced (4.0f, 0.0f);
    g.drawText (juce::String (-processor.lastGrDbForUI.load (std::memory_order_relaxed), 1) + " dB GR",
                grLabelArea, juce::Justification::topRight);
}

//==============================================================================
juce::Point<float> TransferCurveDisplay::positionForHandle(Handle h) const
{
    const float t = thresholdParam->load(std::memory_order_relaxed);
    if (h == Handle::threshold)
        return {pointForDb(juce::jlimit(kMinDb, kMaxDb, t), 0).x, curveArea.getBottom() + 15};
    if (h == Handle::makeup)
        return {curveArea.getRight() + 17,
                juce::jmap(makeupParamObj->convertTo0to1(makeupParam->load()), curveArea.getBottom(), curveArea.getY())};
    auto pos = pointForDb(t, computeOutputDb(t));
    return {juce::jlimit(curveArea.getX() + 6, curveArea.getRight() - 6, pos.x),
            juce::jlimit(curveArea.getY() + 6, curveArea.getBottom() - 6, pos.y)};
}

TransferCurveDisplay::Handle TransferCurveDisplay::hitTestHandle(juce::Point<float> pos) const
{
    for (auto h : {Handle::threshold, Handle::makeup, Handle::combined})
        if (pos.getDistanceFrom(positionForHandle(h)) <= 10.0f) return h;
    return Handle::none;
}

void TransferCurveDisplay::endDrag()
{
    if (dragging == Handle::combined || dragging == Handle::threshold) thresholdParamObj->endChangeGesture();
    if (dragging == Handle::combined) ratioParamObj->endChangeGesture();
    if (dragging == Handle::makeup) makeupParamObj->endChangeGesture();
    dragging = Handle::none;
}

void TransferCurveDisplay::mouseDown(const juce::MouseEvent& e)
{
    endDrag();
    dragging = hitTestHandle(e.position);
    dragStartThreshold = thresholdParam->load();
    dragStartRatio = ratioParam->load();
    dragStartMakeup = makeupParam->load();
    dragStartPos = e.position;
    if (dragging == Handle::combined || dragging == Handle::threshold) thresholdParamObj->beginChangeGesture();
    if (dragging == Handle::combined) ratioParamObj->beginChangeGesture();
    if (dragging == Handle::makeup) makeupParamObj->beginChangeGesture();
}

void TransferCurveDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging == Handle::none || curveArea.isEmpty()) return;
    const float dx = (e.position.x - dragStartPos.x) * 60.0f / curveArea.getWidth();
    const float dy = -(e.position.y - dragStartPos.y) * 60.0f / curveArea.getHeight();
    auto set = [](juce::RangedAudioParameter* p, float value)
    { p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->convertTo0to1(value))); };
    if (dragging == Handle::combined || dragging == Handle::threshold)
        set(thresholdParamObj, juce::jlimit(kMinDb, kMaxDb, dragStartThreshold + dx));
    if (dragging == Handle::combined)
        set(ratioParamObj, juce::jlimit(1.0f, 20.0f, dragStartRatio + dy * 0.5f));
    if (dragging == Handle::makeup)
    {
        const auto range = makeupParamObj->getNormalisableRange();
        const float delta = -(e.position.y - dragStartPos.y) / curveArea.getHeight() * (range.end - range.start);
        set(makeupParamObj, juce::jlimit(range.start, range.end, dragStartMakeup + delta));
    }
    repaint();
}

void TransferCurveDisplay::mouseUp(const juce::MouseEvent&) { endDrag(); repaint(); }
void TransferCurveDisplay::mouseMove(const juce::MouseEvent& e)
{
    hovering = hitTestHandle(e.position);
    setMouseCursor(hovering == Handle::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::DraggingHandCursor);
    repaint();
}
void TransferCurveDisplay::mouseExit(const juce::MouseEvent&) { hovering = Handle::none; repaint(); }
