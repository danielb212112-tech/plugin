#pragma once

#include <JuceHeader.h>

// ---------------------------------------------------------------------------
// Sound: מגדיר שכל הנוטות מנוגנות על ידי אותו "סאונד"
// ---------------------------------------------------------------------------
struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// ---------------------------------------------------------------------------
// Voice: קול אחד בפוליפוניה. אוסצילטור + מעטפת ADSR
// ---------------------------------------------------------------------------
class SynthVoice : public juce::SynthesiserVoice
{
public:
    enum Waveform { Sine = 0, Saw, Square, Triangle };

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void prepare (double sampleRate)
    {
        setCurrentPlaybackSampleRate (sampleRate);
        adsr.setSampleRate (sampleRate);
    }

    // נקרא פעם בכל בלוק מה-Processor
    void setParameters (int newWaveform, const juce::ADSR::Parameters& p)
    {
        waveform = newWaveform;
        adsr.setParameters (p);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        level     = velocity;
        phase     = 0.0;
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const auto sr = getSampleRate();
        const auto dt = (float) (frequency / sr);

        for (int i = 0; i < numSamples; ++i)
        {
            const float env    = adsr.getNextSample();
            const float sample = generate ((float) phase, dt) * env * level * 0.25f;

            for (int ch = 0; ch < output.getNumChannels(); ++ch)
                output.addSample (ch, startSample + i, sample);

            phase += (double) dt;
            if (phase >= 1.0)
                phase -= 1.0;

            if (! adsr.isActive())
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
        switch (waveform)
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

    juce::ADSR adsr;
    double frequency = 440.0;
    double phase     = 0.0;
    float  level     = 0.0f;
    int    waveform  = Saw;
};
