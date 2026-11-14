#pragma once

#include <JuceHeader.h>
#include <array>
#include <map>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "TransferCurveDisplay.h"
#include "UI/Toolkit.h"
#include "UI/Panel.h"
#include "UI/Knob.h"
#include "UI/KeyButton.h"
#include "UI/FloatingModule.h"
#include "UI/MeterPanel.h"

//==============================================================================
/**
    HarmonicCompressor editor, built with 4 electro-optical & cyber-synth colour modes
    (Matrix, Vacuum Amber, Cryo Plasma, Hyperdrive) and CNC-scalloped segmented-LED
    turbine rotary dials.

    Layout:
    - Unlabeled full-field Compression History viewport with peak-tracking auto-scale
      (-18 dBFS = 0 dBu calibration) and integrated Transfer Curve.
    - Four closed-by-default, click-to-open, freely user-movable floating HUD modules inside
      the Compression History field: Compression (timing & knee), Harmonics, Sidechain,
      and end-of-chain Transient Designer.
    - Input and Output Gain horizontal sliders directly beneath their respective segmented lamp meters.
    - Four electro-optical turbine rotary knobs (Threshold, Ratio, Makeup, Mix) with OLED readouts,
      followed by the AUTO GAIN (loudness-matched output) and DELTA (output minus dry) keys.
    - Top toolbar with Compressor Model, Oversampling factor (1x..8x), Oversampling Filter
      (Minimum Phase / Linear Phase), Transient Designer modifier toggle, Curve toggle, and
      4-mode Colour selector.
*/
class HarmonicCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit HarmonicCompressorAudioProcessorEditor (HarmonicCompressorAudioProcessor&);
    ~HarmonicCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setColourMode (int modeIndex);
    int getColourMode() const noexcept { return skin.colourMode; }

    CompressionScope& getScope() noexcept { return scope; }
    FloatingModule& getCompressionModule() noexcept { return compModule; }
    FloatingModule& getHarmonicsModule() noexcept { return harmModule; }
    FloatingModule& getSidechainModule() noexcept { return scModule; }
    FloatingModule& getTransientModule() noexcept { return transientModule; }
    DropdownButton& getColourModeSelector() noexcept { return colourModeSelector; }
    KeyButton& getAutoGainKey() noexcept { return autoGainKey; }
    KeyButton& getDeltaKey() noexcept { return deltaKey; }

    Knob* findControl (const std::string& id) const
    {
        auto it = knobById.find (id);
        return it != knobById.end() ? it->second : nullptr;
    }

