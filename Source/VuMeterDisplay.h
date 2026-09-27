#pragma once

#include <JuceHeader.h>

class HarmonicCompressorAudioProcessor;

//==============================================================================
/**
    Analog-style stereo VU meter. Reads the processor's already-ballistics-
    smoothed level atomics (see PluginProcessor::processBlock) and draws two
    independent gauges side by side -- one per channel (Left, Right), each
    with its own needle and scale, like a hardware stereo VU pair -- plus a
    pair of Dorrough-style LED ladder bargraphs beneath them (a slower solid
    average fill with a fast peak segment riding ahead of it, showing the
    "gap" the way a real Dorrough meter does), and numeric peak
    (bottom left) / RMS (bottom right) readouts. Each readout shows two
    values: the current instantaneous value in black, and a 3-second
    peak-hold value in green. Click the mode label to cycle between Input,
    Output and Gain Reduction.
*/
class VuMeterDisplay : public juce::Component, private juce::Timer
{
public:
    explicit VuMeterDisplay (HarmonicCompressorAudioProcessor& processor);
    ~VuMeterDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void drawGauge (juce::Graphics& g, juce::Rectangle<float> bounds, float valueDb,
                     bool isGr, juce::Colour needleColour, const juce::String& channelLabel);
    void drawDorroughBar (juce::Graphics& g, juce::Rectangle<float> bounds, float avgDb, float peakDb,
                          bool isGr, const juce::String& channelLabel);
    static juce::String formatDb (float db, bool isGr);

    enum class Mode { Input, Output, GainReduction };

    HarmonicCompressorAudioProcessor& processor;
    Mode mode = Mode::Output;

    float displayedL = -60.0f, displayedR = -60.0f;
    float peakL = -60.0f, peakR = -60.0f;

    // 3-second peak-hold state for the numeric readouts
    float peakReadoutHold = -60.0f, rmsReadoutHold = -60.0f;
    float peakHoldTimer = 0.0f, rmsHoldTimer = 0.0f;

    juce::Rectangle<float> meterArea;
    juce::Rectangle<float> ledArea;
    juce::Rectangle<int> modeButtonBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VuMeterDisplay)
};
