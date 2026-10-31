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

    const char* const algorithmNames[] = { "Optical", "VCA", "FET", "Red", "Hybrid", "Transistor Bridge", "Bus VCA", "FET Circuit", "Opto Leveler", "RMS VCA Dynamics" };
    // Each mode keeps the lamp colour it had before the redesign.
    const juce::uint32 algorithmColours[] = { 0xffcda434, 0xffc9702e, 0xff4a86c8, 0xffb3382c, 0xff3fa796, 0xffcda434, 0xffc9702e, 0xff4a86c8, 0xffb58a47, 0xff377f9d };

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
    juce::String dbText (double v, bool sign = false) { return (sign && v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String percentText (double v, int decimals) { return juce::String (v, decimals) + "%"; }

    // Readable names for the developer tools.
    const char* knobName (const std::string& id)
    {
        static const std::pair<const char*, const char*> names[] = {
            { "threshold", "Compression: Threshold" }, { "ratio", "Compression: Ratio" },
            { "attack", "Compression: Attack" }, { "release", "Compression: Release" },
            { "knee", "Compression: Knee" }, { "makeupGain", "Compression: Makeup gain" }, { "mix", "Compression: Mix" },
            { "harmonic2", "Harmonics: 2nd" }, { "harmonic3", "Harmonics: 3rd" },
            { "harmonic4", "Harmonics: 4th" }, { "harmonic5", "Harmonics: 5th" },
            { "scHPFreq", "Sidechain: HP frequency" } };
        for (auto& n : names) if (id == n.first) return n.second;
        return "";
    }
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::Surface::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff8ca6bd));
    g.setColour(juce::Colour(0xff236d9b));
    g.fillRoundedRectangle(24, 24, 4, 32, 2);
    g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff244762));
    g.drawText("HARMONIC", 40, 18, 300, 28, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(juce::Colour(0xff456b87));
    g.drawText("COMPRESSOR  /  DYNAMICS & COLOUR", 41, 47, 350, 18, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("COMPRESSOR MODEL", 714, 10, 270, 18, juce::Justification::centredLeft);

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
Knob& HarmonicCompressorAudioProcessorEditor::addKnob (Panel& parent, const char* id, const char* label, juce::Colour colour,
                                                       Knob::Formatter fmt, juce::Rectangle<int> defaultBounds)
{
    auto k = std::make_unique<Knob> (label, colour, std::move (fmt));
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
    loadLookState();

    // --- Panels on the design canvas ---
    addAndMakeVisible (surface);
    for (auto* c : std::initializer_list<juce::Component*> { &compPanel, &harmPanel, &scPanel, &curvePanel, &meterPanel, &inputMeter, &scope })
        surface.addAndMakeVisible (c);
    compPanel.setBounds (104, 390, 580, 350);
    curvePanel.setBounds (680, 88, 308, 286);
    harmPanel.setBounds (700, 390, 288, 182);
    scPanel.setBounds (700, 588, 288, 152);
    meterPanel.setBounds (1000, 88, 108, 652);
    inputMeter.setBounds(12, 88, 80, 652);
    scope.setBounds(104, 88, 560, 286);
    compPanel.setAccent (juce::Colour (0xffc7a66a));
    curvePanel.setAccent (juce::Colour (0xff39f2ff));
    harmPanel.setAccent (juce::Colour (0xffb3382c));
    scPanel.setAccent (juce::Colour (0xff4a86c8));
    meterPanel.setAccent (juce::Colour (0xff3ccf5a));

    curvePanel.addAndMakeVisible (transferCurve);
    transferCurve.setBounds (8, 44, 292, 230);

    surface.addAndMakeVisible(algorithmSelector);
    algorithmSelector.setBounds(714, 32, 274, 36);
    algorithmSelector.setTooltip("Choose compressor model");
    algorithmSelector.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe9f1f7));
    algorithmSelector.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff285a7b));
    algorithmSelector.onClick = [this]
    {
        juce::PopupMenu menu;
        for (int i = 0; i < numAlgorithms; ++i)
            menu.addItem(i + 1, algorithmNames[i], true, i == currentMode);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&algorithmSelector),
            [safe = juce::Component::SafePointer<HarmonicCompressorAudioProcessorEditor>(this)](int result)
            {
                if (safe != nullptr && result > 0 && result <= numAlgorithms)
                    safe->algorithmAttachment->setValueAsCompleteGesture(float(result - 1));
            });
    };
    algorithmAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.apvts.getParameter (ParamIDs::algorithmMode),
        [this] (float v)
        {
            // Each mode's remembered parameter values are restored by the processor; here we only mirror the selection.
            const int index = juce::jlimit(0, numAlgorithms - 1, juce::roundToInt(v));
            algorithmSelector.setButtonText(juce::String(algorithmNames[index]) + "   /   SELECT");
            if (index != currentMode || ! lookApplied) { lookApplied = true; applyModeLook (index); }
        });
    // The first update below fires before the knobs exist, so it only records the mode.

    // --- Knobs: same parameters as before, drawn by the toolkit ---
    const juce::Colour gold (0xffc7a66a), blue (0xff7fb3d5);
    addKnob (compPanel, ParamIDs::threshold, "THRESHOLD", gold, [] (double v) { return dbText (v); }, { 12, 50, 108, 108 });
    addKnob (compPanel, ParamIDs::ratio, "RATIO", gold, ratioText, { 126, 50, 108, 108 });
    addKnob (compPanel, ParamIDs::attack, "ATTACK", gold, msText, { 240, 50, 108, 108 });
    addKnob (compPanel, ParamIDs::release, "RELEASE", gold, msText, { 354, 50, 108, 108 });
    addKnob (compPanel, ParamIDs::knee, "KNEE", blue, [] (double v) { return dbText (v); }, { 12, 160, 108, 108 });
    addKnob (compPanel, ParamIDs::makeupGain, "MAKEUP", blue, [] (double v) { return dbText (v, true); }, { 126, 160, 108, 108 });
    addKnob (compPanel, ParamIDs::mix, "MIX", blue, [] (double v) { return percentText (v * 100.0, 0); }, { 240, 160, 108, 108 });
    addKnob (harmPanel, ParamIDs::harmonic2, "2ND", juce::Colour (0xffcda434), [] (double v) { return percentText (v, 1); }, { 8, 56, 76, 106 });
    addKnob (harmPanel, ParamIDs::harmonic3, "3RD", juce::Colour (0xffc9702e), [] (double v) { return percentText (v, 1); }, { 86, 56, 76, 106 });
    addKnob (harmPanel, ParamIDs::harmonic4, "4TH", juce::Colour (0xffb3382c), [] (double v) { return percentText (v, 1); }, { 164, 56, 76, 106 });
    addKnob (harmPanel, ParamIDs::harmonic5, "5TH", juce::Colour (0xff3fa796), [] (double v) { return percentText (v, 1); }, { 242, 56, 76, 106 });
    addKnob (scPanel, ParamIDs::scHPFreq, "HP FREQ", juce::Colour (0xff4a86c8),
             [] (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; }, { 8, 56, 84, 106 });

    // --- Keys: auto attack / release grey their own knob, sidechain enable / listen ---
    compPanel.addAndMakeVisible (autoAttackKey);
    compPanel.addAndMakeVisible (autoReleaseKey);
    scPanel.addAndMakeVisible (scEnableKey);
    scPanel.addAndMakeVisible (scListenKey);
    autoAttackKey.setBounds (440, 184, 116, 32);
    autoReleaseKey.setBounds (440, 228, 116, 32);
    scEnableKey.setBounds (152, 55, 112, 30);
    scListenKey.setBounds (152, 100, 112, 30);
    autoAttackKey.setTooltip ("Program-dependent attack");
    autoReleaseKey.setTooltip ("Program-dependent release");
    scListenKey.setTooltip ("Listen to the sidechain signal");
    keyBindings.push_back (std::make_unique<KeyBinding> (audioProcessor, ParamIDs::autoAttack, autoAttackKey,
                                                         [this] (bool on) { knobById["attack"]->setActive (! on); }));
    keyBindings.push_back (std::make_unique<KeyBinding> (audioProcessor, ParamIDs::autoRelease, autoReleaseKey,
                                                         [this] (bool on) { knobById["release"]->setActive (! on); }));
    keyBindings.push_back (std::make_unique<KeyBinding> (audioProcessor, ParamIDs::scEnabled, scEnableKey,
                                                         [this] (bool on) { knobById["scHPFreq"]->setActive (on); }));
    keyBindings.push_back (std::make_unique<KeyBinding> (audioProcessor, ParamIDs::scListen, scListenKey));

    applyBakedDesign();
    algorithmAttachment->sendInitialUpdate();

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
    knobsButton.setTooltip ("Knob class for the current algorithm");
    metersButton.setTooltip ("Meter face: cream VU, black VU, LED ladders");
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
    audioProcessor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
    keyBindings.clear();
    algorithmAttachment.reset();
    sliderAttachments.clear();
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
    for (auto& k : knobs) k->setTextColours (skin.text(), skin.mid());

    for (auto* key : { &autoAttackKey, &autoReleaseKey, &scEnableKey, &scListenKey }) key->setStyle (skin.pushStyle);
    surface.repaint();
    for (auto* c : std::initializer_list<juce::Component*> { &compPanel, &harmPanel, &scPanel, &curvePanel, &meterPanel, &inputMeter, &scope }) c->repaint();
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

