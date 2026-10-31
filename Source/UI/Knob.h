#pragma once
#include "Toolkit.h"
#include "Panel.h"
#include <cmath>
#include <functional>

// An electro-optical parameter control that operates either as:
// - a linear horizontal dark-slot illuminated fader (`rotary = false`), or
// - a bespoke 25-segment LED-arc + 12-lobe CNC-scalloped anodized turbine rotary dial with a
//   recessed OLED digital readout console (`rotary = true`: Threshold, Ratio, Makeup, Mix).
// Inherits juce::Slider so SliderAttachment preserves parameter IDs, automation, gestures, and
// GoodLookinUI design metadata.
class Knob : public juce::Slider, private juce::Timer {
public:
    using Formatter = std::function<juce::String(double)>;

    Knob(const juce::String& label, juce::Colour colour, Formatter fmt,
         bool rotaryMode = false, bool secondaryAccent = false)
        : format(std::move(fmt)), rotary(rotaryMode), useSecondary(secondaryAccent)
    {
        if (rotary) {
            setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f, true);
            setMouseDragSensitivity(170);
        } else {
            setLookAndFeel(&sliderLnf());
            setSliderStyle(juce::Slider::LinearHorizontal);
        }
        setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        setScrollWheelEnabled(true);
        design.label = label.toStdString();
        design.colour = "#" + colour.toDisplayString(false).toStdString();
        design.style = rotary ? "Turbine" : "Mtl";
        design.fontSize = 9.5f;
        textFromValueFunction = format;
        startTimerHz(60);
    }

    ~Knob() override {
        setLookAndFeel(nullptr);
    }

    bool isRotaryKnob() const noexcept { return rotary; }
    void setSkin(const Skin* s) noexcept { skin = s; repaint(); }
    void setSecondaryAccent(bool sec) noexcept { useSecondary = sec; repaint(); }
    void setActive(bool a) { if (active != a) { active = a; repaint(); } }
    void setTextColours(juce::Colour t, juce::Colour m) { text = t; mid = m; repaint(); }

    void resized() override {
        auto b = getLocalBounds();
        if (rotary) {
            const int side = juce::jmin(b.getHeight() - 2, juce::jmin(62, b.getWidth() / 2));
            if (b.getWidth() >= side + 56) {
                dialArea = juce::Rectangle<int>(b.getX() + 2, b.getCentreY() - side / 2, side, side);
                headerArea = juce::Rectangle<int>(dialArea.getRight() + 8, b.getCentreY() - 22,
                                                  b.getRight() - dialArea.getRight() - 10, 44);
            } else {
                headerArea = b.removeFromTop(14);
                dialArea = b.reduced(2);
            }
            trackArea = dialArea;
        } else {
            headerArea = b.removeFromTop(16);
            trackArea = b;
        }
    }

    juce::Rectangle<int> getTrackBounds() const noexcept { return trackArea; }

    void valueChanged() override {
        juce::Slider::valueChanged();
        if (!positionSeeded || !isShowing()) {
            motion.position = valueToProportionOfLength(getValue());
            motion.velocity = 0.0;
            positionSeeded = true;
        }
    }

    void paint(juce::Graphics& g) override {
        if (!positionSeeded || !isShowing()) {
            motion.position = valueToProportionOfLength(getValue());
            motion.velocity = 0.0;
            positionSeeded = true;
        }
        const auto& t = currentTheme();
        const bool enabledState = active && isEnabled();
        const juce::Colour themeAccent(useSecondary ? t.accentSecondary : t.accentPrimary);
        const juce::Colour accent = themeAccent.interpolatedWith(parseDesignColour(), 0.14f)
                                               .withMultipliedAlpha(enabledState ? 1.0f : 0.42f);

        if (rotary) {
            paintRotary(g, t, enabledState, accent);
            return;
        }

        paintHorizontalSlider(g, t, enabledState, accent);
    }

    std::function<void()> onSelect;
    void mouseEnter(const juce::MouseEvent& e) override { hovered = true; repaint(); juce::Slider::mouseEnter(e); }
    void mouseExit(const juce::MouseEvent& e) override  { hovered = false; repaint(); juce::Slider::mouseExit(e); }
    void mouseDown(const juce::MouseEvent& e) override  { draggingKnob = true; repaint(); if (onSelect) onSelect(); juce::Slider::mouseDown(e); }
    void mouseUp(const juce::MouseEvent& e) override    { draggingKnob = false; repaint(); juce::Slider::mouseUp(e); }

    void applyDesign(const glui::Item& item) { design = item; repaint(); }
    const glui::Item& getDesign() const { return design; }

