#include "TransferCurveDisplay.h"
#include "PluginProcessor.h"
#include "LookAndFeelCustom.h"

namespace
{
    constexpr float kMinDb = -60.0f;
    constexpr float kMaxDb = 0.0f;
}

//==============================================================================
TransferCurveDisplay::TransferCurveDisplay (HarmonicCompressorAudioProcessor& p)
    : processor (p)
{
    thresholdParam = processor.apvts.getRawParameterValue (ParamIDs::threshold);
    ratioParam     = processor.apvts.getRawParameterValue (ParamIDs::ratio);
    kneeParam      = processor.apvts.getRawParameterValue (ParamIDs::knee);

    thresholdParamObj = processor.apvts.getParameter (ParamIDs::threshold);
    ratioParamObj     = processor.apvts.getParameter (ParamIDs::ratio);

    setInterceptsMouseClicks (true, false);
    startTimerHz (30);
}

TransferCurveDisplay::~TransferCurveDisplay()
{
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
    curveArea = getLocalBounds().reduced (10).toFloat();
}

//==============================================================================
float TransferCurveDisplay::computeOutputDb (float inputDb) const
{
    const float threshold = thresholdParam->load (std::memory_order_relaxed);
    const float ratio     = ratioParam->load (std::memory_order_relaxed);
    const float knee      = kneeParam->load (std::memory_order_relaxed);
    const float slope     = compressorSlope (ratio);

    const float overDb = inputDb - threshold;

    if (knee > 0.01f && std::abs (overDb) <= knee * 0.5f)
    {
        const float x = overDb + knee * 0.5f;
        const float reduction = slope * (x * x) / (2.0f * knee);
        return inputDb - reduction;
    }

    if (overDb <= 0.0f)
        return inputDb;

    return inputDb - slope * overDb;
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

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (curveArea, 4.0f);

    // Grid
    g.setColour (LookAndFeelCustom::outlineColour.withAlpha (0.4f));
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
    g.setColour (LookAndFeelCustom::mutedTextColour.withAlpha (0.5f));
    g.fillPath (dashedUnity);

    // Transfer curve
    juce::Path curve;
    bool first = true;
    for (float inDb = kMinDb; inDb <= kMaxDb; inDb += 0.5f)
    {
        auto pt = pointForDb (inDb, computeOutputDb (inDb));
        if (first) { curve.startNewSubPath (pt); first = false; }
        else       curve.lineTo (pt);
    }
    g.setColour (LookAndFeelCustom::accentColour);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Knee/ratio handle
    const float threshold = thresholdParam->load (std::memory_order_relaxed);
    handlePos = pointForDb (threshold, computeOutputDb (threshold));

    g.setColour (hovering || dragging ? juce::Colours::white : LookAndFeelCustom::accentColour);
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (handlePos));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (handlePos), 1.2f);

    // Live input/GR dot
    const float liveInputDb = processor.lastInputPeakDb.load (std::memory_order_relaxed);
    const float liveOutputDb = computeOutputDb (liveInputDb);
    auto livePt = pointForDb (liveInputDb, liveOutputDb);
    g.setColour (juce::Colours::orange);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (livePt));

    g.setColour (LookAndFeelCustom::textColour);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    auto grLabelArea = curveArea.withHeight (14.0f).reduced (4.0f, 0.0f);
    g.drawText (juce::String (-processor.lastGrDbForUI.load (std::memory_order_relaxed), 1) + " dB GR",
                grLabelArea, juce::Justification::topRight);
}

//==============================================================================
void TransferCurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (e.position.getDistanceFrom (handlePos) <= 10.0f)
    {
        dragging = true;
        dragStartThreshold = thresholdParam->load (std::memory_order_relaxed);
        dragStartRatio     = ratioParam->load (std::memory_order_relaxed);
        dragStartPos = e.position;

        if (thresholdParamObj != nullptr) thresholdParamObj->beginChangeGesture();
        if (ratioParamObj != nullptr)     ratioParamObj->beginChangeGesture();
    }
}

void TransferCurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || curveArea.isEmpty())
        return;

    const float dbPerPixelX = (kMaxDb - kMinDb) / curveArea.getWidth();
    const float dbPerPixelY = (kMaxDb - kMinDb) / curveArea.getHeight();

    const float dDb = (e.position.x - dragStartPos.x) * dbPerPixelX;
    const float newThreshold = juce::jlimit (kMinDb, kMaxDb, dragStartThreshold + dDb);

    const float dRatio = -(e.position.y - dragStartPos.y) * dbPerPixelY * 0.5f;
    const float newRatio = juce::jlimit (1.0f, 20.0f, dragStartRatio + dRatio);

    if (thresholdParamObj != nullptr)
        thresholdParamObj->setValueNotifyingHost (thresholdParamObj->convertTo0to1 (newThreshold));
    if (ratioParamObj != nullptr)
        ratioParamObj->setValueNotifyingHost (ratioParamObj->convertTo0to1 (newRatio));

    repaint();
}

void TransferCurveDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        if (thresholdParamObj != nullptr) thresholdParamObj->endChangeGesture();
        if (ratioParamObj != nullptr)     ratioParamObj->endChangeGesture();
    }
    dragging = false;
}

void TransferCurveDisplay::mouseMove (const juce::MouseEvent& e)
{
    const bool wasHovering = hovering;
    hovering = e.position.getDistanceFrom (handlePos) <= 10.0f;
    if (hovering != wasHovering)
        repaint();
}

void TransferCurveDisplay::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}
