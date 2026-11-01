#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <vector>
#include <cmath>

// 4x high-quality FIR oversampled distortion.
// The oversampler is ALWAYS in the signal path, including when Mix = 0.
// This keeps its latency constant across bypass changes and allows the DAW
// to compensate accurately. The dry path is delayed by the same amount.
//
// No dynamic memory allocation occurs on the audio thread. All scratch space,
// filters and delay lines are prepared before playback.
class DistortionFx
{
public:
    static constexpr int oversamplingFactor = 4;

    void prepare (double sampleRate, int maximumBlockSize)
    {
        maxBlockSize = juce::jmax (1, maximumBlockSize);
        oversampling.initProcessing ((size_t) maxBlockSize);
        oversampling.reset();
        latencySamples = juce::jmax (0, juce::roundToInt (oversampling.getLatencyInSamples()));

        dryScratch.setSize (2, maxBlockSize, false, false, true);
        dryScratch.clear();

        const size_t delaySize = (size_t) juce::jmax (1, latencySamples);
        for (auto& channel : dryDelay)
            channel.assign (delaySize, 0.0f);
        dryWritePosition = 0;

        driveValues.resize ((size_t) maxBlockSize * oversamplingFactor);
        typeBlends.resize ((size_t) maxBlockSize * oversamplingFactor);

        const double safeRate = juce::jmax (1.0, sampleRate);
        driveSmooth.reset (safeRate * oversamplingFactor, 0.006);
        driveSmooth.setCurrentAndTargetValue (driveTarget);
        mixSmooth.reset (safeRate, 0.012);
        mixSmooth.setCurrentAndTargetValue (0.0f);
        typeBlend.reset (safeRate * oversamplingFactor, 0.006);
        typeBlend.setCurrentAndTargetValue (1.0f);
        previousType = type;
        ready = true;
    }

    int getLatencySamples() const noexcept { return latencySamples; }

    void reset() noexcept
    {
        oversampling.reset();
        for (auto& channel : dryDelay)
            std::fill (channel.begin(), channel.end(), 0.0f);
        dryWritePosition = 0;
        driveSmooth.setCurrentAndTargetValue (driveTarget);
        mixSmooth.setCurrentAndTargetValue (0.0f);
        typeBlend.setCurrentAndTargetValue (1.0f);
        previousType = type;
    }

    void setParams (int newType, float driveDb, float newMix) noexcept
    {
        const int requestedType = juce::jlimit (0, 3, newType);
        if (requestedType != type)
        {
            previousType = type;
            type = requestedType;
            typeBlend.setCurrentAndTargetValue (0.0f);
            typeBlend.setTargetValue (1.0f);
        }

        driveTarget = juce::Decibels::decibelsToGain (
            juce::jlimit (0.0f, 36.0f, driveDb));
        driveSmooth.setTargetValue (driveTarget);
        mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, newMix));
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        // The synth's bus is always stereo. Reject unprepared/mismatched
        // input safely instead of resizing any buffer inside process().
        if (! ready || buffer.getNumChannels() != 2 || buffer.getNumSamples() == 0)
            return;

        juce::dsp::AudioBlock<float> whole (buffer);
        const int totalSamples = buffer.getNumSamples();

        for (int start = 0; start < totalSamples; start += maxBlockSize)
        {
            const int count = juce::jmin (maxBlockSize, totalSamples - start);
            for (int channel = 0; channel < 2; ++channel)
                dryScratch.copyFrom (channel, 0, buffer, channel, start, count);

            auto block = whole.getSubBlock ((size_t) start, (size_t) count);
            auto upsampled = oversampling.processSamplesUp (block);
            const size_t highRateSamples = upsampled.getNumSamples();

            for (size_t i = 0; i < highRateSamples; ++i)
            {
                driveValues[i] = driveSmooth.getNextValue();
                typeBlends[i] = typeBlend.getNextValue();
            }

            for (size_t channel = 0; channel < 2; ++channel)
            {
                float* samples = upsampled.getChannelPointer (channel);
                for (size_t i = 0; i < highRateSamples; ++i)
                {
                    const float x = samples[i] * driveValues[i];
                    const float wet = saturate (type, x);
                    const float blend = typeBlends[i];
                    samples[i] = blend >= 1.0f
                        ? wet
                        : saturate (previousType, x)
                          + blend * (wet - saturate (previousType, x));
                }
            }

            oversampling.processSamplesDown (block);

            for (int i = 0; i < count; ++i)
            {
                const float mix = mixSmooth.getNextValue();
                for (int channel = 0; channel < 2; ++channel)
                {
                    const float dry = dryScratch.getSample (channel, i);
                    const float delayed = latencySamples > 0
                        ? dryDelay[(size_t) channel][dryWritePosition] : dry;

                    if (latencySamples > 0)
                        dryDelay[(size_t) channel][dryWritePosition] = dry;

                    const float wet = buffer.getSample (channel, start + i);
                    buffer.setSample (channel, start + i,
                                      delayed + mix * (wet - delayed));
                }

                if (latencySamples > 0)
                    dryWritePosition = (dryWritePosition + 1) % dryDelay[0].size();
            }
        }
    }