private:
    // Chassis plate background and top header bar, drawn on the scaled design canvas.
    struct Surface : juce::Component
    {
        explicit Surface (Skin& s) : skin (s) { setOpaque (true); }
        void paint (juce::Graphics&) override;
        Skin& skin;
    };

    // A bool parameter bound to a KeyButton (gesture-correct, follows host automation).
    struct KeyBinding
    {
        KeyBinding (HarmonicCompressorAudioProcessor&, const char* paramID, KeyButton&, std::function<void (bool)> onChange = {});
        KeyButton& button;
        std::function<void (bool)> changed;
        juce::ParameterAttachment attachment;
    };

    void timerCallback() override;

    Knob& addKnob (juce::Component& parent, const char* id, const char* label, juce::Colour colour, Knob::Formatter,
                   juce::Rectangle<int> defaultBounds, bool rotary = false, bool secondaryAccent = false);
    void applyBakedDesign();
    void applySkin();
    void applyModeLook (int mode);
    void saveModeLook (const char* what, const juce::String& value);
    void applyKnobStyle (const juce::String& code);
    void onKnobDesignEdited (Knob&, const glui::Item&);
    void saveLookState();
    void loadLookState();
    void saveModulePositions();
    void loadModulePositions();
    void updateModuleSummaries();

    int detectHarmonicsPresetIndex() const;
    void applyHarmonicsPreset (int presetIndex);
    void updateHarmonicsSelectorText();

    int currentSidechainModeIndex() const;
    void applySidechainMode (int modeIndex);
    void updateSidechainSelectorText();

    HarmonicCompressorAudioProcessor& audioProcessor;
    Skin skin;
    ThemedPopupLookAndFeel popupLnf { skin };
    Surface surface { skin };

    MeterPanel inputMeter  { skin, audioProcessor, true };
    MeterPanel meterPanel  { skin, audioProcessor, false };
    Panel inGainPanel      { skin, "IN GAIN",  {}, true };
    Panel outGainPanel     { skin, "OUT GAIN", {}, true };
    Panel knobBar          { skin, {},         {}, true };
    CompressionScope scope { skin, audioProcessor };

    // Closed-by-default, click-to-open, freely movable floating HUD modules inside CompressionScope
    FloatingModule compModule      { "COMPRESSION", juce::Colour (0xff29ec6b), { 198, 28 }, { 406, 108 } };
    FloatingModule harmModule      { "HARMONICS",   juce::Colour (0xfff5cc36), { 200, 28 }, { 430, 126 } };
    FloatingModule scModule        { "SIDECHAIN",   juce::Colour (0xff36deb8), { 186, 28 }, { 306,  82 } };
    FloatingModule transientModule { "TRANSIENT",   juce::Colour (0xff42f5bd), { 204, 28 }, { 454,  84 } };

    static constexpr int numAlgorithms = (int) CompressorMode::Count;
    DropdownButton algorithmSelector  { juce::Colour (0xff29ec6b) };
    DropdownButton oversampleSelector { juce::Colour (0xff36deb8) };
    DropdownButton osFilterSelector   { juce::Colour (0xff36deb8) };
    DropdownButton colourModeSelector { juce::Colour (0xff29ec6b) };
    std::unique_ptr<juce::ParameterAttachment> algorithmAttachment;
    std::unique_ptr<juce::ParameterAttachment> oversampleAttachment;
    std::unique_ptr<juce::ParameterAttachment> osFilterAttachment;
    int currentOversampleIndex = 1;
    int currentOsFilterIndex = 0;

    DropdownButton harmAlgoSelector { juce::Colour (0xfff5cc36) };
    std::array<std::unique_ptr<juce::ParameterAttachment>, 4> harmAttachments;
    std::array<float, 4> harmState { 0.0f, 0.0f, 0.0f, 0.0f };
    bool applyingHarmonicsPreset = false;
    bool customHarmonicsLatched = false;

    DropdownButton scModeSelector { juce::Colour (0xff36deb8) };
    std::unique_ptr<juce::ParameterAttachment> scEnableAttachment;
    std::unique_ptr<juce::ParameterAttachment> scListenAttachment;
    bool scEnabledState = false;
    bool scListenState = false;
    bool transientEnabledState = false;

    KeyButton autoAttackKey      { "AUTO ATTACK",  juce::Colour (0xff29ec6b) },
              autoReleaseKey     { "AUTO RELEASE", juce::Colour (0xff29ec6b) },
              transientHeaderKey { "TRANSIENT",    juce::Colour (0xff42f5bd) },
              transientModuleKey { "ACTIVE",       juce::Colour (0xff42f5bd) },
              curveToggleKey     { "CURVE",        juce::Colour (0xff36deb8) },
              autoGainKey        { "AUTO GAIN",    juce::Colour (0xff36deb8) },
              deltaKey           { "DELTA",        juce::Colour (0xfff5cc36) };
    void updateAutoGainKeyText();
    bool autoGainState = false;
    std::vector<std::unique_ptr<KeyBinding>> keyBindings;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::map<std::string, Knob*> knobById;
    std::map<std::string, glui::Item> designItems;     // as saved / as edited in the inspector
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::map<std::string, std::string> bakedStyle;      // style from the embedded design, per control
    juce::String knobStyleOverride;                     // style chosen by the current algorithm; empty = design
    int currentMode = 0;
    bool lookApplied = false;

#if GOODLOOKINUI_ENABLE_EDITOR
    gj::Studio designStudio;
    juce::TextButton designButton { "DESIGN" }, plateButton { "PLATE" }, knobsButton { "CONTROLS" },
                     metersButton { "METERS" }, keysButton { "KEYS" }, glowButton { "GLOW" };
    struct SelectionRing : juce::Component
    {
        SelectionRing() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { g.setColour (juce::Colour (0xffce9435)); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.5f), 5.0f, 2.5f); }
    } ring;
    juce::TooltipWindow tooltips { this, 500 };
    void highlightKnob (juce::Component*);
    void showPlateMenu();
    void showKnobMenu();
    void showMeterMenu();
    void showKeyMenu();
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicCompressorAudioProcessorEditor)
};
