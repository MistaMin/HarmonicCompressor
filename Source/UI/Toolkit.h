#pragma once
// Everything the plugin takes from GoodLookinUI, with short local names, plus the 4 electro-optical
// and cyber-synth colour themes shared across every editor component.
#include <goodlookinui/Design.h>
#include <GoodLookinUI.h>
#include <Meters.h>
#include <Switches.h>
#include <Retro.h>
#if GOODLOOKINUI_ENABLE_EDITOR
#include <Studio.h>
#endif

namespace glui = goodlookinui;
namespace gj = goodlookinui::juce_adapter;

struct ColourTheme
{
    const char* name;
    bool lightSurface;
    bool synthGrid;
    bool crtScanlines;

    // Brushed metal / cyber chassis & panels
    juce::uint32 chassisTop, chassisBottom;
    juce::uint32 panelTop, panelBottom;
    juce::uint32 panelHeaderTop, panelHeaderBottom;
    juce::uint32 panelBorderHi, panelBorderLo;

    // Recessed optical glass / CRT / OLED display wells
    juce::uint32 wellBgTop, wellBgBottom;
    juce::uint32 wellBezelOuter, wellBezelInner;
    juce::uint32 gridMajor, gridMinor;

    // Typography & recessed OLED readouts
    juce::uint32 textBright, textPrimary, textMuted;
    juce::uint32 oledBg, oledBorder, oledText;

    // Beveled hardware push-keys & dropdown selectors
    juce::uint32 buttonTop, buttonBottom;
    juce::uint32 buttonHoverTop, buttonBorder;

    // Smoked dark-glass floating HUD modules inside CompressionScope
    juce::uint32 hudClosedBg, hudOpenBg, hudHeaderBg, hudBorder;

    // CNC-machined turbine rotary knob metals
    juce::uint32 rotorOuterHi, rotorOuterLo;
    juce::uint32 rotorCapHi, rotorCapLo;
    juce::uint32 rotorFluteHi, rotorFluteLo;

    // 3-zone segmented electro-optical lamp palette (lit + dim unlit ghost cells)
    juce::uint32 lampSafe, lampWarn, lampHot;
    juce::uint32 lampGhostSafe, lampGhostWarn, lampGhostHot;

    // Signal traces & calibration markers
    juce::uint32 accentPrimary, accentSecondary;
    juce::uint32 preStroke, preFillTop, preFillBottom;
    juce::uint32 postStroke, postGlow, postFillTop, postFillBottom;
    juce::uint32 grStroke, grFill;
    juce::uint32 zeroDbuRef, thresholdRef;
};

inline constexpr int kNumColourThemes = 4;

