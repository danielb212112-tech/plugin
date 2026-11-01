#include "PluginProcessor.h"

WaveForgeProcessor::WaveForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice (bank));

    synth.addSound (new SynthSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout WaveForgeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const NormalisableRange<float> timeRange (0.001f, 5.0f, 0.001f, 0.35f);

    // ---------------- Oscillator (Wavetable) ----------------
    // התצוגה מראה באיזה פריים אנחנו, או את המעבר בין שני פריימים
    auto positionText = [] (float v, int)
    {
        const float pos = v * (float) (WavetableBank::numFrames - 1);
        const int   i   = jmin ((int) pos, WavetableBank::numFrames - 1);
        const float fr  = pos - (float) i;

        if (i >= WavetableBank::numFrames - 1 || fr < 0.03f)
            return WavetableBank::frameName (i);
        if (fr > 0.97f)
            return WavetableBank::frameName (i + 1);

        return WavetableBank::frameName (i) + " > " + WavetableBank::frameName (i + 1)
               + " " + String (roundToInt (fr * 100.0f)) + "%";
    };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "wtPos", 1 }, "WT Position",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 2.0f / 7.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (positionText)));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { "uniVoices", 1 }, "Unison Voices", 1, 8, 1));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "uniDetune", 1 }, "Unison Detune (cents)",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.6f), 20.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "uniSpread", 1 }, "Unison Stereo Spread",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));

    // ---------------- Amp envelope ----------------
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 },  "Amp Attack",  timeRange, 0.01f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 },   "Amp Decay",   timeRange, 0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sustain", 1 }, "Amp Sustain",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Amp Release",
                                                       NormalisableRange<float> (0.001f, 8.0f, 0.001f, 0.35f), 0.4f));

    // ---------------- Filter ----------------
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "fType", 1 }, "Filter Type",
        StringArray { "Low-pass", "Band-pass", "High-pass" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "cutoff", 1 }, "Filter Cutoff",
        NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.25f), 3000.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "resonance", 1 }, "Filter Resonance",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "fEnvAmt", 1 }, "Filter Env Amount (oct)",
        NormalisableRange<float> (-6.0f, 6.0f, 0.01f), 0.0f));

    // ---------------- Filter envelope ----------------
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fAttack", 1 },  "Filter Attack",  timeRange, 0.01f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fDecay", 1 },   "Filter Decay",   timeRange, 0.4f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fSustain", 1 }, "Filter Sustain",
                                                       NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "fRelease", 1 }, "Filter Release",
                                                       NormalisableRange<float> (0.001f, 8.0f, 0.001f, 0.35f), 0.4f));

    // ---------------- Output ----------------
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "gain", 1 }, "Gain",
        NormalisableRange<float> (-48.0f, 6.0f, 0.1f), -6.0f));

    return layout;
}

void WaveForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate, samplesPerBlock);
}

bool WaveForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WaveForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    VoiceParams p;
    p.wtPosition   = get ("wtPos");
    p.unisonVoices = (int) get ("uniVoices");
    p.unisonDetune = get ("uniDetune");
    p.unisonSpread = get ("uniSpread");

    p.ampEnv.attack  = get ("attack");
    p.ampEnv.decay   = get ("decay");
    p.ampEnv.sustain = get ("sustain");
    p.ampEnv.release = get ("release");

    p.filterType      = (int) get ("fType");
    p.cutoff          = get ("cutoff");
    p.resonance       = get ("resonance");
    p.filterEnvAmount = get ("fEnvAmt");

    p.filterEnv.attack  = get ("fAttack");
    p.filterEnv.decay   = get ("fDecay");
    p.filterEnv.sustain = get ("fSustain");
    p.filterEnv.release = get ("fRelease");

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->setParameters (p);

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    buffer.applyGain (juce::Decibels::decibelsToGain (get ("gain")));
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
