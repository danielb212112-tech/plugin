#include "PluginProcessor.h"

WaveForgeProcessor::WaveForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice());

    synth.addSound (new SynthSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout WaveForgeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "wave", 1 }, "Waveform",
        StringArray { "Sine", "Saw", "Square", "Triangle" }, 1));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack",
        NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.35f), 0.01f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "decay", 1 }, "Decay",
        NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.35f), 0.3f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "sustain", 1 }, "Sustain",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release",
        NormalisableRange<float> (0.001f, 8.0f, 0.001f, 0.35f), 0.4f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "gain", 1 }, "Gain",
        NormalisableRange<float> (-48.0f, 6.0f, 0.1f), -6.0f));

    return layout;
}

void WaveForgeProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);
}

bool WaveForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WaveForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // קריאת פרמטרים ועדכון כל הקולות
    juce::ADSR::Parameters env;
    env.attack  = apvts.getRawParameterValue ("attack")->load();
    env.decay   = apvts.getRawParameterValue ("decay")->load();
    env.sustain = apvts.getRawParameterValue ("sustain")->load();
    env.release = apvts.getRawParameterValue ("release")->load();
    const int wave = (int) apvts.getRawParameterValue ("wave")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->setParameters (wave, env);

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    const float gainDb = apvts.getRawParameterValue ("gain")->load();
    buffer.applyGain (juce::Decibels::decibelsToGain (gainDb));
}

void WaveForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void WaveForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WaveForgeProcessor();
}