inline constexpr ColourTheme kColourThemes[kNumColourThemes] =
{
    // 0: MATRIX — Dark brushed gunmetal chassis, pitch-black optical glass, emerald/gold/ruby phosphor lamps
    {
        "Matrix", false, false, true,
        0xff171e24, 0xff0c1015,
        0xff182027, 0xff10161c,
        0xff202933, 0xff151c23,
        0xff364350, 0xff05070a,

        0xff060b0e, 0xff030608,
        0xff32404d, 0xff010203,
        0xff132b21, 0xff0b1914,

        0xffeaf7f0, 0xffc0d6cc, 0xff709184,
        0xff04090b, 0xff223b31, 0xff3bf57c,

        0xff27313b, 0xff141a20,
        0xff323f4c, 0xff3a4857,

        0xf0131b22, 0xf710171e, 0xf918232c, 0xff30473f,

        0xff404a54, 0xff0c1014,
        0xff2e363f, 0xff0e1217,
        0xff63707d, 0xff06080a,

        0xff27e868, 0xfff5cc36, 0xffff3340,
        0xff0b2414, 0xff26200b, 0xff290c10,

        0xff29ec6b, 0xff36deb8,
        0xff6ec4a8, 0x4558b093, 0x12265c4a,
        0xff2cf26f, 0xff1be05e, 0x7622db62, 0x18094a22,
        0xffff5238, 0x2eff422e,
        0xfff5cc36, 0xff42f5bd
    },

    // 1: SOFT WHITE — Satin alabaster-ceramic chassis, soft-white paper-glass viewport, dark obsidian ink waveform
    {
        "Soft White", true, false, false,
        0xfff5f4f0, 0xffe9e6df,
        0xfffbfaf7, 0xfff0eee8,
        0xfff4f2ec, 0xffeae7df,
        0xffffffff, 0xffc5c0b6,

        0xfffaf9f6, 0xfff0eee8,
        0xffbdb8ad, 0xffffffff,
        0xffdedad0, 0xffe6e3da,

        0xff12161d, 0xff2c333f, 0xff6d7480,
        0xffe6e3db, 0xffc4bfb4, 0xff141820,

        0xfffaf9f6, 0xffeae7df,
        0xffffffff, 0xffc0bbb0,

        0xedf9f8f4, 0xf6fcfbf8, 0xf8f0eee7, 0xffbdb7ac,

        0xfffdfcfa, 0xffc4bfb4,
        0xfffbfaf7, 0xffd3cec3,
        0xffffffff, 0xff8a8479,

        0xff232b38, 0xffcc7814, 0xffd63626,
        0xffd0ccc2, 0xffd2cbc0, 0xffd4c6c3,

        0xff1d2430, 0xff24639c,
        0xff636f82, 0x3b687588, 0x0c687588,
        0xff141922, 0xff242c3b, 0x7c1b2230, 0x161b2230,
        0xffd63626, 0x24d63626,
        0xffb5690d, 0xff0f7a6c
    },

    // 2: CRYO PLASMA — Frosted arctic-titanium chassis, sub-zero OLED well, electric cryo-cyan/white-ice/magenta
    {
        "Cryo Plasma", false, false, true,
        0xff16273b, 0xff0a131f,
        0xff192c42, 0xff0e1928,
        0xff213852, 0xff142336,
        0xff3c638c, 0xff03070d,

        0xff040b17, 0xff02050c,
        0xff385d82, 0xff010306,
        0xff12304f, 0xff0a1d30,

        0xffecfaff, 0xffbfe2f7, 0xff709ec2,
        0xff030914, 0xff244b6e, 0xff29ecff,

        0xff243a54, 0xff121f2e,
        0xff2f4b6b, 0xff3f658c,

        0xf0122133, 0xf70e1a29, 0xf91a2e45, 0xff345e87,

        0xff3c556e, 0xff0a121c,
        0xff2b3e52, 0xff0c1521,
        0xff6589ad, 0xff05090f,

        0xff18e0ff, 0xffd4f9ff, 0xffff2a88,
        0xff07242e, 0xff162b33, 0xff2b0a1c,

        0xff1ee8ff, 0xffff3898,
        0xff6da3ff, 0x48548ae8, 0x161c3873,
        0xff2bf0ff, 0xff00ccff, 0x7a14dcff, 0x1e07446b,
        0xffff2a88, 0x34ff2480,
        0xffc8f9ff, 0xff52ffd8
    },

    // 3: HYPERDRIVE — Ultraviolet cosmos void, synthwave perspective laser grid, neon-magenta + cyber-cyan + acid-yellow
    {
        "Hyperdrive", false, true, true,
        0xff240842, 0xff0b0217,
        0xff2a0a4c, 0xff130426,
        0xff380f63, 0xff1e0738,
        0xff9b3dff, 0xff05000d,

        0xff0b011a, 0xff04000b,
        0xffa64dff, 0xff020006,
        0xff3b126b, 0xff220940,

        0xfffff2ff, 0xfff0c7ff, 0xffb87ee0,
        0xff090117, 0xff862eff, 0xff00f6ff,

        0xff3c1269, 0xff1c0636,
        0xff4f1987, 0xff9638ff,

        0xf021073d, 0xf718052e, 0xf92e0c54, 0xffa042ff,

        0xff521f85, 0xff0e031d,
        0xff38135e, 0xff100321,
        0xffb057ff, 0xff070110,

        0xffff2bd6, 0xff00f6ff, 0xffffe600,
        0xff2e0726, 0xff05282e, 0xff2e2905,

        0xffff2bd6, 0xff00f6ff,
        0xff00f6ff, 0x5600f6ff, 0x18053b5e,
        0xffff33dc, 0xffff00aa, 0x80ff2bd6, 0x2c7b1aff,
        0xffffe600, 0x34ffe600,
        0xffffe600, 0xff00f6ff
    }
};

