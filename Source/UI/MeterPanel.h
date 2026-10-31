#pragma once
#include "Panel.h"
#include "KeyButton.h"
#include "FloatingModule.h"
#include "../PluginProcessor.h"
#include "../TransferCurveDisplay.h"

// Calibrated Input / Output+GR 44-segment electro-optical lamp meter (-18 dBFS = 0 dBu, dual dBu / dBFS markers).
class MeterPanel : public Panel, private juce::Timer {
public:
    static constexpr float kDbuOffset = 18.0f; // -18 dBFS = 0 dBu

    MeterPanel(Skin& s, HarmonicCompressorAudioProcessor& p, bool inputOnly = false)
      : Panel(s, inputOnly ? "INPUT" : "OUT / GR", {}, true), mrSkin(s), proc(p), input(inputOnly)
    {
        setAccent(juce::Colour(s.theme().accentPrimary));
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override {
        const auto& t = mrSkin.theme();
        Panel::paint(g);

        const int lanes = input ? 2 : 3;
        const float top = input ? 44.0f : 58.0f;
        const float bottom = float(getHeight() - 50);
        const float height = juce::jmax(60.0f, bottom - top);
        const float laneWidth = input ? 11.5f : 10.5f;
        const float left = 8.0f;
        const float rawGr = juce::jlimit(0.0f, 36.0f, -proc.lastGrDbForUI.load(std::memory_order_relaxed));
        const float values[] = {
            input ? proc.peakInputDbL.load(std::memory_order_relaxed) : proc.peakOutputDbL.load(std::memory_order_relaxed),
            input ? proc.peakInputDbR.load(std::memory_order_relaxed) : proc.peakOutputDbR.load(std::memory_order_relaxed),
            rawGr < 0.05f ? 0.0f : rawGr
        };

        // Top GR digital OLED / E-Ink readout on OUT / GR panel
        if (!input) {
            const juce::Rectangle<float> grOled(6.0f, 25.0f, float(getWidth() - 12), 16.0f);
            g.setColour(juce::Colour(t.oledBg));
            g.fillRoundedRectangle(grOled, 3.0f);
            g.setColour(juce::Colour(t.grStroke).withAlpha(0.45f));
            g.drawRoundedRectangle(grOled, 3.0f, 0.8f);
            g.setColour(t.lightSurface ? juce::Colour(t.grStroke).darker(0.15f)
                                       : juce::Colour(t.grStroke).brighter(0.20f));
            g.setFont(juce::FontOptions(8.6f, juce::Font::bold));
            g.drawText(juce::String(values[2], 1) + " dB GR", grOled.toNearestInt(), juce::Justification::centred);
        }

        // Top scale unit hint (cleanly above the +18/0 top tick)
        g.setColour(juce::Colour(t.textMuted));
        g.setFont(juce::FontOptions(7.3f, juce::Font::bold));
        g.drawText("dBu/FS", int(left + laneWidth * 2.0f + 8.0f), int(top) - 16, 42, 10, juce::Justification::centredLeft);

        // Recessed optical / ceramic well behind L/R stereo lamp columns
        const juce::Colour meterWellTop = t.lightSurface ? juce::Colour(0xffe5e2da) : juce::Colour(t.wellBgTop);
        const juce::Colour meterWellBot = t.lightSurface ? juce::Colour(0xffddd9cf) : juce::Colour(t.wellBgBottom);
        const juce::Rectangle<float> lrWell(left - 2.5f, top - 2.5f, laneWidth * 2.0f + 8.5f, height + 5.0f);
        g.setGradientFill(juce::ColourGradient(meterWellTop, lrWell.getX(), lrWell.getY(),
                                               meterWellBot, lrWell.getX(), lrWell.getBottom(), false));
        g.fillRoundedRectangle(lrWell, 3.0f);
        g.setColour(juce::Colour(t.wellBezelOuter).withAlpha(t.lightSurface ? 0.85f : 0.65f));
        g.drawRoundedRectangle(lrWell, 3.0f, 0.9f);

        // Separate recessed optical / ceramic well for downward GR lamp column
        if (!input) {
            const float grX = left + 2.0f * (laneWidth + 3.5f) + 41.5f;
            const juce::Rectangle<float> grWell(grX - 2.5f, top - 2.5f, laneWidth + 5.0f, height + 5.0f);
            g.setGradientFill(juce::ColourGradient(meterWellTop, grWell.getX(), grWell.getY(),
                                                   meterWellBot, grWell.getX(), grWell.getBottom(), false));
            g.fillRoundedRectangle(grWell, 3.0f);
            g.setColour(juce::Colour(t.wellBezelOuter).withAlpha(t.lightSurface ? 0.85f : 0.65f));
            g.drawRoundedRectangle(grWell, 3.0f, 0.9f);
        }

        constexpr int kNumSegments = 44;
        const float segPitch = height / float(kNumSegments);
        const float segH = juce::jmax(1.8f, segPitch - 1.5f);

        for (int i = 0; i < lanes; ++i) {
            const float x = left + float(i) * (laneWidth + 3.5f) + (i == 2 ? 41.5f : 0.0f);
            const float proportion = (i == 2)
                ? juce::jlimit(0.0f, 1.0f, values[i] / 36.0f)
                : juce::jlimit(0.0f, 1.0f, (values[i] + 60.0f) / 60.0f);

            const int litCount = proportion <= 0.002f ? 0 : juce::jlimit(1, kNumSegments, int(std::ceil(proportion * float(kNumSegments))));

            // Soft optical bloom behind active lamp stack
            if (litCount > 0) {
                const float litH = float(litCount) * segPitch;
                const juce::Rectangle<float> glowBox(x - 1.5f,
                                                     i == 2 ? top : (bottom - litH),
                                                     laneWidth + 3.0f, litH);
                const juce::Colour glowCol(i == 2 ? t.grStroke : (proportion > 0.88f ? t.lampHot : (proportion > 0.70f ? t.lampWarn : t.lampSafe)));
                g.setColour(glowCol.withAlpha(0.18f));
                g.fillRoundedRectangle(glowBox, 2.5f);
            }

            for (int s = 0; s < kNumSegments; ++s) {
                const float f = float(s + 1) / float(kNumSegments);
                const bool isLit = (s < litCount);
                const bool isTip = (s == litCount - 1 && litCount > 0);
                const float sy = (i == 2)
                    ? (top + float(s) * segPitch)
                    : (bottom - float(s + 1) * segPitch + (segPitch - segH));

                juce::Colour cellCol;
                if (i == 2) {
                    const juce::Colour grBase = juce::Colour(t.grStroke).interpolatedWith(juce::Colour(t.lampHot), f * 0.55f);
                    cellCol = isLit ? (isTip ? (t.lightSurface ? grBase.darker(0.22f)
                                                               : grBase.interpolatedWith(juce::Colours::white, 0.42f))
                                             : grBase)
                                    : grBase.withAlpha(t.lightSurface ? 0.16f : 0.13f);
                } else {
                    if (f < 0.70f)
                        cellCol = juce::Colour(isLit ? t.lampSafe : t.lampGhostSafe);
                    else if (f < 0.90f)
                        cellCol = juce::Colour(isLit ? t.lampWarn : t.lampGhostWarn);
                    else
                        cellCol = juce::Colour(isLit ? t.lampHot : t.lampGhostHot);

                    if (isTip)
                        cellCol = t.lightSurface ? cellCol.darker(0.22f)
                                                 : cellCol.interpolatedWith(juce::Colours::white, 0.42f);
                }

                g.setColour(cellCol);
                g.fillRoundedRectangle(x, sy, laneWidth, segH, 1.0f);
            }

            g.setColour(juce::Colour(t.textPrimary));
            g.setFont(juce::FontOptions(8.3f, juce::Font::bold));
            g.drawText(i == 2 ? "GR" : (i == 0 ? "L" : "R"),
                       int(x) - 3, int(bottom) + 3, int(laneWidth) + 6, 11, juce::Justification::centred);
        }

        // Dual dBu / dBFS calibration ticks (highlighting 0 dBu = -18 dBFS)
        static constexpr int kTicksDbfs[] = { 0, -12, -18, -24, -36, -48, -60 };
        for (int db : kTicksDbfs) {
            const float y = top + height * float(-db) / 60.0f;
            const bool isRef = (db == -18);
            g.setColour(isRef ? juce::Colour(t.zeroDbuRef) : juce::Colour(t.textMuted).withAlpha(0.65f));
            g.drawHorizontalLine(int(y), left + laneWidth * 2.0f + 4.5f, left + laneWidth * 2.0f + 8.5f);

            const int dbu = db + 18;
            const juce::String lbl = isRef
                ? "0/-18"
                : ((dbu > 0 ? "+" : "") + juce::String(dbu) + "/" + juce::String(db));
            g.setFont(juce::FontOptions(isRef ? 7.8f : 7.2f, isRef ? juce::Font::bold : juce::Font::plain));
            g.setColour(isRef ? juce::Colour(t.zeroDbuRef) : juce::Colour(t.textMuted));
            g.drawText(lbl, int(left + laneWidth * 2.0f + 9.5f), int(y) - 5, 41, 10, juce::Justification::centredLeft);
        }

        // Bottom recessed OLED digital readout window (dBu + dBFS)
        const float peakDbfs = juce::jlimit(-60.0f, 12.0f,
            input ? proc.lastInputPeakDb.load(std::memory_order_relaxed)
                  : proc.lastOutputPeakDb.load(std::memory_order_relaxed));
        const float peakDbu = peakDbfs + kDbuOffset;

        const juce::Rectangle<float> oledBox(5.0f, float(getHeight() - 34), float(getWidth() - 10), 29.0f);
        g.setColour(juce::Colour(t.oledBg));
        g.fillRoundedRectangle(oledBox, 3.2f);
        g.setColour(juce::Colour(t.oledBorder));
        g.drawRoundedRectangle(oledBox, 3.2f, 0.9f);

        g.setColour(juce::Colour(t.oledText));
        g.setFont(juce::FontOptions(8.7f, juce::Font::bold));
        g.drawText((peakDbu > 0.0f ? "+" : "") + juce::String(peakDbu, 1) + " dBu",
                   6, getHeight() - 33, getWidth() - 12, 14, juce::Justification::centred);
        g.setColour(juce::Colour(t.textPrimary).withAlpha(0.85f));
        g.setFont(juce::FontOptions(7.9f, juce::Font::bold));
        g.drawText(juce::String(peakDbfs, 1) + " dBFS",
                   6, getHeight() - 20, getWidth() - 12, 13, juce::Justification::centred);
    }

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    Skin& mrSkin;
    HarmonicCompressorAudioProcessor& proc;
    bool input;
};

// Unlabeled dominant field-of-view Electro-Optical Compression History viewport:
// - Deliberately has NO panel header/title label ("the Compression history should not have a label").
// - Calibrated to -18 dBFS = 0 dBu with dual dBu / dBFS axis markers and highlighted 0 dBu reference.
// - Smoothly auto-adjusts its vertical scale according to recent peak level.
// - Shows ONLY the top half (unipolar rectified rising from bottom baseline) of BOTH pre-compression
//   (input) and post-compression (output) waveforms simultaneously, plus downward GR laser trace.
// - Hosts the integrated TransferCurveDisplay and the 4 user-movable collapsible FloatingModules.
class CompressionScope : public juce::Component, private juce::Timer {
public:
    static constexpr float kDbuOffset = 18.0f; // -18 dBFS = 0 dBu

