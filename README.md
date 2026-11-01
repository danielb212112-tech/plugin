# WaveForge – סינת' VST3 בהשראת Serum 2

פלאגין VST3 שנבנה ב-JUCE ועובד ב-FL Studio.

## שלב 1 (הנוכחי): סינת' בסיסי
- פוליפוניה של 8 קולות
- אוסצילטור: Sine / Saw / Square / Triangle (עם PolyBLEP נגד aliasing)
- מעטפת ADSR
- Gain
- ממשק גנרי אוטומטי (עיצוב אמיתי בשלב 7)

## תוכנית השלבים
1. סינת' בסיסי (זה)
2. פילטר (Low/High/Band-pass) עם Cutoff, Resonance ו-Envelope משלו
3. אוסצילטור Wavetable (טבלאות גלים + Position) ו-Unison/Detune
4. LFO-ים + מטריצת מודולציה (גרירה ממקור ליעד)
5. אפקטים: Delay, Reverb, Chorus, Distortion
6. אוסצילטור שני + Sub + Noise, ומיקסר
7. ממשק גרפי מותאם (גלים, ידיות, צבעים)
8. פריסטים, שמירה וטעינה, ליטוש

## איך מקבלים את הקובץ שנטען ב-FL Studio

### אפשרות א: בנייה אוטומטית בענן (הכי קל, בלי להתקין כלום)
1. צור ריפו חדש ב-GitHub והעלה אליו את כל תוכן התיקייה.
2. היכנס ללשונית **Actions**. הבנייה תרוץ לבד (כמה דקות).
3. בסיום, הורד מ-**Artifacts** את `WaveForge-VST3-Windows`.
4. פתח את ה-zip והעתק את התיקייה `WaveForge.vst3` אל:
   `C:\Program Files\Common Files\VST3\`
5. ב-FL Studio: Options → Manage plugins → **Find installed plugins**.
   הפלאגין יופיע ב-Installed → Generators (או Plugin database).

### אפשרות ב: בנייה מקומית ב-Windows
דרוש: Visual Studio 2022 (עם Desktop C++) ו-CMake.
```
cmake -B build
cmake --build build --config Release
```
התוצאה ב: `build\WaveForge_artefacts\Release\VST3\WaveForge.vst3`

## הערות
- בנייה ראשונה מורידה את JUCE אוטומטית, דרוש אינטרנט.
- ה-VST3 הזה הוא גנרטור, יש לו MIDI in ופלט סטריאו.
