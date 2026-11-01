#include "PluginEditor.h"

namespace
{
    const juce::Colour bg        (0xff090e17);
    const juce::Colour panel     (0xff121b29);
    const juce::Colour panelHi   (0xff182538);
    const juce::Colour accent    (0xff31dfc2);
    const juce::Colour textMain  (0xffedf4ff);
    const juce::Colour textMuted (0xff8395ad);
}

WaveForgeLookAndFeel::WaveForgeLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::TabbedComponent::backgroundColourId, bg);
    setColour (juce::TabbedButtonBar::tabTextColourId, textMuted);
    setColour (juce::TabbedButtonBar::frontTextColourId, textMain);
    setColour (juce::ComboBox::textColourId, textMain);
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2a3b50));
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, textMain);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, textMain);
}

void WaveForgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float startAngle, float endAngle,
                                              juce::Slider& slider)
{
    const float side = (float) juce::jmin (width, height);
    const float radius = side * 0.37f;
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.46f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff0a111d));
    g.fillEllipse (cx - radius - 5.0f, cy - radius - 5.0f,
                   (radius + 5.0f) * 2.0f, (radius + 5.0f) * 2.0f);

    juce::Path track;
    track.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a3b50));
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle,
                         juce::jmax (startAngle + 0.001f, angle), true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0xff1b293b));
    g.fillEllipse (cx - radius + 5.0f, cy - radius + 5.0f,
                   (radius - 5.0f) * 2.0f, (radius - 5.0f) * 2.0f);

    juce::Path pointer;
    const float pointerLength = radius * 0.66f;
    pointer.startNewSubPath (cx + std::cos (angle) * (radius * 0.28f),
                             cy + std::sin (angle) * (radius * 0.28f));
    pointer.lineTo (cx + std::cos (angle) * pointerLength,
                    cy + std::sin (angle) * pointerLength);
    g.setColour (textMain);
    g.strokePath (pointer, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::ignoreUnused (slider);
}

void WaveForgeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                          int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (panelHi);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (juce::Colour (0xff30445d));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

    juce::Path arrow;
    const float cx = (float) width - 17.0f;
    const float cy = (float) height * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (accent);
    g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    juce::ignoreUnused (box);
}

void WaveForgeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                   const juce::Colour&,
                                                   bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = button.getToggleState() ? accent.withAlpha (0.22f) : panelHi;
    if (highlighted) colour = colour.brighter (0.12f);
    if (down) colour = colour.darker (0.12f);
    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (button.getToggleState() ? accent : juce::Colour (0xff30445d));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
}

WaveForgeWaveDisplay::WaveForgeWaveDisplay (WaveForgeProcessor& p, const juce::String& parameterID,
                                             const juce::String& name)
    : processor (p), position (p.apvts.getRawParameterValue (parameterID)),
      positionParameter (p.apvts.getParameter (parameterID)), displayName (name)
{
    startTimerHz (24);
}

void WaveForgeWaveDisplay::timerCallback()
{
    repaint();
}

void WaveForgeWaveDisplay::mouseDown (const juce::MouseEvent& event)
{
    if (positionParameter == nullptr) return;
    dragging = true;
    positionParameter->beginChangeGesture();
    mouseDrag (event);
}

void WaveForgeWaveDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging || positionParameter == nullptr) return;
    const auto plot = getLocalBounds().toFloat().reduced (14.0f, 12.0f);
    const float fraction = juce::jlimit (0.0f, 1.0f,
        (event.position.x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
    positionParameter->setValueNotifyingHost (fraction);
    repaint();
}

void WaveForgeWaveDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging && positionParameter != nullptr) positionParameter->endChangeGesture();
    dragging = false;
}

float WaveForgeWaveDisplay::sampleFrame (int frame, float phase) noexcept
{
    const float p = phase - std::floor (phase);
    const float sine = std::sin (juce::MathConstants<float>::twoPi * p);
    switch (frame)
    {
        case 0: return sine;
        case 1: return std::asin (sine) * (2.0f / juce::MathConstants<float>::pi);
        case 2: return 2.0f * p - 1.0f;
        case 3: return p < 0.5f ? 1.0f : -1.0f;
        case 4: return p < 0.25f ? 1.0f : -1.0f;
        case 5: return 0.58f * std::sin (juce::MathConstants<float>::twoPi * p)
                      + 0.28f * std::sin (juce::MathConstants<float>::twoPi * 2.0f * p)
                      + 0.14f * std::sin (juce::MathConstants<float>::twoPi * 4.0f * p);
        case 6: return 0.62f * std::sin (juce::MathConstants<float>::twoPi * p)
                      + 0.38f * std::sin (juce::MathConstants<float>::twoPi * 5.0f * p);
        default: return 0.55f * sine + 0.30f * std::sin (juce::MathConstants<float>::twoPi * 3.0f * p)
                            + 0.15f * std::sin (juce::MathConstants<float>::twoPi * 7.0f * p);
    }
}

void WaveForgeWaveDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c1522));
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (juce::Colour (0xff263950));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto plot = bounds.reduced (14.0f, 12.0f);
    plot.removeFromTop (18.0f);
    g.setColour (juce::Colour (0xff203249));
    for (int i = 1; i < 4; ++i)
    {
        const float yy = plot.getY() + plot.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) yy, plot.getX(), plot.getRight());
    }
    for (int i = 1; i < 8; ++i)
    {
        const float xx = plot.getX() + plot.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) xx, plot.getY(), plot.getBottom());
    }

    const float pos = position != nullptr ? juce::jlimit (0.0f, 1.0f, position->load()) : 0.0f;
    const float framePos = pos * 7.0f;
    const int frameA = juce::jlimit (0, 7, (int) framePos);
    const int frameB = juce::jmin (7, frameA + 1);
    const float blend = framePos - (float) frameA;

    juce::Path wave;
    constexpr int points = 512;
    for (int i = 0; i < points; ++i)
    {
        const float phase = (float) i / (float) (points - 1);
        const float a = sampleFrame (frameA, phase);
        const float b = sampleFrame (frameB, phase);
        const float value = a + (b - a) * blend;
        const float px = plot.getX() + phase * plot.getWidth();
        const float py = plot.getCentreY() - value * plot.getHeight() * 0.39f;
        if (i == 0) wave.startNewSubPath (px, py);
        else wave.lineTo (px, py);
    }

    g.setColour (accent.withAlpha (0.12f));
    juce::Path glow = wave;
    glow.applyTransform (juce::AffineTransform::translation (0.0f, 1.0f));
    g.strokePath (glow, juce::PathStrokeType (7.0f));
    g.setColour (accent);
    g.strokePath (wave, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    const auto nameA = WavetableBank::frameName (frameA);
    const auto nameB = WavetableBank::frameName (frameB);
    g.setColour (textMain);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText (displayName + "  •  " + (blend < 0.03f ? nameA : nameA + "  →  " + nameB),
                plot.getX(), bounds.getY() + 5.0f, plot.getWidth(), 15.0f,
                juce::Justification::centredLeft);
    g.setColour (textMuted);
    g.setFont (juce::Font (9.0f));
    g.drawText ("DRAG TO SCAN", plot.getRight() - 100.0f, bounds.getY() + 5.0f,
                100.0f, 15.0f, juce::Justification::centredRight);
}

WaveForgeUnisonDisplay::WaveForgeUnisonDisplay (WaveForgeProcessor& p)
    : voices (p.apvts.getRawParameterValue ("uniVoices")),
      detune (p.apvts.getRawParameterValue ("uniDetune")),
      spread (p.apvts.getRawParameterValue ("uniSpread"))
{
    startTimerHz (20);
}

void WaveForgeUnisonDisplay::timerCallback()
{
    repaint();
}

void WaveForgeUnisonDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c1522));
    g.fillRoundedRectangle (area, 9.0f);
    g.setColour (juce::Colour (0xff263950));
    g.drawRoundedRectangle (area.reduced (0.5f), 9.0f, 1.0f);

    const int count = voices != nullptr ? juce::jlimit (1, 8, (int) std::round (voices->load())) : 1;
    const float width = spread != nullptr ? juce::jlimit (0.0f, 1.0f, spread->load()) : 0.0f;
    const float cents = detune != nullptr ? juce::jlimit (0.0f, 100.0f, detune->load()) : 0.0f;

    g.setColour (textMuted);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawText ("UNISON VOICES", 12, 5, 130, 16, juce::Justification::centredLeft);
    g.drawText (juce::String (count) + " VOICES   •   " + juce::String (cents, 1)
                + " CENTS   •   " + juce::String (juce::roundToInt (width * 100.0f))
                + "% WIDTH",
                145, 5, juce::jmax (0, getWidth() - 157), 16, juce::Justification::centredRight);

    auto plot = area.reduced (18.0f, 25.0f);
    const float mid = plot.getCentreX();
    const float spreadPx = plot.getWidth() * 0.44f * width;
    const float centerY = plot.getCentreY();

    g.setColour (juce::Colour (0xff25374c));
    g.drawHorizontalLine ((int) centerY, plot.getX(), plot.getRight());
    g.setColour (accent.withAlpha (0.18f));
    g.drawVerticalLine ((int) mid, plot.getY(), plot.getBottom());

    for (int i = 0; i < count; ++i)
    {
        const float normalized = count == 1 ? 0.0f : ((float) i / (float) (count - 1)) * 2.0f - 1.0f;
        const float x = mid + normalized * spreadPx;
        const float detuneOffset = normalized * cents * 0.22f;
        const float y = centerY + detuneOffset;
        g.setColour (accent.withAlpha (0.28f));
        g.drawLine (x, plot.getY() + 2.0f, x, plot.getBottom() - 2.0f, 2.0f);
        g.setColour (accent);
        g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
    }
}

WaveForgeEnvelopeEditor::WaveForgeEnvelopeEditor (WaveForgeProcessor& p,
                                                                         const juce::String& prefix,
                                                                         const juce::String& name)
    : processor (p), title (name)
{
    attack  = p.apvts.getParameter (prefix == "amp" ? "attack" : "fAttack");
    decay   = p.apvts.getParameter (prefix == "amp" ? "decay" : "fDecay");
    sustain = p.apvts.getParameter (prefix == "amp" ? "sustain" : "fSustain");
    release = p.apvts.getParameter (prefix == "amp" ? "release" : "fRelease");
    startTimerHz (20);
}

WaveForgeEnvelopeEditor::~WaveForgeEnvelopeEditor()
{
    if (activeParameter != nullptr)
        activeParameter->endChangeGesture();
}

juce::Point<float> WaveForgeEnvelopeEditor::getHandlePosition (Handle handle) const
{
    auto bounds = getLocalBounds().toFloat().reduced (18.0f, 28.0f);
    const auto norm = [] (juce::RangedAudioParameter* p)
    {
        return p != nullptr ? p->getValue() : 0.0f;
    };
    const float a = norm (attack);
    const float d = norm (decay);
    const float s = norm (sustain);
    const float r = norm (release);
    const float peakX = bounds.getX() + bounds.getWidth() * (0.12f + 0.20f * a);
    const float sustainX = bounds.getX() + bounds.getWidth() * (0.43f + 0.17f * d);
    const float sustainY = bounds.getBottom() - bounds.getHeight() * s;
    const float releaseX = bounds.getX() + bounds.getWidth() * (0.66f + 0.30f * r);

    switch (handle)
    {
        case Handle::attack:  return { peakX, bounds.getY() };
        case Handle::decay:   return { (peakX + sustainX) * 0.5f, (bounds.getY() + sustainY) * 0.5f };
        case Handle::sustain: return { sustainX, sustainY };
        case Handle::release: return { releaseX, bounds.getBottom() };
        default:              return {};
    }
}

