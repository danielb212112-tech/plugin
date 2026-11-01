#pragma once

#include <JuceHeader.h>
#include <array>
#include "Wavetable.h"

// ---------------------------------------------------------------------------
// מקורות ויעדים של מטריצת המודולציה (הסדר חייב להתאים לרשימות ב-Processor)
// ---------------------------------------------------------------------------
namespace ModSource { enum { None = 0, Lfo1, Lfo2, Velocity, FilterEnv, Count }; }
namespace ModDest   { enum { None = 0, WtPosition, Cutoff, Resonance, Pitch, Volume, Count }; }

struct LfoParams
{
    int   shape   = 0;        // 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square, 5 Sample&Hold
    float rate    = 2.0f;     // Hz
    float fadeIn  = 0.0f;     // שניות
    bool  keySync = true;     // true: מתחיל מאפס בכל נוטה. false: פאזה אקראית
};

struct ModSlot
{
    int   source = ModSource::None;
    int   dest   = ModDest::None;
    float amount = 0.0f;      // -1..+1
};

// ---------------------------------------------------------------------------
// פרמטרים שה-Processor שולח לכל קול בתחילת כל בלוק
// ---------------------------------------------------------------------------
struct VoiceParams
{
    // Oscillator
    float wtPosition    = 0.286f;
    int   unisonVoices  = 1;
    float unisonDetune  = 20.0f;
    float unisonSpread  = 0.7f;

    juce::ADSR::Parameters ampEnv;

    // Filter
    int   filterType      = 0;
    float cutoff          = 3000.0f;
    float resonance       = 0.2f;
    float filterEnvAmount = 0.0f;
    juce::ADSR::Parameters filterEnv;

    // Modulation
    std::array<LfoParams, 2> lfo;
    std::array<ModSlot, 4>   mod;
};

// ---------------------------------------------------------------------------
struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// ---------------------------------------------------------------------------
// Voice: Wavetable (Unison) -> פילטר סטריאו -> ווליום, כולל 2 LFO ומטריצת מודולציה
// ---------------------------------------------------------------------------
class SynthVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int maxUnison   = 8;
    static constexpr int controlRate = 16;   // כל כמה דגימות מעדכנים מודולציה

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
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        level     = velocity;

        for (int u = 0; u < maxUnison; ++u)
            phases[(size_t) u] = (u == 0) ? 0.0 : (double) random.nextFloat();

        for (int l = 0; l < 2; ++l)
        {
            auto& st = lfoState[(size_t) l];
            st.phase = params.lfo[(size_t) l].keySync ? 0.0 : (double) random.nextFloat();
            st.held  = random.nextFloat() * 2.0f - 1.0f;
            st.value = 0.0f;
        }

        noteTime         = 0.0;
        lastFilterEnv    = 0.0f;
        controlCountdown = 0;       // מכריח עדכון מודולציה בדגימה הראשונה

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

        const double sr = getSampleRate();

        // ---- הכנת Unison לבלוק (דטיון, פאן) ----
        nUni = juce::jlimit (1, maxUnison, params.unisonVoices);
        const float norm = 1.0f / std::sqrt ((float) nUni);

        for (int u = 0; u < nUni; ++u)
        {
            const auto  su    = (size_t) u;
            const float x     = (nUni == 1) ? 0.0f : (2.0f * (float) u / (float) (nUni - 1) - 1.0f);
            const float angle = (x * params.unisonSpread + 1.0f) * juce::MathConstants<float>::pi * 0.25f;

            baseFreq[su] = frequency * std::pow (2.0, (double) (x * params.unisonDetune) / 1200.0);
            gL[su]       = std::cos (angle) * juce::MathConstants<float>::sqrt2 * norm;
            gR[su]       = std::sin (angle) * juce::MathConstants<float>::sqrt2 * norm;
        }

        // עדכון מודולציה בתחילת הבלוק (בלי להתקדם בזמן)
        updateModulation (lastFilterEnv, 0);

        for (int i = 0; i < numSamples; ++i)
        {
            const float fEnv = filterAdsr.getNextSample();
            lastFilterEnv = fEnv;

            if (--controlCountdown <= 0)
            {
                updateModulation (fEnv, controlRate);
                controlCountdown = controlRate;
            }

            float oscL = 0.0f, oscR = 0.0f;

            for (int u = 0; u < nUni; ++u)
            {
                const auto  su = (size_t) u;
                const float ph = (float) phases[su];

                float s = bank.read (frameA, lvl[su], ph);
                if (frameMix > 0.0f)
                    s += frameMix * (bank.read (frameA + 1, lvl[su], ph) - s);

                oscL += s * gL[su];
                oscR += s * gR[su];

                phases[su] += inc[su];
                if (phases[su] >= 1.0)
                    phases[su] -= 1.0;
            }

            const float g    = ampAdsr.getNextSample() * level * 0.25f * volumeGain;
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

        (void) sr;
    }

