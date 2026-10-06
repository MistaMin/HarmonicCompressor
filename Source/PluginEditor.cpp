#include "PluginEditor.h"
#include "EmbeddedDesign.h"
#include <sstream>

namespace
{
    constexpr int canvasWidth = 800;
    constexpr int panelsHeight = 516;
#if GOODLOOKINUI_ENABLE_EDITOR
    constexpr int devStripHeight = 34;
#else
    constexpr int devStripHeight = 0;
#endif
    constexpr int canvasHeight = panelsHeight + devStripHeight;

    const char* const algorithmNames[] = { "OPTO", "VCA", "FET", "RED", "HYBRID" };
    // Each mode keeps the lamp colour it had before the redesign.
    const juce::uint32 algorithmColours[] = { 0xffcda434, 0xffc9702e, 0xff4a86c8, 0xffb3382c, 0xff3fa796 };

    // Each algorithm has its own plate, knob class and key style (built-in defaults). Developer mode can
    // change them per algorithm; the choices are saved in the plugin state as plate<i> / knob<i> / push<i>.
    struct ModeLook { const char* plate; const char* knob; int push; };
    const ModeLook modeLooks[] = {
        { "Champagne", "Brz", 0 },   // Opto:   warm gold plate, polished brass knobs
        { "Steel",     "Mtl", 0 },   // VCA:    brushed steel plate, knurled aluminium knobs
        { "Slate",     "Snk", 3 },   // FET:    rack slate plate, matte black knobs
        { "Scarlet",   "Pie", 1 },   // Red:    signal scarlet plate, serrated skirt knobs
        { "Cyber",     "Neo", 4 } }; // Hybrid: cyber cyan plate, neon ring knobs

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
    const auto& p = *skin.plate;
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (p.bgTop), 0, 0, juce::Colour (p.bgBottom), 0, b.getHeight(), false));
    g.fillRect (b);
    gj::drawFinish (g, b, p.finish);

    // title bar
    auto bar = b.removeFromTop (44.0f);
    g.setColour (juce::Colour (p.titleBar)); g.fillRect (bar);
    g.setColour (juce::Colour (p.border)); g.drawHorizontalLine (43, 0.0f, bar.getWidth());
    g.setColour (juce::Colour (p.accent)); g.fillRect (12.0f, 12.0f, 4.0f, 20.0f);
    g.setColour (juce::Colour (p.text)); g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("HARMONIC COMPRESSOR", 24, 6, 270, 22, juce::Justification::centredLeft);
    g.setColour (juce::Colour (p.textMid)); g.setFont (juce::FontOptions (9.0f));
    g.drawText ("MULTI-MODE HARMONIC COLOUR", 24, 25, 270, 12, juce::Justification::centredLeft);
    for (auto pt : { juce::Point<float> (8, 8), { bar.getWidth() - 8, 8 }, { 8, float (panelsHeight) - 8 }, { bar.getWidth() - 8, float (panelsHeight) - 8 } })
        gj::drawScrew (g, pt.x, pt.y);
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
    for (auto* c : std::initializer_list<juce::Component*> { &compPanel, &harmPanel, &scPanel, &curvePanel, &meterPanel })
        surface.addAndMakeVisible (c);
    compPanel.setBounds (12, 52, 470, 280);
    curvePanel.setBounds (494, 52, 294, 280);
    harmPanel.setBounds (12, 344, 330, 160);
    scPanel.setBounds (354, 344, 190, 160);
    meterPanel.setBounds (556, 344, 232, 160);
    compPanel.setAccent (juce::Colour (0xffc7a66a));
    curvePanel.setAccent (juce::Colour (0xff39f2ff));
    harmPanel.setAccent (juce::Colour (0xffb3382c));
    scPanel.setAccent (juce::Colour (0xff4a86c8));
    meterPanel.setAccent (juce::Colour (0xff3ccf5a));

    curvePanel.addAndMakeVisible (transferCurve);
    transferCurve.setBounds (10, 46, 274, 224);

    // --- Algorithm keys (radio), synced to the algorithmMode parameter ---
    for (int i = 0; i < numAlgorithms; ++i)
    {
        auto key = std::make_unique<KeyButton> (algorithmNames[i], juce::Colour (algorithmColours[i]));
        key->setRadioGroupId (1001);
        key->setBounds (300 + i * 80, 8, 76, 28);
        key->onClick = [this, i]
        {
            if (algorithmAttachment != nullptr) algorithmAttachment->setValueAsCompleteGesture ((float) i);
        };
        surface.addAndMakeVisible (*key);
        algorithmKeys[(size_t) i] = std::move (key);
    }
    algorithmAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.apvts.getParameter (ParamIDs::algorithmMode),
        [this] (float v)
        {
            // Each mode's remembered parameter values are restored by the processor; here we only mirror the selection.
            const int index = juce::roundToInt (v);
            for (int i = 0; i < numAlgorithms; ++i) algorithmKeys[(size_t) i]->setToggleState (i == index, juce::dontSendNotification);
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
    autoAttackKey.setBounds (356, 176, 104, 30);
    autoReleaseKey.setBounds (356, 216, 104, 30);
    scEnableKey.setBounds (100, 64, 82, 34);
    scListenKey.setBounds (100, 108, 82, 34);
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
    for (auto& key : algorithmKeys) key->setStyle (skin.pushStyle);
    for (auto* key : { &autoAttackKey, &autoReleaseKey, &scEnableKey, &scListenKey }) key->setStyle (skin.pushStyle);
    surface.repaint();
    for (auto* c : std::initializer_list<juce::Component*> { &compPanel, &harmPanel, &scPanel, &curvePanel, &meterPanel }) c->repaint();
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
    auto& s = audioProcessor.apvts.state;
    const auto n = juce::String (mode);
    const auto& d = modeLooks[mode];
    if (auto* f = glui::findFaceplate (s.getProperty ("plate" + n, d.plate).toString().toStdString())) skin.plate = f;
    skin.pushStyle = juce::jlimit (0, gj::switches::numPushStyles - 1, (int) s.getProperty ("push" + n, d.push));
    const auto knob = s.getProperty ("knob" + n, d.knob).toString();
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
    skin.meterFace = juce::jlimit (0, 2, (int) s.getProperty ("meterFace", 0));
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