void WaveForgeEnvelopeEditor::updateParameter (juce::RangedAudioParameter* parameter, float normalized)
{
    if (parameter == nullptr)
        return;
    parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
}

void WaveForgeEnvelopeEditor::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c1522));
    g.fillRoundedRectangle (area, 9.0f);
    g.setColour (juce::Colour (0xff263950));
    g.drawRoundedRectangle (area.reduced (0.5f), 9.0f, 1.0f);

    g.setColour (textMain);
    g.setFont (juce::Font (10.5f, juce::Font::bold));
    g.drawText (title.toUpperCase() + "  •  DRAG POINTS TO EDIT", 12, 5,
                getWidth() - 24, 16, juce::Justification::centredLeft);

    auto plot = area.reduced (18.0f, 28.0f);
    g.setColour (juce::Colour (0xff203249));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 4.0f),
                              plot.getX(), plot.getRight());

    const auto start = juce::Point<float> (plot.getX(), plot.getBottom());
    const auto a = getHandlePosition (Handle::attack);
    const auto d = getHandlePosition (Handle::decay);
    const auto s = getHandlePosition (Handle::sustain);
    const auto r = getHandlePosition (Handle::release);

    juce::Path envelope;
    envelope.startNewSubPath (start);
    envelope.lineTo (a);
    envelope.lineTo (s);
    envelope.lineTo (r);
    g.setColour (accent.withAlpha (0.13f));
    auto fill = envelope;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (envelope, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    const juce::Point<float> handles[] { a, d, s, r };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (i == (int) activeHandle - 1 ? textMain : accent);
        g.fillEllipse (handles[i].x - 4.5f, handles[i].y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colour (0xff07111c));
        g.drawEllipse (handles[i].x - 4.5f, handles[i].y - 4.5f, 9.0f, 9.0f, 1.0f);
    }
}

void WaveForgeEnvelopeEditor::mouseDown (const juce::MouseEvent& event)
{
    const juce::Point<float> point = event.position;
    const Handle candidates[] { Handle::attack, Handle::decay, Handle::sustain, Handle::release };
    juce::RangedAudioParameter* parameters[] { attack, decay, sustain, release };
    float bestDistance = 18.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float distance = getHandlePosition (candidates[i]).getDistanceFrom (point);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            activeHandle = candidates[i];
            activeParameter = parameters[i];
        }
    }

    if (activeParameter != nullptr)
    {
        activeParameter->beginChangeGesture();
        mouseDrag (event);
    }
}

void WaveForgeEnvelopeEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (activeParameter == nullptr)
        return;

    auto plot = getLocalBounds().toFloat().reduced (18.0f, 28.0f);
    const float x = juce::jlimit (plot.getX(), plot.getRight(), event.position.x);
    const float y = juce::jlimit (plot.getY(), plot.getBottom(), event.position.y);
    float normalized = 0.0f;

    switch (activeHandle)
    {
        case Handle::attack:
            normalized = (x - plot.getX()) / plot.getWidth();
            normalized = (normalized - 0.12f) / 0.20f;
            break;
        case Handle::decay:
        {
            const float a = attack != nullptr ? attack->getValue() : 0.0f;
            const float peakX = 0.12f + 0.20f * a;
            const float handleX = (x - plot.getX()) / plot.getWidth();
            normalized = (2.0f * handleX - peakX - 0.43f) / 0.17f;
            break;
        }
        case Handle::sustain:
            normalized = 1.0f - (y - plot.getY()) / plot.getHeight();
            break;
        case Handle::release:
            normalized = (x - plot.getX()) / plot.getWidth();
            normalized = (normalized - 0.66f) / 0.30f;
            break;
        default:
            return;
    }

    updateParameter (activeParameter, normalized);
    repaint();
}

void WaveForgeEnvelopeEditor::mouseUp (const juce::MouseEvent&)
{
    if (activeParameter != nullptr)
        activeParameter->endChangeGesture();
    activeParameter = nullptr;
    activeHandle = Handle::none;
    repaint();
}


WaveForgeFilterDisplay::WaveForgeFilterDisplay (WaveForgeProcessor& p)
    : type (p.apvts.getRawParameterValue ("fType")),
      cutoff (p.apvts.getRawParameterValue ("cutoff")),
      resonance (p.apvts.getRawParameterValue ("resonance"))
{
    startTimerHz (24);
}

void WaveForgeFilterDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c1522));
    g.fillRoundedRectangle (area, 9.0f);
    g.setColour (juce::Colour (0xff263950));
    g.drawRoundedRectangle (area.reduced (0.5f), 9.0f, 1.0f);
    g.setColour (textMain);
    g.setFont (juce::Font (10.5f, juce::Font::bold));
    g.drawText ("FILTER RESPONSE  /  APPROXIMATE PREVIEW", 13, 6, getWidth() - 26, 17,
                juce::Justification::centredLeft);
    auto plot = area.reduced (24.0f, 30.0f);
    g.setColour (juce::Colour (0xff203249));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (plot.getY() + i * plot.getHeight() / 4.0f),
                              plot.getX(), plot.getRight());
    for (int i = 1; i < 6; ++i)
        g.drawVerticalLine ((int) (plot.getX() + i * plot.getWidth() / 6.0f),
                            plot.getY(), plot.getBottom());
    const float freq = cutoff != nullptr ? juce::jlimit (20.0f, 20000.0f, cutoff->load()) : 3000.0f;
    const float res = resonance != nullptr ? juce::jlimit (0.0f, 1.0f, resonance->load()) : 0.2f;
    const int mode = type != nullptr ? (int) type->load() : 0;
    const float f0 = std::log (freq / 20.0f) / std::log (1000.0f);
    juce::Path curve;
    for (int i = 0; i <= 240; ++i)
    {
        const float x = (float) i / 240.0f;
        const float diff = (x - f0) * 13.0f;
        const float low = 1.0f / std::sqrt (1.0f + std::exp (juce::jlimit (-30.0f, 30.0f, 2.0f * diff)));
        const float high = 1.0f / std::sqrt (1.0f + std::exp (juce::jlimit (-30.0f, 30.0f, -2.0f * diff)));
        const float peak = res * 0.45f * std::exp (-diff * diff * 1.2f);
        const float band = std::exp (-diff * diff * 0.28f) * (0.55f + res * 0.4f);
        const float magnitude = juce::jlimit (0.0f, 1.0f, (mode == 0 ? low : mode == 1 ? band : high) + (mode == 1 ? 0.0f : peak));
        const float px = plot.getX() + x * plot.getWidth();
        const float py = plot.getBottom() - magnitude * plot.getHeight() * 0.89f;
        if (i == 0) curve.startNewSubPath (px, py);
        else curve.lineTo (px, py);
    }
    g.setColour (accent.withAlpha (0.2f));
    g.strokePath (curve, juce::PathStrokeType (6.0f));
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.4f));
    const float cx = plot.getX() + f0 * plot.getWidth();
    g.setColour (textMuted.withAlpha (0.7f));
    g.drawVerticalLine ((int) cx, plot.getY(), plot.getBottom());
    g.setFont (juce::Font (10.0f));
    g.drawText (juce::String (juce::roundToInt (freq)) + " Hz", (int) cx - 35,
                (int) plot.getBottom() - 16, 70, 16, juce::Justification::centred);
}

WaveForgeLfoDisplay::WaveForgeLfoDisplay (WaveForgeProcessor& p, int index)
    : number (index),
      shape (p.apvts.getRawParameterValue ("lfo" + juce::String (index) + "Shape")),
      rate (p.apvts.getRawParameterValue ("lfo" + juce::String (index) + "Rate"))
{
    startTimerHz (20);
}

void WaveForgeLfoDisplay::paint (juce::Graphics& g)
{
    auto rect = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c1522));
    g.fillRoundedRectangle (rect, 9.0f);
    g.setColour (juce::Colour (0xff263950));
    g.drawRoundedRectangle (rect.reduced (0.5f), 9.0f, 1.0f);
    const int waveform = shape != nullptr ? (int) shape->load() : 0;
    const float hz = rate != nullptr ? rate->load() : 2.0f;
    g.setColour (textMain);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("LFO " + juce::String (number), 12, 7, 88, 17, juce::Justification::centredLeft);
    g.setColour (textMuted);
    g.drawText (juce::String (hz, 2) + " Hz", getWidth() - 102, 7, 90, 17,
                juce::Justification::centredRight);
    auto plot = rect.reduced (15.0f, 30.0f);
    g.setColour (juce::Colour (0xff203249));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    juce::Path wave;
    for (int i = 0; i <= 240; ++i)
    {
        const float phase = (float) i / 240.0f * 2.0f;
        const float t = phase - std::floor (phase);
        float v = 0.0f;
        switch (waveform)
        {
            case 0: v = std::sin (juce::MathConstants<float>::twoPi * t); break;
            case 1: v = 1.0f - 4.0f * std::abs (t - 0.5f); break;
            case 2: v = 2.0f * t - 1.0f; break;
            case 3: v = 1.0f - 2.0f * t; break;
            case 4: v = t < 0.5f ? 1.0f : -1.0f; break;
            default:
            {
                const int step = (int) std::floor (phase * 8.0f);
                v = std::sin ((float) (step * 127 + 31) * 9.72f);
                break;
            }
        }
        const float xx = plot.getX() + plot.getWidth() * (float) i / 240.0f;
        const float yy = plot.getCentreY() - v * plot.getHeight() * 0.42f;
        if (i == 0) wave.startNewSubPath (xx, yy);
        else wave.lineTo (xx, yy);
    }
    g.setColour (accent.withAlpha (0.17f));
    g.strokePath (wave, juce::PathStrokeType (6.0f));
    g.setColour (accent);
    g.strokePath (wave, juce::PathStrokeType (2.0f));
}