    CompressionScope(Skin& s, HarmonicCompressorAudioProcessor& p)
      : skin(s),
        proc(p),
        thresholdRaw(p.apvts.getRawParameterValue(ParamIDs::threshold)),
        transferCurve(p, s)
    {
        proc.historyFifo.finishedRead(proc.historyFifo.getNumReady());
        addAndMakeVisible(transferCurve);
        transferCurve.setDisplayRange(displayMinDb, displayMaxDb);
        startTimerHz(30);
    }

    void setCurveVisible(bool visible) {
        transferCurve.setVisible(visible);
        repaint();
    }
    bool isCurveVisible() const noexcept { return transferCurve.isVisible(); }

    float getDisplayMinDb() const noexcept { return displayMinDb; }
    float getDisplayMaxDb() const noexcept { return displayMaxDb; }

    void snapAutoScaleImmediate() noexcept {
        drainFifo();
        updateAutoScale(true);
        repaint();
    }

    juce::Rectangle<float> getPlotRect() const noexcept {
        return juce::Rectangle<float>(62.0f, 14.0f,
                                      juce::jmax(80.0f, float(getWidth()) - 102.0f),
                                      juce::jmax(80.0f, float(getHeight()) - 42.0f));
    }

    void resized() override {
        const auto r = getPlotRect();
        const float side = juce::jmin(r.getHeight(), r.getWidth() * 0.50f);
        transferCurve.setBounds(juce::roundToInt(r.getX() - TransferCurveDisplay::kPadLeft),
                                juce::roundToInt(r.getY() - TransferCurveDisplay::kPadTop),
                                juce::roundToInt(side + TransferCurveDisplay::kPadLeft + TransferCurveDisplay::kPadRight),
                                juce::roundToInt(r.getHeight() + TransferCurveDisplay::kPadTop + TransferCurveDisplay::kPadBottom));

        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* mod = dynamic_cast<FloatingModule*>(getChildComponent(i)))
                mod->clampToParent();
    }

