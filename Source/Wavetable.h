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

    // קריאה עם אינטרפולציה לינארית. phase בטווח [0,1)
    float read (int frame, int level, float phase) const
    {
        const float* t = tables[(size_t) frame][(size_t) level].data();
        const float idx = phase * (float) tableSize;
        const int   i0  = (int) idx;
        const float f   = idx - (float) i0;
        const int   i   = i0 & (tableSize - 1);
        return t[i] + f * (t[i + 1] - t[i]);
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

                if (peak > 1.0e-6f)
                    for (int n = 0; n < tableSize; ++n)
                        t[(size_t) n] /= peak;

                t[(size_t) tableSize] = t[0]; // נקודת שמירה לאינטרפולציה
            }
        }
    }

    std::array<std::array<std::vector<float>, numLevels>, numFrames> tables;
};
