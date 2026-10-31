#include "PluginEditor.h"
#include "EmbeddedDesign.h"
#include <sstream>

namespace
{
    constexpr int canvasWidth = 1120;
    constexpr int panelsHeight = 760;
#if GOODLOOKINUI_ENABLE_EDITOR
    constexpr int devStripHeight = 34;
#else
    constexpr int devStripHeight = 0;
#endif
    constexpr int canvasHeight = panelsHeight + devStripHeight;

    const char* const algorithmNames[] = {
        "Red", "Hybrid", "Bridge", "Bus VCA", "76", "Opto Leveler", "VCA 160"
    };
    const juce::uint32 algorithmColours[] = {
        0xffb3382c, 0xff3fa796, 0xffcda434, 0xffc9702e, 0xff4a86c8, 0xffb58a47, 0xff377f9d
    };

    const char* const kOversampleNames[] = { "1x", "2x", "4x", "8x" };
    constexpr int kNumOversampleModes = 4;

    const char* const kOsFilterNames[] = { "Min Phase", "Lin Phase" };
    constexpr int kNumOsFilters = 2;

    struct HarmonicsPreset
    {
        const char* name;
        const char* shortTag;
        float h2, h3, h4, h5;
    };

    constexpr HarmonicsPreset kHarmonicsPresets[] = {
        { "Off (Clean)",  "Off",   0.0f,  0.0f,  0.0f,  0.0f },
        { "Tube (Even)",  "Tube", 35.0f,  0.0f, 15.0f,  0.0f },
        { "Tape (Odd)",   "Tape",  0.0f, 35.0f,  0.0f, 15.0f },
        { "Triode",       "Tri",  45.0f, 12.0f,  8.0f,  0.0f },
        { "Pentode",      "Pent", 10.0f, 45.0f,  5.0f, 12.0f },
        { "Full Warmth",  "Warm", 30.0f, 30.0f, 15.0f, 15.0f },
    };
    constexpr int kNumHarmonicsPresets = (int) (sizeof (kHarmonicsPresets) / sizeof (kHarmonicsPresets[0]));
    constexpr int kCustomHarmonicsIndex = kNumHarmonicsPresets;

    const char* const kSidechainModes[] = {
        "Off",
        "HPF On",
        "Listen (HPF)",
        "Listen (Dry)"
    };
    const char* const kSidechainShortTags[] = {
        "Off",
        "HPF",
        "LstHP",
        "LstDry"
    };
    constexpr int kNumSidechainModes = 4;

