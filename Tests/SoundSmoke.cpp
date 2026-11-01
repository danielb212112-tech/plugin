#include <JuceHeader.h>
#include "../Source/Voice.h"

#include <cmath>
#include <iostream>

// A deterministic offline smoke test for the synth's actual audio path.
// Runs in CI without an audio device or plugin host.

static bool testWavetableContinuity()
{
    WavetableBank bank;
    constexpr double sampleRate = 48000.0;

    for (int frame = 0; frame < WavetableBank::numFrames - 1; ++frame)
    {
        for (int level = 0; level < WavetableBank::numLevels - 1; ++level)
        {
            const double threshold = sampleRate * 0.5
                / (double) (WavetableBank::maxHarmonics >> level);
            for (float phase : { 0.037f, 0.131f, 0.391f, 0.712f })
            {
                const float a = bank.readMorph (frame, 0.37f,
                    WavetableBank::selectMip (threshold * (1.0 - 1.0e-6), sampleRate),
                    phase);
                const float b = bank.readMorph (frame, 0.37f,
                    WavetableBank::selectMip (threshold * (1.0 + 1.0e-6), sampleRate),
                    phase);
                if (! std::isfinite (a) || ! std::isfinite (b) || std::abs (a - b) > 0.025f)
                {
                    std::cerr << "Mip discontinuity at frame " << frame
                              << " level " << level << "\n";
                    return false;
                }
            }
        }
    }

    for (int i = 0; i < 4096; ++i)
    {
        const float phase = (float) i / 4096.0f;
        const auto selection = WavetableBank::selectMip (440.0, sampleRate);
        const float sample = bank.readMorph (2, 0.4f, selection, phase);
        if (! std::isfinite (sample) || std::abs (sample) > 2.0f)
        {
            std::cerr << "Invalid wavetable output\n";
            return false;
        }
    }
    return true;
}

static bool testVoiceRendering()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    WavetableBank bank;
    juce::Synthesiser synth;
    for (int i = 0; i < 2; ++i)
        synth.addVoice (new SynthVoice (bank));
    synth.addSound (new SynthSound());
    synth.setCurrentPlaybackSampleRate (sampleRate);

    VoiceParams params;
    params.wtPosition = 2.0f / 7.0f;
    params.unisonVoices = 4;
    params.unisonDetune = 20.0f;
    params.unisonSpread = 0.8f;
    params.osc2Enabled = true;
    params.osc2WtPosition = 3.0f / 7.0f;
    params.osc2Level = 0.45f;
    params.osc2Semitones = 7;
    params.subLevel = 0.12f;
    params.noiseLevel = 0.08f;
    params.ampEnv.attack = 0.005f;
    params.ampEnv.decay = 0.1f;
    params.ampEnv.sustain = 0.65f;
    params.ampEnv.release = 0.12f;
    params.filterEnv.attack = 0.01f;
    params.filterEnv.decay = 0.15f;
    params.filterEnv.sustain = 0.4f;
    params.filterEnv.release = 0.2f;
    params.cutoff = 2600.0f;
    params.resonance = 0.35f;
    params.filterEnvAmount = 3.0f;
    params.lfo[0].shape = 0;
    params.lfo[0].rate = 5.0f;
    params.mod[0] = { ModSource::Lfo1, ModDest::Cutoff, 0.55f };
    params.mod[1] = { ModSource::Lfo1, ModDest::Pitch, 0.10f };

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i));
        if (voice == nullptr)
            return false;
        voice->prepare (sampleRate, blockSize);
        voice->setParameters (params);
    }

    juce::AudioBuffer<float> audio (2, blockSize);
    double totalEnergy = 0.0;
    double stereoDifference = 0.0;

    for (int block = 0; block < 50; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 90), 16);
        }
        if (block == 25)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
        }

        audio.clear();
        synth.renderNextBlock (audio, midi, 0, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            const float left = audio.getSample (0, i);
            const float right = audio.getSample (1, i);
            if (! std::isfinite (left) || ! std::isfinite (right)
                || std::abs (left) > 20.0f || std::abs (right) > 20.0f)
            {
                std::cerr << "Invalid voice sample in block " << block << "\n";
                return false;
            }
            totalEnergy += (double) left * left + (double) right * right;
            stereoDifference += std::abs ((double) left - right);
        }
    }

    if (totalEnergy < 0.001 || stereoDifference < 0.001)
    {
        std::cerr << "Synth rendered silence or lost stereo spread\n";
        return false;
    }
    return true;
}

int main()
{
    if (! testWavetableContinuity() || ! testVoiceRendering())
        return 1;
    std::cout << "WaveForge sound smoke tests passed.\n";
    return 0;
}