// Shared look state: the active electro-optical colour mode plus UI-only switches. Owned by the
// editor, referenced by every component, saved in the plugin state (never touches the DSP).
struct Skin {
    const glui::Faceplate* plate = glui::findFaceplate("Graphite");
    int colourMode = 0;   // 0 Matrix, 1 Soft White, 2 Cryo Plasma, 3 Hyperdrive
    int meterFace = 2;    // LED ladders
    int pushStyle = 0;

    const ColourTheme& theme() const noexcept
    {
        return kColourThemes[juce::jlimit(0, kNumColourThemes - 1, colourMode)];
    }

    static const ColourTheme& defaultTheme() noexcept { return kColourThemes[0]; }

    juce::Colour bottom() const noexcept     { return juce::Colour(theme().panelBottom); }
    juce::Colour textBright() const noexcept { return juce::Colour(theme().textBright); }
    juce::Colour text() const noexcept       { return juce::Colour(theme().textPrimary); }
    juce::Colour mid() const noexcept        { return juce::Colour(theme().textMuted); }
    juce::Colour accent() const noexcept     { return juce::Colour(theme().accentPrimary); }
    juce::Colour secondary() const noexcept  { return juce::Colour(theme().accentSecondary); }
    juce::Colour backdrop() const noexcept
    {
        return juce::Colour(theme().panelTop).interpolatedWith(juce::Colour(theme().panelBottom), 0.5f);
    }
    int finish() const noexcept { return plate->finish; }
};

// Dark electro-optical popup menu styling matching the active Skin colour mode.
class ThemedPopupLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit ThemedPopupLookAndFeel(const Skin& s) : skin(s) {}

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        const auto& t = skin.theme();
        juce::Rectangle<float> r(0.0f, 0.0f, float(width), float(height));
        g.setGradientFill(juce::ColourGradient(juce::Colour(t.panelHeaderTop), 0.0f, 0.0f,
                                               juce::Colour(t.chassisBottom), 0.0f, float(height), false));
        g.fillRect(r);
        g.setColour(juce::Colour(t.accentPrimary).withAlpha(0.65f));
        g.drawRect(r.reduced(0.5f), 1.2f);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                           bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable*, const juce::Colour*) override
    {
        juce::ignoreUnused(hasSubMenu, shortcutKeyText);
        const auto& t = skin.theme();
        if (isSeparator)
        {
            g.setColour(juce::Colour(t.panelBorderHi).withAlpha(0.55f));
            g.drawHorizontalLine(area.getCentreY(), float(area.getX() + 6), float(area.getRight() - 6));
            return;
        }

        auto row = area.reduced(3, 1).toFloat();
        if (isHighlighted && isActive)
        {
            g.setColour(juce::Colour(t.accentPrimary).withAlpha(0.22f));
            g.fillRoundedRectangle(row, 3.0f);
            g.setColour(juce::Colour(t.accentPrimary).withAlpha(0.75f));
            g.drawRoundedRectangle(row, 3.0f, 0.9f);
        }

        if (isTicked)
        {
            const float cy = row.getCentreY();
            g.setColour(juce::Colour(t.accentPrimary).withAlpha(0.30f));
            g.fillEllipse(row.getX() + 5.0f, cy - 4.5f, 9.0f, 9.0f);
            g.setColour(juce::Colour(t.accentPrimary));
            g.fillEllipse(row.getX() + 7.2f, cy - 2.3f, 4.6f, 4.6f);
        }

        g.setColour(!isActive ? juce::Colour(t.textMuted).withAlpha(0.5f)
                              : (isHighlighted || isTicked ? juce::Colour(t.textBright)
                                                           : juce::Colour(t.textPrimary)));
        g.setFont(juce::FontOptions(12.0f, isTicked ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText(text, area.withTrimmedLeft(22).withTrimmedRight(8),
                         juce::Justification::centredLeft, 1);
    }

private:
    const Skin& skin;
};
