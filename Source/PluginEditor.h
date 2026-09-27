#pragma once

#include <JuceHeader.h>
#include <array>

#include "PluginProcessor.h"
#include "LookAndFeelCustom.h"
#include "VuMeterDisplay.h"
#include "TransferCurveDisplay.h"

//==============================================================================
/**
    HarmonicCompressor editor.

    Layout:
      - Top: horizontal row of 5 discrete algorithm toggle buttons (no
        dropdowns anywhere), synced to the algorithmMode parameter via a
        ParameterAttachment.
      - Middle-left: 7 rotary sliders (threshold, ratio, attack, release,
        knee, makeup gain, mix).
      - Middle-right: VuMeterDisplay (analog-style Input/Output/GR meter).
      - Sidechain row: enable toggle, HP-freq slider, listen toggle.
      - Bottom: 4 harmonic mix sliders (2nd/3rd/4th/5th).
*/
class HarmonicCompressorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit HarmonicCompressorAudioProcessorEditor (HarmonicCompressorAudioProcessor&);
    ~HarmonicCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void addRotarySlider (juce::Slider& slider, juce::Label& label, const juce::String& name);
    void drawPanel (juce::Graphics& g, const juce::Rectangle<int>& bounds, const juce::String& title);
    void layoutKnobColumn (juce::Rectangle<int> column, juce::Slider& slider, juce::Label& label, int maxKnobSize);
    void updateDisplaySelection();

    HarmonicCompressorAudioProcessor& audioProcessor;

    LookAndFeelCustom lookAndFeel;

    juce::Rectangle<int> algorithmPanelBounds;
    juce::Rectangle<int> curvePanelBounds;
    juce::Rectangle<int> controlsPanelBounds;
    juce::Rectangle<int> harmonicsPanelBounds;
    juce::Rectangle<int> sidechainPanelBounds;

    // Algorithm selector (top row)
    static constexpr int numAlgorithms = 5;
    std::array<juce::TextButton, (size_t) numAlgorithms> algorithmButtons;
    std::unique_ptr<juce::ParameterAttachment> algorithmAttachment;

    // Main controls
    juce::Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider,
                 kneeSlider, makeupGainSlider, mixSlider;
    juce::Label thresholdLabel, ratioLabel, attackLabel, releaseLabel,
                kneeLabel, makeupGainLabel, mixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        thresholdAttachment, ratioAttachment, attackAttachment, releaseAttachment,
        kneeAttachment, makeupGainAttachment, mixAttachment;

    // Harmonics
    juce::Slider harmonic2Slider, harmonic3Slider, harmonic4Slider, harmonic5Slider;
    juce::Label harmonic2Label, harmonic3Label, harmonic4Label, harmonic5Label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        harmonic2Attachment, harmonic3Attachment, harmonic4Attachment, harmonic5Attachment;

    // Auto Release
    juce::ToggleButton autoReleaseButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoReleaseAttachment;

    // Sidechain
    juce::ToggleButton scEnabledButton, scListenButton;
    juce::Slider scHpFreqSlider;
    juce::Label scHpFreqLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        scEnabledAttachment, scListenAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scHpFreqAttachment;

    // Display area: switchable between the VU meter and the transfer curve.
    juce::ComboBox displaySelector;
    VuMeterDisplay vuMeter;
    TransferCurveDisplay transferCurve;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicCompressorAudioProcessorEditor)
};
