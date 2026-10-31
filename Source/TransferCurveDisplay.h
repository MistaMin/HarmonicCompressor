#pragma once

#include <JuceHeader.h>
#include "UI/Toolkit.h"

class HarmonicCompressorAudioProcessor;

//==============================================================================
/**
    Compressor transfer-curve display (input dB vs output dB), integrated
    directly as a transparent overlay inside the unlabeled Compression History
    plot so both share the exact auto-ranging vertical axis (-18 dBFS = 0 dBu).
*/
class TransferCurveDisplay : public juce::Component, private juce::Timer
{
public:
    static constexpr float kPadLeft   = 12.0f;
    static constexpr float kPadTop    = 12.0f;
    static constexpr float kPadRight  = 40.0f;
    static constexpr float kPadBottom = 24.0f;

    TransferCurveDisplay (HarmonicCompressorAudioProcessor& processor, Skin& skin);
    ~TransferCurveDisplay() override;

    void setDisplayRange (float minDb, float maxDb) noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool hitTest (int x, int y) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    enum class Handle { none, combined, makeup, threshold };
    juce::Point<float> positionForHandle (Handle) const;
    Handle hitTestHandle (juce::Point<float>) const;
    void endDrag();
    float computeOutputDb (float inputDb) const;
    juce::Point<float> pointForDb (float inputDb, float outputDb) const;

    HarmonicCompressorAudioProcessor& processor;
    Skin& skin;

    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* kneeParam = nullptr;
    std::atomic<float>* makeupParam = nullptr;

    juce::RangedAudioParameter* thresholdParamObj = nullptr;
    juce::RangedAudioParameter* ratioParamObj = nullptr;
    juce::RangedAudioParameter* makeupParamObj = nullptr;

    float displayMinDb = -60.0f;
    float displayMaxDb = 0.0f;

    juce::Rectangle<float> curveArea;
    Handle dragging = Handle::none;
    Handle hovering = Handle::none;
    float dragStartThreshold = 0.0f, dragStartRatio = 1.0f, dragStartMakeup = 0.0f;
    juce::Point<float> dragStartPos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransferCurveDisplay)
};