    void paint(juce::Graphics& g) override {
        const auto& t = skin.theme();
        const auto outer = getLocalBounds().toFloat().reduced(0.8f);

        // Outer dark-anodized housing bezel
        g.setColour(juce::Colour(t.panelBorderLo));
        g.fillRoundedRectangle(outer.expanded(0.4f), 5.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(t.panelTop), outer.getX(), outer.getY(),
                                               juce::Colour(t.panelBottom), outer.getX(), outer.getBottom(), false));
        g.fillRoundedRectangle(outer, 4.5f);
        if (t.synthGrid) {
            g.setColour(juce::Colour(t.panelBorderHi).withAlpha(0.24f));
            g.drawRoundedRectangle(outer.expanded(0.4f), 4.8f, 2.2f);
        }
        g.setColour(juce::Colour(t.panelBorderHi).withAlpha(t.synthGrid ? 0.80f : 0.68f));
        g.drawRoundedRectangle(outer, 4.5f, 1.0f);

        // Recessed pitch-black CRT / OLED optical glass viewport
        const auto r = getPlotRect();
        g.setGradientFill(juce::ColourGradient(juce::Colour(t.wellBgTop), r.getX(), r.getY(),
                                               juce::Colour(t.wellBgBottom), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 3.5f);

        // Optional Hyperdrive Synthwave Outrun Perspective Horizon Grid inside the optical well
        if (t.synthGrid) {
            juce::Graphics::ScopedSaveState synthClip(g);
            g.reduceClipRegion(r.toNearestInt());

            const float horizonY = r.getY() + r.getHeight() * 0.44f;
            // Warm ultraviolet/magenta horizon glow
            g.setGradientFill(juce::ColourGradient(juce::Colour(0x00ff2bd6), r.getCentreX(), r.getY(),
                                                   juce::Colour(0x28ff2bd6), r.getCentreX(), horizonY, false));
            g.fillRect(r.getX(), r.getY(), r.getWidth(), horizonY - r.getY());
            g.setGradientFill(juce::ColourGradient(juce::Colour(0x2cff2bd6), r.getCentreX(), horizonY,
                                                   juce::Colour(0x168c2eff), r.getCentreX(), r.getBottom(), false));
            g.fillRect(r.getX(), horizonY, r.getWidth(), r.getBottom() - horizonY);

            // Perspective rays radiating from vanishing point on horizon
            const float vx = r.getCentreX();
            g.setColour(juce::Colour(0x2cff2bd6));
            for (int ray = -9; ray <= 9; ++ray) {
                const float bx = vx + float(ray) * (r.getWidth() * 0.135f);
                g.drawLine(vx + float(ray) * 18.0f, horizonY, bx, r.getBottom(), 0.85f);
            }
            // Perspective horizontal laser bars toward foreground
            for (int row = 1; row <= 8; ++row) {
                const float p = std::pow(float(row) / 8.0f, 1.85f);
                const float py = horizonY + p * (r.getBottom() - horizonY);
                g.setColour(juce::Colour(0xff00f6ff).interpolatedWith(juce::Colour(0xffff2bd6), 1.0f - p).withAlpha(0.14f + 0.14f * p));
                g.drawHorizontalLine(int(py), r.getX(), r.getRight());
            }
        }

        // Micro unit column headers (not a panel title/label)
        g.setFont(juce::FontOptions(7.6f, juce::Font::bold));
        g.setColour(juce::Colour(t.textMuted));
        g.drawText("dBu / dBFS", 3, 2, 56, 11, juce::Justification::centredRight);
        g.setColour(juce::Colour(t.grStroke).withAlpha(0.92f));
        g.drawText("GR dB", int(r.getRight()) + 4, 2, 34, 11, juce::Justification::centredLeft);

        // Vertical time gridlines: -6 s .. NOW
        g.setFont(juce::FontOptions(8.3f, juce::Font::bold));
        for (int sec = 0; sec <= 6; ++sec) {
            const float x = r.getX() + r.getWidth() * float(sec) / 6.0f;
            g.setColour(juce::Colour(t.gridMinor));
            g.drawVerticalLine(int(x), r.getY(), r.getBottom());

            g.setColour(juce::Colour(t.textMuted));
            const juce::String tLabel = (sec == 6) ? "NOW" : ("-" + juce::String(6 - sec) + " s");
            const int labelY = int(r.getBottom()) + (transferCurve.isVisible() ? 16 : 6);
            g.drawText(tLabel, int(x) - 18, labelY, 36, 11, juce::Justification::centred);
        }

        // Auto-scaled horizontal dBu / dBFS gridlines (every 6 dB, highlighting 0 dBu = -18 dBFS)
        const float spanDb = juce::jmax(12.0f, displayMaxDb - displayMinDb);
        const int stepDb = (spanDb > 48.0f) ? 12 : 6;
        for (int db = -72; db <= 12; db += 6) {
            const bool isZeroDbu = (db == -18);
            if (!isZeroDbu && (db % stepDb != 0))
                continue;
            if (float(db) < displayMinDb - 0.2f || float(db) > displayMaxDb + 0.2f)
                continue;

            const float y = juce::jmap(float(db), displayMinDb, displayMaxDb, r.getBottom(), r.getY());
            if (isZeroDbu) {
                juce::Path refLine, dashedRef;
                refLine.startNewSubPath(r.getX(), y);
                refLine.lineTo(r.getRight(), y);
                const float dashes[] = { 6.0f, 3.5f };
                juce::PathStrokeType(1.3f).createDashedStroke(dashedRef, refLine, dashes, 2);
                g.setColour(juce::Colour(t.zeroDbuRef).withAlpha(0.22f));
                g.drawLine(r.getX(), y, r.getRight(), y, 3.5f);
                g.setColour(juce::Colour(t.zeroDbuRef).withAlpha(0.82f));
                g.fillPath(dashedRef);
            } else {
                g.setColour(juce::Colour(t.gridMajor));
                g.drawHorizontalLine(int(y), r.getX(), r.getRight());
            }

            const int dbu = db + 18;
            const juce::String dualLbl = isZeroDbu
                ? "0dBu/-18"
                : ((dbu > 0 ? "+" : "") + juce::String(dbu) + " / " + juce::String(db));
            g.setFont(juce::FontOptions(isZeroDbu ? 8.3f : 7.9f, isZeroDbu ? juce::Font::bold : juce::Font::plain));
            g.setColour(isZeroDbu ? juce::Colour(t.zeroDbuRef) : juce::Colour(t.textPrimary).withAlpha(0.82f));
            g.drawText(dualLbl, 2, int(y) - 6, 57, 12, juce::Justification::centredRight);
        }

        // Right axis: downward Gain Reduction scale (0..-36 dB)
        g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
        for (int step = 0; step <= 5; ++step) {
            const float y = r.getY() + r.getHeight() * float(step) / 5.0f;
            const float grTick = 36.0f * float(step) / 5.0f;
            g.setColour(juce::Colour(t.grStroke).withAlpha(0.88f));
            g.drawText(step == 0 ? "0" : ("-" + juce::String(grTick, 0)),
                       int(r.getRight()) + 5, int(y) - 6, 33, 12, juce::Justification::centredLeft);
        }

        // Top-half rectified waveforms: BOTH Pre-Compression (Input) and Post-Compression (Output)
        // rise strictly from r.getBottom() (displayMinDb) to r.getY() (displayMaxDb) simultaneously.
        {
            juce::Graphics::ScopedSaveState clipState(g);
            g.reduceClipRegion(r.toNearestInt());

            auto normDb = [this, spanDb](float peak) noexcept -> float {
                const float db = juce::Decibels::gainToDecibels(peak, -80.0f);
                return juce::jlimit(0.0f, 1.0f, (db - displayMinDb) / spanDb);
            };

            if (count > 0) {
                juce::Path preFill, preTop, postFill, postTop, grFill, grLine;
                bool first = true;
                float elapsed = 0.0f;
                float lastX = r.getRight();
                float lastYIn = r.getBottom(), lastYOut = r.getBottom(), lastYGr = r.getY();

                for (int i = 0; i < count; ++i) {
                    const auto& f = frames[(size_t)((write - 1 - i + capacity) % capacity)];
                    const float xRight = r.getRight() - (elapsed / 6.0f) * r.getWidth();
                    if (xRight < r.getX()) break;

                    const float w = juce::jmax(1.0f, (f.seconds / 6.0f) * r.getWidth());
                    const float xLeft = juce::jmax(r.getX(), xRight - w);
                    const float xMid = 0.5f * (xRight + xLeft);

                    const float yIn  = r.getBottom() - normDb(f.input)  * r.getHeight();
                    const float yOut = r.getBottom() - normDb(f.output) * r.getHeight();
                    const float yGr  = r.getY() + (juce::jlimit(0.0f, 36.0f, f.reduction) / 36.0f) * r.getHeight();

                    if (first) {
                        preFill.startNewSubPath(xRight, r.getBottom());
                        preFill.lineTo(xRight, yIn);
                        preTop.startNewSubPath(xRight, yIn);

                        postFill.startNewSubPath(xRight, r.getBottom());
                        postFill.lineTo(xRight, yOut);
                        postTop.startNewSubPath(xRight, yOut);

                        grFill.startNewSubPath(xRight, r.getY());
                        grFill.lineTo(xRight, yGr);
                        grLine.startNewSubPath(xRight, yGr);
                        first = false;
                    }

                    preFill.lineTo(xMid, yIn);
                    preTop.lineTo(xMid, yIn);

                    postFill.lineTo(xMid, yOut);
                    postTop.lineTo(xMid, yOut);

                    grFill.lineTo(xMid, yGr);
                    grLine.lineTo(xMid, yGr);

                    lastX = xLeft;
                    lastYIn = yIn;
                    lastYOut = yOut;
                    lastYGr = yGr;
                    elapsed += f.seconds;
                }

                if (!first) {
                    preFill.lineTo(lastX, lastYIn);
                    preFill.lineTo(lastX, r.getBottom());
                    preFill.closeSubPath();
                    preTop.lineTo(lastX, lastYIn);

                    postFill.lineTo(lastX, lastYOut);
                    postFill.lineTo(lastX, r.getBottom());
                    postFill.closeSubPath();
                    postTop.lineTo(lastX, lastYOut);

                    grFill.lineTo(lastX, lastYGr);
                    grFill.lineTo(lastX, r.getY());
                    grFill.closeSubPath();
                    grLine.lineTo(lastX, lastYGr);

                    // 1. Pre-compression (input) top-half envelope
                    g.setGradientFill(juce::ColourGradient(juce::Colour(t.preFillTop), 0.0f, r.getY(),
                                                           juce::Colour(t.preFillBottom), 0.0f, r.getBottom(), false));
                    g.fillPath(preFill);
                    g.setColour(juce::Colour(t.preStroke).withAlpha(0.28f));
                    g.strokePath(preTop, juce::PathStrokeType(3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour(juce::Colour(t.preStroke));
                    g.strokePath(preTop, juce::PathStrokeType(1.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                    // 2. Post-compression (output) top-half envelope (phosphor/neon bloom on dark modes; obsidian ink on Soft White)
                    g.setGradientFill(juce::ColourGradient(juce::Colour(t.postFillTop), 0.0f, r.getY(),
                                                           juce::Colour(t.postFillBottom), 0.0f, r.getBottom(), false));
                    g.fillPath(postFill);
                    g.setColour(juce::Colour(t.postGlow).withAlpha(t.lightSurface ? 0.12f : 0.22f));
                    g.strokePath(postTop, juce::PathStrokeType(t.lightSurface ? 5.2f : 6.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour(juce::Colour(t.postGlow).withAlpha(t.lightSurface ? 0.24f : 0.42f));
                    g.strokePath(postTop, juce::PathStrokeType(3.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour(juce::Colour(t.postStroke));
                    g.strokePath(postTop, juce::PathStrokeType(2.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour(t.lightSurface ? juce::Colour(0xff07090d)
                                               : juce::Colour(t.postStroke).interpolatedWith(juce::Colours::white, 0.52f));
                    g.strokePath(postTop, juce::PathStrokeType(0.90f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                    // 3. Downward Gain Reduction trace
                    g.setColour(juce::Colour(t.grFill));
                    g.fillPath(grFill);
                    g.setColour(juce::Colour(t.grStroke).withAlpha(t.lightSurface ? 0.18f : 0.25f));
                    g.strokePath(grLine, juce::PathStrokeType(t.lightSurface ? 4.4f : 5.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour(juce::Colour(t.grStroke));
                    g.strokePath(grLine, juce::PathStrokeType(2.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour(t.lightSurface ? juce::Colour(t.grStroke).darker(0.30f)
                                               : juce::Colour(t.grStroke).interpolatedWith(juce::Colours::white, 0.45f));
                    g.strokePath(grLine, juce::PathStrokeType(0.85f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }

            // Horizontal Threshold reference line mapped onto the shared auto-scaled axis
            if (thresholdRaw != nullptr) {
                const float threshDb = juce::jlimit(-60.0f, 0.0f, thresholdRaw->load(std::memory_order_relaxed));
                if (threshDb >= displayMinDb - 1.0f && threshDb <= displayMaxDb + 1.0f) {
                    const float yThresh = juce::jmap(threshDb, displayMinDb, displayMaxDb, r.getBottom(), r.getY());
                    juce::Path threshLine, dashedThresh;
                    threshLine.startNewSubPath(r.getX(), yThresh);
                    threshLine.lineTo(r.getRight(), yThresh);
                    const float dashes[] = { 5.0f, 4.0f };
                    juce::PathStrokeType(1.15f).createDashedStroke(dashedThresh, threshLine, dashes, 2);
                    g.setColour(juce::Colour(t.thresholdRef).withAlpha(0.72f));
                    g.fillPath(dashedThresh);

                    const float threshDbu = threshDb + kDbuOffset;
                    g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
                    const int tagY = (yThresh < r.getY() + 18.0f) ? int(yThresh + 3.0f) : int(yThresh - 15.0f);
                    const juce::Rectangle<float> tagPill(r.getRight() - 138.0f, float(tagY), 133.0f, 13.0f);
                    g.setColour(juce::Colour(t.oledBg).withAlpha(0.90f));
                    g.fillRoundedRectangle(tagPill, 3.0f);
                    g.setColour(juce::Colour(t.thresholdRef).withAlpha(0.55f));
                    g.drawRoundedRectangle(tagPill, 3.0f, 0.8f);
                    g.setColour(juce::Colour(t.thresholdRef));
                    g.drawText(juce::String("THR ") + (threshDbu > 0.0f ? "+" : "") + juce::String(threshDbu, 1) + " dBu (" + juce::String(threshDb, 1) + ")",
                               tagPill.toNearestInt().reduced(4, 0), juce::Justification::centredRight);
                }
            }

            // Subtle horizontal CRT micro-scanlines & top curved optical glass reflection
            if (t.crtScanlines) {
                g.setColour(juce::Colour(0x0b000000));
                for (int sy = int(r.getY()) + 2; sy < int(r.getBottom()); sy += 3)
                    g.drawHorizontalLine(sy, r.getX(), r.getRight());
            }
            g.setGradientFill(juce::ColourGradient(juce::Colour(0x10ffffff), r.getX(), r.getY(),
                                                   juce::Colour(0x00ffffff), r.getX(), r.getY() + 52.0f, false));
            g.fillRect(r.getX(), r.getY(), r.getWidth(), 52.0f);
        }

        // Optical viewport double-bezel frame
        g.setColour(juce::Colour(t.wellBezelInner));
        g.drawRoundedRectangle(r.reduced(0.8f), 3.2f, 1.1f);
        g.setColour(juce::Colour(t.wellBezelOuter));
        g.drawRoundedRectangle(r, 3.5f, 1.1f);
    }

private:
    void drainFifo() noexcept {
        if (proc.historyDrops.exchange(0) > 0) { count = 0; write = 0; pending = {}; }
        int a, n, b, m;
        proc.historyFifo.prepareToRead(proc.historyFifo.getNumReady(), a, n, b, m);
        auto append = [&](int start, int size) {
            for (int i = 0; i < size; ++i) {
                const auto f = proc.historyFrames[(size_t)(start + i)];
                pending.input = juce::jmax(pending.input, f.input);
                pending.output = juce::jmax(pending.output, f.output);
                pending.reduction = juce::jmax(pending.reduction, f.reduction);
                pending.seconds += f.seconds;
                if (pending.seconds >= 0.01f) {
                    frames[(size_t)write] = pending;
                    write = (write + 1) % capacity;
                    count = juce::jmin(capacity, count + 1);
                    pending = {};
                }
            }
        };
        append(a, n);
        append(b, m);
        proc.historyFifo.finishedRead(n + m);
    }

    void updateAutoScale(bool snapImmediate) noexcept {
        float windowPeak = 0.0f;
        float elapsed = 0.0f;
        for (int i = 0; i < count; ++i) {
            const auto& f = frames[(size_t)((write - 1 - i + capacity) % capacity)];
            windowPeak = juce::jmax(windowPeak, juce::jmax(f.input, f.output));
            elapsed += f.seconds;
            if (elapsed >= 6.0f)
                break;
        }

        float targetMax = 0.0f;
        float targetMin = -60.0f;
        if (count > 0 && windowPeak > 1.0e-4f) {
            const float peakDb = juce::Decibels::gainToDecibels(windowPeak, -72.0f);
            targetMax = juce::jlimit(-21.0f, 6.0f, peakDb + 4.5f);
            targetMin = juce::jlimit(-72.0f, targetMax - 36.0f, targetMax - 42.0f);
        }

        if (snapImmediate) {
            displayMaxDb = targetMax;
            displayMinDb = targetMin;
        } else {
            const float maxRate = (targetMax > displayMaxDb) ? 0.40f : 0.10f;
            const float minRate = (targetMin < displayMinDb) ? 0.30f : 0.10f;
            displayMaxDb += (targetMax - displayMaxDb) * maxRate;
            displayMinDb += (targetMin - displayMinDb) * minRate;
            if (std::abs(targetMax - displayMaxDb) < 0.03f) displayMaxDb = targetMax;
            if (std::abs(targetMin - displayMinDb) < 0.03f) displayMinDb = targetMin;
        }

        transferCurve.setDisplayRange(displayMinDb, displayMaxDb);
    }

    void timerCallback() override {
        drainFifo();
        updateAutoScale(false);
        if (isShowing()) repaint();
    }

    static constexpr int capacity = 640;
    Skin& skin;
    HarmonicCompressorAudioProcessor& proc;
    std::atomic<float>* thresholdRaw = nullptr;
    TransferCurveDisplay transferCurve;
    float displayMinDb = -60.0f;
    float displayMaxDb = 0.0f;
    std::array<HarmonicCompressorAudioProcessor::HistoryFrame, capacity> frames{};
    HarmonicCompressorAudioProcessor::HistoryFrame pending{};
    int write = 0, count = 0;
};
