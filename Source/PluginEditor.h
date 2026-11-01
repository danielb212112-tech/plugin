#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <vector>

class WaveForgeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    WaveForgeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override;
};

class WaveForgeWaveDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit WaveForgeWaveDisplay (WaveForgeProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static float sampleFrame (int frame, float phase) noexcept;
    WaveForgeProcessor& processor;
    std::atomic<float>* position = nullptr;
};

class WaveForgePageContent final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    std::vector<juce::Rectangle<int>> cards;
};

class WaveForgeParameterPage final : public juce::Component
{
public:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    WaveForgeParameterPage (WaveForgeProcessor&,
                            std::initializer_list<std::pair<const char*, const char*>>,
                            bool showWaveform = false);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WaveForgeProcessor& audioProcessor;
    WaveForgeLookAndFeel lookAndFeel;
    WaveForgePageContent content;
    juce::Viewport viewport;
    std::unique_ptr<WaveForgeWaveDisplay> waveform;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
};

class WaveForgeEditor final : public juce::AudioProcessorEditor
{
public:
    explicit WaveForgeEditor (WaveForgeProcessor&);
    ~WaveForgeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WaveForgeProcessor& audioProcessor;
    WaveForgeLookAndFeel lookAndFeel;
    juce::Label title;
    juce::Label subtitle;
    juce::ComboBox presetBrowser;
    juce::TextButton savePreset { "SAVE PRESET" };
    juce::TextButton loadPreset { "LOAD PRESET" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
};
