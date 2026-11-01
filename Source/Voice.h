#pragma once

#include <JuceHeader.h>
#include <array>
#include "Wavetable.h"

// ---------------------------------------------------------------------------
// פרמטרים שה-Processor שולח לכל קול בתחילת כל בלוק
// ---------------------------------------------------------------------------
struct VoiceParams
{
    // Oscillator
    float wtPosition    = 0.286f;   // 0..1 לאורך הפריימים של ה-Wavetable
    int   unisonVoices  = 1;        // 1..8
    float unisonDetune  = 20.0f;    // סנטים (הקצה החיצוני)
    float unisonSpread  = 0.7f;     // 0..1 פיזור סטריאו

    juce::ADSR::Parameters ampEnv;

    // Filter
    int   filterType      = 0;      // 0 Low-pass, 1 Band-pass, 2 High-pass
    float cutoff          = 3000.0f;
    float resonance       = 0.2f;   // 0..1
    float filterEnvAmount = 0.0f;   // באוקטבות
    juce::ADSR::Parameters filterEnv;
};

// ---------------------------------------------------------------------------
struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// ---------------------------------------------------------------------------
// Voice: Wavetable (עם Unison) -> פילטר סטריאו -> ווליום (ADSR)
// ---------------------------------------------------------------------------
class SynthVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int maxUnison = 8;

    explicit SynthVoice (const WavetableBank& b) : bank (b) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void prepare (double sampleRate, int maxBlockSize)
    {
        setCurrentPlaybackSampleRate (sampleRate);
        ampAdsr.setSampleRate (sampleRate);
        filterAdsr.setSampleRate (sampleRate);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize), 2 };
        filter.prepare (spec);
        filter.reset();
    }

    void setParameters (const VoiceParams& p)
    {
        params = p;

        ampAdsr.setParameters (p.ampEnv);
        filterAdsr.setParameters (p.filterEnv);

        using FT = juce::dsp::StateVariableTPTFilterType;
        filter.setType (p.filterType == 1 ? FT::bandpass
                      : p.filterType == 2 ? FT::highpass
                                          : FT::lowpass);

        filter.setResonance (0.707f + p.resonance * 10.0f);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        level     = velocity;

        // פאזה אקראית לקולות ה-Unison (הראשון מתחיל מאפס) - נשמע רחב יותר
        for (int u = 0; u < maxUnison; ++u)
            phases[(size_t) u] = (u == 0) ? 0.0 : (double) random.nextFloat();

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

        const double sr      = getSampleRate();
        const float maxFreq  = (float) (sr * 0.45);

        // ---- הכנת Unison לבלוק ----
        const int nUni = juce::jlimit (1, maxUnison, params.unisonVoices);

        std::array<double, maxUnison> inc {};
        std::array<float,  maxUnison> gL {}, gR {};
        std::array<int,    maxUnison> lvl {};

        const float norm = 1.0f / std::sqrt ((float) nUni);

        for (int u = 0; u < nUni; ++u)
        {
            const float x     = (nUni == 1) ? 0.0f : (2.0f * (float) u / (float) (nUni - 1) - 1.0f);
            const double fu   = frequency * std::pow (2.0, (double) (x * params.unisonDetune) / 1200.0);
            const float angle = (x * params.unisonSpread + 1.0f) * juce::MathConstants<float>::pi * 0.25f;

            inc[(size_t) u] = fu / sr;
            lvl[(size_t) u] = WavetableBank::levelForFrequency (fu, sr);
            gL[(size_t) u]  = std::cos (angle) * juce::MathConstants<float>::sqrt2 * norm;
            gR[(size_t) u]  = std::sin (angle) * juce::MathConstants<float>::sqrt2 * norm;
        }

        // ---- מיקום ב-Wavetable: אינטרפולציה בין שני פריימים סמוכים ----
        const float posF = juce::jlimit (0.0f, 1.0f, params.wtPosition) * (float) (WavetableBank::numFrames - 1);
        const int   f0   = juce::jlimit (0, WavetableBank::numFrames - 2, (int) posF);
        const float fr   = juce::jlimit (0.0f, 1.0f, posF - (float) f0);

        for (int i = 0; i < numSamples; ++i)
        {
            float oscL = 0.0f, oscR = 0.0f;

            for (int u = 0; u < nUni; ++u)
            {
                const auto  su = (size_t) u;
                const float ph = (float) phases[su];

                float s = bank.read (f0, lvl[su], ph);
                if (fr > 0.0f)
                    s += fr * (bank.read (f0 + 1, lvl[su], ph) - s);

                oscL += s * gL[su];
                oscR += s * gR[su];

                phases[su] += inc[su];
                if (phases[su] >= 1.0)
                    phases[su] -= 1.0;
            }

            // Cutoff = בסיס * 2^(כמות * מעטפת)
            const float fEnv   = filterAdsr.getNextSample();
            const float cutoff = juce::jlimit (20.0f, maxFreq,
                                    params.cutoff * std::pow (2.0f, params.filterEnvAmount * fEnv));
            filter.setCutoffFrequency (cutoff);

            const float g = ampAdsr.getNextSample() * level * 0.25f;
            const float outL = filter.processSample (0, oscL) * g;
            const float outR = filter.processSample (1, oscR) * g;

            if (output.getNumChannels() >= 2)
            {
                output.addSample (0, startSample + i, outL);
                output.addSample (1, startSample + i, outR);
            }
            else
            {
                output.addSample (0, startSample + i, 0.5f * (outL + outR));
            }

            if (! ampAdsr.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    const WavetableBank& bank;
    VoiceParams params;

    juce::ADSR ampAdsr, filterAdsr;
    juce::dsp::StateVariableTPTFilter<float> filter;

    std::array<double, maxUnison> phases {};
    juce::Random random;

    double frequency = 440.0;
    float  level     = 0.0f;
};
