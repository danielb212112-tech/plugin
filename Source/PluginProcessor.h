#pragma once

#include <JuceHeader.h>
#include "Voice.h"

class WaveForgeProcessor : public juce::AudioProcessor
{
public:
    WaveForgeProcessor();
    ~WaveForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // שלב 1: ממשק גנרי אוטומטי. בשלב מאוחר נחליף בממשק מעוצב.
    juce::AudioProcessorEditor* createEditor() override
    {
        return new juce::GenericAudioProcessorEditor (*this);
    }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveForgeProcessor)
};
