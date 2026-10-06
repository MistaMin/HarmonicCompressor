#pragma once
#include "Toolkit.h"
#include "Panel.h"
#include "../PluginProcessor.h"

// IN / OUT level meters plus a gain-reduction bar. The face (cream VU, black VU, LED ladders)
// comes from Skin::meterFace. Levels are read from the processor's lock-free atomics.
class MeterPanel : public Panel, private juce::Timer {
public:
    MeterPanel(Skin& s, HarmonicCompressorAudioProcessor& p) : Panel(s, "METERS", "IN / OUT / GAIN REDUCTION"), skin(s), proc(p) { startTimerHz(30); }

    void paint(juce::Graphics& g) override {
        Panel::paint(g);
        namespace m = gj::meters;
        auto area = getLocalBounds().reduced(12, 0).withTrimmedTop(50).withTrimmedBottom(6);
        auto grRow = area.removeFromBottom(30);
        area.removeFromBottom(2);

        const float inL = proc.vuInputDbL.load(), inRt = proc.vuInputDbR.load();
        const float outL = proc.vuOutputDbL.load(), outRt = proc.vuOutputDbR.load();
        if (skin.meterFace == 2) {
            // Digital face: horizontal LED ladders, left/right stacked, one shared dB scale underneath.
            auto labelCol = area.removeFromLeft(26).toFloat();
            auto scaleRow = area.removeFromBottom(10).toFloat();
            const float rowH = (float(area.getHeight()) - 4.0f) / 2.0f;
            auto group = [&](float top, const char* name, float l, float r, float pl, float pr) {
                auto bars = juce::Rectangle<float>(float(area.getX()), top, float(area.getWidth()), rowH);
                const float barH = (rowH - 3.0f) / 2.0f;
                m::drawLedLadder(g, bars.withHeight(barH), true, l, pl, 24);
                m::drawLedLadder(g, bars.withTop(bars.getBottom() - barH), true, r, pr, 24);
                g.setColour(skin.mid()); g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
                g.drawText(name, labelCol.withY(top).withHeight(rowH), juce::Justification::centredLeft, false);
            };
            group(float(area.getY()), "IN", inL, inRt, proc.peakInputDbL.load(), proc.peakInputDbR.load());
            group(float(area.getY()) + rowH + 4.0f, "OUT", outL, outRt, proc.peakOutputDbL.load(), proc.peakOutputDbR.load());
            g.setColour(skin.mid()); g.setFont(juce::FontOptions(7.5f));
            for (int d : {-48, -36, -24, -12, 0}) {
                const float x = float(area.getX()) + m::dbPosition(float(d)) * float(area.getWidth());
                g.drawText(juce::String(d), juce::Rectangle<float>(24, 9).withCentre({juce::jlimit(float(area.getX()) + 12, float(area.getRight()) - 12, x), scaleRow.getCentreY()}),
                           juce::Justification::centred, false);
            }
        } else {
            auto half = area.getWidth() / 2;
            auto inR = area.removeFromLeft(half).reduced(2).withTrimmedTop(12).toFloat();
            auto outR = area.reduced(2).withTrimmedTop(12).toFloat();
            const auto face = skin.meterFace == 1 ? m::VuFace::Black : m::VuFace::Cream;
            constexpr float vuReferenceDbfs = -18.0f;     // 0 VU
            m::drawVuMeter(g, inR, m::vuPositionForDb(0.5f * (inL + inRt) - vuReferenceDbfs), face);
            m::drawVuMeter(g, outR, m::vuPositionForDb(0.5f * (outL + outRt) - vuReferenceDbfs), face);
            g.setColour(skin.mid()); g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
            g.drawText("IN", inR.withY(inR.getY() - 12).withHeight(11), juce::Justification::centred, false);
            g.drawText("OUT", outR.withY(outR.getY() - 12).withHeight(11), juce::Justification::centred, false);
        }

        // gain reduction: fills from the right edge towards the left (0 .. 24 dB)
        const float gr = juce::jlimit(0.0f, 24.0f, proc.vuGrDb.load());
        auto bar = grRow.toFloat().withTrimmedLeft(26).withTrimmedBottom(11).withHeight(16);
        g.setColour(juce::Colour(0xff0b0c0d)); g.fillRoundedRectangle(bar, 3);
        auto fill = bar.reduced(2);
        fill = fill.withLeft(fill.getRight() - fill.getWidth() * gr / 24.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff0b323), fill.getX(), 0, juce::Colour(0xffe5382a), fill.getRight(), 0, false));
        g.fillRoundedRectangle(fill, 2);
        g.setColour(skin.mid()); g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText("GR", grRow.removeFromLeft(24).withHeight(18), juce::Justification::centredLeft, false);
        g.setFont(juce::FontOptions(7.5f));
        for (int d : {0, 6, 12, 18, 24}) {
            const float x = bar.getRight() - 2 - (bar.getWidth() - 4) * float(d) / 24.0f;
            g.drawText(juce::String(d), juce::Rectangle<float>(20, 9).withCentre({x, bar.getBottom() + 6.0f}), juce::Justification::centred, false);
        }
    }
private:
    void timerCallback() override { if (isShowing()) repaint(); }
    Skin& skin;
    HarmonicCompressorAudioProcessor& proc;
};