    juce::String ratioText (double v)
    {
        if (v >= 99.5) return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) + ":1";
        return juce::String (v, 1) + ":1";
    }
    juce::String msText (double v)
    {
        if (v >= 1000.0) return juce::String (v / 1000.0, 2) + " s";
        return v >= 100.0 ? juce::String (juce::roundToInt (v)) + " ms" : juce::String (v, 1) + " ms";
    }
    juce::String compactMsText (double v)
    {
        if (v >= 1000.0) return juce::String (v / 1000.0, 1) + "s";
        return v >= 20.0 ? juce::String (juce::roundToInt (v)) + "m" : juce::String (v, 1) + "m";
    }
    juce::String dbText (double v, bool sign = false) { return (sign && v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String percentText (double v, int decimals, bool sign = false)
    {
        return (sign && v > 0.0 ? "+" : "") + juce::String (v, decimals) + "%";
    }

    // Readable names for the developer tools.
    [[maybe_unused]] const char* knobName (const std::string& id)
    {
        static const std::pair<const char*, const char*> names[] = {
            { "threshold",        "Main Knob: Threshold" },
            { "ratio",            "Main Knob: Ratio" },
            { "makeupGain",       "Main Knob: Makeup gain" },
            { "mix",              "Main Knob: Mix" },
            { "inputGain",        "Input Meter: Input gain" },
            { "outputGain",       "Output Meter: Output gain" },
            { "attack",           "Compression Module: Attack" },
            { "release",          "Compression Module: Release" },
            { "knee",             "Compression Module: Knee" },
            { "harmonic2",        "Harmonics Module: 2nd" },
            { "harmonic3",        "Harmonics Module: 3rd" },
            { "harmonic4",        "Harmonics Module: 4th" },
            { "harmonic5",        "Harmonics Module: 5th" },
            { "scHPFreq",         "Sidechain Module: HP frequency" },
            { "transientAttack",  "Transient Module: Attack" },
            { "transientSustain", "Transient Module: Sustain" },
            { "transientSpeed",   "Transient Module: Speed" }
        };
        for (auto& n : names) if (id == n.first) return n.second;
        return "";
    }
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::Surface::paint (juce::Graphics& g)
{
    const auto& t = skin.theme();
    const float w = float (getWidth());
    const float h = float (getHeight());

    // Brushed dark-anodized / cyber-void chassis background
    g.setGradientFill (juce::ColourGradient (juce::Colour (t.chassisTop), 0.0f, 0.0f,
                                             juce::Colour (t.chassisBottom), 0.0f, h, false));
    g.fillRect (0.0f, 0.0f, w, h);

    // Subtle horizontal micro-brushed anodized / satin-ceramic grain across the chassis
    for (int y = 2; y < int (h); y += 3)
    {
        g.setColour (juce::Colour (t.lightSurface ? (y % 6 == 2 ? 0x09ffffff : 0x04000000)
                                                  : (y % 6 == 2 ? 0x06ffffff : 0x0b000000)));
        g.drawHorizontalLine (y, 0.0f, w);
    }

    // Top toolbar recessed header deck & etched groove
    g.setGradientFill (juce::ColourGradient (juce::Colour (t.panelHeaderTop).withAlpha (0.85f), 0.0f, 0.0f,
                                             juce::Colour (t.chassisTop).withAlpha (0.45f), 0.0f, 44.0f, false));
    g.fillRect (0.0f, 0.0f, w, 44.0f);

    if (t.synthGrid)
    {
        // Dual-tone cyber light-pipes along the top header groove and bottom chassis edge
        g.setGradientFill (juce::ColourGradient (juce::Colour (t.accentPrimary).withAlpha (0.75f), 12.0f, 44.0f,
                                                 juce::Colour (t.accentSecondary).withAlpha (0.75f), w - 12.0f, 44.0f, false));
        g.fillRect (12.0f, 43.0f, w - 24.0f, 1.5f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (t.accentSecondary).withAlpha (0.55f), 12.0f, h - 2.0f,
                                                 juce::Colour (t.accentPrimary).withAlpha (0.55f), w - 12.0f, h - 2.0f, false));
        g.fillRect (12.0f, h - 2.2f, w - 24.0f, 1.4f);
    }
    else
    {
        g.setColour (juce::Colour (t.lightSurface ? 0x22000000 : 0xa0000000));
        g.drawHorizontalLine (43, 12.0f, w - 12.0f);
        g.setColour (juce::Colour (t.panelBorderHi).withAlpha (t.lightSurface ? 0.85f : 0.40f));
        g.drawHorizontalLine (44, 12.0f, w - 12.0f);
    }

    // Brand indicator bar + Title & calibration subtitle
    const juce::Colour brandLamp (t.lightSurface ? t.lampHot : t.accentPrimary);
    g.setColour (brandLamp.withAlpha (t.lightSurface ? 0.22f : 0.30f));
    g.fillRoundedRectangle (11.5f, 8.5f, 7.5f, 27.0f, 3.0f);
    g.setColour (brandLamp);
    g.fillRoundedRectangle (13.5f, 10.0f, 3.5f, 24.0f, 1.7f);
    g.setColour (brandLamp.interpolatedWith (juce::Colours::white, t.lightSurface ? 0.30f : 0.55f));
    g.fillRoundedRectangle (14.4f, 11.5f, 1.6f, 21.0f, 0.8f);

    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.setColour (juce::Colour (t.textBright));
    g.drawText ("HARMONIC COMPRESSOR", 23, 6, 248, 19, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    g.setColour (juce::Colour (t.textMuted));
    g.drawText ("0 dBu = -18 dBFS  /  ADAA + MULTIRATE", 24, 25, 248, 13, juce::Justification::centredLeft);

    // Live theme-matched trace key in the top bar so the Compression History viewport stays unlabeled
    g.setFont (juce::FontOptions (8.3f, juce::Font::bold));

    g.setColour (juce::Colour (t.preStroke));
    g.drawLine (276.0f, 22.0f, 288.0f, 22.0f, 2.0f);
    g.setColour (juce::Colour (t.textPrimary));
    g.drawText ("PRE", 291, 14, 26, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colour (t.postGlow).withAlpha (0.35f));
    g.fillRoundedRectangle (317.5f, 16.5f, 15.0f, 11.0f, 3.0f);
    g.setColour (juce::Colour (t.postStroke));
    g.fillRoundedRectangle (319.0f, 18.0f, 12.0f, 8.0f, 2.0f);
    g.setColour (juce::Colour (t.textPrimary));
    g.drawText ("POST", 334, 14, 32, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colour (t.grStroke));
    g.drawLine (368.0f, 22.0f, 380.0f, 22.0f, 2.3f);
    g.setColour (juce::Colour (t.textPrimary));
    g.drawText ("GR", 383, 14, 20, 16, juce::Justification::centredLeft);

    // Compact hardware section labels in top bar
    g.setFont (juce::FontOptions (8.6f, juce::Font::bold));
    g.setColour (juce::Colour (t.textMuted));
    g.drawText ("MODEL", 406, 11, 41, 22, juce::Justification::centredRight);
    g.drawText ("OS",    605, 11, 20, 22, juce::Justification::centredRight);
    g.drawText ("COLOR", 951, 11, 41, 22, juce::Justification::centredRight);
}

//==============================================================================
HarmonicCompressorAudioProcessorEditor::KeyBinding::KeyBinding (HarmonicCompressorAudioProcessor& p, const char* paramID,
                                                                  KeyButton& b, std::function<void (bool)> onChange)
    : button (b), changed (std::move (onChange)),
      attachment (*p.apvts.getParameter (paramID), [this] (float v)
      {
          const bool on = v > 0.5f;
          button.setToggleState (on, juce::dontSendNotification);
          if (changed) changed (on);
      })
{
    button.onClick = [this] { attachment.setValueAsCompleteGesture (button.getToggleState() ? 1.0f : 0.0f); };
    attachment.sendInitialUpdate();
}

//==============================================================================
Knob& HarmonicCompressorAudioProcessorEditor::addKnob (juce::Component& parent, const char* id, const char* label,
                                                       juce::Colour colour, Knob::Formatter fmt,
                                                       juce::Rectangle<int> defaultBounds, bool rotary, bool secondaryAccent)
{
    auto k = std::make_unique<Knob> (label, colour, std::move (fmt), rotary, secondaryAccent);
    k->setSkin (&skin);
    auto& ref = *k;
    parent.addAndMakeVisible (ref);
    ref.setBounds (defaultBounds);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, id, ref));
    knobById[id] = &ref;

    auto item = ref.getDesign();
    item.id = id; item.parameter = id;
    item.x = float (defaultBounds.getX()); item.y = float (defaultBounds.getY());
    item.width = float (defaultBounds.getWidth()); item.height = float (defaultBounds.getHeight());
    designItems[id] = item;
    bakedStyle[id] = item.style;
    knobs.push_back (std::move (k));
    return ref;
}