private:
    struct LfoState
    {
        double phase = 0.0;
        float  held  = 0.0f;   // ערך ל-Sample & Hold
        float  value = 0.0f;   // הערך הנוכחי (אחרי Fade-in)
    };

    static float lfoShape (int shape, float p, float held)
    {
        switch (shape)
        {
            case 0:  return std::sin (juce::MathConstants<float>::twoPi * p);                          // Sine
            case 1:  return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);    // Triangle
            case 2:  return 2.0f * p - 1.0f;                                                           // Saw Up
            case 3:  return 1.0f - 2.0f * p;                                                           // Saw Down
            case 4:  return p < 0.5f ? 1.0f : -1.0f;                                                   // Square
            case 5:  return held;                                                                      // Sample & Hold
            default: return 0.0f;
        }
    }

    float sourceValue (int source, float filterEnvValue) const
    {
        switch (source)
        {
            case ModSource::Lfo1:      return lfoState[0].value;
            case ModSource::Lfo2:      return lfoState[1].value;
            case ModSource::Velocity:  return level;
            case ModSource::FilterEnv: return filterEnvValue;
            default:                   return 0.0f;
        }
    }

    // מתקדם ב-elapsed דגימות (0 = רק חישוב מחדש) ומחשב את כל היעדים
    void updateModulation (float filterEnvValue, int elapsed)
    {
        const double sr    = getSampleRate();
        const double dtSec = (double) elapsed / sr;
        noteTime += dtSec;

        // ---- LFO-ים ----
        for (size_t l = 0; l < 2; ++l)
        {
            auto& st       = lfoState[l];
            const auto& lp = params.lfo[l];

            st.phase += (double) lp.rate * dtSec;
            if (st.phase >= 1.0)
            {
                st.phase -= std::floor (st.phase);
                st.held   = random.nextFloat() * 2.0f - 1.0f;
            }

            const float fade = lp.fadeIn > 0.001f ? juce::jmin (1.0f, (float) (noteTime / (double) lp.fadeIn))
                                                  : 1.0f;
            st.value = lfoShape (lp.shape, (float) st.phase, st.held) * fade;
        }

        // ---- סכימת המודולציות לפי היעדים ----
        float wtMod = 0.0f, cutOct = 0.0f, resMod = 0.0f, pitchSemis = 0.0f, volAdd = 0.0f;

        for (const auto& slot : params.mod)
        {
            if (slot.source == ModSource::None || slot.dest == ModDest::None || slot.amount == 0.0f)
                continue;

            const float v = slot.amount * sourceValue (slot.source, filterEnvValue);

            switch (slot.dest)
            {
                case ModDest::WtPosition:  wtMod      += v;          break;
                case ModDest::Cutoff:      cutOct     += v * 4.0f;   break;   // ±4 אוקטבות
                case ModDest::Resonance:   resMod     += v;          break;
                case ModDest::Pitch:       pitchSemis += v * 12.0f;  break;   // ±12 חצאי טון
                case ModDest::Volume:      volAdd     += v;          break;
                default: break;
            }
        }

        // ---- Wavetable ----
        const float pos  = juce::jlimit (0.0f, 1.0f, params.wtPosition + wtMod);
        const float posF = pos * (float) (WavetableBank::numFrames - 1);
        frameA   = juce::jlimit (0, WavetableBank::numFrames - 2, (int) posF);
        frameMix = juce::jlimit (0.0f, 1.0f, posF - (float) frameA);

        // ---- פיץ' (משפיע על קצב הפאזה ועל רמת ה-Mip) ----
        const double pitchMul = std::pow (2.0, (double) pitchSemis / 12.0);
        const double sr2      = sr;
        for (int u = 0; u < nUni; ++u)
        {
            const auto su = (size_t) u;
            const double f = baseFreq[su] * pitchMul;
            inc[su] = f / sr2;
            lvl[su] = WavetableBank::levelForFrequency (f, sr2);
        }

        // ---- פילטר ----
        const float maxFreq = (float) (sr * 0.45);
        const float cutoff  = juce::jlimit (20.0f, maxFreq,
                                params.cutoff * std::pow (2.0f, params.filterEnvAmount * filterEnvValue + cutOct));
        filter.setCutoffFrequency (cutoff);

        const float res = juce::jlimit (0.0f, 1.0f, params.resonance + resMod);
        filter.setResonance (0.707f + res * 10.0f);

        // ---- ווליום ----
        volumeGain = juce::jlimit (0.0f, 2.0f, 1.0f + volAdd);
    }

    const WavetableBank& bank;
    VoiceParams params;

    juce::ADSR ampAdsr, filterAdsr;
    juce::dsp::StateVariableTPTFilter<float> filter;

    std::array<double, maxUnison> phases   {};
    std::array<double, maxUnison> baseFreq {};
    std::array<double, maxUnison> inc      {};
    std::array<float,  maxUnison> gL {}, gR {};
    std::array<int,    maxUnison> lvl {};
    int nUni = 1;

    std::array<LfoState, 2> lfoState {};
    juce::Random random;

    int   frameA = 2;
    float frameMix = 0.0f;
    float volumeGain = 1.0f;

    double noteTime         = 0.0;
    float  lastFilterEnv    = 0.0f;
    int    controlCountdown = 0;

    double frequency = 440.0;
    float  level     = 0.0f;
};
