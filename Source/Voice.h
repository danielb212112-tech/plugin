#pragma once

#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// פרמטרים שה-Processor שולח לכל קול בתחילת כל בלוק
// ---------------------------------------------------------------------------
struct VoiceParams
{
    int waveform = 1;                  // 0 Sine, 1 Saw, 2 Square, 3 Triangle
    juce::ADSR::Parameters ampEnv;

    int   filterType      = 0;         // 0 Low-pass, 1 Band-pass, 2 High-pass
    float cutoff          = 3000.0f;   // Hz
    float resonance       = 0.2f;      // 0..1
    float filterEnvAmount = 0.0f;      // באוקטבות, -6..+6
    juce::ADSR::Parameters filterEnv;
};

// ---------------------------------------------------------------------------
// Sound: מגדיר שכל הנוטות מנוגנות על ידי אותו "סאונד"
// ---------------------------------------------------------------------------
struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// ---------------------------------------------------------------------------
// Voice: אוסצילטור -> פילטר -> ווליום (ADSR)
// ---------------------------------------------------------------------------
class SynthVoice : public juce::SynthesiserVoice
{
public:
    enum Waveform { Sine = 0, Saw, Square, Triangle };

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void prepare (double sampleRate, int maxBlockSize)
    {
        setCurrentPlaybackSampleRate (sampleRate);
        ampAdsr.setSampleRate (sampleRate);
        filterAdsr.setSampleRate (sampleRate);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize), 1 };
        filter.prepare (spec);
        filter.reset();
    }

    // נקרא פעם בכל בלוק מה-Processor
    void setParameters (const VoiceParams& p)
    {
        params = p;

        ampAdsr.setParameters (p.ampEnv);
        filterAdsr.setParameters (p.filterEnv);

        using FT = juce::dsp::StateVariableTPTFilterType;
        filter.setType (p.filterType == 1 ? FT::bandpass
                      : p.filterType == 2 ? FT::highpass
                                          : FT::lowpass);

        // Resonance 0..1  ->  Q של 0.707 עד ~11
        filter.setResonance (0.707f + p.resonance * 10.0f);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        level     = velocity;
        phase     = 0.0;
        filter.reset();
        ampAdsr.noteOn();
        filterAdsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampAdsr.noteOff();
            filterAdsr.noteOff();
        }
        else
        {
            ampAdsr.reset();
            filterAdsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const auto sr       = getSampleRate();
        const auto dt       = (float) (frequency / sr);
        const float maxFreq = (float) (sr * 0.45);

        for (int i = 0; i < numSamples; ++i)
        {
            // Cutoff = בסיס * 2^(כמות * מעטפת)
            const float fEnv   = filterAdsr.getNextSample();
            const float cutoff = juce::jlimit (20.0f, maxFreq,
                                    params.cutoff * std::pow (2.0f, params.filterEnvAmount * fEnv));
            filter.setCutoffFrequency (cutoff);

            const float osc  = generate ((float) phase, dt);
            const float filt = filter.processSample (0, osc);
            const float env  = ampAdsr.getNextSample();
            const float out  = filt * env * level * 0.25f;

            for (int ch = 0; ch < output.getNumChannels(); ++ch)
                output.addSample (ch, startSample + i, out);

            phase += (double) dt;
            if (phase >= 1.0)
                phase -= 1.0;

            if (! ampAdsr.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    // PolyBLEP - מפחית aliasing בגלים חדים (Saw / Square)
    static float polyBlep (float t, float dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float generate (float t, float dt) const
    {
        switch (params.waveform)
        {
            case Sine:
                return std::sin (juce::MathConstants<float>::twoPi * t);

            case Saw:
                return (2.0f * t - 1.0f) - polyBlep (t, dt);

            case Square:
            {
                float v = t < 0.5f ? 1.0f : -1.0f;
                v += polyBlep (t, dt);
                v -= polyBlep (std::fmod (t + 0.5f, 1.0f), dt);
                return v;
            }

            case Triangle:
                return 4.0f * std::abs (t - 0.5f) - 1.0f;

            default:
                return 0.0f;
        }
    }

    VoiceParams params;

    juce::ADSR ampAdsr, filterAdsr;
    juce::dsp::StateVariableTPTFilter<float> filter;

    double frequency = 440.0;
    double phase     = 0.0;
    float  level     = 0.0f;
};