//==============================================================================
HarmonicCompressorAudioProcessorEditor::HarmonicCompressorAudioProcessorEditor (HarmonicCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setOpaque (true);
    setLookAndFeel (&popupLnf);
    loadLookState();

    // --- Canvas layout (1120 x 760) ---
    // Unlabeled Compression History viewport dominates the center field (888 x 618),
    // flanked by calibrated 44-segment Input & Output/GR lamp meters with horizontal Gain sliders
    // directly underneath each meter, and an electro-optical turbine rotary knob bar.
    addAndMakeVisible (surface);
    for (auto* c : std::initializer_list<juce::Component*> { &inputMeter, &inGainPanel, &scope, &knobBar, &meterPanel, &outGainPanel })
        surface.addAndMakeVisible (c);

    inputMeter  .setBounds (  12,  48,  96, 618);
    inGainPanel .setBounds (  12, 672,  96,  76);
    scope       .setBounds ( 116,  48, 888, 618);
    knobBar     .setBounds ( 116, 672, 888,  76);
    meterPanel  .setBounds (1012,  48,  96, 618);
    outGainPanel.setBounds (1012, 672,  96,  76);

    // --- Four closed-by-default, click-to-open, user-movable floating HUD modules inside CompressionScope ---
    for (auto* m : { &compModule, &harmModule, &scModule, &transientModule })
    {
        m->setSkin (&skin);
        scope.addAndMakeVisible (*m);
        m->setOpen (false, false);
        m->onStateChanged = [this] { saveModulePositions(); };
    }
    compModule     .setTopLeftPosition ( 68, 12);
    harmModule     .setTopLeftPosition (274, 12);
    scModule       .setTopLeftPosition (482, 12);
    transientModule.setTopLeftPosition (676, 12);
    loadModulePositions();

    // --- Top Toolbar: Model, Oversampling (1x..8x + Min/Lin Phase), Transient modifier, Curve toggle, 4-Mode Colour selector ---
    for (auto* dd : { &algorithmSelector, &oversampleSelector, &osFilterSelector, &colourModeSelector, &harmAlgoSelector, &scModeSelector })
        dd->setSkin (&skin);
    for (auto* kb : { &autoAttackKey, &autoReleaseKey, &transientHeaderKey, &transientModuleKey, &curveToggleKey })
        kb->setSkin (&skin);

    for (auto* c : std::initializer_list<juce::Component*> {
             &algorithmSelector, &oversampleSelector, &osFilterSelector,
             &transientHeaderKey, &curveToggleKey, &colourModeSelector })
        surface.addAndMakeVisible (c);

    algorithmSelector .setBounds (451, 9, 150, 27);
    oversampleSelector.setBounds (629, 9,  52, 27);
    osFilterSelector  .setBounds (685, 9,  90, 27);
    transientHeaderKey.setBounds (781, 9,  90, 27);
    curveToggleKey    .setBounds (876, 9,  72, 27);
    colourModeSelector.setBounds (996, 9, 112, 27);

    algorithmSelector.setTooltip ("Choose compressor model");
    algorithmSelector.onClick = [this]
    {
        juce::PopupMenu menu;
        for (int i = 0; i < numAlgorithms; ++i)
            menu.addItem (i + 1, algorithmNames[i], true, i == currentMode);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&algorithmSelector),
            [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int result)
            {
                if (safe != nullptr && result > 0 && result <= numAlgorithms)
                    safe->algorithmAttachment->setValueAsCompleteGesture (float (result - 1));
            });
    };
    algorithmAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.apvts.getParameter (ParamIDs::algorithmMode),
        [this] (float v)
        {
            const int index = juce::jlimit (0, numAlgorithms - 1, juce::roundToInt (v));
            algorithmSelector.setButtonText (algorithmNames[index]);
            if (index != currentMode || ! lookApplied) { lookApplied = true; applyModeLook (index); }
        });

    oversampleSelector.setTooltip ("Oversampling factor");
    oversampleSelector.onClick = [this]
    {
        juce::PopupMenu menu;
        for (int i = 0; i < kNumOversampleModes; ++i)
            menu.addItem (i + 1, kOversampleNames[i], true, i == currentOversampleIndex);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&oversampleSelector),
            [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int result)
            {
                if (safe != nullptr && result > 0 && result <= kNumOversampleModes)
                    safe->oversampleAttachment->setValueAsCompleteGesture (float (result - 1));
            });
    };
    oversampleAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.apvts.getParameter (ParamIDs::oversampleMode),
        [this] (float v)
        {
            currentOversampleIndex = juce::jlimit (0, kNumOversampleModes - 1, juce::roundToInt (v));
            oversampleSelector.setButtonText (kOversampleNames[currentOversampleIndex]);
            osFilterSelector.setEnabled (currentOversampleIndex > 0);
            osFilterSelector.setAlpha (currentOversampleIndex > 0 ? 1.0f : 0.55f);
        });

    osFilterSelector.setTooltip ("Oversampling filter phase response (Minimum Phase IIR vs Linear Phase FIR)");
    osFilterSelector.onClick = [this]
    {
        juce::PopupMenu menu;
        for (int i = 0; i < kNumOsFilters; ++i)
            menu.addItem (i + 1, kOsFilterNames[i], true, i == currentOsFilterIndex);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&osFilterSelector),
            [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int result)
            {
                if (safe != nullptr && result > 0 && result <= kNumOsFilters)
                    safe->osFilterAttachment->setValueAsCompleteGesture (float (result - 1));
            });
    };
    osFilterAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.apvts.getParameter (ParamIDs::oversampleFilter),
        [this] (float v)
        {
            currentOsFilterIndex = juce::jlimit (0, kNumOsFilters - 1, juce::roundToInt (v));
            osFilterSelector.setButtonText (kOsFilterNames[currentOsFilterIndex]);
        });

    // Curve visibility toggle (UI view option, persisted in APVTS state)
    const bool curveOn = audioProcessor.apvts.state.getProperty ("showCurve", true);
    curveToggleKey.setToggleState (curveOn, juce::dontSendNotification);
    scope.setCurveVisible (curveOn);
    curveToggleKey.setTooltip ("Show or hide the interactive Transfer Curve overlay");
    curveToggleKey.onClick = [this]
    {
        const bool show = curveToggleKey.getToggleState();
        scope.setCurveVisible (show);
        audioProcessor.apvts.state.setProperty ("showCurve", show, nullptr);
    };

    // 4-Mode Electro-Optical, Soft-White Ceramic & Cyber Colour selector (UI look option, persisted in APVTS state)
    colourModeSelector.setButtonText (skin.theme().name);
    colourModeSelector.setTooltip ("Choose colour mode (Matrix / Soft White / Cryo Plasma / Hyperdrive)");
    colourModeSelector.onClick = [this]
    {
        juce::PopupMenu menu;
        for (int i = 0; i < kNumColourThemes; ++i)
            menu.addItem (i + 1, kColourThemes[i].name, true, i == skin.colourMode);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&colourModeSelector),
            [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int result)
            {
                if (safe != nullptr && result > 0 && result <= kNumColourThemes)
                    safe->setColourMode (result - 1);
            });
    };

    // --- Input & Output Gain horizontal sliders directly beneath their respective meters ---
    addKnob (inGainPanel,  ParamIDs::inputGain,  "GAIN", juce::Colour (0xff2e86c1),
             [] (double v) { return dbText (v, true); }, { 6, 26, 84, 44 }, false, true);
    addKnob (outGainPanel, ParamIDs::outputGain, "GAIN", juce::Colour (0xff3ccf5a),
             [] (double v) { return dbText (v, true); }, { 6, 26, 84, 44 }, false, false);

    // --- Four electro-optical turbine rotary knobs (Threshold, Ratio, Makeup, Mix) in the bottom bar ---
    const juce::Colour gold (0xffc7a66a), blue (0xff5899c4);
    addKnob (knobBar, ParamIDs::threshold,  "THRESHOLD", gold, [] (double v) { return dbText (v); },                  {  19, 6, 196, 64 }, true, false);
    addKnob (knobBar, ParamIDs::ratio,      "RATIO",     gold, ratioText,                                             { 237, 6, 196, 64 }, true, false);
    addKnob (knobBar, ParamIDs::makeupGain, "MAKEUP",    blue, [] (double v) { return dbText (v, true); },            { 455, 6, 196, 64 }, true, true);
    addKnob (knobBar, ParamIDs::mix,        "MIX",       blue, [] (double v) { return percentText (v * 100.0, 0); },  { 673, 6, 196, 64 }, true, true);

    // --- 1. Compression floating module (Attack, Release, Knee sliders + Auto Attack/Release toggles) ---
    addKnob (compModule.content(), ParamIDs::attack,  "ATTACK",  gold, msText,                               {  10, 6, 122, 42 }, false, false);
    addKnob (compModule.content(), ParamIDs::release, "RELEASE", gold, msText,                               { 142, 6, 122, 42 }, false, false);
    addKnob (compModule.content(), ParamIDs::knee,    "KNEE",    blue, [] (double v) { return dbText (v); }, { 274, 6, 122, 42 }, false, true);

    compModule.content().addAndMakeVisible (autoAttackKey);
    compModule.content().addAndMakeVisible (autoReleaseKey);
    autoAttackKey .setBounds ( 10, 52, 122, 22);
    autoReleaseKey.setBounds (142, 52, 122, 22);
    autoAttackKey .setTooltip ("Program-dependent attack");
    autoReleaseKey.setTooltip ("Program-dependent release");

    keyBindings.push_back (std::make_unique<KeyBinding> (audioProcessor, ParamIDs::autoAttack, autoAttackKey,
        [this] (bool on)
        {
            if (auto* k = findControl (ParamIDs::attack)) k->setActive (! on);
            updateModuleSummaries();
        }));
    keyBindings.push_back (std::make_unique<KeyBinding> (audioProcessor, ParamIDs::autoRelease, autoReleaseKey,
        [this] (bool on)
        {
            if (auto* k = findControl (ParamIDs::release)) k->setActive (! on);
            updateModuleSummaries();
        }));

    // --- 2. Harmonics floating module (Algorithm dropdown + 2nd..5th horizontal sliders) ---
    harmModule.content().addAndMakeVisible (harmAlgoSelector);
    harmAlgoSelector.setBounds (10, 8, 146, 28);
    harmAlgoSelector.setTooltip ("Choose harmonics saturation character or custom harmonic mix");
    harmAlgoSelector.onClick = [this]
    {
        const int activePreset = detectHarmonicsPresetIndex();
        juce::PopupMenu menu;
        for (int i = 0; i < kNumHarmonicsPresets; ++i)
            menu.addItem (i + 1, kHarmonicsPresets[i].name, true, i == activePreset);
        menu.addSeparator();
        menu.addItem (kCustomHarmonicsIndex + 1, "Custom", true, activePreset == kCustomHarmonicsIndex);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&harmAlgoSelector),
            [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int result)
            {
                if (safe != nullptr && result > 0)
                    safe->applyHarmonicsPreset (result - 1);
            });
    };

    addKnob (harmModule.content(), ParamIDs::harmonic2, "2ND", juce::Colour (0xffcda434), [] (double v) { return percentText (v, 1); }, { 166,  5, 122, 42 }, false, false);
    addKnob (harmModule.content(), ParamIDs::harmonic3, "3RD", juce::Colour (0xffc9702e), [] (double v) { return percentText (v, 1); }, { 298,  5, 122, 42 }, false, true);
    addKnob (harmModule.content(), ParamIDs::harmonic4, "4TH", juce::Colour (0xffb3382c), [] (double v) { return percentText (v, 1); }, { 166, 51, 122, 42 }, false, false);
    addKnob (harmModule.content(), ParamIDs::harmonic5, "5TH", juce::Colour (0xff3fa796), [] (double v) { return percentText (v, 1); }, { 298, 51, 122, 42 }, false, true);

    const char* const harmIds[4] = { ParamIDs::harmonic2, ParamIDs::harmonic3, ParamIDs::harmonic4, ParamIDs::harmonic5 };
    for (size_t i = 0; i < 4; ++i)
    {
        harmAttachments[i] = std::make_unique<juce::ParameterAttachment> (
            *audioProcessor.apvts.getParameter (harmIds[i]),
            [this, i] (float v)
            {
                harmState[i] = v;
                if (! applyingHarmonicsPreset)
                    customHarmonicsLatched = false;
                updateHarmonicsSelectorText();
                updateModuleSummaries();
            });
        harmAttachments[i]->sendInitialUpdate();
    }

    // --- 3. Sidechain floating module (Options dropdown + HP Freq horizontal slider) ---
    scModule.content().addAndMakeVisible (scModeSelector);
    scModeSelector.setBounds (10, 13, 132, 28);
    scModeSelector.setTooltip ("Choose sidechain detector filter and audition mode");
    scModeSelector.onClick = [this]
    {
        const int activeSc = currentSidechainModeIndex();
        juce::PopupMenu menu;
        for (int i = 0; i < kNumSidechainModes; ++i)
            menu.addItem (i + 1, kSidechainModes[i], true, i == activeSc);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&scModeSelector),
            [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int result)
            {
                if (safe != nullptr && result > 0 && result <= kNumSidechainModes)
                    safe->applySidechainMode (result - 1);
            });
    };

    addKnob (scModule.content(), ParamIDs::scHPFreq, "HP FREQ", juce::Colour (0xff4a86c8),
             [] (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; }, { 152, 6, 144, 42 }, false, true);

    scEnableAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.apvts.getParameter (ParamIDs::scEnabled),
        [this] (float v)
        {
            scEnabledState = v > 0.5f;
            if (auto* k = findControl (ParamIDs::scHPFreq))
                k->setActive (scEnabledState);
            scModule.setHighlighted (scEnabledState || scListenState);
            updateSidechainSelectorText();
            updateModuleSummaries();
        });
    scListenAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.apvts.getParameter (ParamIDs::scListen),
        [this] (float v)
        {
            scListenState = v > 0.5f;
            scModule.setHighlighted (scEnabledState || scListenState);
            updateSidechainSelectorText();
            updateModuleSummaries();
        });
    scEnableAttachment->sendInitialUpdate();
    scListenAttachment->sendInitialUpdate();

    // --- 4. End-of-Chain Transient Designer modifier & floating module ---
    transientHeaderKey.setTooltip ("Enable end-of-chain Transient Designer modifier (post-mix, pre-output-gain)");
    transientModuleKey.setTooltip ("Enable or bypass Transient Designer");
    transientModule.content().addAndMakeVisible (transientModuleKey);
    transientModuleKey.setBounds (10, 15, 68, 26);

    addKnob (transientModule.content(), ParamIDs::transientAttack,  "ATTACK",  juce::Colour (0xff2c9973),
             [] (double v) { return percentText (v, 0, true); }, {  88, 7, 112, 42 }, false, false);
    addKnob (transientModule.content(), ParamIDs::transientSustain, "SUSTAIN", juce::Colour (0xff2d8bba),
             [] (double v) { return percentText (v, 0, true); }, { 208, 7, 112, 42 }, false, true);
    addKnob (transientModule.content(), ParamIDs::transientSpeed,   "SPEED",   juce::Colour (0xff6a8caf),
             msText,                                             { 328, 7, 116, 42 }, false, false);

    keyBindings.push_back (std::make_unique<KeyBinding> (audioProcessor, ParamIDs::transientEnabled, transientHeaderKey,
        [this] (bool on)
        {
            transientEnabledState = on;
            transientModuleKey.setToggleState (on, juce::dontSendNotification);
            transientModule.setHighlighted (on);
            for (const char* id : { ParamIDs::transientAttack, ParamIDs::transientSustain, ParamIDs::transientSpeed })
                if (auto* k = findControl (id))
                    k->setActive (on);
            updateModuleSummaries();
        }));
    transientModuleKey.onClick = [this]
    {
        if (auto* param = audioProcessor.apvts.getParameter (ParamIDs::transientEnabled))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (transientModuleKey.getToggleState() ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
    };

    applyBakedDesign();
    updateHarmonicsSelectorText();
    oversampleAttachment->sendInitialUpdate();
    osFilterAttachment->sendInitialUpdate();
    algorithmAttachment->sendInitialUpdate();
    updateModuleSummaries();
    startTimerHz (20);

#if GOODLOOKINUI_ENABLE_EDITOR
    for (auto& [id, knob] : knobById)
    {
        designStudio.add (designItems[id], *knob, [this, k = knob] (const glui::Item& v) { onKnobDesignEdited (*k, v); }, knobName (id));
        knob->setTooltip (juce::String (knobName (id)) + "   [" + id + "]");
        knob->onSelect = [this, k = knob] { if (designStudio.isVisible()) designStudio.select (k); };
    }
    addChildComponent (designStudio);
    designStudio.onSelect = [this] (juce::Component* c) { highlightKnob (c); };
    surface.addChildComponent (ring);
    for (auto* b : { &designButton, &plateButton, &knobsButton, &metersButton, &keysButton, &glowButton })
        surface.addAndMakeVisible (*b);
    designButton.onClick = [this]
    {
        const bool show = ! designStudio.isVisible();
        designStudio.setVisible (show);
        designStudio.toFront (false);
        highlightKnob (designStudio.selected());
    };
    plateButton.onClick  = [this] { showPlateMenu(); };
    knobsButton.onClick  = [this] { showKnobMenu(); };
    metersButton.onClick = [this] { showMeterMenu(); };
    keysButton.onClick   = [this] { showKeyMenu(); };
    glowButton.setClickingTogglesState (true);
    glowButton.setToggleState (gj::retro::glowEnabled, juce::dontSendNotification);
    glowButton.onClick = [this] { gj::retro::glowEnabled = glowButton.getToggleState(); saveLookState(); repaint(); };
    plateButton.setTooltip ("Faceplate for the current algorithm");
    knobsButton.setTooltip ("Rotary knob style for the current algorithm");
    metersButton.setTooltip ("Meter face setting");
    keysButton.setTooltip ("Push-key style for the current algorithm");
#endif

    setResizable (true, true);
    setResizeLimits (560, int (560.0 * canvasHeight / canvasWidth), 1600, int (1600.0 * canvasHeight / canvasWidth));
    getConstrainer()->setFixedAspectRatio ((double) canvasWidth / canvasHeight);
    const int savedWidth = juce::jlimit (560, 1600, (int) audioProcessor.apvts.state.getProperty ("editorWidth", canvasWidth));
    setSize (savedWidth, juce::roundToInt (savedWidth * (double) canvasHeight / canvasWidth));
    applySkin();
}

