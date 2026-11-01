#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

// ---------------------------------------------------------------------------
// WavetableBank: 8 "פריימים" (צורות גל) שאפשר לעבור ביניהם ברצף (Position).
// כל פריים נבנה בכמה רמות Mip-map (מוגבלות רוחב-פס) כדי למנוע aliasing.
// הטבלאות נבנות פעם אחת בטעינת הפלאגין (בערך עשרות אלפיות שנייה).
// ---------------------------------------------------------------------------
class WavetableBank
{
public:
    static constexpr int tableSize    = 2048;
    static constexpr int numFrames    = 8;
    static constexpr int maxHarmonics = 1024;
    static constexpr int numLevels    = 11;   // רמה k שומרת הרמוניות עד (1024 >> k)

    WavetableBank() { build(); }

    static juce::String frameName (int i)
    {
        static const char* names[numFrames] = { "Sine", "Triangle", "Saw", "Square",
                                                "Pulse", "Organ", "Formant", "Bright" };
        return names[juce::jlimit (0, numFrames - 1, i)];
    }

    // בוחר את רמת ה-Mip המתאימה כך שההרמוניה הגבוהה ביותר לא עוברת את Nyquist
    static int levelForFrequency (double freq, double sampleRate)
    {
        const double maxHarm = sampleRate * 0.5 / juce::jmax (1.0, freq);
        int k = 0;
        while (k < numLevels - 1 && (double) (maxHarmonics >> k) > maxHarm)
            ++k;
        return k;
    }

    // Mip crossfades finish BEFORE a brighter level becomes unsafe at Nyquist.
    // This prevents stepping between spectral bands on pitch bends or vibrato.
    struct MipSelection
    {
        int finer = 0;
        int darker = 0;
        float blend = 0.0f;
    };

    static MipSelection selectMip (double frequency, double sampleRate) noexcept
    {
        if (sampleRate <= 0.0 || ! std::isfinite (frequency))
            return { numLevels - 1, numLevels - 1, 0.0f };

        const double f = juce::jmax (1.0, std::abs (frequency));
        const int safeLevel = levelForFrequency (f, sampleRate);
        if (safeLevel >= numLevels - 1)
            return { numLevels - 1, numLevels - 1, 0.0f };

        const int harmonics = maxHarmonics >> safeLevel;
        const double transitionEnd = sampleRate * 0.5 / (double) harmonics;
        const double transitionStart = transitionEnd * 0.75;
        if (f <= transitionStart)
            return { safeLevel, safeLevel, 0.0f };

        const float t = juce::jlimit (0.0f, 1.0f,
                                     (float) ((f - transitionStart) / (transitionEnd - transitionStart)));
        const float smooth = t * t * (3.0f - 2.0f * t);
        return { safeLevel, safeLevel + 1, smooth };
    }

    // Lookup with wrapped four-point cubic interpolation (smoother high notes
    // than linear interpolation, with no allocations on the audio thread).
    float read (int frame, int level, float phase) const noexcept
    {
        const float* t = tables[(size_t) frame][(size_t) level].data();
        const float wrapped = phase - std::floor (phase);
        const float index = wrapped * (float) tableSize;
        const int base = (int) index;
        const float x = index - (float) base;
        const int mask = tableSize - 1;
        const float y0 = t[(base - 1) & mask];
        const float y1 = t[base & mask];
        const float y2 = t[(base + 1) & mask];
        const float y3 = t[(base + 2) & mask];
        const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c = -0.5f * y0 + 0.5f * y2;
        return ((a * x + b) * x + c) * x + y1;
    }

    float readMorph (int frame, float frameBlend, const MipSelection& mip, float phase) const noexcept
    {
        const float first = read (frame, mip.finer, phase);
        const float frameFine = frameBlend > 0.00001f
            ? first + frameBlend * (read (frame + 1, mip.finer, phase) - first)
            : first;
        if (mip.blend <= 0.00001f)
            return frameFine;

        const float second = read (frame, mip.darker, phase);
        const float frameDark = frameBlend > 0.00001f
            ? second + frameBlend * (read (frame + 1, mip.darker, phase) - second)
            : second;
        return frameFine + mip.blend * (frameDark - frameFine);
    }

private:
    // משרעת והזזת פאזה (באינדקסים) של הרמוניה h בפריים נתון
    static void harmonic (int frame, int h, float& amp, int& shift)
    {
        amp = 0.0f;
        shift = 0;
        const float fh = (float) h;

        switch (frame)
        {
            case 0: // Sine
                amp = (h == 1) ? 1.0f : 0.0f;
                break;

            case 1: // Triangle
                if (h & 1)
                    amp = ((((h - 1) / 2) & 1) ? -1.0f : 1.0f) / (fh * fh);
                break;

            case 2: // Saw
                amp = 1.0f / fh;
                break;

            case 3: // Square
                if (h & 1)
                    amp = 1.0f / fh;
                break;

            case 4: // Pulse 25%
                amp   = 2.0f * std::sin (juce::MathConstants<float>::pi * fh * 0.25f) / fh;
                shift = -h * (tableSize / 8) + tableSize / 4;
                break;

            case 5: // Organ (הרמוניות 1,2,3,4,6,8)
            {
                static const float w[9] = { 0.0f, 1.0f, 0.8f, 0.6f, 0.45f, 0.0f, 0.3f, 0.0f, 0.2f };
                if (h < 9)
                    amp = w[h];
                break;
            }

            case 6: // Formant (שתי "גבעות" בספקטרום)
                amp = std::exp (-0.25f * (fh - 5.0f) * (fh - 5.0f))
                    + 0.7f * std::exp (-0.125f * (fh - 12.0f) * (fh - 12.0f));
                if (amp < 1.0e-4f)
                    amp = 0.0f;
                break;

            case 7: // Bright
                amp = 1.0f / std::pow (fh, 0.7f);
                break;

            default:
                break;
        }
    }

    void build()
    {
        constexpr int mask = tableSize - 1;

        std::array<float, tableSize> sinT {};
        for (int n = 0; n < tableSize; ++n)
            sinT[(size_t) n] = std::sin (juce::MathConstants<float>::twoPi * (float) n / (float) tableSize);

        for (int f = 0; f < numFrames; ++f)
        {
            // Preserve the spectral balance: ALL mip levels of a frame use
            // the same peak normalization, measured at the fullest level.
            float referencePeak = 1.0f;
            for (int k = 0; k < numLevels; ++k)
            {
                auto& t = tables[(size_t) f][(size_t) k];
                t.assign ((size_t) tableSize + 1, 0.0f);

                const int maxH = maxHarmonics >> k;
                for (int h = 1; h <= maxH; ++h)
                {
                    float amp = 0.0f;
                    int shift = 0;
                    harmonic (f, h, amp, shift);
                    if (amp == 0.0f)
                        continue;

                    for (int n = 0; n < tableSize; ++n)
                        t[(size_t) n] += amp * sinT[(size_t) ((h * n + shift) & mask)];
                }

                float peak = 0.0f;
                for (int n = 0; n < tableSize; ++n)
                    peak = juce::jmax (peak, std::abs (t[(size_t) n]));

                if (k == 0)
                    referencePeak = juce::jmax (1.0e-6f, peak);

                for (int n = 0; n < tableSize; ++n)
                    t[(size_t) n] /= referencePeak;

                t[(size_t) tableSize] = t[0]; // נקודת שמירה לאינטרפולציה
            }
        }
    }

    std::array<std::array<std::vector<float>, numLevels>, numFrames> tables;
};
