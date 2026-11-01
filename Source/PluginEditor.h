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
    WaveForgeWaveDisplay (WaveForgeProcessor&, const juce::String& parameterID, const juce::String& displayName);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static float sampleFrame (int frame, float phase) noexcept;
    WaveForgeProcessor& processor;
    std::atomic<float>* position = nullptr;
    juce::String displayName;
};

class WaveForgeUnisonDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit WaveForgeUnisonDisplay (WaveForgeProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::atomic<float>* voices = nullptr;
    std::atomic<float>* detune = nullptr;
    std::atomic<float>* spread = nullptr;
};

class WaveForgeEnvelopeEditor final : public juce::Component
{
public:
    WaveForgeEnvelopeEditor (WaveForgeProcessor&, const juce::String& prefix, const juce::String& title);
    ~WaveForgeEnvelopeEditor() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Handle { none, attack, decay, sustain, release };
    juce::Point<float> getHandlePosition (Handle) const;
    void updateParameter (juce::RangedAudioParameter*, float);
    WaveForgeProcessor& processor;
    juce::String title;
    juce::RangedAudioParameter* attack = nullptr;
    juce::RangedAudioParameter* decay = nullptr;
    juce::RangedAudioParameter* sustain = nullptr;
    juce::RangedAudioParameter* release = nullptr;
    Handle activeHandle = Handle::none;
    juce::RangedAudioParameter* activeParameter = nullptr;
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
                            bool showWaveform = false,
                            const juce::String& envelopePrefix = {});
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WaveForgeProcessor& audioProcessor;
    WaveForgeLookAndFeel lookAndFeel;
    WaveForgePageContent content;
    juce::Viewport viewport;
    std::unique_ptr<WaveForgeWaveDisplay> waveform;
    std::unique_ptr<WaveForgeWaveDisplay> waveform2;
    std::unique_ptr<WaveForgeUnisonDisplay> unisonDisplay;
    std::unique_ptr<WaveForgeEnvelopeEditor> envelopeEditor;
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
