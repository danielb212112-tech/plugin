#include <JuceHeader.h>
#include "../Source/Voice.h"
#include "../Source/Effects.h"

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

// Compare spectral aliasing against the old sample-rate clipper. At 48 kHz,
// clipping a 7 kHz sinusoid creates a strong 49 kHz harmonic that folds down
// to 1 kHz. With x4 oversampling the decimator should largely remove it.
static bool testOversampledDistortion()
{
    constexpr double sr = 48000.0;
    constexpr int blockSize = 256;
    constexpr int total = 9600;
    constexpr int analyzed = 4800;
    constexpr double inputHz = 7000.0;
    constexpr double aliasHz = 1000.0;
    const double twoPi = juce::MathConstants<double>::twoPi;
    const float drive = juce::Decibels::decibelsToGain (30.0f);

    DistortionFx distortion;
    distortion.prepare (sr, blockSize);
    distortion.setParams (1, 30.0f, 1.0f); // hard clip, strong drive, wet
    if (distortion.getLatencySamples() <= 0)
    {
        std::cerr << "Oversampling latency was not reported\n";
        return false;
    }

    double legacyCos = 0.0, legacySin = 0.0, oversampledCos = 0.0, oversampledSin = 0.0;
    juce::AudioBuffer<float> block (2, blockSize);
    for (int offset = 0; offset < total; offset += blockSize)
    {
        const int count = juce::jmin (blockSize, total - offset);
        juce::AudioBuffer<float> working (2, count);
        for (int i = 0; i < count; ++i)
        {
            const int n = offset + i;
            const float input = 0.8f * std::sin ((float) (twoPi * inputHz * n / sr));
            working.setSample (0, i, input);
            working.setSample (1, i, input);
        }

        distortion.process (working);
        for (int i = 0; i < count; ++i)
        {
            const int n = offset + i;
            const float value = working.getSample (0, i);
            if (! std::isfinite (value) || std::abs (value) > 2.0f)
            {
                std::cerr << "Non-finite or excessive oversampled distortion output\n";
                return false;
            }

            if (n >= total - analyzed)
            {
                const double phase = twoPi * aliasHz * n / sr;
                const float input = 0.8f * std::sin ((float) (twoPi * inputHz * n / sr));
                const float oldClip = juce::jlimit (-1.0f, 1.0f, input * drive);
                const double cs = std::cos (phase), sn = std::sin (phase);
                legacyCos += oldClip * cs;
                legacySin += oldClip * sn;
                oversampledCos += value * cs;
                oversampledSin += value * sn;
            }
        }
    }

    const double oldAlias = std::hypot (legacyCos, legacySin);
    const double newAlias = std::hypot (oversampledCos, oversampledSin);
    std::cout << "Alias energy at 1 kHz: baseline=" << oldAlias
              << " oversampled=" << newAlias << "\n";
    if (oldAlias < 1.0 || newAlias >= oldAlias * 0.7)
    {
        std::cerr << "4x distortion did not sufficiently suppress the folded harmonic\n";
        return false;
    }

    // 0% wet must remain an exact latency-compensated dry path. This ensures
    // that automation between dry/wet does not create comb filtering.
    distortion.reset();
    distortion.setParams (0, 12.0f, 0.0f);
    const int delay = distortion.getLatencySamples();
    std::vector<float> reference ((size_t) total, 0.0f);
    for (int offset = 0; offset < total; offset += blockSize)
    {
        const int count = juce::jmin (blockSize, total - offset);
        juce::AudioBuffer<float> working (2, count);
        for (int i = 0; i < count; ++i)
        {
            const int n = offset + i;
            reference[(size_t) n] = 0.35f * std::sin ((float) (twoPi * 349.0 * n / sr));
            working.setSample (0, i, reference[(size_t) n]);
            working.setSample (1, i, reference[(size_t) n]);
        }
        distortion.process (working);
        for (int i = 0; i < count; ++i)
        {
            const int n = offset + i;
            const float expected = n >= delay ? reference[(size_t) (n - delay)] : 0.0f;
            if (std::abs (working.getSample (0, i) - expected) > 1.0e-5f
                || std::abs (working.getSample (1, i) - expected) > 1.0e-5f)
            {
                std::cerr << "Distortion dry/latency alignment failed at sample " << n << "\n";
                return false;
            }
        }
    }

    // Exercise all nonlinear modes, changing drive and wet mix mid-stream.
    for (int mode = 0; mode < 4; ++mode)
    {
        distortion.setParams (mode, 24.0f + mode, 0.5f);
        block.clear();
        for (int i = 0; i < blockSize; ++i)
        {
            block.setSample (0, i, 0.5f);
            block.setSample (1, i, -0.5f);
        }
        distortion.process (block);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < blockSize; ++i)
                if (! std::isfinite (block.getSample (channel, i)))
                    return false;
    }
    return true;
}


