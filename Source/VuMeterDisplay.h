#pragma once

#include <JuceHeader.h>

class HarmonicCompressorAudioProcessor;

//==============================================================================
/**
    Analog-style stereo VU meter. Reads the processor's already-ballistics-
    smoothed level atomics (see PluginProcessor::processBlock) and draws two
    independent hardware-style gauges side by side -- one per channel (Left,
    Right) -- each with a brushed-metal housing, chrome bezel, illuminated
    amber glass face, VU scale with a red +1..+3 zone, and a tapered black
    needle on a raised pivot dome, like a classic analog studio VU meter.
    Beneath the gauges sits a pair of segmented peak/average ladder
    bargraphs (a slower solid average fill with a fast peak segment riding
    ahead of it, showing the level "gap"), and numeric peak (bottom left) /
    RMS (bottom right) readouts. Each readout shows two values: the current
    instantaneous value in black, and a 3-second peak-hold value in green.
    Click the mode label to cycle between Input, Output and Gain Reduction.
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
    void drawLadderBar (juce::Graphics& g, juce::Rectangle<float> bounds, float avgDb, float peakDb,
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
