#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <vector>
#include <cmath>

// Lightweight zero-latency distortion and stereo feedback delay used by WaveForge.
// All memory is allocated in prepare(); process() performs no allocations.
class DistortionFx
{
public:
    void prepare (double, int) {}
    int getLatencySamples() const noexcept { return 0; }

    void setParams (int newType, float driveDb, float newMix) noexcept
    {
        type = juce::jlimit (0, 3, newType);
        drive = juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 36.0f, driveDb));
        mix = juce::jlimit (0.0f, 1.0f, newMix);
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int channels = buffer.getNumChannels();
        const int samples = buffer.getNumSamples();
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < samples; ++i)
            {
                const float dry = data[i];
                const float x = dry * drive;
                float wet = x;

                switch (type)
                {
                    case 0: wet = std::tanh (x); break;                         // Soft clip
                    case 1: wet = juce::jlimit (-1.0f, 1.0f, x); break;         // Hard clip
                    case 2:
                    {
                        float folded = std::fmod (x + 1.0f, 4.0f);
                        if (folded < 0.0f) folded += 4.0f;
                        wet = folded < 2.0f ? folded - 1.0f : 3.0f - folded;
                        break;
                    }
                    case 3: wet = std::sin (x * juce::MathConstants<float>::pi * 0.5f); break;
                    default: break;
                }

                data[i] = dry + mix * (wet - dry);
            }
        }
    }

private:
    int type = 0;
    float drive = 1.0f, mix = 0.0f;
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
    }

    void reset() noexcept
    {
        for (auto& channel : ring)
            std::fill (channel.begin(), channel.end(), 0.0f);
        writeIndex = 0;
        toneState = {};
    }

    void setParams (float timeMs, float newFeedback, float toneHz,
                    bool newPingPong, float newMix) noexcept
    {
        const auto maxDelay = ring[0].empty() ? 1 : (int) ring[0].size() - 2;
        delaySamples = juce::jlimit (1, maxDelay,
            (int) std::round (sampleRate * juce::jlimit (1.0f, 2000.0f, timeMs) * 0.001));
        feedback = juce::jlimit (0.0f, 0.95f, newFeedback);
        pingPong = newPingPong;
        mix = juce::jlimit (0.0f, 1.0f, newMix);
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
            const size_t readIndex = (writeIndex + size - (size_t) delaySamples) % size;
            const float inL = buffer.getSample (0, i);
            const float inR = channels > 1 ? buffer.getSample (1, i) : inL;
            const float delayedL = ring[0][readIndex];
            const float delayedR = channels > 1 ? ring[1][readIndex] : delayedL;

            toneState[0] += toneCoeff * (delayedL - toneState[0]);
            toneState[1] += toneCoeff * (delayedR - toneState[1]);

            const float fbL = pingPong ? toneState[1] : toneState[0];
            const float fbR = pingPong ? toneState[0] : toneState[1];
            ring[0][writeIndex] = inL + fbL * feedback;
            if (channels > 1)
                ring[1][writeIndex] = inR + fbR * feedback;

            buffer.setSample (0, i, inL + mix * (toneState[0] - inL));
            if (channels > 1)
                buffer.setSample (1, i, inR + mix * (toneState[1] - inR));

            writeIndex = (writeIndex + 1) % size;
        }
    }

private:
    std::array<std::vector<float>, 2> ring;
    std::array<float, 2> toneState {};
    double sampleRate = 44100.0;
    size_t writeIndex = 0;
    int delaySamples = 1;
    float feedback = 0.35f, mix = 0.0f, toneCoeff = 1.0f;
    bool pingPong = false;
};
