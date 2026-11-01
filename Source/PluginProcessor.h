#pragma once

#include <JuceHeader.h>
#include "Voice.h"
#include "Effects.h"

class WaveForgeProcessor : public juce::AudioProcessor
{
public:
    WaveForgeProcessor();
    ~WaveForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> outputPeakLeft { 0.0f }, outputPeakRight { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // מצביעים לפרמטרים של LFO ומטריצת המודולציה (נשמרים פעם אחת בבנאי)
    struct LfoPtrs { std::atomic<float> *shape = nullptr, *rate = nullptr, *fade = nullptr, *sync = nullptr; };
    struct ModPtrs { std::atomic<float> *src = nullptr, *dst = nullptr, *amt = nullptr; };
    std::array<LfoPtrs, 2> lfoPtrs;
    std::array<ModPtrs, 4> modPtrs;

    static constexpr int numVoices = 8;
    WavetableBank bank;          // חייב להיות לפני synth (הקולות מחזיקים הפניה אליו)
    juce::Synthesiser synth;

    // ---- שרשרת אפקטים: Distortion -> Chorus -> Delay -> Reverb ----
    DistortionFx distortion;
    juce::dsp::Chorus<float> chorus;
    StereoDelay delay;
    juce::dsp::Reverb reverb;

    int   maxBlock = 512;
    bool  chorusActive = false, delayActive = false, reverbActive = false;
    float lastGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveForgeProcessor)
};
