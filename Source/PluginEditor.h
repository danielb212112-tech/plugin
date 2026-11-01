#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class WaveForgeParameterPage final : public juce::Component
{
public:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    WaveForgeParameterPage (WaveForgeProcessor& processor,
                            std::initializer_list<std::pair<const char*, const char*>> parameters);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WaveForgeProcessor& audioProcessor;
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
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WaveForgeProcessor& audioProcessor;
    juce::Label title;
    juce::Label subtitle;
    juce::TextButton savePreset { "SAVE PRESET" };
    juce::TextButton loadPreset { "LOAD PRESET" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
};