// Switching algorithm switches the plate, the knob set and the key style.
void HarmonicCompressorAudioProcessorEditor::applyModeLook (int mode)
{
    mode = juce::jlimit (0, numAlgorithms - 1, mode);
    currentMode = mode;
    const bool circuitTiming = mode == (int)CompressorMode::OptoLeveler || mode == (int)CompressorMode::RMSVCADynamics;
    for (const char* id : {ParamIDs::attack, ParamIDs::release, ParamIDs::knee})
        if (auto it = knobById.find(id); it != knobById.end()) {
            it->second->setEnabled(!circuitTiming);
            it->second->setAlpha(circuitTiming ? 0.4f : 1.0f);
            it->second->setTooltip(circuitTiming ? "Circuit-defined response" : "");
        }
    autoAttackKey.setEnabled(!circuitTiming);autoReleaseKey.setEnabled(!circuitTiming);
    if (auto it=knobById.find(ParamIDs::ratio);it!=knobById.end())
        it->second->setTooltip(mode==(int)CompressorMode::OptoLeveler ? "Compress below 10:1; Limit at 10:1 and above" : "");

    auto& s = audioProcessor.apvts.state;
    const auto n = juce::String (mode);

    if (auto* f = glui::findFaceplate (s.getProperty ("plate" + n, "Graphite").toString().toStdString())) skin.plate = f;
    skin.pushStyle = juce::jlimit (0, gj::switches::numPushStyles - 1, (int) s.getProperty ("push" + n, 3));
    const auto knob = s.getProperty ("knob" + n, "Mtl").toString();
    applyKnobStyle (knob == "design" ? juce::String() : knob);
    const juce::Colour lamp (algorithmColours[mode]);
    compPanel.setAccent (lamp);
    for (auto* key : { &autoAttackKey, &autoReleaseKey }) key->setLamp (lamp.brighter (0.2f));
    applySkin();
}

void HarmonicCompressorAudioProcessorEditor::onKnobDesignEdited (Knob& knob, const glui::Item& item)
{
    auto shown = item;
    // Studio keeps the style it was registered with; a knob whose style was not changed there follows the algorithm.
    if (knobStyleOverride.isNotEmpty() && item.style == bakedStyle[item.id]) shown.style = knobStyleOverride.toStdString();
    designItems[item.id] = item;
    knob.applyDesign (shown);
}

//==============================================================================
// Look choices are UI-only: saved in the plugin state next to the parameters, never read by the DSP.
void HarmonicCompressorAudioProcessorEditor::saveLookState()
{
    auto& s = audioProcessor.apvts.state;
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
    skin.meterFace = juce::jlimit (0, 2, (int) s.getProperty ("meterFace", 1));
    gj::retro::glowEnabled = s.getProperty ("glow", "on").toString() != "off";
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (skin.plate->bgFill));
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