HarmonicCompressorAudioProcessorEditor::~HarmonicCompressorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    audioProcessor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
    saveModulePositions();
    keyBindings.clear();
    for (auto& a : harmAttachments) a.reset();
    scEnableAttachment.reset();
    scListenAttachment.reset();
    oversampleAttachment.reset();
    osFilterAttachment.reset();
    algorithmAttachment.reset();
    sliderAttachments.clear();
}

void HarmonicCompressorAudioProcessorEditor::timerCallback()
{
    updateModuleSummaries();
}

void HarmonicCompressorAudioProcessorEditor::setColourMode (int modeIndex)
{
    const int clamped = juce::jlimit (0, kNumColourThemes - 1, modeIndex);
    skin.colourMode = clamped;
    colourModeSelector.setButtonText (skin.theme().name);
    saveLookState();
    applySkin();
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::saveModulePositions()
{
    auto& s = audioProcessor.apvts.state;
    s.setProperty ("modCompX",  compModule.getX(),      nullptr);
    s.setProperty ("modCompY",  compModule.getY(),      nullptr);
    s.setProperty ("modHarmX",  harmModule.getX(),      nullptr);
    s.setProperty ("modHarmY",  harmModule.getY(),      nullptr);
    s.setProperty ("modScX",    scModule.getX(),        nullptr);
    s.setProperty ("modScY",    scModule.getY(),        nullptr);
    s.setProperty ("modTransX", transientModule.getX(), nullptr);
    s.setProperty ("modTransY", transientModule.getY(), nullptr);
}

void HarmonicCompressorAudioProcessorEditor::loadModulePositions()
{
    const auto& s = audioProcessor.apvts.state;
    compModule     .setTopLeftPosition ((int) s.getProperty ("modCompX",   68), (int) s.getProperty ("modCompY",  12));
    harmModule     .setTopLeftPosition ((int) s.getProperty ("modHarmX",  274), (int) s.getProperty ("modHarmY",  12));
    scModule       .setTopLeftPosition ((int) s.getProperty ("modScX",    482), (int) s.getProperty ("modScY",    12));
    transientModule.setTopLeftPosition ((int) s.getProperty ("modTransX", 676), (int) s.getProperty ("modTransY", 12));
    for (auto* m : { &compModule, &harmModule, &scModule, &transientModule })
        m->clampToParent();
}

void HarmonicCompressorAudioProcessorEditor::updateModuleSummaries()
{
    auto rawVal = [this] (const char* id) -> float
    {
        if (auto* p = audioProcessor.apvts.getRawParameterValue (id))
            return p->load (std::memory_order_relaxed);
        return 0.0f;
    };

    // 1. Compression summary (Attack, Release, Knee, Auto Attack, Auto Release)
    const float attMs   = rawVal (ParamIDs::attack);
    const float relMs   = rawVal (ParamIDs::release);
    const float kneeDb  = rawVal (ParamIDs::knee);
    const bool  autoAtt = rawVal (ParamIDs::autoAttack) > 0.5f;
    const bool  autoRel = rawVal (ParamIDs::autoRelease) > 0.5f;
    compModule.setSummary ((autoAtt ? "A" : "") + compactMsText (attMs)
                           + " " + (autoRel ? "A" : "") + compactMsText (relMs)
                           + " K" + juce::String (kneeDb, 1));

    // 2. Harmonics summary (Preset + 2nd/3rd/4th/5th amounts)
    const float h2 = rawVal (ParamIDs::harmonic2);
    const float h3 = rawVal (ParamIDs::harmonic3);
    const float h4 = rawVal (ParamIDs::harmonic4);
    const float h5 = rawVal (ParamIDs::harmonic5);
    const int hIdx = detectHarmonicsPresetIndex();
    const char* hTag = (hIdx == kCustomHarmonicsIndex) ? "Cst" : kHarmonicsPresets[hIdx].shortTag;
    harmModule.setHighlighted ((h2 + h3 + h4 + h5) > 0.25f);
    harmModule.setSummary (juce::String (hTag) + " "
                           + juce::String (juce::roundToInt (h2)) + "/"
                           + juce::String (juce::roundToInt (h3)) + "/"
                           + juce::String (juce::roundToInt (h4)) + "/"
                           + juce::String (juce::roundToInt (h5)));

    // 3. Sidechain summary (Mode + HP Freq Hz)
    const float scHz = rawVal (ParamIDs::scHPFreq);
    scModule.setSummary (juce::String (kSidechainShortTags[currentSidechainModeIndex()])
                         + " " + juce::String (juce::roundToInt (scHz)) + "Hz");

    // 4. Transient Designer summary (ON/OFF + Attack/Sustain/Speed)
    const bool  trOn  = rawVal (ParamIDs::transientEnabled) > 0.5f;
    const int   trAtt = juce::roundToInt (rawVal (ParamIDs::transientAttack));
    const int   trSus = juce::roundToInt (rawVal (ParamIDs::transientSustain));
    const float trSpd = rawVal (ParamIDs::transientSpeed);
    const auto fmtSigned = [] (int v) { return (v > 0 ? "+" : "") + juce::String (v); };
    transientModule.setSummary (juce::String (trOn ? "ON " : "OFF ")
                                + fmtSigned (trAtt) + "/" + fmtSigned (trSus)
                                + " " + juce::String (trSpd, 1) + "m");
}

//==============================================================================
int HarmonicCompressorAudioProcessorEditor::detectHarmonicsPresetIndex() const
{
    const float h2 = harmState[0];
    const float h3 = harmState[1];
    const float h4 = harmState[2];
    const float h5 = harmState[3];

    for (int i = 0; i < kNumHarmonicsPresets; ++i)
    {
        if (i == 0 && customHarmonicsLatched)
            continue;
        const auto& p = kHarmonicsPresets[i];
        if (std::abs (h2 - p.h2) <= 0.25f &&
            std::abs (h3 - p.h3) <= 0.25f &&
            std::abs (h4 - p.h4) <= 0.25f &&
            std::abs (h5 - p.h5) <= 0.25f)
            return i;
    }
    return kCustomHarmonicsIndex;
}

void HarmonicCompressorAudioProcessorEditor::applyHarmonicsPreset (int presetIndex)
{
    if (presetIndex == kCustomHarmonicsIndex)
    {
        customHarmonicsLatched = true;
        updateHarmonicsSelectorText();
        updateModuleSummaries();
        return;
    }
    if (presetIndex < 0 || presetIndex >= kNumHarmonicsPresets)
        return;

    customHarmonicsLatched = false;
    applyingHarmonicsPreset = true;

    const auto& preset = kHarmonicsPresets[presetIndex];
    const float targets[4] = { preset.h2, preset.h3, preset.h4, preset.h5 };

    for (size_t i = 0; i < 4; ++i)
        if (harmAttachments[i] != nullptr)
            harmAttachments[i]->setValueAsCompleteGesture (targets[i]);

    applyingHarmonicsPreset = false;
    updateHarmonicsSelectorText();
    updateModuleSummaries();
}

void HarmonicCompressorAudioProcessorEditor::updateHarmonicsSelectorText()
{
    const int idx = detectHarmonicsPresetIndex();
    harmAlgoSelector.setButtonText (idx == kCustomHarmonicsIndex ? "Custom" : kHarmonicsPresets[idx].name);
}

//==============================================================================
int HarmonicCompressorAudioProcessorEditor::currentSidechainModeIndex() const
{
    if (! scEnabledState && ! scListenState) return 0; // Off
    if (  scEnabledState && ! scListenState) return 1; // HPF On
    if (  scEnabledState &&   scListenState) return 2; // Listen (HPF)
    return 3;                                          // Listen (Dry)
}

void HarmonicCompressorAudioProcessorEditor::applySidechainMode (int modeIndex)
{
    const bool wantEnabled = (modeIndex == 1 || modeIndex == 2);
    const bool wantListen  = (modeIndex == 2 || modeIndex == 3);

    if (scEnableAttachment && wantEnabled != scEnabledState)
        scEnableAttachment->setValueAsCompleteGesture (wantEnabled ? 1.0f : 0.0f);
    if (scListenAttachment && wantListen != scListenState)
        scListenAttachment->setValueAsCompleteGesture (wantListen ? 1.0f : 0.0f);
    updateSidechainSelectorText();
    updateModuleSummaries();
}

void HarmonicCompressorAudioProcessorEditor::updateSidechainSelectorText()
{
    scModeSelector.setButtonText (kSidechainModes[currentSidechainModeIndex()]);
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::applyBakedDesign()
{
    try
    {
        std::istringstream in (harmonicCompressorDesign);
        for (auto& item : glui::readDesign (in))
        {
            auto it = knobById.find (item.id);
            if (it == knobById.end() || item.parameter != item.id) continue;
            designItems[item.id] = item;
            bakedStyle[item.id] = item.style;
            it->second->applyDesign (item);
            it->second->setBounds (juce::roundToInt (item.x), juce::roundToInt (item.y), juce::roundToInt (item.width), juce::roundToInt (item.height));
        }
    }
    catch (const std::exception&) { /* keep the built-in layout */ }
}

void HarmonicCompressorAudioProcessorEditor::applySkin()
{
    const auto& t = skin.theme();
    colourModeSelector.setButtonText (t.name);

    const juce::Colour primary (t.accentPrimary);
    const juce::Colour secondary (t.accentSecondary);
    const juce::Colour warn (t.lampWarn);
    const juce::Colour threshCol (t.thresholdRef);
    const juce::Colour algoTint = primary.interpolatedWith (juce::Colour (algorithmColours[juce::jlimit (0, numAlgorithms - 1, currentMode)]), 0.28f);

    inputMeter  .setAccent (warn);
    meterPanel  .setAccent (primary);
    inGainPanel .setAccent (secondary);
    outGainPanel.setAccent (primary);

    compModule     .setAccent (algoTint);
    harmModule     .setAccent (warn);
    scModule       .setAccent (secondary);
    transientModule.setAccent (threshCol);

    algorithmSelector .setAccent (algoTint);
    oversampleSelector.setAccent (secondary);
    osFilterSelector  .setAccent (secondary);
    colourModeSelector.setAccent (primary);
    harmAlgoSelector  .setAccent (warn);
    scModeSelector    .setAccent (secondary);

    autoAttackKey     .setLamp (t.lightSurface ? algoTint : algoTint.brighter (0.15f));
    autoReleaseKey    .setLamp (t.lightSurface ? algoTint : algoTint.brighter (0.15f));
    transientHeaderKey.setLamp (threshCol);
    transientModuleKey.setLamp (threshCol);
    curveToggleKey    .setLamp (secondary);

    for (auto& k : knobs)
    {
        k->setSkin (&skin);
        k->setTextColours (skin.text(), skin.mid());
    }

    for (auto* key : { &autoAttackKey, &autoReleaseKey, &transientHeaderKey, &transientModuleKey, &curveToggleKey })
    {
        key->setSkin (&skin);
        key->setStyle (skin.pushStyle);
    }
    for (auto* dd : { &algorithmSelector, &oversampleSelector, &osFilterSelector, &colourModeSelector, &harmAlgoSelector, &scModeSelector })
        dd->setSkin (&skin);
    for (auto* mod : { &compModule, &harmModule, &scModule, &transientModule })
        mod->setSkin (&skin);

    surface.repaint();
    for (auto* c : std::initializer_list<juce::Component*> {
             &inputMeter, &inGainPanel, &scope, &knobBar, &meterPanel, &outGainPanel,
             &compModule, &harmModule, &scModule, &transientModule })
        c->repaint();
    repaint();
}

void HarmonicCompressorAudioProcessorEditor::applyKnobStyle (const juce::String& code)
{
    knobStyleOverride = code;
    for (auto& [id, knob] : knobById)
    {
        auto item = designItems[id];
        if (code.isNotEmpty()) item.style = code.toStdString();
        knob->applyDesign (item);
    }
}

// Switching algorithm updates timing enablement and harmonizes the model accent with the active colour mode.
void HarmonicCompressorAudioProcessorEditor::applyModeLook (int mode)
{
    mode = juce::jlimit (0, numAlgorithms - 1, mode);
    currentMode = mode;
    const bool circuitTiming = mode == (int) CompressorMode::OptoLeveler || mode == (int) CompressorMode::VCA160;
    for (const char* id : { ParamIDs::attack, ParamIDs::release, ParamIDs::knee })
        if (auto* k = findControl (id))
        {
            k->setEnabled (! circuitTiming);
            k->setAlpha (circuitTiming ? 0.45f : 1.0f);
            k->setTooltip (circuitTiming ? "Circuit-defined response" : "");
        }
    autoAttackKey.setEnabled (! circuitTiming);
    autoReleaseKey.setEnabled (! circuitTiming);
    if (auto* k = findControl (ParamIDs::ratio))
        k->setTooltip (mode == (int) CompressorMode::OptoLeveler ? "Compress below 10:1; Limit at 10:1 and above" : "");

    auto& s = audioProcessor.apvts.state;
    const auto n = juce::String (mode);

    if (auto* f = glui::findFaceplate (s.getProperty ("plate" + n, "Graphite").toString().toStdString())) skin.plate = f;
    skin.pushStyle = juce::jlimit (0, gj::switches::numPushStyles - 1, (int) s.getProperty ("push" + n, 3));
    const auto knob = s.getProperty ("knob" + n, "Turbine").toString();
    applyKnobStyle (knob == "design" ? juce::String() : knob);
    updateHarmonicsSelectorText();
    updateSidechainSelectorText();
    updateModuleSummaries();
    applySkin();
}

void HarmonicCompressorAudioProcessorEditor::onKnobDesignEdited (Knob& knob, const glui::Item& item)
{
    auto shown = item;
    if (knobStyleOverride.isNotEmpty() && item.style == bakedStyle[item.id]) shown.style = knobStyleOverride.toStdString();
    designItems[item.id] = item;
    knob.applyDesign (shown);
}

//==============================================================================
// Look choices are UI-only: saved in the plugin state next to the parameters, never read by the DSP.
void HarmonicCompressorAudioProcessorEditor::saveLookState()
{
    auto& s = audioProcessor.apvts.state;
    s.setProperty ("colourMode", skin.colourMode, nullptr);
    s.setProperty ("meterFace", skin.meterFace, nullptr);
    s.setProperty ("glow", gj::retro::glowEnabled ? "on" : "off", nullptr);
}

// Developer menus change the look of the algorithm that is currently selected.
void HarmonicCompressorAudioProcessorEditor::saveModeLook (const char* what, const juce::String& value)
{
    audioProcessor.apvts.state.setProperty (juce::String (what) + juce::String (currentMode), value, nullptr);
}

void HarmonicCompressorAudioProcessorEditor::loadLookState()
{
    auto& s = audioProcessor.apvts.state;
    skin.colourMode = juce::jlimit (0, kNumColourThemes - 1, (int) s.getProperty ("colourMode", 0));
    skin.meterFace = juce::jlimit (0, 2, (int) s.getProperty ("meterFace", 2));
    gj::retro::glowEnabled = s.getProperty ("glow", "on").toString() != "off";
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (skin.theme().chassisBottom));
}

void HarmonicCompressorAudioProcessorEditor::resized()
{
    const float scale = juce::jmin (float (getWidth()) / canvasWidth, float (getHeight()) / canvasHeight);
    surface.setBounds (0, 0, canvasWidth, canvasHeight);
    surface.setTransform (juce::AffineTransform::scale (scale).translated ((getWidth() - canvasWidth * scale) * 0.5f,
                                                                           (getHeight() - canvasHeight * scale) * 0.5f));
#if GOODLOOKINUI_ENABLE_EDITOR
    int x = 12;
    for (auto* b : { &designButton, &plateButton, &knobsButton, &metersButton, &keysButton, &glowButton })
    {
        b->setBounds (x, panelsHeight + 5, 88, 24);
        x += 96;
    }
    designStudio.setBounds (8, 8, getWidth() - 16, 114);
#endif
}

//==============================================================================
#if GOODLOOKINUI_ENABLE_EDITOR
void HarmonicCompressorAudioProcessorEditor::highlightKnob (juce::Component* c)
{
    if (c == nullptr || ! designStudio.isVisible()) { ring.setVisible (false); return; }
    ring.setBounds (surface.getLocalArea (c->getParentComponent(), c->getBounds()).expanded (2));
    ring.setVisible (true);
    ring.toFront (false);
}

void HarmonicCompressorAudioProcessorEditor::showPlateMenu()
{
    juce::PopupMenu menu;
    int id = 1;
    for (const auto& f : glui::faceplates)
        menu.addItem (id++, juce::String (f.name), true, skin.plate == &f);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&plateButton),
        [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            safe->skin.plate = &glui::faceplates[r - 1];
            safe->saveModeLook ("plate", glui::faceplates[r - 1].code);
            safe->applySkin();
        });
}