WaveForgeModulationPage::WaveForgeModulationPage (WaveForgeProcessor& p)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);

    for (int i = 0; i < 2; ++i)
    {
        const juce::String prefix = "lfo" + juce::String (i + 1);
        lfoDisplays[(size_t) i] = std::make_unique<WaveForgeLfoDisplay> (p, i + 1);
        content.addAndMakeVisible (*lfoDisplays[(size_t) i]);
        auto shapeBox = std::make_unique<juce::ComboBox>();
        shapeBox->addItemList (p.apvts.getParameter (prefix + "Shape")->getAllValueStrings(), 1);
        shapeBox->setLookAndFeel (&lookAndFeel);
        comboAttachments.push_back (std::make_unique<ComboAttachment> (p.apvts, prefix + "Shape", *shapeBox));
        content.addAndMakeVisible (*shapeBox);
        shape[(size_t) i] = std::move (shapeBox);
        for (int j = 0; j < 2; ++j)
        {
            const juce::String id = prefix + (j == 0 ? "Rate" : "Fade");
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 75, 19);
            slider->setLookAndFeel (&lookAndFeel);
            slider->setColour (juce::Slider::textBoxTextColourId, textMain);
            slider->setColour (juce::Slider::trackColourId, accent);
            sliderAttachments.push_back (std::make_unique<SliderAttachment> (p.apvts, id, *slider));
            content.addAndMakeVisible (*slider);
            rateFade[(size_t) (i * 2 + j)] = std::move (slider);
        }
        auto button = std::make_unique<juce::ToggleButton> ("KEY SYNC");
        button->setColour (juce::ToggleButton::textColourId, textMain);
        buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            p.apvts, prefix + "Sync", *button));
        content.addAndMakeVisible (*button);
        sync[(size_t) i] = std::move (button);
        auto label = std::make_unique<juce::Label>();
        label->setText ("RATE                                          FADE IN", juce::dontSendNotification);
        label->setColour (juce::Label::textColourId, textMuted);
        label->setFont (juce::Font (9.0f, juce::Font::bold));
        content.addAndMakeVisible (*label);
        lfoLabels[(size_t) i] = std::move (label);
    }

    matrixTitle = std::make_unique<juce::Label>();
    matrixTitle->setText ("MODULATION MATRIX", juce::dontSendNotification);
    matrixTitle->setFont (juce::Font (15.0f, juce::Font::bold));
    matrixTitle->setColour (juce::Label::textColourId, textMain);
    content.addAndMakeVisible (*matrixTitle);
    matrixHint = std::make_unique<juce::Label>();
    matrixHint->setText ("SOURCE  /  DESTINATION  /  BIPOLAR AMOUNT", juce::dontSendNotification);
    matrixHint->setFont (juce::Font (10.0f, juce::Font::bold));
    matrixHint->setColour (juce::Label::textColourId, textMuted);
    content.addAndMakeVisible (*matrixHint);

    for (int i = 0; i < 4; ++i)
    {
        const juce::String pre = "mod" + juce::String (i + 1);
        auto label = std::make_unique<juce::Label>();
        label->setText ("ROUTE " + juce::String (i + 1), juce::dontSendNotification);
        label->setColour (juce::Label::textColourId, textMain);
        label->setFont (juce::Font (11.0f, juce::Font::bold));
        content.addAndMakeVisible (*label);
        routeLabels[(size_t) i] = std::move (label);

        auto src = std::make_unique<juce::ComboBox>();
        src->addItemList (p.apvts.getParameter (pre + "Src")->getAllValueStrings(), 1);
        src->setLookAndFeel (&lookAndFeel);
        comboAttachments.push_back (std::make_unique<ComboAttachment> (p.apvts, pre + "Src", *src));
        content.addAndMakeVisible (*src);
        source[(size_t) i] = std::move (src);

        auto dst = std::make_unique<juce::ComboBox>();
        dst->addItemList (p.apvts.getParameter (pre + "Dst")->getAllValueStrings(), 1);
        dst->setLookAndFeel (&lookAndFeel);
        comboAttachments.push_back (std::make_unique<ComboAttachment> (p.apvts, pre + "Dst", *dst));
        content.addAndMakeVisible (*dst);
        target[(size_t) i] = std::move (dst);

        auto amt = std::make_unique<juce::Slider>();
        amt->setSliderStyle (juce::Slider::LinearHorizontal);
        amt->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        amt->setLookAndFeel (&lookAndFeel);
        amt->setColour (juce::Slider::trackColourId, accent);
        amt->setColour (juce::Slider::textBoxTextColourId, textMain);
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (p.apvts, pre + "Amt", *amt));
        content.addAndMakeVisible (*amt);
        amount[(size_t) i] = std::move (amt);
    }
}

WaveForgeModulationPage::~WaveForgeModulationPage()
{
    for (auto& item : shape) if (item != nullptr) item->setLookAndFeel (nullptr);
    for (auto& item : rateFade) if (item != nullptr) item->setLookAndFeel (nullptr);
    for (auto& item : source) if (item != nullptr) item->setLookAndFeel (nullptr);
    for (auto& item : target) if (item != nullptr) item->setLookAndFeel (nullptr);
    for (auto& item : amount) if (item != nullptr) item->setLookAndFeel (nullptr);
}

void WaveForgeModulationPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111824));
}

void WaveForgeModulationPage::resized()
{
    viewport.setBounds (getLocalBounds());
    const int width = juce::jmax (680, viewport.getWidth() - 12);
    content.setSize (width, juce::jmax (viewport.getHeight(), 694));
    const int half = (width - 44) / 2;
    for (int i = 0; i < 2; ++i)
    {
        const int x = 16 + i * (half + 12);
        lfoDisplays[(size_t) i]->setBounds (x, 12, half, 158);
        shape[(size_t) i]->setBounds (x + 10, 178, half / 2 - 17, 30);
        sync[(size_t) i]->setBounds (x + half / 2 + 5, 179, half / 2 - 15, 28);
        lfoLabels[(size_t) i]->setBounds (x + 10, 216, half - 20, 16);
        rateFade[(size_t) (i * 2)]->setBounds (x + 8, 237, half - 16, 27);
        rateFade[(size_t) (i * 2 + 1)]->setBounds (x + 8, 268, half - 16, 27);
    }
    matrixTitle->setBounds (22, 312, 260, 24);
    matrixHint->setBounds (290, 313, width - 312, 22);
    content.cards.clear();
    for (int i = 0; i < 4; ++i)
    {
        const int y = 356 + i * 76;
        content.cards.push_back (juce::Rectangle<int> (16, y - 7, width - 32, 48));
        routeLabels[(size_t) i]->setBounds (25, y, 95, 32);
        source[(size_t) i]->setBounds (125, y, (width - 170) / 3, 32);
        target[(size_t) i]->setBounds (137 + (width - 170) / 3, y, (width - 170) / 3, 32);
        amount[(size_t) i]->setBounds (149 + 2 * (width - 170) / 3, y, (width - 170) / 3 - 12, 32);
    }
    content.repaint();
}