private:
    static constexpr int kThumbRadius = 6;

    const ColourTheme& currentTheme() const noexcept {
        if (skin != nullptr)
            return skin->theme();
        if (auto* p = dynamic_cast<const Panel*>(getParentComponent()))
            return p->getSkin().theme();
        return Skin::defaultTheme();
    }

    static juce::Colour segmentColourFor(const ColourTheme& t, juce::Colour baseAccent, float f) noexcept {
        const juce::Colour safe = baseAccent.interpolatedWith(juce::Colour(t.lampSafe), 0.35f);
        const juce::Colour warn(t.lampWarn);
        const juce::Colour hot(t.lampHot);
        if (f < 0.60f)
            return safe.interpolatedWith(warn, juce::jlimit(0.0f, 1.0f, (f - 0.38f) / 0.22f) * 0.35f);
        if (f < 0.86f)
            return safe.interpolatedWith(warn, 0.35f + 0.65f * ((f - 0.60f) / 0.26f));
        return warn.interpolatedWith(hot, juce::jlimit(0.0f, 1.0f, (f - 0.86f) / 0.14f));
    }

    static juce::Colour ghostColourFor(const ColourTheme& t, float f) noexcept {
        if (f < 0.60f) return juce::Colour(t.lampGhostSafe);
        if (f < 0.86f) return juce::Colour(t.lampGhostWarn);
        return juce::Colour(t.lampGhostHot);
    }

    void paintHorizontalSlider(juce::Graphics& g, const ColourTheme& t, bool enabledState, juce::Colour accent) {
        const float alpha = enabledState ? 1.0f : 0.45f;
        const int readoutW = juce::jlimit(40, 58, getWidth() <= 96 ? (getWidth() - 34) : (getWidth() / 2 - 2));
        auto topRow = headerArea;
        auto valueBox = topRow.removeFromRight(readoutW).toFloat().reduced(0.4f, 0.4f);
        auto labelBox = topRow.withTrimmedRight(2);

        // Parameter label
        g.setColour(enabledState ? juce::Colour(t.textBright) : juce::Colour(t.textMuted).withAlpha(0.65f));
        g.setFont(juce::FontOptions(juce::jlimit(8.2f, 10.0f, getWidth() <= 96 ? 8.5f : design.fontSize), juce::Font::bold));
        g.drawFittedText(design.label, labelBox, juce::Justification::centredLeft, 1, 0.78f);

        // Recessed OLED / e-ink value pill
        g.setColour(juce::Colour(t.oledBg).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(valueBox, 2.8f);
        g.setColour((hovered || draggingKnob ? accent.withAlpha(0.80f) : juce::Colour(t.oledBorder)).withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(valueBox, 2.8f, 0.85f);

        const juce::Colour digitCol = t.lightSurface
            ? juce::Colour(t.oledText).interpolatedWith(accent.darker(0.25f), 0.28f)
            : accent.interpolatedWith(juce::Colour(t.oledText), 0.50f).brighter(0.22f);
        g.setColour(enabledState ? digitCol : juce::Colour(t.textMuted).withAlpha(0.65f));
        g.setFont(juce::FontOptions(getWidth() <= 96 ? 8.5f : 9.1f, juce::Font::bold));
        g.drawFittedText(format(getValue()), valueBox.toNearestInt().reduced(2, 0), juce::Justification::centred, 1, 0.76f);

        // Recessed fader slot + luminous/ink bar + CNC anodized thumb cap
        const float thumbHalfW = float(kThumbRadius);
        const float x0 = float(trackArea.getX()) + thumbHalfW;
        const float x1 = juce::jmax(x0 + 1.0f, float(trackArea.getRight()) - thumbHalfW);
        const float cy = float(trackArea.getCentreY());
        const float pos = juce::jlimit(0.0f, 1.0f, float(motion.position));
        const float thumbX = x0 + pos * (x1 - x0);

        const float grooveH = 6.4f;
        juce::Rectangle<float> groove(x0, cy - grooveH * 0.5f, x1 - x0, grooveH);

        // Scale ticks beneath groove
        g.setColour(juce::Colour(t.textMuted).withAlpha(0.50f * alpha));
        for (int i = 0; i <= 4; ++i) {
            const float tx = x0 + (x1 - x0) * (float(i) / 4.0f);
            const float tickLen = (i == 0 || i == 2 || i == 4) ? 3.8f : 2.3f;
            g.drawLine(tx, groove.getBottom() + 1.8f, tx, groove.getBottom() + 1.8f + tickLen, 0.85f);
        }

        // Recessed fader groove
        g.setColour(t.lightSurface ? juce::Colour(0xffd8d4ca) : juce::Colour(t.wellBgBottom));
        g.fillRoundedRectangle(groove.expanded(0.6f), (grooveH + 1.2f) * 0.5f);
        g.setColour(t.lightSurface ? juce::Colour(0xffb6b1a5).withMultipliedAlpha(alpha)
                                   : juce::Colour(t.panelBorderHi).withAlpha(0.55f * alpha));
        g.drawRoundedRectangle(groove, grooveH * 0.5f, 0.85f);

        // Active fill bar
        if (thumbX > x0 + 0.6f) {
            juce::Rectangle<float> fillRect(x0, groove.getY() + 0.6f, thumbX - x0, grooveH - 1.2f);
            const juce::Colour endCol = segmentColourFor(t, accent, pos * 0.85f).withMultipliedAlpha(alpha);
            g.setColour(endCol.withAlpha((t.lightSurface ? 0.16f : 0.24f) * alpha));
            g.fillRoundedRectangle(fillRect.expanded(1.4f, 1.6f), grooveH * 0.6f);
            g.setGradientFill(juce::ColourGradient(accent.darker(t.lightSurface ? 0.10f : 0.25f), fillRect.getX(), fillRect.getY(),
                                                   t.lightSurface ? endCol : endCol.brighter(0.20f), fillRect.getRight(), fillRect.getY(), false));
            g.fillRoundedRectangle(fillRect, (grooveH - 1.2f) * 0.5f);
        }

        // Machined anodized / satin-ceramic fader cap with precision center slit
        const float capW = 11.5f;
        const float capH = juce::jmin(18.0f, juce::jmax(13.5f, float(trackArea.getHeight()) - 6.0f));
        const auto cap = juce::Rectangle<float>(capW, capH).withCentre({ thumbX, cy });

        g.setColour(juce::Colour(t.lightSurface ? 0x2c000000 : 0x88000000));
        g.fillRoundedRectangle(cap.translated(0.0f, 1.5f), 2.6f);

        g.setGradientFill(juce::ColourGradient(juce::Colour(t.rotorOuterHi).withMultipliedAlpha(alpha), cap.getX(), cap.getY(),
                                               juce::Colour(t.rotorOuterLo).withMultipliedAlpha(alpha), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 2.6f);
        g.setColour((hovered || draggingKnob ? accent.withAlpha(0.90f)
                                             : (t.lightSurface ? juce::Colour(0xff948e83)
                                                               : juce::Colour(t.rotorFluteHi).withAlpha(0.85f))).withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(cap, 2.6f, 0.95f);

        // Center indicator slit on fader cap
        g.setColour(accent.withAlpha((t.lightSurface ? 0.22f : 0.36f) * alpha));
        g.fillRoundedRectangle(thumbX - 2.3f, cap.getY() + 1.8f, 4.6f, cap.getHeight() - 3.6f, 1.6f);
        g.setColour((t.lightSurface ? accent : accent.brighter(0.35f)).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(thumbX - 1.0f, cap.getY() + 2.4f, 2.0f, cap.getHeight() - 4.8f, 0.9f);
        g.setColour((t.lightSurface ? juce::Colour(0xff090c10) : juce::Colours::white).withAlpha(0.82f * alpha));
        g.fillRect(thumbX - 0.35f, cap.getY() + 3.2f, 0.7f, cap.getHeight() - 6.4f);
    }

    void paintRotary(juce::Graphics& g, const ColourTheme& t, bool enabledState, juce::Colour accent) {
        const float pi = juce::MathConstants<float>::pi;
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float alpha = enabledState ? 1.0f : 0.45f;
        const float diameter = float(juce::jmin(dialArea.getWidth(), dialArea.getHeight()));
        const juce::Point<float> c = dialArea.toFloat().getCentre();
        const float R = diameter * 0.5f;
        const float pos = juce::jlimit(0.0f, 1.0f, float(motion.position));
        const float loAng = -0.80f * pi;
        const float hiAng =  0.80f * pi;
        const float curAng = loAng + pos * (hiAng - loAng);

        auto ptAt = [&](float ang, float rad) noexcept -> juce::Point<float> {
            return { c.x + std::sin(ang) * rad, c.y - std::cos(ang) * rad };
        };

        // 1. Recessed circular optical / ceramic housing dish
        const float rDish = R * 0.985f;
        g.setGradientFill(juce::ColourGradient(t.lightSurface ? juce::Colour(0xffdedad0) : juce::Colour(t.wellBgBottom), c.x, c.y - rDish,
                                               t.lightSurface ? juce::Colour(0xffece9e2) : juce::Colour(t.wellBgTop), c.x, c.y + rDish, false));
        g.fillEllipse(c.x - rDish, c.y - rDish, rDish * 2.0f, rDish * 2.0f);
        g.setColour(t.lightSurface ? juce::Colour(0xffbdb8ad) : juce::Colour(t.panelBorderHi).withAlpha(0.60f));
        g.drawEllipse(c.x - rDish, c.y - rDish, rDish * 2.0f, rDish * 2.0f, 1.0f);

        // 2. 25-Segment Radial Electro-Optical LED Lamp Arc
        constexpr int kNumSegs = 25;
        const float rSegIn  = R * 0.745f;
        const float rSegOut = R * 0.935f;
        const float sweep = hiAng - loAng;
        const float stepAng = sweep / float(kNumSegs - 1);
        const float halfSeg = stepAng * 0.34f;

        const juce::Colour tipCol = segmentColourFor(t, accent, pos).withMultipliedAlpha(alpha);

        // Continuous soft bloom behind the lit arc portion
        if (pos > 0.005f && enabledState) {
            juce::Path glowArc;
            glowArc.addCentredArc(c.x, c.y, (rSegIn + rSegOut) * 0.5f, (rSegIn + rSegOut) * 0.5f,
                                  0.0f, loAng, curAng, true);
            g.setColour(tipCol.withAlpha(t.lightSurface ? (hovered || draggingKnob ? 0.16f : 0.10f)
                                                        : (hovered || draggingKnob ? 0.28f : 0.19f)));
            g.strokePath(glowArc, juce::PathStrokeType(R * 0.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        int lastLitIdx = -1;
        for (int i = 0; i < kNumSegs; ++i) {
            const float f = float(i) / float(kNumSegs - 1);
            if ((i == 0 && pos > 0.002f) || (i > 0 && f <= pos + 0.012f))
                lastLitIdx = i;
        }

        for (int i = 0; i < kNumSegs; ++i) {
            const float f = float(i) / float(kNumSegs - 1);
            const float ang = loAng + f * sweep;
            const float a0 = ang - halfSeg;
            const float a1 = ang + halfSeg;

            juce::Path seg;
            seg.startNewSubPath(ptAt(a0, rSegIn));
            seg.lineTo(ptAt(a0, rSegOut));
            seg.lineTo(ptAt(a1, rSegOut));
            seg.lineTo(ptAt(a1, rSegIn));
            seg.closeSubPath();

            const bool lit = (i <= lastLitIdx);
            const bool isTip = (i == lastLitIdx && lastLitIdx >= 0);
            if (lit) {
                juce::Colour col = segmentColourFor(t, accent, f).withMultipliedAlpha(alpha);
                if (isTip)
                    col = t.lightSurface ? col.darker(0.20f) : col.interpolatedWith(juce::Colours::white, 0.50f);
                g.setColour(col);
                g.fillPath(seg);
            } else {
                g.setColour(ghostColourFor(t, f).withMultipliedAlpha(alpha));
                g.fillPath(seg);
            }
        }

        // 3. Recessed moat separating lamp ring from physical turbine rotor
        const float rMoat = R * 0.70f;
        g.setColour(juce::Colour(t.lightSurface ? 0xffbab4a8 : 0xff020406));
        g.fillEllipse(c.x - rMoat, c.y - rMoat, rMoat * 2.0f, rMoat * 2.0f);

        // 4. 12-Lobe CNC-Scalloped Rotating Anodized / Satin-Ceramic Turbine Rotor
        const float rRotor = R * 0.655f;
        constexpr int kSteps = 72;
        juce::Path turbine;
        for (int s = 0; s < kSteps; ++s) {
            const float theta = twoPi * float(s) / float(kSteps);
            const float lobe = 0.952f + 0.048f * std::cos(12.0f * (theta - curAng));
            const auto p = ptAt(theta, rRotor * lobe);
            if (s == 0) turbine.startNewSubPath(p);
            else        turbine.lineTo(p);
        }
        turbine.closeSubPath();

        // Rotor contact shadow
        g.setColour(juce::Colour(t.lightSurface ? 0x2e000000 : 0x95000000));
        g.fillPath(turbine, juce::AffineTransform::translation(0.9f, 1.9f));

        // Directional anodized / satin-ceramic body
        g.setGradientFill(juce::ColourGradient(juce::Colour(t.rotorOuterHi).withMultipliedAlpha(alpha), c.x - rRotor * 0.8f, c.y - rRotor * 0.8f,
                                               juce::Colour(t.rotorOuterLo).withMultipliedAlpha(alpha), c.x + rRotor * 0.85f, c.y + rRotor * 0.85f, false));
        g.fillPath(turbine);

        // 36 rotating radial knurl flutes on the outer skirt
        constexpr int kFlutes = 36;
        for (int fIdx = 0; fIdx < kFlutes; ++fIdx) {
            const float fa = curAng + twoPi * float(fIdx) / float(kFlutes);
            g.setColour((fIdx % 2 == 0 ? juce::Colour(t.rotorFluteHi).withAlpha(t.lightSurface ? 0.72f : 0.52f)
                                       : juce::Colour(t.rotorFluteLo).withAlpha(t.lightSurface ? 0.62f : 0.78f)).withMultipliedAlpha(alpha));
            g.drawLine(juce::Line<float>(ptAt(fa, rRotor * 0.73f), ptAt(fa, rRotor * 0.91f)), 1.05f);
        }

        // Directional 3D metallic rim highlight on turbine
        g.setGradientFill(juce::ColourGradient(juce::Colour(t.rotorFluteHi).withAlpha(0.85f * alpha), c.x - rRotor, c.y - rRotor,
                                               t.lightSurface ? juce::Colour(0x90706b61) : juce::Colour(0xd0000000), c.x + rRotor, c.y + rRotor, false));
        g.strokePath(turbine, juce::PathStrokeType(1.05f));

        // 5. 360-degree Collar Ring & Satin-Lathed Core Cap
        const float rCollar = R * 0.465f;
        g.setColour(juce::Colour(t.lightSurface ? 0xffc7c2b7 : 0xff040609));
        g.fillEllipse(c.x - rCollar - 1.2f, c.y - rCollar - 1.2f, (rCollar + 1.2f) * 2.0f, (rCollar + 1.2f) * 2.0f);

        g.setColour(tipCol.withAlpha((hovered || draggingKnob ? 0.36f : 0.22f) * alpha));
        g.drawEllipse(c.x - rCollar, c.y - rCollar, rCollar * 2.0f, rCollar * 2.0f, 3.0f);
        g.setColour(tipCol.withAlpha((hovered || draggingKnob ? 0.95f : 0.78f) * alpha));
        g.drawEllipse(c.x - rCollar, c.y - rCollar, rCollar * 2.0f, rCollar * 2.0f, 1.15f);

        const float rCap = R * 0.415f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(t.rotorCapHi).withMultipliedAlpha(alpha), c.x - rCap * 0.7f, c.y - rCap * 0.7f,
                                               juce::Colour(t.rotorCapLo).withMultipliedAlpha(alpha), c.x + rCap * 0.8f, c.y + rCap * 0.8f, false));
        g.fillEllipse(c.x - rCap, c.y - rCap, rCap * 2.0f, rCap * 2.0f);

        // Micro-lathe concentric satin grooves
        for (int ringIdx = 1; ringIdx <= 3; ++ringIdx) {
            const float rr = rCap * float(ringIdx) / 3.5f;
            g.setColour(juce::Colour(ringIdx % 2 ? (t.lightSurface ? 0x34ffffff : 0x16ffffff)
                                                 : (t.lightSurface ? 0x12000000 : 0x22000000)));
            g.drawEllipse(c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.55f);
        }

        // Top-left curved specular sheen on core cap
        juce::Path capGloss;
        capGloss.addCentredArc(c.x, c.y, rCap * 0.84f, rCap * 0.84f, 0.0f, -0.78f * pi, 0.12f * pi, true);
        g.setColour(juce::Colours::white.withAlpha((t.lightSurface ? 0.55f : 0.18f) * alpha));
        g.strokePath(capGloss, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 6. Optical Laser / Obsidian Ink Pointer Blade + Center Hub
        const auto pInner = ptAt(curAng, R * 0.10f);
        const auto pOuter = ptAt(curAng, rRotor * 0.95f);
        g.setColour(tipCol.withAlpha((t.lightSurface ? 0.20f : 0.34f) * alpha));
        g.drawLine({ pInner, pOuter }, t.lightSurface ? 4.2f : 5.0f);
        g.setColour(t.lightSurface ? tipCol : tipCol.brighter(0.32f));
        g.drawLine({ pInner, pOuter }, 2.3f);
        g.setColour((t.lightSurface ? juce::Colour(0xff090c10) : juce::Colours::white).withAlpha(0.92f * alpha));
        g.drawLine({ ptAt(curAng, R * 0.14f), ptAt(curAng, rRotor * 0.90f) }, 0.95f);

        const auto tipJewel = ptAt(curAng, rRotor * 0.88f);
        g.setColour(tipCol.withAlpha((t.lightSurface ? 0.30f : 0.45f) * alpha));
        g.fillEllipse(tipJewel.x - 3.2f, tipJewel.y - 3.2f, 6.4f, 6.4f);
        g.setColour((t.lightSurface ? tipCol.darker(0.25f) : juce::Colours::white).withAlpha(0.95f * alpha));
        g.fillEllipse(tipJewel.x - 1.3f, tipJewel.y - 1.3f, 2.6f, 2.6f);

        const float rHub = R * 0.115f;
        g.setColour(t.lightSurface ? juce::Colour(0xffb5afa3) : juce::Colour(t.rotorCapLo));
        g.fillEllipse(c.x - rHub, c.y - rHub, rHub * 2.0f, rHub * 2.0f);
        g.setColour((t.lightSurface ? juce::Colour(0xff78736a) : juce::Colour(t.rotorFluteHi)).withAlpha(0.75f * alpha));
        g.drawEllipse(c.x - rHub, c.y - rHub, rHub * 2.0f, rHub * 2.0f, 0.8f);

        // 7. Recessed OLED / E-Ink Readout Console next to the Rotary Dial
        auto info = headerArea;
        auto labelRow = info.removeFromTop(17);
        info.removeFromTop(2);
        auto oledRect = info.removeFromTop(24).toFloat();

        // Micro-LED status pip + tracked title
        const float pipY = float(labelRow.getCentreY()) - 2.0f;
        g.setColour(tipCol.withAlpha(0.32f * alpha));
        g.fillEllipse(float(labelRow.getX()) - 1.2f, pipY - 1.2f, 6.4f, 6.4f);
        g.setColour(tipCol);
        g.fillEllipse(float(labelRow.getX()), pipY, 4.0f, 4.0f);

        g.setColour(enabledState ? juce::Colour(t.textBright) : juce::Colour(t.textMuted));
        g.setFont(juce::FontOptions(9.8f, juce::Font::bold));
        g.drawFittedText(design.label, labelRow.withTrimmedLeft(8), juce::Justification::centredLeft, 1, 0.84f);

        // Recessed OLED / E-Ink display window
        g.setColour(juce::Colour(t.oledBg));
        g.fillRoundedRectangle(oledRect, 3.8f);

        // Subtle readout micro-scanlines
        g.setColour(juce::Colour(t.lightSurface ? 0x06000000 : 0x0cffffff));
        for (int sy = int(oledRect.getY()) + 3; sy < int(oledRect.getBottom()) - 3; sy += 3)
            g.drawHorizontalLine(sy, oledRect.getX() + 3.0f, oledRect.getRight() - 3.0f);

        // Thin proportional bar along the bottom inside the readout display
        if (pos > 0.005f) {
            const float barMaxW = juce::jmax(4.0f, oledRect.getWidth() - 8.0f);
            const juce::Rectangle<float> miniBar(oledRect.getX() + 4.0f, oledRect.getBottom() - 3.3f,
                                                 barMaxW * pos, 1.7f);
            g.setColour(tipCol.withAlpha(0.82f * alpha));
            g.fillRoundedRectangle(miniBar, 0.8f);
        }

        // Readout bezel
        if (hovered || draggingKnob) {
            g.setColour(tipCol.withAlpha(0.24f * alpha));
            g.drawRoundedRectangle(oledRect.expanded(0.6f), 4.2f, 2.2f);
        }
        g.setColour((hovered || draggingKnob ? tipCol.withAlpha(0.88f) : juce::Colour(t.oledBorder)).withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(oledRect, 3.8f, 1.0f);

        // Digital value digits
        const auto valStr = format(getValue());
        const auto textBox = oledRect.withTrimmedBottom(2.0f).toNearestInt().reduced(4, 0);
        const juce::Colour oledCol = t.lightSurface
            ? juce::Colour(t.oledText).interpolatedWith(tipCol.darker(0.25f), 0.25f)
            : tipCol.interpolatedWith(juce::Colour(t.oledText), 0.45f).brighter(0.28f);
        g.setFont(juce::FontOptions(10.6f, juce::Font::bold));
        if (enabledState && !t.lightSurface) {
            g.setColour(oledCol.withAlpha(0.25f));
            g.drawFittedText(valStr, textBox.translated(0, 1), juce::Justification::centred, 1, 0.80f);
        }
        g.setColour(enabledState ? oledCol : juce::Colour(t.textMuted));
        g.drawFittedText(valStr, textBox, juce::Justification::centred, 1, 0.80f);
    }

    struct HorizontalTrackLnf : juce::LookAndFeel_V4 {
        juce::Slider::SliderLayout getSliderLayout(juce::Slider& s) override {
            juce::Slider::SliderLayout layout;
            if (auto* k = dynamic_cast<const Knob*>(&s))
                layout.sliderBounds = k->getTrackBounds();
            else
                layout.sliderBounds = s.getLocalBounds();
            return layout;
        }
        int getSliderThumbRadius(juce::Slider&) override { return kThumbRadius; }
    };

    static HorizontalTrackLnf& sliderLnf() {
        static HorizontalTrackLnf instance;
        return instance;
    }

    juce::Colour parseDesignColour() const noexcept {
        if (design.colour.size() == 7 && design.colour[0] == '#')
            return juce::Colour::fromString("ff" + juce::String(design.colour.substr(1)));
        return juce::Colour(0xff29ec6b);
    }

    void timerCallback() override {
        const auto target = valueToProportionOfLength(getValue());
        if (!isShowing()) { motion.position = target; motion.velocity = 0; return; }
        if (std::abs(motion.position - target) < 0.0001 && std::abs(motion.velocity) < 0.0001) return;
        motion.step(target, 1.0 / 60.0);
        repaint();
    }

    const Skin* skin = nullptr;
    Formatter format;
    glui::Item design;
    glui::Motion motion;
    bool rotary = false;
    bool useSecondary = false;
    bool active = true;
    bool hovered = false;
    bool draggingKnob = false;
    bool positionSeeded = false;
    juce::Colour text{0xffc0d6cc}, mid{0xff709184};
    juce::Rectangle<int> headerArea, trackArea, dialArea;
};