void HarmonicCompressorAudioProcessorEditor::showKnobMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "DESIGN (as saved)", true, knobStyleOverride.isEmpty());
    menu.addSeparator();
    int id = 2;
    for (const auto& s : glui::knobStyles)
        menu.addItem (id++, juce::String (s.code) + "  -  " + s.description, true, knobStyleOverride == s.code);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&knobsButton),
        [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            const juce::String code = r == 1 ? juce::String() : juce::String (glui::knobStyles[r - 2].code);
            safe->saveModeLook ("knob", code.isEmpty() ? juce::String ("design") : code);
            safe->applyKnobStyle (code);
        });
}

void HarmonicCompressorAudioProcessorEditor::showMeterMenu()
{
    juce::PopupMenu menu;
    const char* names[] = { "Cream VU needles", "Black VU needles", "LED ladders" };
    for (int i = 0; i < 3; ++i) menu.addItem (i + 1, names[i], true, skin.meterFace == i);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&metersButton),
        [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            safe->skin.meterFace = r - 1;
            safe->saveLookState();
            safe->applySkin();
        });
}

void HarmonicCompressorAudioProcessorEditor::showKeyMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < gj::switches::numPushStyles; ++i) menu.addItem (i + 1, gj::switches::pushNames[i], true, skin.pushStyle == i);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keysButton),
        [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            safe->skin.pushStyle = r - 1;
            safe->saveModeLook ("push", juce::String (r - 1));
            safe->applySkin();
        });
}
#endif