WaveForgeLevelMeter::WaveForgeLevelMeter (WaveForgeProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void WaveForgeLevelMeter::timerCallback()
{
    smoothLeft = juce::jmax (processor.outputPeakLeft.exchange (0.0f, std::memory_order_relaxed), smoothLeft * 0.88f);
    smoothRight = juce::jmax (processor.outputPeakRight.exchange (0.0f, std::memory_order_relaxed), smoothRight * 0.88f);
    repaint();
}

void WaveForgeLevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c1522));
    g.fillRoundedRectangle (area, 9.0f);
    g.setColour (juce::Colour (0xff263950));
    g.drawRoundedRectangle (area.reduced (0.5f), 9.0f, 1.0f);
    g.setColour (textMain);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawText ("STEREO OUTPUT  /  PEAK", 18, 8, getWidth() - 36, 20, juce::Justification::centredLeft);
    const float levels[] { smoothLeft, smoothRight };
    const char* labels[] { "L", "R" };
    for (int ch = 0; ch < 2; ++ch)
    {
        const int y = 45 + ch * 47;
        g.setColour (textMuted);
        g.drawText (labels[ch], 17, y, 26, 24, juce::Justification::centred);
        auto bar = juce::Rectangle<float> (49.0f, (float) y + 3.0f, area.getWidth() - 150.0f, 18.0f);
        g.setColour (juce::Colour (0xff223247));
        g.fillRoundedRectangle (bar, 4.0f);
        const float value = juce::jlimit (0.0f, 1.0f, levels[ch]);
        const float db = juce::Decibels::gainToDecibels (value, -60.0f);
        const float fraction = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto filled = bar.withWidth (bar.getWidth() * fraction);
        g.setColour (value >= 1.0f ? juce::Colour (0xffff6d77) : accent);
        if (filled.getWidth() > 0.0f) g.fillRoundedRectangle (filled, 4.0f);
        g.setColour (textMain);
        g.drawText (juce::String (db, 1) + " dB", (int) bar.getRight() + 8, y, 85, 24,
                    juce::Justification::centredRight);
    }
}

void WaveForgePageContent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111824));
    for (const auto& card : cards)
    {
        g.setColour (juce::Colour (0xff172334));
        g.fillRoundedRectangle (card.toFloat(), 9.0f);
        g.setColour (juce::Colour (0xff26384e));
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 9.0f, 1.0f);
    }
}

WaveForgeParameterPage::WaveForgeParameterPage (
    WaveForgeProcessor& processor,
    std::initializer_list<std::pair<const char*, const char*>> parameters,
    bool showWaveform, const juce::String& envelopePrefix, bool showOutputMeter)
    : audioProcessor (processor)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);

    if (showWaveform)
    {
        waveform = std::make_unique<WaveForgeWaveDisplay> (processor, "wtPos", "OSCILLATOR 1");
        waveform2 = std::make_unique<WaveForgeWaveDisplay> (processor, "osc2Pos", "OSCILLATOR 2");
        content.addAndMakeVisible (*waveform);
        content.addAndMakeVisible (*waveform2);
        unisonDisplay = std::make_unique<WaveForgeUnisonDisplay> (processor);
        content.addAndMakeVisible (*unisonDisplay);
    }

    if (envelopePrefix == "filter")
    {
        filterDisplay = std::make_unique<WaveForgeFilterDisplay> (processor);
        content.addAndMakeVisible (*filterDisplay);
    }
    if (showOutputMeter)
    {
        levelMeter = std::make_unique<WaveForgeLevelMeter> (processor);
        content.addAndMakeVisible (*levelMeter);
    }

    if (envelopePrefix.isNotEmpty())
    {
        envelopeEditor = std::make_unique<WaveForgeEnvelopeEditor> (processor, envelopePrefix,
                                  envelopePrefix == "amp" ? "AMP ENVELOPE" : "FILTER ENVELOPE");
        content.addAndMakeVisible (*envelopeEditor);
    }

    for (const auto& item : parameters)
    {
        const juce::String id (item.first);
        const juce::String name (item.second);
        auto* parameter = audioProcessor.apvts.getParameter (id);
        if (parameter == nullptr)
            continue;

        auto label = std::make_unique<juce::Label>();
        label->setText (name, juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        label->setFont (juce::Font (10.5f, juce::Font::bold));
        label->setColour (juce::Label::textColourId, textMuted);
        content.addAndMakeVisible (*label);
        labels.push_back (std::move (label));

        if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
        {
            auto button = std::make_unique<juce::ToggleButton>();
            button->setButtonText (name);
            button->setLookAndFeel (&lookAndFeel);
            button->setColour (juce::ToggleButton::textColourId, textMain);
            content.addAndMakeVisible (*button);
            buttonAttachments.push_back (std::make_unique<ButtonAttachment> (audioProcessor.apvts, id, *button));
            controls.push_back (std::move (button));
        }
        else if (dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr)
        {
            auto combo = std::make_unique<juce::ComboBox>();
            combo->addItemList (parameter->getAllValueStrings(), 1);
            combo->setLookAndFeel (&lookAndFeel);
            content.addAndMakeVisible (*combo);
            comboAttachments.push_back (std::make_unique<ComboAttachment> (audioProcessor.apvts, id, *combo));
            controls.push_back (std::move (combo));
        }
        else
        {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
            slider->setLookAndFeel (&lookAndFeel);
            slider->setColour (juce::Slider::textBoxTextColourId, textMain);
            slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0b1420));
            slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff26384e));
            content.addAndMakeVisible (*slider);
            sliderAttachments.push_back (std::make_unique<SliderAttachment> (audioProcessor.apvts, id, *slider));
            controls.push_back (std::move (slider));
        }
    }
}

