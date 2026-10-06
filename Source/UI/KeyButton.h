#pragma once
#include "Toolkit.h"

// A lit push key. Works as a latching toggle or as one member of a radio group; the owner
// connects it to a parameter (ButtonAttachment or ParameterAttachment).
class KeyButton : public juce::Button {
public:
    KeyButton(const juce::String& text, juce::Colour lampColour) : juce::Button(text), lamp(lampColour) { setClickingTogglesState(true); }
    void setStyle(int s) { style = s; repaint(); }
    void setLamp(juce::Colour c) { lamp = c; repaint(); }
    void paintButton(juce::Graphics& g, bool, bool down) override {
        gj::switches::drawPush(g, getLocalBounds().toFloat().reduced(2), getButtonText(), gj::switches::PushStyle(style), lamp, getToggleState(), down);
    }
private:
    juce::Colour lamp;
    int style = 0;
};
