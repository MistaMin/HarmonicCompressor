#include "PluginEditor.h"

//==============================================================================
HarmonicCompressorAudioProcessorEditor::HarmonicCompressorAudioProcessorEditor (HarmonicCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      vuMeter (p),
      transferCurve (p)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (true, true);
    setResizeLimits (480, 360, 1280, 960);
    getConstrainer()->setFixedAspectRatio (640.0 / 480.0);
    setSize (640, 480);

    // --- Algorithm selector: horizontal row of discrete toggle buttons ---
    // Each mode gets its own accent colour, loosely evoking the tone of the
    // classic hardware topology it emulates (warm gold opto cell, brushed
    // VCA steel, blue FET stripe, red-enamel transformer unit, teal for the
    // modern hybrid) without naming or copying any specific brand.
    const char* algorithmNames[numAlgorithms] = { "Opto", "VCA", "FET", "Red", "Hybrid" };
    const juce::Colour algorithmColours[numAlgorithms] = {
        juce::Colour (0xffcda434),   // Opto   -- warm gold
        juce::Colour (0xffc9702e),   // VCA    -- brushed copper/wood
        juce::Colour (0xff4a86c8),   // FET    -- steel blue
        juce::Colour (0xffb3382c),   // Red    -- enamel red
        juce::Colour (0xff3fa796),   // Hybrid -- teal
    };

    for (int i = 0; i < numAlgorithms; ++i)
    {
        auto& button = algorithmButtons[(size_t) i];
        button.setButtonText (algorithmNames[i]);
        button.setRadioGroupId (1001);
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonColourId, LookAndFeelCustom::panelColour);
        button.setColour (juce::TextButton::buttonOnColourId, algorithmColours[i]);
        button.onClick = [this, i]
        {
            if (algorithmAttachment != nullptr)
                algorithmAttachment->setValueAsCompleteGesture ((float) i);
        };
        addAndMakeVisible (button);
    }

    // Sync the buttons to the algorithmMode parameter (host automation and
    // preset recall move the buttons too). The callback runs on the message
    // thread, so updating button states here is safe.
    algorithmAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.apvts.getParameter (ParamIDs::algorithmMode),
        [this] (float newValue)
        {
            // Restoring each mode's own remembered parameter values (attack,
            // release, knee, threshold, ratio, etc.) on mode switch is
            // handled centrally by the processor (see
            // HarmonicCompressorAudioProcessor::parameterChanged), so the
            // editor only needs to keep the button highlighting in sync.
            const int index = juce::roundToInt (newValue);
            for (int i = 0; i < numAlgorithms; ++i)
                algorithmButtons[(size_t) i].setToggleState (i == index, juce::dontSendNotification);
        });

    // --- Main rotary sliders ---
    addRotarySlider (thresholdSlider, thresholdLabel, "Threshold");
    addRotarySlider (ratioSlider, ratioLabel, "Ratio");
    ratioSlider.textFromValueFunction = [] (double v)
    {
        if (v >= 99.5) return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) + ":1";
        if (v < 0.0)   return juce::String (v, 1) + ":1";
        return juce::String (v, 1) + ":1";
    };
    ratioSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        if (text.containsIgnoreCase ("inf") || text.contains (juce::CharPointer_UTF8 ("\xe2\x88\x9e")))
            return 100.0;
        return text.upToFirstOccurrenceOf (":", false, true).getDoubleValue();
    };
    addRotarySlider (attackSlider, attackLabel, "Attack");
    addRotarySlider (releaseSlider, releaseLabel, "Release");
    addRotarySlider (kneeSlider, kneeLabel, "Knee");
    addRotarySlider (makeupGainSlider, makeupGainLabel, "Makeup");
    addRotarySlider (mixSlider, mixLabel, "Mix");

    thresholdAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::threshold, thresholdSlider);
    ratioAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::ratio, ratioSlider);
    attackAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::attack, attackSlider);
    releaseAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::release, releaseSlider);
    kneeAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::knee, kneeSlider);
    makeupGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::makeupGain, makeupGainSlider);
    mixAttachment        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::mix, mixSlider);

    // --- Harmonic sliders ---
    addRotarySlider (harmonic2Slider, harmonic2Label, "2nd");
    addRotarySlider (harmonic3Slider, harmonic3Label, "3rd");
    addRotarySlider (harmonic4Slider, harmonic4Label, "4th");
    addRotarySlider (harmonic5Slider, harmonic5Label, "5th");

    harmonic2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::harmonic2, harmonic2Slider);
    harmonic3Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::harmonic3, harmonic3Slider);
    harmonic4Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::harmonic4, harmonic4Slider);
    harmonic5Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::harmonic5, harmonic5Slider);

    // --- Sidechain row ---
    scEnabledButton.setButtonText ("Sidechain");
    scEnabledButton.setColour (juce::ToggleButton::textColourId, LookAndFeelCustom::textColour);
    addAndMakeVisible (scEnabledButton);

    scHpFreqLabel.setText ("Hz", juce::dontSendNotification);
    scHpFreqLabel.setJustificationType (juce::Justification::centred);
    scHpFreqLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (scHpFreqLabel);

    scHpFreqSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    scHpFreqSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 55, 16);
    addAndMakeVisible (scHpFreqSlider);

    // Drawn as a small headphone icon (see drawHeadphoneIcon) rather than
    // a text label, so it reads clearly at the panel's small size.
    scListenButton.setButtonText ("");
    scListenButton.setTooltip ("Listen to sidechain signal");
    addAndMakeVisible (scListenButton);

    scEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, ParamIDs::scEnabled, scEnabledButton);
    scListenAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, ParamIDs::scListen, scListenButton);
    scHpFreqAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, ParamIDs::scHPFreq, scHpFreqSlider);
    scListenButton.onClick = [this] { repaint(); };

    // --- Auto Attack / Auto Release: independent small "AUTO" badge
    // buttons at each knob's 4-o'clock position. When engaged, that knob's
    // time becomes program-dependent in the DSP and the knob is disabled
    // (greyed out) so the user can't fight the automatic behaviour. Attack
    // and Release can be set to Auto independently of one another.
    for (auto* b : { &autoAttackButton, &autoReleaseButton })
    {
        b->setButtonText ("AUTO");
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    autoAttackButton.setTooltip ("Program-dependent attack time");
    autoReleaseButton.setTooltip ("Program-dependent release time");
    autoAttackAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, ParamIDs::autoAttack, autoAttackButton);
    autoReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, ParamIDs::autoRelease, autoReleaseButton);
    autoAttackButton.onClick  = [this] { updateAutoAttackReleaseUI(); };
    autoReleaseButton.onClick = [this] { updateAutoAttackReleaseUI(); };
    updateAutoAttackReleaseUI();

    // --- Display selector: switch between VU meter and transfer curve ---
    displaySelector.addItem ("VU Meter", 1);
    displaySelector.addItem ("Transfer Curve", 2);
    displaySelector.setSelectedId (1, juce::dontSendNotification);
    displaySelector.onChange = [this] { updateDisplaySelection(); };
    addAndMakeVisible (displaySelector);

    addChildComponent (vuMeter);
    addChildComponent (transferCurve);
    updateDisplaySelection();

    // Sync button highlighting to whatever mode is already active (state
    // restore or the default Opto on first load) without disturbing any
    // per-mode parameter values.
    algorithmAttachment->sendInitialUpdate();
}

