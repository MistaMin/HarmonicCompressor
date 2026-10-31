#pragma once
#include "Toolkit.h"

// A dark-anodized electro-optical push key with a recessed horizontal top LED bar slot.
class KeyButton : public juce::Button {
public:
    KeyButton(const juce::String& text, juce::Colour lampColour)
        : juce::Button(text), lamp(lampColour)
    {
        setClickingTogglesState(true);
    }

    void setSkin(const Skin* s) noexcept { skin = s; repaint(); }
    void setStyle(int s) { style = s; repaint(); }
    void setLamp(juce::Colour c) { lamp = c; repaint(); }

    void paintButton(juce::Graphics& g, bool over, bool down) override {
        juce::ignoreUnused(style);
        const auto& t = skin != nullptr ? skin->theme() : Skin::defaultTheme();
        const float alpha = isEnabled() ? 1.0f : 0.42f;
        const bool on = getToggleState();

        auto outer = getLocalBounds().toFloat().reduced(0.8f);
        g.setColour(juce::Colour(t.lightSurface ? t.panelBorderLo : t.wellBgBottom).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(outer, 4.2f);

        auto b = outer.reduced(1.0f).translated(0.0f, down ? 0.5f : 0.0f);
        juce::Colour topCol = juce::Colour(over ? t.buttonHoverTop : t.buttonTop);
        juce::Colour botCol = juce::Colour(t.buttonBottom);
        if (on) {
            topCol = topCol.interpolatedWith(lamp, t.lightSurface ? 0.12f : 0.20f);
            botCol = botCol.interpolatedWith(t.lightSurface ? lamp : lamp.darker(0.6f), t.lightSurface ? 0.18f : 0.18f);
        }

        g.setGradientFill(juce::ColourGradient(topCol.withMultipliedAlpha(alpha), b.getX(), b.getY(),
                                               botCol.withMultipliedAlpha(alpha), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle(b, 3.6f);

        if (on) {
            g.setColour(lamp.withAlpha(0.20f * alpha));
            g.drawRoundedRectangle(b.expanded(0.5f), 4.0f, 2.0f);
        }
        g.setColour((on ? lamp.withAlpha(0.85f)
                        : (over ? juce::Colour(t.accentPrimary).withAlpha(0.65f)
                                : juce::Colour(t.buttonBorder))).withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(b, 3.6f, down ? 1.4f : 1.0f);

        // Recessed top horizontal LED light-bar slot
        const float slotW = juce::jmin(36.0f, b.getWidth() * 0.48f);
        const float slotH = b.getHeight() <= 23.0f ? 2.1f : 2.6f;
        const juce::Rectangle<float> ledSlot(b.getCentreX() - slotW * 0.5f, b.getY() + 2.8f, slotW, slotH);

        g.setColour(juce::Colour(t.lightSurface ? 0xffcdc8be : 0xff040608).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(ledSlot.expanded(0.6f), slotH * 0.5f);

        if (on) {
            g.setColour(lamp.withAlpha(0.34f * alpha));
            g.fillRoundedRectangle(ledSlot.expanded(2.6f, 1.8f), slotH);
            g.setColour(lamp.withMultipliedAlpha(alpha));
            g.fillRoundedRectangle(ledSlot, slotH * 0.5f);
            g.setColour(lamp.interpolatedWith(juce::Colours::white, t.lightSurface ? 0.35f : 0.58f).withMultipliedAlpha(alpha));
            g.fillRoundedRectangle(ledSlot.reduced(slotW * 0.16f, 0.45f), slotH * 0.4f);
        } else {
            g.setColour(lamp.withAlpha(t.lightSurface ? 0.24f : 0.18f).withMultipliedAlpha(alpha));
            g.fillRoundedRectangle(ledSlot, slotH * 0.5f);
        }

        // Crisp hardware button label
        const auto textRect = juce::Rectangle<int>(int(b.getX() + 2.0f), int(b.getY() + 5.2f),
                                                   int(b.getWidth() - 4.0f), int(b.getHeight() - 5.6f));
        g.setColour((on ? juce::Colour(t.textBright) : juce::Colour(t.textPrimary)).withMultipliedAlpha(alpha));
        g.setFont(juce::FontOptions(b.getHeight() <= 23.0f ? 8.3f : 8.9f, juce::Font::bold));
        g.drawFittedText(getButtonText(), textRect, juce::Justification::centred, 1, 0.80f);
    }

private:
    const Skin* skin = nullptr;
    juce::Colour lamp;
    int style = 0;
};

// A dark-anodized hardware dropdown selector button with glowing LED pip and illuminated chevron.
class DropdownButton : public juce::Button {
public:
    explicit DropdownButton(juce::Colour accentColour = juce::Colour(0xff29ec6b))
        : juce::Button({}), accent(accentColour) {}

    void setSkin(const Skin* s) noexcept { skin = s; repaint(); }
    void setAccent(juce::Colour c) { accent = c; repaint(); }

    void paintButton(juce::Graphics& g, bool over, bool down) override {
        const auto& t = skin != nullptr ? skin->theme() : Skin::defaultTheme();
        const float alpha = isEnabled() ? 1.0f : 0.50f;

        auto outer = getLocalBounds().toFloat().reduced(0.6f);
        g.setColour(juce::Colour(t.lightSurface ? t.panelBorderLo : t.wellBgBottom).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(outer, 4.4f);

        auto b = outer.reduced(1.0f).translated(0.0f, down ? 0.4f : 0.0f);
        const juce::Colour topCol(over ? t.buttonHoverTop : t.buttonTop);
        const juce::Colour botCol(t.buttonBottom);
        g.setGradientFill(juce::ColourGradient(topCol.withMultipliedAlpha(alpha), b.getX(), b.getY(),
                                               botCol.withMultipliedAlpha(alpha), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle(b, 3.8f);

        if (over || down) {
            g.setColour(accent.withAlpha(0.22f * alpha));
            g.drawRoundedRectangle(b.expanded(0.5f), 4.2f, 2.0f);
        }
        g.setColour((over || down ? accent.withAlpha(0.88f) : juce::Colour(t.buttonBorder)).withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(b, 3.8f, 1.05f);

        // Left glowing LED pip
        g.setColour(accent.withAlpha(0.28f * alpha));
        g.fillRoundedRectangle(b.getX() + 2.8f, b.getY() + 4.0f, 5.0f, b.getHeight() - 8.0f, 2.0f);
        g.setColour(accent.withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(b.getX() + 4.0f, b.getY() + 5.0f, 2.5f, b.getHeight() - 10.0f, 1.2f);

        // Right chevron compartment divider
        const float arrowZoneW = b.getWidth() < 68.0f ? 17.0f : 21.0f;
        const float sepX = b.getRight() - arrowZoneW;
        g.setColour(juce::Colour(t.lightSurface ? 0x26000000 : 0x88000000).withMultipliedAlpha(alpha));
        g.drawVerticalLine(int(sepX), b.getY() + 3.5f, b.getBottom() - 3.5f);
        g.setColour(juce::Colour(t.panelBorderHi).withAlpha((t.lightSurface ? 0.80f : 0.35f) * alpha));
        g.drawVerticalLine(int(sepX) + 1, b.getY() + 3.5f, b.getBottom() - 3.5f);

        // Illuminated chevron
        const float cx = sepX + arrowZoneW * 0.5f;
        const float cy = b.getCentreY();
        juce::Path chevron;
        chevron.startNewSubPath(cx - 3.6f, cy - 1.8f);
        chevron.lineTo(cx, cy + 2.2f);
        chevron.lineTo(cx + 3.6f, cy - 1.8f);
        g.setColour((over ? (t.lightSurface ? accent : accent.brighter(0.35f)) : juce::Colour(t.textPrimary)).withMultipliedAlpha(alpha));
        g.strokePath(chevron, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Selected item text
        g.setColour(juce::Colour(t.textBright).withMultipliedAlpha(alpha));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        auto textRect = juce::Rectangle<int>(int(b.getX() + 10.0f), int(b.getY()),
                                             juce::jmax(16, int(sepX - b.getX() - 13.0f)), int(b.getHeight()));
        g.drawFittedText(getButtonText(), textRect, juce::Justification::centredLeft, 1, 0.80f);
    }

private:
    const Skin* skin = nullptr;
    juce::Colour accent;
};