private:
    static float saturate (int mode, float x) noexcept
    {
        switch (mode)
        {
            case 0: return std::tanh (x);
            case 1: return juce::jlimit (-1.0f, 1.0f, x);
            case 2:
            {
                float folded = std::fmod (x + 1.0f, 4.0f);
                if (folded < 0.0f) folded += 4.0f;
                return folded < 2.0f ? folded - 1.0f : 3.0f - folded;
            }
            case 3: return std::sin (x * juce::MathConstants<float>::halfPi);
            default: return x;
        }
    }

    // Two FIR half-band stages provide x4 oversampling with linear phase.
    // Integer-latency mode compensates the FIR's fractional sample delay.
    juce::dsp::Oversampling<float> oversampling {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        true, true
    };

    juce::AudioBuffer<float> dryScratch;
    std::array<std::vector<float>, 2> dryDelay;
    std::vector<float> driveValues, typeBlends;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveSmooth;
    juce::SmoothedValue<float> mixSmooth, typeBlend;

    int maxBlockSize = 1, latencySamples = 0;
    int type = 0, previousType = 0;
    float driveTarget = 1.0f;
    size_t dryWritePosition = 0;
    bool ready = false;
};

class StereoDelay
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);
        const auto capacity = (size_t) std::ceil (sampleRate * 2.05) + 4;
        for (auto& channel : ring)
            channel.assign (capacity, 0.0f);
        writeIndex = 0;
        toneState = {};
        delayTimeSmooth.reset (sampleRate, 0.035);
        delayTimeSmooth.setCurrentAndTargetValue ((float) (sampleRate * 0.35));
        mixSmooth.reset (sampleRate, 0.012);
        mixSmooth.setCurrentAndTargetValue (0.0f);
    }

    void reset() noexcept
    {
        for (auto& channel : ring)
            std::fill (channel.begin(), channel.end(), 0.0f);
        writeIndex = 0;
        toneState = {};
        mixSmooth.setCurrentAndTargetValue (0.0f);
    }

    bool isBypassed() const noexcept
    {
        return ! mixSmooth.isSmoothing() && mixSmooth.getCurrentValue() <= 0.0001f;
    }

    void setParams (float timeMs, float newFeedback, float toneHz,
                    bool newPingPong, float newMix) noexcept
    {
        const auto maxDelay = ring[0].empty() ? 1 : (int) ring[0].size() - 2;
        const float targetSamples = juce::jlimit (1.0f, (float) maxDelay,
            (float) (sampleRate * juce::jlimit (1.0f, 2000.0f, timeMs) * 0.001));
        // Smooth time changes rather than jumping to a different ring-buffer
        // read point, avoiding the clicks of an integer-sample delay switch.
        delayTimeSmooth.setTargetValue (targetSamples);
        feedback = juce::jlimit (0.0f, 0.95f, newFeedback);
        pingPong = newPingPong;
        mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, newMix));
        const float cutoff = juce::jlimit (20.0f, (float) (sampleRate * 0.45),
                                           toneHz);
        toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                     * cutoff / (float) sampleRate);
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (ring[0].empty() || buffer.getNumChannels() == 0)
            return;

        const int channels = juce::jmin (2, buffer.getNumChannels());
        const size_t size = ring[0].size();

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float timeSamples = delayTimeSmooth.getNextValue();
            const int whole = (int) std::ceil (timeSamples);
            const float fraction = (float) whole - timeSamples;
            const size_t read0 = (writeIndex + size - (size_t) whole) % size;
            const size_t read1 = (read0 + 1) % size;
            const float inL = buffer.getSample (0, i);
            const float inR = channels > 1 ? buffer.getSample (1, i) : inL;
            const float delayedL = ring[0][read0]
                + fraction * (ring[0][read1] - ring[0][read0]);
            const float delayedR = channels > 1
                ? ring[1][read0] + fraction * (ring[1][read1] - ring[1][read0])
                : delayedL;
            const float wetMix = mixSmooth.getNextValue();

            toneState[0] += toneCoeff * (delayedL - toneState[0]);
            toneState[1] += toneCoeff * (delayedR - toneState[1]);

            const float fbL = pingPong ? toneState[1] : toneState[0];
            const float fbR = pingPong ? toneState[0] : toneState[1];
            ring[0][writeIndex] = inL + fbL * feedback;
            if (channels > 1)
                ring[1][writeIndex] = inR + fbR * feedback;

            buffer.setSample (0, i, inL + wetMix * (toneState[0] - inL));
            if (channels > 1)
                buffer.setSample (1, i, inR + wetMix * (toneState[1] - inR));

            writeIndex = (writeIndex + 1) % size;
        }
    }

private:
    std::array<std::vector<float>, 2> ring;
    std::array<float, 2> toneState {};
    double sampleRate = 44100.0;
    size_t writeIndex = 0;
    juce::SmoothedValue<float> delayTimeSmooth, mixSmooth;
    float feedback = 0.35f, toneCoeff = 1.0f;
    bool pingPong = false;
};