HarmonicCompressorAudioProcessorEditor::~HarmonicCompressorAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::updateDisplaySelection()
{
    const bool showCurve = displaySelector.getSelectedId() == 2;
    vuMeter.setVisible (! showCurve);
    transferCurve.setVisible (showCurve);
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::updateAutoAttackReleaseUI()
{
    const bool attackAuto  = autoAttackButton.getToggleState();
    const bool releaseAuto = autoReleaseButton.getToggleState();

    attackSlider.setEnabled (! attackAuto);
    attackSlider.setAlpha (attackAuto ? 0.35f : 1.0f);
    attackLabel.setAlpha (attackAuto ? 0.5f : 1.0f);

    releaseSlider.setEnabled (! releaseAuto);
    releaseSlider.setAlpha (releaseAuto ? 0.35f : 1.0f);
    releaseLabel.setAlpha (releaseAuto ? 0.5f : 1.0f);
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::addRotarySlider (juce::Slider& slider, juce::Label& label, const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    addAndMakeVisible (label);
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (LookAndFeelCustom::bgColour);

    g.setColour (LookAndFeelCustom::textColour);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText ("HARMONIC COMPRESSOR", getLocalBounds().removeFromTop (18).reduced (10, 0),
                juce::Justification::centredLeft);

    drawPanel (g, algorithmPanelBounds, "");
    drawPanel (g, curvePanelBounds, "");
    drawPanel (g, controlsPanelBounds, "");
    drawPanel (g, harmonicsPanelBounds, "HARMONICS");
    drawPanel (g, sidechainPanelBounds, "SIDECHAIN");
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::drawPanel (juce::Graphics& g, const juce::Rectangle<int>& bounds, const juce::String& title)
{
    // Subtle raised control-panel surface (Flux-style): a faint charcoal
    // fill with a thin border, distinct from the near-black background.
    if (bounds.isEmpty())
        return;

    const float corner = 4.0f;
    auto fb = bounds.toFloat();

    g.setColour (LookAndFeelCustom::panelColour.withAlpha (0.6f));
    g.fillRoundedRectangle (fb, corner);
    g.setColour (LookAndFeelCustom::outlineColour);
    g.drawRoundedRectangle (fb, corner, 1.0f);

    if (title.isNotEmpty())
    {
        g.setColour (LookAndFeelCustom::mutedTextColour);
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText (title, bounds.getX() + 6, bounds.getY() + 3, 200, 12, juce::Justification::centredLeft);
    }
}

//==============================================================================
juce::Rectangle<int> HarmonicCompressorAudioProcessorEditor::layoutKnobColumn (juce::Rectangle<int> column,
                                                                juce::Slider& slider, juce::Label& label,
                                                                int maxKnobSize)
{
    label.setBounds (column.removeFromTop (18));
    auto valueBox = column.removeFromBottom (18);

    const int knobSize = juce::jmin (maxKnobSize, column.getWidth() - 4, column.getHeight());
    auto knobArea = column.withSizeKeepingCentre (knobSize, knobSize);
    slider.setBounds (knobArea.getUnion (valueBox.withSizeKeepingCentre (60, valueBox.getHeight())));
    return knobArea;
}

//==============================================================================
void HarmonicCompressorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (18);

    algorithmPanelBounds = area.removeFromTop (20);
    {
        auto content = algorithmPanelBounds;
        juce::FlexBox algoBox;
        algoBox.flexDirection = juce::FlexBox::Direction::row;
        algoBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
        for (auto& button : algorithmButtons)
            algoBox.items.add (juce::FlexItem (button).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 2, 0, 2)));
        algoBox.performLayout (content);
    }
    area.removeFromTop (4);

    auto bottomRow = area.removeFromBottom (72);
    area.removeFromBottom (4);

    controlsPanelBounds = area.removeFromBottom (78);
    area.removeFromBottom (4);

    curvePanelBounds = area;
    {
        auto content = curvePanelBounds.reduced (3);
        displaySelector.setBounds (content.removeFromTop (16).reduced (30, 0));
        vuMeter.setBounds (content);
        transferCurve.setBounds (content);
    }
    {
        auto content = controlsPanelBounds.reduced (6, 3);
        juce::Slider* mainSliders[] = { &thresholdSlider, &ratioSlider, &attackSlider,
                                        &releaseSlider, &kneeSlider, &makeupGainSlider, &mixSlider };
        juce::Label* mainLabels[] = { &thresholdLabel, &ratioLabel, &attackLabel,
                                      &releaseLabel, &kneeLabel, &makeupGainLabel, &mixLabel };
        const int n = 7;
        const int w = content.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            auto knobArea = layoutKnobColumn (content.removeFromLeft (w), *mainSliders[i], *mainLabels[i], 38);

            // Attack and Release get a small "Auto" badge button sitting at
            // the knob's "4 o'clock" position, just outside its rim.
            if (mainSliders[i] == &attackSlider || mainSliders[i] == &releaseSlider)
            {
                auto& autoButton = (mainSliders[i] == &attackSlider) ? autoAttackButton : autoReleaseButton;
                const int badgeW = 30, badgeH = 14;
                const juce::Point<int> badgeCentre (knobArea.getCentreX() + (int) (knobArea.getWidth() * 0.40f),
                                                     knobArea.getCentreY() + (int) (knobArea.getHeight() * 0.34f));
                autoButton.setBounds (juce::Rectangle<int> (badgeW, badgeH).withCentre (badgeCentre));
                autoButton.toFront (false);
            }
        }
    }

    harmonicsPanelBounds = bottomRow.removeFromLeft (bottomRow.getWidth() / 2 - 3);
    bottomRow.removeFromLeft (6);
    sidechainPanelBounds = bottomRow;

    {
        auto content = harmonicsPanelBounds.reduced (6, 3);
        content.removeFromTop (14);
        juce::Slider* harmSliders[] = { &harmonic2Slider, &harmonic3Slider,
                                        &harmonic4Slider, &harmonic5Slider };
        juce::Label* harmLabels[] = { &harmonic2Label, &harmonic3Label,
                                      &harmonic4Label, &harmonic5Label };
        const int w = content.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            layoutKnobColumn (content.removeFromLeft (w), *harmSliders[i], *harmLabels[i], 32);
    }

    {
        auto content = sidechainPanelBounds.reduced (6, 3);
        content.removeFromTop (14);

        auto row1 = content.removeFromTop (18);
        content.removeFromTop (3);
        auto row2 = content.removeFromTop (20);

        scEnabledButton.setBounds (row1.removeFromLeft (120));

        juce::FlexBox row2Box;
        row2Box.flexDirection = juce::FlexBox::Direction::row;
        row2Box.alignItems = juce::FlexBox::AlignItems::center;
        row2Box.items.add (juce::FlexItem (scHpFreqLabel).withWidth (22).withHeight ((float) row2.getHeight()));
        row2Box.items.add (juce::FlexItem (scHpFreqSlider).withFlex (1.0f).withHeight ((float) row2.getHeight()).withMargin (juce::FlexItem::Margin (0, 6, 0, 4)));
        row2Box.items.add (juce::FlexItem (scListenButton).withWidth (28).withHeight ((float) row2.getHeight()));
        row2Box.performLayout (row2);
    }
}
