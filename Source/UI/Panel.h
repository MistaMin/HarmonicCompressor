#pragma once
#include "Toolkit.h"

// A section plate: faceplate gradient and finish, accent tab, title and subtitle.
class Panel : public juce::Component {
public:
    Panel(Skin& s, juce::String titleText, juce::String subtitleText = {}) : skin(s), title(std::move(titleText)), subtitle(std::move(subtitleText)) {}
    void setAccent(juce::Colour c) { accent = c; repaint(); }
    juce::Colour getBackdrop() const { return skin.backdrop(); }
    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(1);
        gj::drawPanel(g, b, juce::Colour(skin.plate->panelTop), skin.bottom(), skin.finish());
        g.setColour(accent.withAlpha(0.9f)); g.fillRect(10.0f, 9.0f, 3.0f, 15.0f);
        g.setColour(skin.text()); g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(title, 18, 7, getWidth() - 36, 20, juce::Justification::centredLeft);
        if (subtitle.isNotEmpty()) {
            g.setColour(skin.mid()); g.setFont(juce::FontOptions(9.0f));
            g.drawText(subtitle, 18, 25, getWidth() - 36, 13, juce::Justification::centredLeft);
        }
        g.setColour(juce::Colour(0x55000000)); g.drawHorizontalLine(40, 10.0f, float(getWidth() - 10));
        g.setColour(juce::Colour(0x18ffffff)); g.drawHorizontalLine(41, 10.0f, float(getWidth() - 10));
    }
private:
    Skin& skin;
    juce::String title, subtitle;
    juce::Colour accent{0xffc7a66a};
};
