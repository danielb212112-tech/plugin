#include "PluginEditor.h"

WaveForgeParameterPage::WaveForgeParameterPage (
    WaveForgeProcessor& processor,
    std::initializer_list<std::pair<const char*, const char*>> parameters)
    : audioProcessor (processor)
{
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
        label->setColour (juce::Label::textColourId, juce::Colour (0xffc8d0df));
        addAndMakeVisible (*label);
        labels.push_back (std::move (label));

        if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
        {
            auto button = std::make_unique<juce::ToggleButton>();
            button->setButtonText (name);
            addAndMakeVisible (*button);
            buttonAttachments.push_back (std::make_unique<ButtonAttachment> (audioProcessor.apvts, id, *button));
            controls.push_back (std::move (button));
        }
        else if (dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr)
        {
            auto combo = std::make_unique<juce::ComboBox>();
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
                combo->addItemList (parameter->getAllValueStrings(), 1);
            addAndMakeVisible (*combo);
            comboAttachments.push_back (std::make_unique<ComboAttachment> (audioProcessor.apvts, id, *combo));
            controls.push_back (std::move (combo));
        }
        else
        {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20);
            slider->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff29d3c2));
            slider->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff35445a));
            slider->setColour (juce::Slider::thumbColourId, juce::Colour (0xfff3f6ff));
            slider->setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff3f6ff));
            slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff171f2d));
            addAndMakeVisible (*slider);
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
    auto area = getLocalBounds().reduced (18, 12);
    constexpr int columns = 5;
    constexpr int cellHeight = 132;
    const int cellWidth = area.getWidth() / columns;

    for (size_t i = 0; i < controls.size(); ++i)
    {
        const int row = (int) i / columns;
        const int col = (int) i % columns;
        auto cell = juce::Rectangle<int> (area.getX() + col * cellWidth,
                                          area.getY() + row * cellHeight,
                                          cellWidth, cellHeight).reduced (5);
        labels[i]->setBounds (cell.removeFromTop (22));
        if (auto* slider = dynamic_cast<juce::Slider*> (controls[i].get()))
            slider->setBounds (cell.reduced (8, 0));
        else
            controls[i]->setBounds (cell.reduced (8, 28));
    }
}

WaveForgeEditor::WaveForgeEditor (WaveForgeProcessor& processor)
    : AudioProcessorEditor (&processor), audioProcessor (processor)
{
    title.setText ("WAVEFORGE", juce::dontSendNotification);
    title.setFont (juce::Font (25.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, juce::Colour (0xfff2f6ff));
    subtitle.setText ("WAVETABLE SYNTHESIZER  /  JUCE", juce::dontSendNotification);
    subtitle.setFont (juce::Font (10.0f, juce::Font::bold));
    subtitle.setColour (juce::Label::textColourId, juce::Colour (0xff7f91a8));
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);
    addAndMakeVisible (savePreset);
    addAndMakeVisible (loadPreset);
    addAndMakeVisible (tabs);

    savePreset.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b9f98));
    savePreset.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff08121c));
    loadPreset.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202c3d));
    loadPreset.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8eef8));

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
                if (file == juce::File{})
                    return;
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
                if (file == juce::File{})
                    return;
                std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
                if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
                    audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
            });
    };

    tabs.addTab ("OSC", juce::Colour (0xff111824),
        new WaveForgeParameterPage (processor, {
            { "wtPos", "WT Position" }, { "uniVoices", "Unison Voices" },
            { "uniDetune", "Detune" }, { "uniSpread", "Stereo Spread" },
            { "osc2On", "Oscillator 2" }, { "osc2Pos", "OSC 2 Position" },
            { "osc2Level", "OSC 2 Level" }, { "osc2Semi", "OSC 2 Semitones" },
            { "subLevel", "Sub Level" }, { "noiseLevel", "Noise Level" },
            { "attack", "Amp Attack" }, { "decay", "Amp Decay" },
            { "sustain", "Amp Sustain" }, { "release", "Amp Release" }
        }), true);

    tabs.addTab ("FILTER", juce::Colour (0xff111824),
        new WaveForgeParameterPage (processor, {
            { "fType", "Filter Type" }, { "cutoff", "Cutoff" },
            { "resonance", "Resonance" }, { "fEnvAmt", "Envelope Amount" },
            { "fAttack", "Filter Attack" }, { "fDecay", "Filter Decay" },
            { "fSustain", "Filter Sustain" }, { "fRelease", "Filter Release" }
        }), false);

    tabs.addTab ("MOD", juce::Colour (0xff111824),
        new WaveForgeParameterPage (processor, {
            { "lfo1Shape", "LFO 1 Shape" }, { "lfo1Rate", "LFO 1 Rate" },
            { "lfo1Fade", "LFO 1 Fade" }, { "lfo1Sync", "LFO 1 Key Sync" },
            { "lfo2Shape", "LFO 2 Shape" }, { "lfo2Rate", "LFO 2 Rate" },
            { "lfo2Fade", "LFO 2 Fade" }, { "lfo2Sync", "LFO 2 Key Sync" },
            { "mod1Src", "Mod 1 Source" }, { "mod1Dst", "Mod 1 Target" }, { "mod1Amt", "Mod 1 Amount" },
            { "mod2Src", "Mod 2 Source" }, { "mod2Dst", "Mod 2 Target" }, { "mod2Amt", "Mod 2 Amount" },
            { "mod3Src", "Mod 3 Source" }, { "mod3Dst", "Mod 3 Target" }, { "mod3Amt", "Mod 3 Amount" },
            { "mod4Src", "Mod 4 Source" }, { "mod4Dst", "Mod 4 Target" }, { "mod4Amt", "Mod 4 Amount" }
        }), false);

    tabs.addTab ("FX", juce::Colour (0xff111824),
        new WaveForgeParameterPage (processor, {
            { "distType", "Distortion Type" }, { "distDrive", "Drive" }, { "distMix", "Distortion Mix" },
            { "chRate", "Chorus Rate" }, { "chDepth", "Chorus Depth" },
            { "chFeedback", "Chorus Feedback" }, { "chMix", "Chorus Mix" },
            { "dlSync", "Delay Sync" }, { "dlTime", "Delay Time" },
            { "dlFeedback", "Delay Feedback" }, { "dlTone", "Delay Tone" },
            { "dlPing", "Ping Pong" }, { "dlMix", "Delay Mix" },
            { "rvSize", "Reverb Size" }, { "rvDamp", "Damping" },
            { "rvWidth", "Width" }, { "rvMix", "Reverb Mix" }
        }), false);

    tabs.addTab ("MASTER", juce::Colour (0xff111824),
        new WaveForgeParameterPage (processor, { { "gain", "Output Gain (dB)" } }), false);

    setResizable (true, true);
    setResizeLimits (820, 600, 1500, 1100);
    setSize (1120, 760);
}

void WaveForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b111b));
    g.setColour (juce::Colour (0xff29d3c2));
    g.fillRect (0, 0, getWidth(), 3);
}

void WaveForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (18);
    auto header = area.removeFromTop (66);
    auto buttons = header.removeFromRight (230);
    savePreset.setBounds (buttons.removeFromLeft (108).reduced (3, 10));
    loadPreset.setBounds (buttons.removeFromLeft (108).reduced (3, 10));
    title.setBounds (header.removeFromTop (34));
    subtitle.setBounds (header);
    tabs.setBounds (area);
}
