#include "PluginEditor.h"

namespace
{
    const juce::Colour bg        (0xff090e17);
    const juce::Colour panel     (0xff121b29);
    const juce::Colour panelHi   (0xff182538);
    const juce::Colour accent    (0xff31dfc2);
    const juce::Colour textMain  (0xffedf4ff);
    const juce::Colour textMuted (0xff8395ad);
}

WaveForgeLookAndFeel::WaveForgeLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::TabbedComponent::backgroundColourId, bg);
    setColour (juce::TabbedButtonBar::tabTextColourId, textMuted);
    setColour (juce::TabbedButtonBar::frontTextColourId, textMain);
    setColour (juce::ComboBox::textColourId, textMain);
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2a3b50));
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, textMain);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, textMain);
}

void WaveForgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float startAngle, float endAngle,
                                              juce::Slider& slider)
{
    const float side = (float) juce::jmin (width, height);
    const float radius = side * 0.37f;
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.46f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff0a111d));
    g.fillEllipse (cx - radius - 5.0f, cy - radius - 5.0f,
                   (radius + 5.0f) * 2.0f, (radius + 5.0f) * 2.0f);

    juce::Path track;
    track.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a3b50));
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle,
                         juce::jmax (startAngle + 0.001f, angle), true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0xff1b293b));
    g.fillEllipse (cx - radius + 5.0f, cy - radius + 5.0f,
                   (radius - 5.0f) * 2.0f, (radius - 5.0f) * 2.0f);

    juce::Path pointer;
    const float pointerLength = radius * 0.66f;
    pointer.startNewSubPath (cx + std::cos (angle) * (radius * 0.28f),
                             cy + std::sin (angle) * (radius * 0.28f));
    pointer.lineTo (cx + std::cos (angle) * pointerLength,
                    cy + std::sin (angle) * pointerLength);
    g.setColour (textMain);
    g.strokePath (pointer, juce::PathStrokeType (3.0f, juce::PathStrokeType::rounded));

    juce::ignoreUnused (slider);
}

void WaveForgeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                          int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (panelHi);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (juce::Colour (0xff30445d));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    juce::Path arrow;
    const float cx = (float) width - 17.0f;
    const float cy = (float) height * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (accent);
    g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    juce::ignoreUnused (box);
}

void WaveForgeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                   const juce::Colour&,
                                                   bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = button.getToggleState() ? accent.withAlpha (0.22f) : panelHi;
    if (highlighted) colour = colour.brighter (0.12f);
    if (down) colour = colour.darker (0.12f);
    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (button.getToggleState() ? accent : juce::Colour (0xff30445d));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
}

WaveForgeWaveDisplay::WaveForgeWaveDisplay (WaveForgeProcessor& p)
    : processor (p), position (p.apvts.getRawParameterValue ("wtPos"))
{
    startTimerHz (24);
}

void WaveForgeWaveDisplay::timerCallback()
{
    repaint();
}

float WaveForgeWaveDisplay::sampleFrame (int frame, float phase) noexcept
{
    const float p = phase - std::floor (phase);
    const float sine = std::sin (juce::MathConstants<float>::twoPi * p);
    switch (frame)
    {
        case 0: return sine;
        case 1: return std::asin (sine) * (2.0f / juce::MathConstants<float>::pi);
        case 2: return 2.0f * p - 1.0f;
        case 3: return p < 0.5f ? 1.0f : -1.0f;
        case 4: return p < 0.25f ? 1.0f : -1.0f;
        case 5: return 0.58f * std::sin (juce::MathConstants<float>::twoPi * p)
                      + 0.28f * std::sin (juce::MathConstants<float>::twoPi * 2.0f * p)
                      + 0.14f * std::sin (juce::MathConstants<float>::twoPi * 4.0f * p);
        case 6: return 0.62f * std::sin (juce::MathConstants<float>::twoPi * p)
                      + 0.38f * std::sin (juce::MathConstants<float>::twoPi * 5.0f * p);
        default: return 0.55f * sine + 0.30f * std::sin (juce::MathConstants<float>::twoPi * 3.0f * p)
                            + 0.15f * std::sin (juce::MathConstants<float>::twoPi * 7.0f * p);
    }
}

void WaveForgeWaveDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c1522));
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (juce::Colour (0xff263950));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto plot = bounds.reduced (14.0f, 12.0f);
    plot.removeFromTop (18.0f);
    g.setColour (juce::Colour (0xff203249));
    for (int i = 1; i < 4; ++i)
    {
        const float yy = plot.getY() + plot.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) yy, plot.getX(), plot.getRight());
    }
    for (int i = 1; i < 8; ++i)
    {
        const float xx = plot.getX() + plot.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) xx, plot.getY(), plot.getBottom());
    }

    const float pos = position != nullptr ? juce::jlimit (0.0f, 1.0f, position->load()) : 0.0f;
    const float framePos = pos * 7.0f;
    const int frameA = juce::jlimit (0, 7, (int) framePos);
    const int frameB = juce::jmin (7, frameA + 1);
    const float blend = framePos - (float) frameA;

    juce::Path wave;
    constexpr int points = 512;
    for (int i = 0; i < points; ++i)
    {
        const float phase = (float) i / (float) (points - 1);
        const float a = sampleFrame (frameA, phase);
        const float b = sampleFrame (frameB, phase);
        const float value = a + (b - a) * blend;
        const float px = plot.getX() + phase * plot.getWidth();
        const float py = plot.getCentreY() - value * plot.getHeight() * 0.39f;
        if (i == 0) wave.startNewSubPath (px, py);
        else wave.lineTo (px, py);
    }

    g.setColour (accent.withAlpha (0.12f));
    juce::Path glow = wave;
    glow.applyTransform (juce::AffineTransform::translation (0.0f, 1.0f));
    g.strokePath (glow, juce::PathStrokeType (7.0f));
    g.setColour (accent);
    g.strokePath (wave, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    const auto nameA = WavetableBank::frameName (frameA);
    const auto nameB = WavetableBank::frameName (frameB);
    g.setColour (textMain);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText (blend < 0.03f ? nameA : nameA + "  →  " + nameB,
                plot.getX(), bounds.getY() + 5.0f, plot.getWidth(), 15.0f,
                juce::Justification::centredLeft);
}

void WaveForgePageContent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111824));
    for (const auto& card : cards)
    {
        g.setColour (juce::Colour (0xff172334));
        g.fillRoundedRectangle (card.toFloat(), 9.0f);
        g.setColour (juce::Colour (0xff26384e));
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 9.0f, 1.0f);
    }
}

WaveForgeParameterPage::WaveForgeParameterPage (
    WaveForgeProcessor& processor,
    std::initializer_list<std::pair<const char*, const char*>> parameters,
    bool showWaveform)
    : audioProcessor (processor)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);

    if (showWaveform)
    {
        waveform = std::make_unique<WaveForgeWaveDisplay> (processor);
        content.addAndMakeVisible (*waveform);
    }

    for (const auto& item : parameters)
    {
        const juce::String id (item.first);
        const juce::String name (item.second);
        auto* parameter = audioProcessor.apvts.getParameter (id);
        if (parameter == nullptr)
            continue;

        auto label = std::make_unique<juce::Label>();
        label->setText (name, juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::Font (10.5f, juce::Font::bold));
        label->setColour (juce::Label::textColourId, textMuted);
        content.addAndMakeVisible (*label);
        labels.push_back (std::move (label));

        if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
        {
            auto button = std::make_unique<juce::ToggleButton>();
            button->setButtonText (name);
            button->setLookAndFeel (&lookAndFeel);
            button->setColour (juce::ToggleButton::textColourId, textMain);
            content.addAndMakeVisible (*button);
            buttonAttachments.push_back (std::make_unique<ButtonAttachment> (audioProcessor.apvts, id, *button));
            controls.push_back (std::move (button));
        }
        else if (dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr)
        {
            auto combo = std::make_unique<juce::ComboBox>();
            combo->addItemList (parameter->getAllValueStrings(), 1);
            combo->setLookAndFeel (&lookAndFeel);
            content.addAndMakeVisible (*combo);
            comboAttachments.push_back (std::make_unique<ComboAttachment> (audioProcessor.apvts, id, *combo));
            controls.push_back (std::move (combo));
        }
        else
        {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
            slider->setLookAndFeel (&lookAndFeel);
            slider->setColour (juce::Slider::textBoxTextColourId, textMain);
            slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0b1420));
            slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff26384e));
            content.addAndMakeVisible (*slider);
            sliderAttachments.push_back (std::make_unique<SliderAttachment> (audioProcessor.apvts, id, *slider));
            controls.push_back (std::move (slider));
        }
    }
}

void WaveForgeParameterPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111824));
}

void WaveForgeParameterPage::resized()
{
    viewport.setBounds (getLocalBounds());
    const int width = juce::jmax (1, viewport.getWidth() - 12);
    constexpr int columns = 4;
    constexpr int cellHeight = 126;
    constexpr int gap = 8;
    const int cellWidth = (width - 32 - gap * (columns - 1)) / columns;
    const int top = waveform != nullptr ? 176 : 14;
    const int rows = (int) (controls.size() + (size_t) columns - 1) / columns;
    const int totalHeight = top + rows * (cellHeight + gap) + 14;
    content.setSize (width, juce::jmax (viewport.getHeight(), totalHeight));
    content.cards.clear();

    if (waveform != nullptr)
        waveform->setBounds (16, 12, width - 32, 150);

    for (size_t i = 0; i < controls.size(); ++i)
    {
        const int row = (int) i / columns;
        const int col = (int) i % columns;
        const int x = 16 + col * (cellWidth + gap);
        const int y = top + row * (cellHeight + gap);
        auto card = juce::Rectangle<int> (x, y, cellWidth, cellHeight);
        content.cards.push_back (card);
        labels[i]->setBounds (card.withY (card.getY() + 8).withHeight (20).reduced (5, 0));

        if (auto* slider = dynamic_cast<juce::Slider*> (controls[i].get()))
            slider->setBounds (card.withTrimmedTop (27).reduced (8, 1));
        else
            controls[i]->setBounds (card.withTrimmedTop (34).reduced (8, 5));
    }
    content.repaint();
}

