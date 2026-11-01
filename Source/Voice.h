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

    // Additional sound sources
    bool  osc2Enabled = false;
    float osc2WtPosition = 0.286f;
    float osc2Level = 0.0f;
    int   osc2Semitones = 0;
    float subLevel = 0.0f;
    float noiseLevel = 0.0f;

    juce::ADSR::Parameters ampEnv;

    // Filter
    int   filterType      = 0;
    float cutoff          = 3000.0f;
    float resonance       = 0.2f;
    float filterEnvAmount = 0.0f;
    int   filterSlope     = 0;      // 0 = 12 dB/oct, 1 = 24 dB/oct
    float filterKeyTrack  = 0.0f;   // 0..1; MIDI note 60 is the reference
    float filterDriveDb   = 0.0f;   // 0..18 dB of soft input saturation
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
        filterSecond.prepare (spec);
        filterSecond.reset();

        // Sample-accurate smoothing of pitch, wavetable motion, output and
        // filter controls prevents zippering during automation and fast LFOs.
        wtPositionSmooth.reset (sampleRate, 0.004);
        osc2PositionSmooth.reset (sampleRate, 0.004);
        pitchRatioSmooth.reset (sampleRate, 0.003);
        cutoffSmooth.reset (sampleRate, 0.006);
        resonanceSmooth.reset (sampleRate, 0.006);
        volumeSmooth.reset (sampleRate, 0.004);
        osc2GainSmooth.reset (sampleRate, 0.004);
        subGainSmooth.reset (sampleRate, 0.004);
        noiseGainSmooth.reset (sampleRate, 0.004);
        slopeSmooth.reset (sampleRate, 0.012);
        driveSmooth.reset (sampleRate, 0.008);
    }

    void setParameters (const VoiceParams& p)
    {
        params = p;

        ampAdsr.setParameters (p.ampEnv);
        filterAdsr.setParameters (p.filterEnv);

        using FT = juce::dsp::StateVariableTPTFilterType;
        const auto type = p.filterType == 1 ? FT::bandpass
                        : p.filterType == 2 ? FT::highpass
                                            : FT::lowpass;
        filter.setType (type);
        filterSecond.setType (type);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        noteNumber = midiNote;
        level     = velocity;

        subPhase = 0.0;
        for (int u = 0; u < maxUnison; ++u)
        {
            phases[(size_t) u] = (u == 0) ? 0.0 : (double) random.nextFloat();
            phases2[(size_t) u] = phases[(size_t) u];
        }

        for (int l = 0; l < 2; ++l)
        {
            auto& st = lfoState[(size_t) l];
            st.phase = params.lfo[(size_t) l].keySync ? 0.0 : (double) random.nextFloat();
            st.held  = random.nextFloat() * 2.0f - 1.0f;
            st.value = 0.0f;
        }

        noteTime         = 0.0;
        lastFilterEnv    = 0.0f;
        controlCountdown = 0;       // Force a zero-elapsed update on the first sample.
        modulationStarted = false;

        // Each new voice starts from the note's actual parameter values, not
        // from an unrelated previous note's smoothing history.
        wtPositionSmooth.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, params.wtPosition));
        osc2PositionSmooth.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, params.osc2WtPosition));
        pitchRatioSmooth.setCurrentAndTargetValue (1.0f);
        cutoffSmooth.setCurrentAndTargetValue (juce::jlimit (20.0f,
            (float) (getSampleRate() * 0.45), params.cutoff));
        resonanceSmooth.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, params.resonance));
        volumeSmooth.setCurrentAndTargetValue (1.0f);
        osc2GainSmooth.setCurrentAndTargetValue (0.0f);
        subGainSmooth.setCurrentAndTargetValue (0.0f);
        noiseGainSmooth.setCurrentAndTargetValue (0.0f);
        slopeSmooth.setCurrentAndTargetValue (params.filterSlope != 0 ? 1.0f : 0.0f);
        driveSmooth.setCurrentAndTargetValue (juce::jlimit (0.0f, 18.0f, params.filterDriveDb));

        filter.reset();
        filterSecond.reset();
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
        if (sr <= 0.0)
            return;

        const double invSr = 1.0 / sr;
        const float nyquist = (float) (sr * 0.5);

        // Preserve equal-power spread for stereo Unison.
        nUni = juce::jlimit (1, maxUnison, params.unisonVoices);
        const float norm = 1.0f / std::sqrt ((float) nUni);

        for (int u = 0; u < nUni; ++u)
        {
            const auto su = (size_t) u;
            const float x = nUni == 1 ? 0.0f
                                       : (2.0f * (float) u / (float) (nUni - 1) - 1.0f);
            const float angle = (x * params.unisonSpread + 1.0f)
                                  * juce::MathConstants<float>::pi * 0.25f;
            baseFreq[su] = frequency * std::pow (2.0, (double) (x * params.unisonDetune) / 1200.0);
            gL[su] = std::cos (angle) * juce::MathConstants<float>::sqrt2 * norm;
            gR[su] = std::sin (angle) * juce::MathConstants<float>::sqrt2 * norm;
        }

        const double osc2Transpose = std::pow (2.0, (double) params.osc2Semitones / 12.0);

        for (int i = 0; i < numSamples; ++i)
        {
            const float fEnv = filterAdsr.getNextSample();
            lastFilterEnv = fEnv;

            if (--controlCountdown <= 0)
            {
                // Zero elapsed for the very first sample: the previous code
                // advanced LFOs 16 samples early on note-on.
                updateModulation (fEnv, modulationStarted ? controlRate : 0);
                modulationStarted = true;
                controlCountdown = controlRate;
            }

            const float pitchRatio = pitchRatioSmooth.getNextValue();
            const float wtPos = wtPositionSmooth.getNextValue()
                               * (float) (WavetableBank::numFrames - 1);
            const int frame = juce::jlimit (0, WavetableBank::numFrames - 2, (int) wtPos);
            const float frameBlend = juce::jlimit (0.0f, 1.0f, wtPos - (float) frame);

            const float wt2Pos = osc2PositionSmooth.getNextValue()
                                * (float) (WavetableBank::numFrames - 1);
            const int frame2 = juce::jlimit (0, WavetableBank::numFrames - 2, (int) wt2Pos);
            const float frame2Blend = juce::jlimit (0.0f, 1.0f, wt2Pos - (float) frame2);

            const float osc2Gain = osc2GainSmooth.getNextValue();
            const float subGain = subGainSmooth.getNextValue();
            const float noiseGain = noiseGainSmooth.getNextValue();
            const float voiceGain = volumeSmooth.getNextValue();

            const float smoothCutoff = cutoffSmooth.getNextValue();
            // Cascading two 12dB state-variable stages gives 24dB/oct.
            // Both stages always run, so switching slopes can crossfade without
            // discontinuities or stale filter state.
            const float res = resonanceSmooth.getNextValue();
            const float slopeBlend = slopeSmooth.getNextValue();
            // Cascading resonant stages squares their peaks. Decrease the
            // first stage's Q as the 24 dB path engages, keeping resonant
            // patches manageable at both slope settings.
            const float q = 0.707f + res * (7.0f - 4.5f * slopeBlend);
            filter.setCutoffFrequency (smoothCutoff);
            filter.setResonance (q);
            filterSecond.setCutoffFrequency (smoothCutoff);
            filterSecond.setResonance (0.707f + res * 1.8f);
            const float driveDb = driveSmooth.getNextValue();

            float oscL = 0.0f, oscR = 0.0f;

            for (int u = 0; u < nUni; ++u)
            {
                const auto su = (size_t) u;
                const double f = baseFreq[su] * (double) pitchRatio;
                const float fade = juce::jlimit (0.0f, 1.0f,
                    (nyquist - (float) f) / (nyquist * 0.10f));

                const float ph = (float) phases[su];
                if (fade > 0.0f)
                {
                    // Mip selection is bandwidth-safe for the *instantaneous*
                    // fundamental, including every detuned Unison voice.
                    const auto mip = WavetableBank::selectMip (f, sr);
                    const float value = bank.readMorph (frame, frameBlend, mip, ph) * fade;
                    oscL += value * gL[su];
                    oscR += value * gR[su];
                }

                phases[su] += f * invSr;
                if (phases[su] >= 1.0)
                    phases[su] -= std::floor (phases[su]);

                const double f2 = f * osc2Transpose;
                const float fade2 = juce::jlimit (0.0f, 1.0f,
                    (nyquist - (float) f2) / (nyquist * 0.10f));
                if (osc2Gain > 0.00001f && fade2 > 0.0f)
                {
                    const auto mip2 = WavetableBank::selectMip (f2, sr);
                    const float value2 = bank.readMorph (frame2, frame2Blend, mip2,
                                                          (float) phases2[su])
                                         * osc2Gain * fade2;
                    oscL += value2 * gL[su];
                    oscR += value2 * gR[su];
                }
                // Free-run OSC2 even while muted to avoid resetting its phase
                // when the oscillator is enabled mid-note.
                phases2[su] += f2 * invSr;
                if (phases2[su] >= 1.0)
                    phases2[su] -= std::floor (phases2[su]);
            }

            // Pitch modulation also tracks the sub, keeping octaves in tune.
            const float sub = std::sin (juce::MathConstants<float>::twoPi * (float) subPhase)
                              * subGain;
            subPhase += frequency * 0.5 * (double) pitchRatio * invSr;
            if (subPhase >= 1.0)
                subPhase -= std::floor (subPhase);

            // Independent left/right noise avoids a collapsed stereo image.
            oscL += sub + (random.nextFloat() * 2.0f - 1.0f) * noiseGain;
            oscR += sub + (random.nextFloat() * 2.0f - 1.0f) * noiseGain;

            // An optional gentle pre-filter saturator adds character while
            // leaving legacy presets bit-for-bit clean at Drive = 0 dB.
            // A separate 4x-oversampled Distortion FX is available for heavy
            // nonlinear processing; this filter drive is deliberately moderate.
            if (driveDb > 0.00001f)
            {
                const float gain = juce::Decibels::decibelsToGain (driveDb);
                const float normalize = 1.0f / std::tanh (gain);
                const float wetAmount = juce::jlimit (0.0f, 1.0f, driveDb / 18.0f);
                oscL += wetAmount * (std::tanh (oscL * gain) * normalize - oscL);
                oscR += wetAmount * (std::tanh (oscR * gain) * normalize - oscR);
            }

            const float firstL = filter.processSample (0, oscL);
            const float firstR = filter.processSample (1, oscR);
            const float secondL = filterSecond.processSample (0, firstL);
            const float secondR = filterSecond.processSample (1, firstR);
            const float amp = ampAdsr.getNextSample() * level * 0.25f * voiceGain;
            const float outL = (firstL + slopeBlend * (secondL - firstL)) * amp;
            const float outR = (firstR + slopeBlend * (secondR - firstR)) * amp;

            if (output.getNumChannels() >= 2)
            {
                output.addSample (0, startSample + i, outL);
                output.addSample (1, startSample + i, outR);
            }
            else if (output.getNumChannels() > 0)
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

    // Updates modulation targets at the control rate. SmoothedValue then
    // interpolates those targets on EVERY sample in renderNextBlock().
    void updateModulation (float filterEnvValue, int elapsed)
    {
        const double sr = getSampleRate();
        const double dtSec = (double) elapsed / sr;
        noteTime += dtSec;

        for (size_t l = 0; l < 2; ++l)
        {
            auto& st = lfoState[l];
            const auto& lp = params.lfo[l];

            st.phase += (double) lp.rate * dtSec;
            if (st.phase >= 1.0)
            {
                st.phase -= std::floor (st.phase);
                st.held = random.nextFloat() * 2.0f - 1.0f;
            }

            const float fade = lp.fadeIn > 0.001f
                ? juce::jmin (1.0f, (float) (noteTime / (double) lp.fadeIn))
                : 1.0f;
            st.value = lfoShape (lp.shape, (float) st.phase, st.held) * fade;
        }

        float wtMod = 0.0f, cutOct = 0.0f, resMod = 0.0f;
        float pitchSemis = 0.0f, volAdd = 0.0f;
        for (const auto& slot : params.mod)
        {
            if (slot.source == ModSource::None || slot.dest == ModDest::None
                || slot.amount == 0.0f)
                continue;

            const float v = slot.amount * sourceValue (slot.source, filterEnvValue);
            switch (slot.dest)
            {
                case ModDest::WtPosition: wtMod += v; break;
                case ModDest::Cutoff:     cutOct += v * 4.0f; break;
                case ModDest::Resonance:  resMod += v; break;
                case ModDest::Pitch:      pitchSemis += v * 12.0f; break;
                case ModDest::Volume:     volAdd += v; break;
                default: break;
            }
        }

        wtPositionSmooth.setTargetValue (
            juce::jlimit (0.0f, 1.0f, params.wtPosition + wtMod));
        osc2PositionSmooth.setTargetValue (
            juce::jlimit (0.0f, 1.0f, params.osc2WtPosition));

        // Multiple matrix routes can add up, so cap modulation to an
        // intentionally musical range before converting to a pitch ratio.
        const float pitch = juce::jlimit (-48.0f, 48.0f, pitchSemis);
        pitchRatioSmooth.setTargetValue (std::pow (2.0f, pitch / 12.0f));

        // 100% key tracking shifts cutoff by one octave per MIDI octave.
        // Middle C (60) stays at the user's selected cutoff frequency.
        const float keyOctaves = juce::jlimit (0.0f, 1.0f, params.filterKeyTrack)
            * ((float) noteNumber - 60.0f) / 12.0f;
        const float maxFreq = (float) (sr * 0.45);
        const float cutoff = juce::jlimit (20.0f, maxFreq,
            params.cutoff * std::pow (2.0f,
                juce::jlimit (-16.0f, 16.0f,
                    params.filterEnvAmount * filterEnvValue + cutOct + keyOctaves)));
        cutoffSmooth.setTargetValue (cutoff);
        slopeSmooth.setTargetValue (params.filterSlope == 0 ? 0.0f : 1.0f);
        driveSmooth.setTargetValue (juce::jlimit (0.0f, 18.0f, params.filterDriveDb));
        resonanceSmooth.setTargetValue (
            juce::jlimit (0.0f, 1.0f, params.resonance + resMod));
        volumeSmooth.setTargetValue (juce::jlimit (0.0f, 2.0f, 1.0f + volAdd));

        osc2GainSmooth.setTargetValue (params.osc2Enabled
            ? juce::jlimit (0.0f, 1.0f, params.osc2Level) : 0.0f);
        subGainSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, params.subLevel));
        noiseGainSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, params.noiseLevel));
    }

    const WavetableBank& bank;
    VoiceParams params;

    juce::ADSR ampAdsr, filterAdsr;
    juce::dsp::StateVariableTPTFilter<float> filter, filterSecond;

    std::array<double, maxUnison> phases {}, phases2 {}, baseFreq {};
    std::array<float, maxUnison> gL {}, gR {};
    double subPhase = 0.0;
    int nUni = 1;

    juce::SmoothedValue<float> wtPositionSmooth, osc2PositionSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> pitchRatioSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    juce::SmoothedValue<float> resonanceSmooth, volumeSmooth;
    juce::SmoothedValue<float> osc2GainSmooth, subGainSmooth, noiseGainSmooth;
    juce::SmoothedValue<float> slopeSmooth, driveSmooth;

    std::array<LfoState, 2> lfoState {};
    juce::Random random;

    double noteTime = 0.0;
    float lastFilterEnv = 0.0f;
    int controlCountdown = 0;
    bool modulationStarted = false;

    double frequency = 440.0;
    int noteNumber = 69;
    float level = 0.0f;
};