void WaveForgeParameterPage::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111824));
}

void WaveForgeParameterPage::resized()
{
    viewport.setBounds (getLocalBounds());
    const int width = juce::jmax (1, viewport.getWidth() - 12);
    constexpr int columns = 4;
    constexpr int cellHeight = 126;
    constexpr int gap = 8;
    const int cellWidth = (width - 32 - gap * (columns - 1)) / columns;
    const int top = waveform != nullptr ? 394 : (filterDisplay != nullptr ? 354 : (levelMeter != nullptr ? 166 : (envelopeEditor != nullptr ? 166 : 14)));
    const int rows = (int) (controls.size() + (size_t) columns - 1) / columns;
    const int totalHeight = top + rows * (cellHeight + gap) + 14;
    content.setSize (width, juce::jmax (viewport.getHeight(), totalHeight));
    content.cards.clear();

    if (waveform != nullptr)
    {
        const int displayGap = 12;
        const int displayWidth = (width - 32 - displayGap) / 2;
        waveform->setBounds (16, 12, displayWidth, 138);
        if (waveform2 != nullptr)
            waveform2->setBounds (16 + displayWidth + displayGap, 12, displayWidth, 138);
        if (unisonDisplay != nullptr)
            unisonDisplay->setBounds (16, 158, width - 32, 82);
    }

    if (filterDisplay != nullptr)
        filterDisplay->setBounds (16, 12, width - 32, 170);
    if (levelMeter != nullptr)
        levelMeter->setBounds (16, 12, width - 32, 140);
    if (envelopeEditor != nullptr)
        envelopeEditor->setBounds (16, waveform != nullptr ? 248 : (filterDisplay != nullptr ? 192 : 12), width - 32, 132);

    for (size_t i = 0; i < controls.size(); ++i)
    {
        const int row = (int) i / columns;
        const int col = (int) i % columns;
        const int x = 16 + col * (cellWidth + gap);
        const int y = top + row * (cellHeight + gap);
        auto card = juce::Rectangle<int> (x, y, cellWidth, cellHeight);
        content.cards.push_back (card);
        labels[i]->setBounds (card.withY (card.getY() + 8).withHeight (20).reduced (5, 0));

        if (auto* slider = dynamic_cast<juce::Slider*> (controls[i].get()))
            slider->setBounds (card.withTrimmedTop (27).reduced (8, 1));
        else
            controls[i]->setBounds (card.withTrimmedTop (34).reduced (8, 5));
    }
    content.repaint();
}