WaveForgeEditor::WaveForgeEditor (WaveForgeProcessor& processor)
    : AudioProcessorEditor (&processor), audioProcessor (processor)
{
    setLookAndFeel (&lookAndFeel);
    title.setText ("WAVEFORGE", juce::dontSendNotification);
    title.setFont (juce::Font (25.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, textMain);
    subtitle.setText ("PROFESSIONAL WAVETABLE SYNTHESIZER", juce::dontSendNotification);
    subtitle.setFont (juce::Font (9.5f, juce::Font::bold));
    subtitle.setColour (juce::Label::textColourId, textMuted);
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);
    addAndMakeVisible (presetBrowser);
    addAndMakeVisible (savePreset);
    addAndMakeVisible (loadPreset);
    addAndMakeVisible (tabs);

    presetBrowser.setLookAndFeel (&lookAndFeel);
    savePreset.setLookAndFeel (&lookAndFeel);
    loadPreset.setLookAndFeel (&lookAndFeel);
    tabs.setLookAndFeel (&lookAndFeel);

    presetBrowser.addItem ("FACTORY PRESETS", 1);
    presetBrowser.addItem ("Init", 2);
    presetBrowser.addItem ("Deep Bass", 3);
    presetBrowser.addItem ("Wide Lead", 4);
    presetBrowser.addItem ("Soft Pad", 5);
    presetBrowser.addItem ("Pluck", 6);
    presetBrowser.addItem ("Noise Sweep", 7);
    presetBrowser.setSelectedId (1, juce::dontSendNotification);
    presetBrowser.onChange = [this]
    {
        const int preset = presetBrowser.getSelectedId();
        if (preset <= 1) return;
        for (auto* parameter : audioProcessor.getParameters())
            parameter->setValueNotifyingHost (parameter->getDefaultValue());

        auto set = [this] (const char* id, float value)
        {
            if (auto* parameter = audioProcessor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        if (preset == 3)
        {
            set ("osc2On", 1.0f); set ("osc2Pos", 0.43f); set ("osc2Level", 0.38f);
            set ("osc2Semi", -12.0f); set ("subLevel", 0.72f);
            set ("cutoff", 190.0f); set ("resonance", 0.36f); set ("fEnvAmt", 2.8f);
            set ("fDecay", 0.32f); set ("fSustain", 0.0f); set ("attack", 0.005f);
            set ("decay", 0.5f); set ("sustain", 0.72f); set ("release", 0.16f);
            set ("distMix", 0.18f); set ("distDrive", 16.0f);
        }
        else if (preset == 4)
        {
            set ("uniVoices", 6.0f); set ("uniDetune", 28.0f); set ("uniSpread", 1.0f);
            set ("osc2On", 1.0f); set ("osc2Level", 0.32f); set ("osc2Semi", 7.0f);
            set ("cutoff", 8500.0f); set ("chMix", 0.24f); set ("dlMix", 0.13f);
            set ("rvMix", 0.10f); set ("attack", 0.015f); set ("release", 0.55f);
        }
        else if (preset == 5)
        {
            set ("uniVoices", 4.0f); set ("uniDetune", 16.0f); set ("uniSpread", 0.95f);
            set ("attack", 1.1f); set ("decay", 2.0f); set ("sustain", 0.82f);
            set ("release", 3.2f); set ("cutoff", 4200.0f); set ("chMix", 0.28f);
            set ("rvMix", 0.38f);
        }
        else if (preset == 6)
        {
            set ("attack", 0.001f); set ("decay", 0.42f); set ("sustain", 0.04f);
            set ("release", 0.24f); set ("cutoff", 7800.0f); set ("fEnvAmt", 3.0f);
            set ("fAttack", 0.001f); set ("fDecay", 0.28f); set ("fSustain", 0.03f);
            set ("subLevel", 0.12f); set ("dlMix", 0.18f);
        }
        else if (preset == 7)
        {
            set ("noiseLevel", 0.48f); set ("cutoff", 2400.0f); set ("fEnvAmt", 4.0f);
            set ("fAttack", 0.18f); set ("fDecay", 2.2f); set ("fSustain", 0.08f);
            set ("rvMix", 0.22f); set ("release", 1.2f);
        }
    };

    savePreset.setColour (juce::TextButton::textColourOffId, bg);
    loadPreset.setColour (juce::TextButton::textColourOffId, textMain);
    savePreset.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Save WaveForge Preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.wfpreset");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                if (file == juce::File{}) return;
                if (auto xml = audioProcessor.apvts.copyState().createXml())
                    xml->writeTo (file);
            });
    };
    loadPreset.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load WaveForge Preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.wfpreset");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                if (file == juce::File{}) return;
                std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
                if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
                    audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
            });
    };

    tabs.addTab ("OSC", panel, new WaveForgeParameterPage (processor, {
        { "wtPos", "WT POSITION" }, { "uniVoices", "UNISON VOICES" },
        { "uniDetune", "DETUNE" }, { "uniSpread", "STEREO SPREAD" },
        { "osc2On", "OSCILLATOR 2" }, { "osc2Pos", "OSC 2 POSITION" },
        { "osc2Level", "OSC 2 LEVEL" }, { "osc2Semi", "OSC 2 SEMITONES" },
        { "subLevel", "SUB LEVEL" }, { "noiseLevel", "NOISE LEVEL" },
        { "attack", "AMP ATTACK" }, { "decay", "AMP DECAY" },
        { "sustain", "AMP SUSTAIN" }, { "release", "AMP RELEASE" }
    }, true), true);

    tabs.addTab ("FILTER", panel, new WaveForgeParameterPage (processor, {
        { "fType", "FILTER TYPE" }, { "cutoff", "CUTOFF" },
        { "resonance", "RESONANCE" }, { "fEnvAmt", "ENV AMOUNT" },
        { "fAttack", "ENV ATTACK" }, { "fDecay", "ENV DECAY" },
        { "fSustain", "ENV SUSTAIN" }, { "fRelease", "ENV RELEASE" }
    }), false);

    tabs.addTab ("MOD", panel, new WaveForgeParameterPage (processor, {
        { "lfo1Shape", "LFO 1 SHAPE" }, { "lfo1Rate", "LFO 1 RATE" },
        { "lfo1Fade", "LFO 1 FADE" }, { "lfo1Sync", "LFO 1 KEY SYNC" },
        { "lfo2Shape", "LFO 2 SHAPE" }, { "lfo2Rate", "LFO 2 RATE" },
        { "lfo2Fade", "LFO 2 FADE" }, { "lfo2Sync", "LFO 2 KEY SYNC" },
        { "mod1Src", "MOD 1 SOURCE" }, { "mod1Dst", "MOD 1 TARGET" }, { "mod1Amt", "MOD 1 AMOUNT" },
        { "mod2Src", "MOD 2 SOURCE" }, { "mod2Dst", "MOD 2 TARGET" }, { "mod2Amt", "MOD 2 AMOUNT" },
        { "mod3Src", "MOD 3 SOURCE" }, { "mod3Dst", "MOD 3 TARGET" }, { "mod3Amt", "MOD 3 AMOUNT" },
        { "mod4Src", "MOD 4 SOURCE" }, { "mod4Dst", "MOD 4 TARGET" }, { "mod4Amt", "MOD 4 AMOUNT" }
    }), false);

    tabs.addTab ("FX", panel, new WaveForgeParameterPage (processor, {
        { "distType", "DISTORTION TYPE" }, { "distDrive", "DRIVE" }, { "distMix", "DIST MIX" },
        { "chRate", "CHORUS RATE" }, { "chDepth", "CHORUS DEPTH" },
        { "chFeedback", "CHORUS FEEDBACK" }, { "chMix", "CHORUS MIX" },
        { "dlSync", "DELAY SYNC" }, { "dlTime", "DELAY TIME" },
        { "dlFeedback", "DELAY FEEDBACK" }, { "dlTone", "DELAY TONE" },
        { "dlPing", "PING PONG" }, { "dlMix", "DELAY MIX" },
        { "rvSize", "REVERB SIZE" }, { "rvDamp", "DAMPING" },
        { "rvWidth", "WIDTH" }, { "rvMix", "REVERB MIX" }
    }), false);

    tabs.addTab ("MASTER", panel, new WaveForgeParameterPage (processor, {
        { "gain", "OUTPUT GAIN" }
    }), false);

    setResizable (true, true);
    setResizeLimits (900, 650, 1600, 1200);
    setSize (1180, 820);
}

WaveForgeEditor::~WaveForgeEditor()
{
    setLookAndFeel (nullptr);
}

void WaveForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.9f), 0.0f, 0.0f,
                                             juce::Colour (0xff168d9d), (float) getWidth(), 0.0f, false));
    g.fillRect (0, 0, getWidth(), 3);
    g.setColour (juce::Colour (0xff1b2a3e));
    g.drawHorizontalLine (77, 18.0f, (float) getWidth() - 18.0f);
}

void WaveForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (18);
    auto header = area.removeFromTop (64);
    auto controlsArea = header.removeFromRight (420);
    presetBrowser.setBounds (controlsArea.removeFromLeft (172).reduced (3, 13));
    savePreset.setBounds (controlsArea.removeFromLeft (116).reduced (3, 13));
    loadPreset.setBounds (controlsArea.removeFromLeft (116).reduced (3, 13));
    title.setBounds (header.removeFromTop (35));
    subtitle.setBounds (header);
    area.removeFromTop (12);
    tabs.setBounds (area);
}
