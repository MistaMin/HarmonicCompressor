#pragma once
#include "Toolkit.h"

// A dark-anodized hardware section plate: brushed gradient finish, double metallic bevel,
// glowing LED accent bar, title and subtitle. Supports headless (0px), compact (22px), and full (40px) headers.
class Panel : public juce::Component {
public:
    Panel(Skin& s, juce::String titleText, juce::String subtitleText = {}, bool compactHeader = false)
        : skin(s), title(std::move(titleText)), subtitle(std::move(subtitleText)), compact(compactHeader) {}

    void setAccent(juce::Colour c) { accent = c; repaint(); }
    void setCompact(bool c) { if (compact != c) { compact = c; repaint(); } }
    bool isCompact() const noexcept { return compact; }
    int headerHeight() const noexcept { return title.isEmpty() ? 0 : (compact ? 22 : 40); }
    juce::Colour getBackdrop() const { return skin.backdrop(); }
    const Skin& getSkin() const noexcept { return skin; }

    void paint(juce::Graphics& g) override {
        const auto& t = skin.theme();
        auto outer = getLocalBounds().toFloat().reduced(0.5f);
        auto b = outer.reduced(0.8f);

        // Outer recessed chassis groove
        g.setColour(juce::Colour(t.panelBorderLo));
        g.fillRoundedRectangle(outer, 5.0f);

        // Brushed dark-anodized plate gradient
        g.setGradientFill(juce::ColourGradient(juce::Colour(t.panelTop), b.getX(), b.getY(),
                                               juce::Colour(t.panelBottom), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle(b, 4.5f);

        // Subtle horizontal micro-brushed anodized texture
        g.setColour(juce::Colour(t.lightSurface ? 0x05000000 : 0x07ffffff));
        for (int y = int(b.getY()) + 3; y < int(b.getBottom()) - 2; y += 4)
            g.drawHorizontalLine(y, b.getX() + 3.0f, b.getRight() - 3.0f);

        if (!title.isEmpty()) {
            const float hh = float(headerHeight());
            auto hdr = b.withHeight(hh);
            g.setGradientFill(juce::ColourGradient(juce::Colour(t.panelHeaderTop), hdr.getX(), hdr.getY(),
                                                   juce::Colour(t.panelHeaderBottom), hdr.getX(), hdr.getBottom(), false));
            g.fillRoundedRectangle(hdr, 4.5f);

            const juce::Colour lamp = accent;
            if (compact) {
                g.setColour(lamp.withAlpha(0.28f));
                g.fillRoundedRectangle(6.5f, 3.5f, 6.0f, 15.0f, 2.5f);
                g.setColour(lamp);
                g.fillRoundedRectangle(8.0f, 4.8f, 2.8f, 12.4f, 1.3f);
                g.setColour(lamp.interpolatedWith(juce::Colours::white, t.lightSurface ? 0.25f : 0.45f));
                g.fillRoundedRectangle(8.8f, 5.8f, 1.2f, 10.4f, 0.6f);

                g.setColour(juce::Colour(t.textBright));
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText(title, 15, 2, getWidth() - 20, 18, juce::Justification::centredLeft);
                if (subtitle.isNotEmpty()) {
                    g.setColour(juce::Colour(t.textMuted));
                    g.setFont(juce::FontOptions(8.4f));
                    g.drawText(subtitle, 108, 3, getWidth() - 118, 16, juce::Justification::centredRight);
                }
            } else {
                g.setColour(lamp.withAlpha(0.26f));
                g.fillRoundedRectangle(8.5f, 7.5f, 6.0f, 25.0f, 2.5f);
                g.setColour(lamp);
                g.fillRoundedRectangle(10.0f, 9.0f, 3.0f, 22.0f, 1.4f);

                g.setColour(juce::Colour(t.textBright));
                g.setFont(juce::FontOptions(11.8f, juce::Font::bold));
                g.drawText(title, 18, 7, getWidth() - 36, 20, juce::Justification::centredLeft);
                if (subtitle.isNotEmpty()) {
                    g.setColour(juce::Colour(t.textMuted));
                    g.setFont(juce::FontOptions(8.8f));
                    g.drawText(subtitle, 18, 25, getWidth() - 36, 13, juce::Justification::centredLeft);
                }
            }

            // Etched header groove
            g.setColour(juce::Colour(t.lightSurface ? 0x24000000 : 0x90000000));
            g.drawHorizontalLine(int(b.getY() + hh), b.getX() + 4.0f, b.getRight() - 4.0f);
            g.setColour(juce::Colour(t.panelBorderHi).withAlpha(t.lightSurface ? 0.85f : 0.45f));
            g.drawHorizontalLine(int(b.getY() + hh) + 1, b.getX() + 4.0f, b.getRight() - 4.0f);
        }

        // Metallic bevel rim (with subtle neon edge glow in synthGrid mode)
        if (t.synthGrid) {
            g.setColour(juce::Colour(t.panelBorderHi).withAlpha(0.22f));
            g.drawRoundedRectangle(b.expanded(0.4f), 4.8f, 2.2f);
        }
        g.setColour(juce::Colour(t.panelBorderHi).withAlpha(t.lightSurface ? 0.90f : (t.synthGrid ? 0.80f : 0.68f)));
        g.drawRoundedRectangle(b, 4.5f, 1.0f);
    }

private:
    Skin& skin;
    juce::String title, subtitle;
    juce::Colour accent{0xff29ec6b};
    bool compact = false;
};
