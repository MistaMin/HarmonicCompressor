#pragma once
#include "Toolkit.h"
#include "Panel.h"
#include <functional>

// A toolkit-drawn rotary control. It is a plain juce::Slider, so SliderAttachment keeps the
// parameter IDs, automation and host behaviour exactly as before; only the drawing is generated.
class Knob : public juce::Slider, private juce::Timer {
public:
    using Formatter = std::function<juce::String(double)>;

    Knob(const juce::String& label, juce::Colour colour, Formatter fmt) : format(std::move(fmt)) {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        setMouseDragSensitivity(180);
        setScrollWheelEnabled(false);
        design.label = label.toStdString();
        design.colour = "#" + colour.toDisplayString(false).toStdString();
        design.style = "Brit";
        textFromValueFunction = format;
        startTimerHz(60);
    }

    void setActive(bool a) { if (active != a) { active = a; repaint(); } }
    void setTextColours(juce::Colour t, juce::Colour m) { text = t; mid = m; repaint(); }
    void resized() override {
        auto b = getLocalBounds();
        labelArea = b.removeFromTop(14);
        valueArea = b.removeFromBottom(18);
        knobArea = b;
    }
    void paint(juce::Graphics& g) override {
        auto bounds = knobArea.toFloat();
        const float d = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto kb = juce::Rectangle<float>(d, d).withCentre(bounds.getCentre());
        juce::Colour backdrop;
        if (auto* p = dynamic_cast<Panel*>(getParentComponent())) backdrop = p->getBackdrop();
        g.setColour(text); g.setFont(juce::FontOptions(design.fontSize, juce::Font::bold));
        g.drawText(design.label, labelArea, juce::Justification::centred);
        gj::drawKnob(g, kb, motion.position, design, active, backdrop);
        auto ro = valueArea.toFloat().withSizeKeepingCentre(juce::jmin(68.0f, float(getWidth()) - 6.0f), 15.0f);
        g.setColour(juce::Colour(0xff161d20)); g.fillRoundedRectangle(ro, 2);
        g.setColour(juce::Colour(0xff465255)); g.drawRoundedRectangle(ro, 2, 0.6f);
        g.setColour(active ? juce::Colour(0xffe9d9b0) : juce::Colour(0xff7d8788));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(format(getValue()), valueArea, juce::Justification::centred);
    }
    std::function<void()> onSelect;   // developer mode: a click selects the knob in the inspector
    void mouseDown(const juce::MouseEvent& e) override { if (onSelect) onSelect(); juce::Slider::mouseDown(e); }
    void applyDesign(const glui::Item& item) { design = item; repaint(); }
    const glui::Item& getDesign() const { return design; }
private:
    void timerCallback() override {
        const auto target = valueToProportionOfLength(getValue());
        if (!isShowing()) { motion.position = target; motion.velocity = 0; return; }
        if (std::abs(motion.position - target) < 0.0001 && std::abs(motion.velocity) < 0.0001) return;
        motion.step(target, 1.0 / 60.0);
        repaint();
    }
    Formatter format;
    glui::Item design;
    glui::Motion motion;
    bool active = true;
    juce::Colour text{0xffdde1d5}, mid{0xff909c9f};
    juce::Rectangle<int> labelArea, valueArea, knobArea;
};
