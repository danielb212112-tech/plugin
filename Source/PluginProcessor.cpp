#include "PluginProcessor.h"

WaveForgeProcessor::WaveForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice (bank));

    synth.addSound (new SynthSound());

    for (int i = 0; i < 2; ++i)
    {
        const juce::String pre = "lfo" + juce::String (i + 1);
        lfoPtrs[(size_t) i] = { apvts.getRawParameterValue (pre + "Shape"),
                                apvts.getRawParameterValue (pre + "Rate"),
                                apvts.getRawParameterValue (pre + "Fade"),
                                apvts.getRawParameterValue (pre + "Sync") };
    }

    for (int i = 0; i < 4; ++i)
    {
        const juce::String pre = "mod" + juce::String (i + 1);
        modPtrs[(size_t) i] = { apvts.getRawParameterValue (pre + "Src"),
                                apvts.getRawParameterValue (pre + "Dst"),
                                apvts.getRawParameterValue (pre + "Amt") };
    }
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

    // ---------------- Oscillator 2 / Sub / Noise ----------------
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "osc2On", 1 }, "Oscillator 2 On", false));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "osc2Pos", 1 }, "Oscillator 2 WT Position",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 2.0f / 7.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "osc2Level", 1 }, "Oscillator 2 Level",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { "osc2Semi", 1 }, "Oscillator 2 Semitones", -24, 24, 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "subLevel", 1 }, "Sub Level",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "noiseLevel", 1 }, "Noise Level",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

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

    // ---------------- LFO 1 / LFO 2 ----------------
    const StringArray lfoShapes { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold" };

    for (int i = 1; i <= 2; ++i)
    {
        const String pre  = "lfo" + String (i);
        const String name = "LFO " + String (i);

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pre + "Shape", 1 }, name + " Shape", lfoShapes, 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pre + "Rate", 1 }, name + " Rate (Hz)",
                                                           NormalisableRange<float> (0.01f, 30.0f, 0.01f, 0.3f), 2.0f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pre + "Fade", 1 }, name + " Fade-in (s)",
                                                           NormalisableRange<float> (0.0f, 5.0f, 0.01f, 0.5f), 0.0f));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pre + "Sync", 1 }, name + " Key Sync", true));
    }

    // ---------------- מטריצת מודולציה (4 משבצות) ----------------
    // הסדר ברשימות חייב להתאים ל-ModSource / ModDest ב-Voice.h
    const StringArray srcNames { "None", "LFO 1", "LFO 2", "Velocity", "Filter Env" };
    const StringArray dstNames { "None", "WT Position", "Filter Cutoff", "Filter Resonance", "Pitch", "Volume" };

    for (int i = 1; i <= 4; ++i)
    {
        const String pre  = "mod" + String (i);
        const String name = "Mod " + String (i);

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pre + "Src", 1 }, name + " Source", srcNames, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pre + "Dst", 1 }, name + " Destination", dstNames, 0));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pre + "Amt", 1 }, name + " Amount",
                                                           NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    }

    // ---------------- Effects: Distortion ----------------
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "distType", 1 }, "Distortion Type",
        StringArray { "Soft Clip", "Hard Clip", "Foldback", "Sine Fold" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "distDrive", 1 }, "Distortion Drive (dB)",
        NormalisableRange<float> (0.0f, 36.0f, 0.1f), 12.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "distMix", 1 }, "Distortion Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    // ---------------- Effects: Chorus ----------------
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "chRate", 1 }, "Chorus Rate (Hz)",
        NormalisableRange<float> (0.05f, 5.0f, 0.01f, 0.5f), 0.8f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "chDepth", 1 }, "Chorus Depth",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "chFeedback", 1 }, "Chorus Feedback",
        NormalisableRange<float> (-0.9f, 0.9f, 0.001f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "chMix", 1 }, "Chorus Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    // ---------------- Effects: Delay ----------------
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "dlSync", 1 }, "Delay Sync",
        StringArray { "Free (ms)", "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2", "1 bar" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "dlTime", 1 }, "Delay Time (ms)",
        NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f), 350.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "dlFeedback", 1 }, "Delay Feedback",
        NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "dlTone", 1 }, "Delay Tone (Hz)",
        NormalisableRange<float> (200.0f, 20000.0f, 1.0f, 0.3f), 8000.0f));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "dlPing", 1 }, "Delay Ping-Pong", false));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "dlMix", 1 }, "Delay Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    // ---------------- Effects: Reverb ----------------
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "rvSize", 1 }, "Reverb Size",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "rvDamp", 1 }, "Reverb Damping",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "rvWidth", 1 }, "Reverb Width",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "rvMix", 1 }, "Reverb Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

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

    // ---- אפקטים ----
    maxBlock = juce::jmax (1, samplesPerBlock);
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };

    distortion.prepare (sampleRate, maxBlock);
    setLatencySamples ((int) std::round (distortion.getLatencySamples()));

    chorus.prepare (spec);
    chorus.reset();
    reverb.prepare (spec);
    reverb.reset();
    delay.prepare (sampleRate);

    chorusActive = delayActive = reverbActive = false;
    lastGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("gain")->load());
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

    p.osc2Enabled    = get ("osc2On") > 0.5f;
    p.osc2WtPosition = get ("osc2Pos");
    p.osc2Level      = get ("osc2Level");
    p.osc2Semitones  = (int) get ("osc2Semi");
    p.subLevel       = get ("subLevel");
    p.noiseLevel     = get ("noiseLevel");

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

    for (size_t i = 0; i < 2; ++i)
    {
        p.lfo[i].shape   = (int) lfoPtrs[i].shape->load();
        p.lfo[i].rate    = lfoPtrs[i].rate->load();
        p.lfo[i].fadeIn  = lfoPtrs[i].fade->load();
        p.lfo[i].keySync = lfoPtrs[i].sync->load() > 0.5f;
    }

    for (size_t i = 0; i < 4; ++i)
    {
        p.mod[i].source = (int) modPtrs[i].src->load();
        p.mod[i].dest   = (int) modPtrs[i].dst->load();
        p.mod[i].amount = modPtrs[i].amt->load();
    }

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->setParameters (p);

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // ======================= שרשרת אפקטים =======================
    const int numSamples = buffer.getNumSamples();

    // מפצל לחתיכות בגודל שהוכן ב-prepareToPlay (חלק מהאפקטים דורשים זאת)
    auto runChunks = [&] (auto&& fn)
    {
        juce::dsp::AudioBlock<float> whole (buffer);
        for (int start = 0; start < numSamples; start += maxBlock)
        {
            auto sub = whole.getSubBlock ((size_t) start, (size_t) juce::jmin (maxBlock, numSamples - start));
            fn (sub);
        }
    };

    // 1. Distortion
    const float distMix = get ("distMix");
    if (distMix > 0.0001f)
    {
        distortion.setParams ((int) get ("distType"), get ("distDrive"), distMix);
        distortion.process (buffer);
    }

    // 2. Chorus
    const float chMix = get ("chMix");
    if (chMix > 0.0001f)
    {
        chorus.setRate (get ("chRate"));
        chorus.setDepth (get ("chDepth"));
        chorus.setFeedback (get ("chFeedback"));
        chorus.setMix (chMix);
        runChunks ([&] (auto& sub) { chorus.process (juce::dsp::ProcessContextReplacing<float> (sub)); });
        chorusActive = true;
    }
    else if (chorusActive)
    {
        chorus.reset();
        chorusActive = false;
    }

    // 3. Delay (אפשר לסנכרן לטמפו של ה-DAW)
    const float dlMix = get ("dlMix");
    if (dlMix > 0.0001f)
    {
        float timeMs = get ("dlTime");
        const int sync = (int) get ("dlSync");

        if (sync > 0)
        {
            double bpm = 120.0;
            if (auto* ph = getPlayHead())
                if (auto position = ph->getPosition())
                    if (auto b = position->getBpm())
                        bpm = *b;

            static const double beats[] = { 0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0 };
            timeMs = (float) (60000.0 / juce::jmax (20.0, bpm) * beats[juce::jlimit (0, 7, sync)]);
        }

        delay.setParams (timeMs, get ("dlFeedback"), get ("dlTone"), get ("dlPing") > 0.5f, dlMix);
        delay.process (buffer);
        delayActive = true;
    }
    else if (delayActive)
    {
        delay.reset();
        delayActive = false;
    }

    // 4. Reverb
    const float rvMix = get ("rvMix");
    if (rvMix > 0.0001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = get ("rvSize");
        rp.damping    = get ("rvDamp");
        rp.width      = get ("rvWidth");
        rp.wetLevel   = rvMix * 0.4f;            // ה-Reverb מכפיל פנימית ב-3
        rp.dryLevel   = 0.5f * (1.0f - rvMix);   // ומכפיל את היבש ב-2
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        runChunks ([&] (auto& sub) { reverb.process (juce::dsp::ProcessContextReplacing<float> (sub)); });
        reverbActive = true;
    }
    else if (reverbActive)
    {
        reverb.reset();
        reverbActive = false;
    }

    // ======================= ווליום ראשי (עם ramp נגד קליקים) =======================
    const float newGain = juce::Decibels::decibelsToGain (get ("gain"));
    buffer.applyGainRamp (0, numSamples, lastGain, newGain);
    lastGain = newGain;
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