WaveForgeEditor::WaveForgeEditor (WaveForgeProcessor& processor)
    : AudioProcessorEditor (&processor), audioProcessor (processor)
{
    setLookAndFeel (&lookAndFeel);
    title.setText ("WAVEFORGE", juce::dontSendNotification);
    title.setFont (juce::Font (25.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, textMain);
    subtitle.setText ("PROFESSIONAL WAVETABLE SYNTHESIZER", juce::dontSendNotification);
    subtitle.setFont (juce::Font (9.5f, juce::Font::bold));
    subtitle.setColour (juce::Label::textColourId, textMuted);
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);
    addAndMakeVisible (presetBrowser);
    addAndMakeVisible (savePreset);
    addAndMakeVisible (loadPreset);
    addAndMakeVisible (tabs);

    presetBrowser.setLookAndFeel (&lookAndFeel);
    savePreset.setLookAndFeel (&lookAndFeel);
    loadPreset.setLookAndFeel (&lookAndFeel);
    tabs.setLookAndFeel (&lookAndFeel);

    presetBrowser.addItem ("FACTORY PRESETS", 1);
    presetBrowser.addItem ("Init", 2);
    presetBrowser.addItem ("Deep Bass", 3);
    presetBrowser.addItem ("Wide Lead", 4);
    presetBrowser.addItem ("Soft Pad", 5);
    presetBrowser.addItem ("Pluck", 6);
    presetBrowser.addItem ("Noise Sweep", 7);
    presetBrowser.setSelectedId (1, juce::dontSendNotification);
    presetBrowser.onChange = [this]
    {
        const int preset = presetBrowser.getSelectedId();
        if (preset <= 1) return;
        for (auto* parameter : audioProcessor.getParameters())
            parameter->setValueNotifyingHost (parameter->getDefaultValue());

        auto set = [this] (const char* id, float value)
        {
            if (auto* parameter = audioProcessor.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        if (preset == 3)
        {
            set ("osc2On", 1.0f); set ("osc2Pos", 0.43f); set ("osc2Level", 0.38f);
            set ("osc2Semi", -12.0f); set ("subLevel", 0.72f);
            set ("cutoff", 190.0f); set ("resonance", 0.36f); set ("fEnvAmt", 2.8f);
            set ("fDecay", 0.32f); set ("fSustain", 0.0f); set ("attack", 0.005f);
            set ("decay", 0.5f); set ("sustain", 0.72f); set ("release", 0.16f);
            set ("distMix", 0.18f); set ("distDrive", 16.0f);
        }
        else if (preset == 4)
        {
            set ("uniVoices", 6.0f); set ("uniDetune", 28.0f); set ("uniSpread", 1.0f);
            set ("osc2On", 1.0f); set ("osc2Level", 0.32f); set ("osc2Semi", 7.0f);
            set ("cutoff", 8500.0f); set ("chMix", 0.24f); set ("dlMix", 0.13f);
            set ("rvMix", 0.10f); set ("attack", 0.015f); set ("release", 0.55f);
        }
        else if (preset == 5)
        {
            set ("uniVoices", 4.0f); set ("uniDetune", 16.0f); set ("uniSpread", 0.95f);
            set ("attack", 1.1f); set ("decay", 2.0f); set ("sustain", 0.82f);
            set ("release", 3.2f); set ("cutoff", 4200.0f); set ("chMix", 0.28f);
            set ("rvMix", 0.38f);
        }
        else if (preset == 6)
        {
            set ("attack", 0.001f); set ("decay", 0.42f); set ("sustain", 0.04f);
            set ("release", 0.24f); set ("cutoff", 7800.0f); set ("fEnvAmt", 3.0f);
            set ("fAttack", 0.001f); set ("fDecay", 0.28f); set ("fSustain", 0.03f);
            set ("subLevel", 0.12f); set ("dlMix", 0.18f);
        }
        else if (preset == 7)
        {
            set ("noiseLevel", 0.48f); set ("cutoff", 2400.0f); set ("fEnvAmt", 4.0f);
            set ("fAttack", 0.18f); set ("fDecay", 2.2f); set ("fSustain", 0.08f);
            set ("rvMix", 0.22f); set ("release", 1.2f);
        }
    };

    savePreset.setColour (juce::TextButton::textColourOffId, bg);
    loadPreset.setColour (juce::TextButton::textColourOffId, textMain);
    savePreset.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Save WaveForge Preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.wfpreset");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                if (file == juce::File{}) return;
                if (auto xml = audioProcessor.apvts.copyState().createXml())
                    xml->writeTo (file);
            });
    };
    loadPreset.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load WaveForge Preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.wfpreset");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                if (file == juce::File{}) return;
                std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
                if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
                    audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
            });
    };

    tabs.addTab ("OSC", panel, new WaveForgeParameterPage (processor, {
        { "wtPos", "WT POSITION" }, { "uniVoices", "UNISON VOICES" },
        { "uniDetune", "DETUNE" }, { "uniSpread", "STEREO SPREAD" },
        { "osc2On", "OSCILLATOR 2" }, { "osc2Pos", "OSC 2 POSITION" },
        { "osc2Level", "OSC 2 LEVEL" }, { "osc2Semi", "OSC 2 SEMITONES" },
        { "subLevel", "SUB LEVEL" }, { "noiseLevel", "NOISE LEVEL" }
    }, true, "amp"), true);

    tabs.addTab ("FILTER", panel, new WaveForgeParameterPage (processor, {
        { "fType", "FILTER TYPE" }, { "cutoff", "CUTOFF" },
        { "resonance", "RESONANCE" }, { "fEnvAmt", "ENV AMOUNT" }
    }, false, "filter"), false);

    tabs.addTab ("MOD", panel, new WaveForgeModulationPage (processor), false);

    auto* effects = new juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop);
    effects->setTabBarDepth (34);
    effects->addTab ("DISTORTION", panel, new WaveForgeParameterPage (processor, {
        { "distType", "TYPE" }, { "distDrive", "DRIVE" }, { "distMix", "MIX" }
    }), true);
    effects->addTab ("CHORUS", panel, new WaveForgeParameterPage (processor, {
        { "chRate", "RATE" }, { "chDepth", "DEPTH" },
        { "chFeedback", "FEEDBACK" }, { "chMix", "MIX" }
    }), true);
    effects->addTab ("DELAY", panel, new WaveForgeParameterPage (processor, {
        { "dlSync", "SYNC" }, { "dlTime", "TIME" }, { "dlFeedback", "FEEDBACK" },
        { "dlTone", "TONE" }, { "dlPing", "PING PONG" }, { "dlMix", "MIX" }
    }), true);
    effects->addTab ("REVERB", panel, new WaveForgeParameterPage (processor, {
        { "rvSize", "SIZE" }, { "rvDamp", "DAMPING" },
        { "rvWidth", "WIDTH" }, { "rvMix", "MIX" }
    }), true);
    tabs.addTab ("FX", panel, effects, true);

    tabs.addTab ("MASTER", panel, new WaveForgeParameterPage (processor, {
        { "gain", "OUTPUT GAIN" }
    }, false, {}, true), false);

    setResizable (true, true);
    setResizeLimits (900, 650, 1600, 1200);
    setSize (1180, 820);
}

WaveForgeEditor::~WaveForgeEditor()
{
    setLookAndFeel (nullptr);
}

void WaveForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.9f), 0.0f, 0.0f,
                                             juce::Colour (0xff168d9d), (float) getWidth(), 0.0f, false));
    g.fillRect (0, 0, getWidth(), 3);
    g.setColour (juce::Colour (0xff1b2a3e));
    g.drawHorizontalLine (77, 18.0f, (float) getWidth() - 18.0f);
}

void WaveForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (18);
    auto header = area.removeFromTop (64);
    auto controlsArea = header.removeFromRight (420);
    presetBrowser.setBounds (controlsArea.removeFromLeft (172).reduced (3, 13));
    savePreset.setBounds (controlsArea.removeFromLeft (116).reduced (3, 13));
    loadPreset.setBounds (controlsArea.removeFromLeft (116).reduced (3, 13));
    title.setBounds (header.removeFromTop (35));
    subtitle.setBounds (header);
    area.removeFromTop (12);
    tabs.setBounds (area);
}