static bool testAdvancedFilter()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    WavetableBank bank;

    // Measure actual voice rendering, not a mocked filter approximation.
    // Settled RMS over several blocks excludes ADSR start transients.
    auto rmsFor = [&bank] (int midiNote, int filterType, int slope,
                           float keyTrack, float driveDb, float cutoff)
    {
        juce::Synthesiser synth;
        synth.addVoice (new SynthVoice (bank));
        synth.addSound (new SynthSound());
        synth.setCurrentPlaybackSampleRate (sampleRate);

        auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (0));
        voice->prepare (sampleRate, blockSize);

        VoiceParams p;
        p.wtPosition = 0.0f; // sine: a clean one-frequency probe
        p.unisonVoices = 1;
        p.cutoff = cutoff;
        p.resonance = 0.0f;
        p.filterType = filterType;
        p.filterSlope = slope;
        p.filterKeyTrack = keyTrack;
        p.filterDriveDb = driveDb;
        p.filterEnvAmount = 0.0f;
        p.ampEnv.attack = 0.001f;
        p.ampEnv.decay = 0.03f;
        p.ampEnv.sustain = 1.0f;
        p.ampEnv.release = 0.1f;
        p.filterEnv.attack = 0.001f;
        p.filterEnv.decay = 0.03f;
        p.filterEnv.sustain = 1.0f;
        p.filterEnv.release = 0.1f;
        voice->setParameters (p);

        juce::AudioBuffer<float> block (2, blockSize);
        double energy = 0.0;
        int measured = 0;
        for (int b = 0; b < 42; ++b)
        {
            block.clear();
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) 120), 0);

            synth.renderNextBlock (block, midi, 0, blockSize);
            for (int i = 0; i < blockSize; ++i)
            {
                const float left = block.getSample (0, i);
                const float right = block.getSample (1, i);
                if (! std::isfinite (left) || ! std::isfinite (right)
                    || std::abs (left) > 20.0f || std::abs (right) > 20.0f)
                    return -1.0;

                if (b >= 32)
                {
                    energy += (double) left * left;
                    ++measured;
                }
            }
        }
        return std::sqrt (energy / (double) measured);
    };

    // 2,093 Hz sine, cutoff well below it: an LP24 must attenuate more
    // than LP12, while 100% keytrack should open the filter for high notes.
    const double lp12 = rmsFor (96, 0, 0, 0.0f, 0.0f, 350.0f);
    const double lp24 = rmsFor (96, 0, 1, 0.0f, 0.0f, 350.0f);
    const double keytracked = rmsFor (96, 0, 0, 1.0f, 0.0f, 350.0f);
    const double driven = rmsFor (96, 0, 1, 0.8f, 18.0f, 1200.0f);
    const double bp24 = rmsFor (96, 1, 1, 0.0f, 8.0f, 2000.0f);
    const double hp24 = rmsFor (60, 2, 1, 0.0f, 8.0f, 1500.0f);
    std::cout << "Filter RMS LP12=" << lp12
              << " LP24=" << lp24
              << " keytracked=" << keytracked
              << " driven=" << driven
              << " BP24=" << bp24
              << " HP24=" << hp24 << "\n";

    if (! (lp12 > 1.0e-5 && lp24 >= 0.0 && lp24 < lp12 * 0.65
           && keytracked > lp12 * 3.0
           && driven >= 0.0 && bp24 >= 0.0 && hp24 >= 0.0))
    {
        std::cerr << "Advanced filter response, key tracking or drive test failed\n";
        return false;
    }

    return true;
}

int main()
{
    if (! testWavetableContinuity() || ! testVoiceRendering()
        || ! testOversampledDistortion() || ! testAdvancedFilter())
        return 1;
    std::cout << "WaveForge sound smoke tests passed.\n";
    return 0;
}
