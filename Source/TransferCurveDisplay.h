#pragma once

#include <JuceHeader.h>
#include "UI/Toolkit.h"

class HarmonicCompressorAudioProcessor;

//==============================================================================
/**
    Compressor transfer-curve display (input dB vs output dB), with a
    draggable threshold/ratio handle at the knee corner and a live moving dot
    showing the current input level and gain reduction.

    Dragging the handle horizontally changes threshold, vertically changes
    ratio; both drive beginChangeGesture()/setValueNotifyingHost()/
    endChangeGesture() on the corresponding RangedAudioParameter so the APVTS
    and the rotary knobs in the editor stay in sync.
*/
class TransferCurveDisplay : public juce::Component, private juce::Timer
{
public:
    TransferCurveDisplay (HarmonicCompressorAudioProcessor& processor, Skin& skin);
    ~TransferCurveDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    enum class Handle { none, combined, makeup, threshold };
    juce::Point<float> positionForHandle(Handle) const;
    Handle hitTestHandle(juce::Point<float>) const;
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

    juce::Rectangle<float> curveArea;
    juce::Point<float> handlePos { -1000.0f, -1000.0f };

    Handle dragging = Handle::none;
    Handle hovering = Handle::none;
    float dragStartMakeup = 0.0f;
    float dragStartThreshold = 0.0f;
    float dragStartRatio = 1.0f;
    juce::Point<float> dragStartPos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransferCurveDisplay)
};
