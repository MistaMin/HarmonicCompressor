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
        auto b = getLocalBounds().toFloat().reduced(2);
        g.setColour(getToggleState() ? lamp.withAlpha(0.22f) : juce::Colour(0xffedf3f8));
        g.fillRoundedRectangle(b, 5);
        g.setColour(getToggleState() ? lamp : juce::Colour(0xff94b2c9));
        g.drawRoundedRectangle(b, 5, down ? 2.0f : 1.0f);
        g.setColour(getToggleState() ? juce::Colour(0xff1a6493) : juce::Colour(0xff355c78));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred);
    }
private:
    juce::Colour lamp;
    int style = 0;
};
