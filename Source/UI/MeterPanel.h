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
        auto area = getLocalBounds().reduced(12, 0).withTrimmedTop(54).withTrimmedBottom(8);
        auto grRow = area.removeFromBottom(28);
        area.removeFromBottom(4);
        auto half = area.getWidth() / 2;
        auto inR = area.removeFromLeft(half).reduced(2).toFloat();
        auto outR = area.reduced(2).toFloat();

        const float inL = proc.vuInputDbL.load(), inRt = proc.vuInputDbR.load();
        const float outL = proc.vuOutputDbL.load(), outRt = proc.vuOutputDbR.load();
        if (skin.meterFace == 2) {
            m::drawStereoLadder(g, inR, inL, inRt, proc.peakInputDbL.load());
            m::drawStereoLadder(g, outR, outL, outRt, proc.peakOutputDbL.load());
        } else {
            const auto face = skin.meterFace == 1 ? m::VuFace::Black : m::VuFace::Cream;
            constexpr float vuReferenceDbfs = -18.0f;     // 0 VU
            m::drawVuMeter(g, inR, m::vuPositionForDb(0.5f * (inL + inRt) - vuReferenceDbfs), face);
            m::drawVuMeter(g, outR, m::vuPositionForDb(0.5f * (outL + outRt) - vuReferenceDbfs), face);
        }
        g.setColour(skin.mid()); g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText("IN", inR.withY(inR.getY() - 11).withHeight(10), juce::Justification::centred);
        g.drawText("OUT", outR.withY(outR.getY() - 11).withHeight(10), juce::Justification::centred);

        // gain reduction: fills from the right edge towards the left (0 .. 24 dB)
        const float gr = juce::jlimit(0.0f, 24.0f, proc.vuGrDb.load());
        auto bar = grRow.toFloat().withTrimmedLeft(22).withTrimmedBottom(10);
        g.setColour(juce::Colour(0xff0b0c0d)); g.fillRoundedRectangle(bar, 3);
        auto fill = bar.reduced(2);
        fill = fill.withLeft(fill.getRight() - fill.getWidth() * gr / 24.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff0b323), fill.getX(), 0, juce::Colour(0xffe5382a), fill.getRight(), 0, false));
        g.fillRoundedRectangle(fill, 2);
        g.setColour(skin.mid()); g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText("GR", grRow.removeFromLeft(20).withHeight(16), juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(7.5f));
        for (int d : {0, 6, 12, 18, 24}) {
            const float x = bar.getRight() - 2 - (bar.getWidth() - 4) * float(d) / 24.0f;
            g.drawText(juce::String(d), juce::Rectangle<float>(20, 9).withCentre({x, bar.getBottom() + 6.0f}), juce::Justification::centred);
        }
    }
private:
    void timerCallback() override { if (isShowing()) repaint(); }
    Skin& skin;
    HarmonicCompressorAudioProcessor& proc;
};
