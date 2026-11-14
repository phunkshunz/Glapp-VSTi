#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Sprak.h"
#include "Formsprak.h"

// Utseende: MASKINHALLENS grundschema. Mörk kolgrå, natriumorange och kallt turkos.
// Formen (typsnitt, rattar, knappar) kommer från Formsprak.h.
namespace ArmColours
{
const juce::Colour ground   { mh::Form::kBg };
const juce::Colour panel    { mh::Form::kPanel };
const juce::Colour raised   { mh::Form::kRaised };
const juce::Colour line     { mh::Form::kHair };
const juce::Colour line2    { mh::Form::kHair2 };
const juce::Colour text     { mh::Form::kInk };
const juce::Colour muted    { mh::Form::kInk3 };
const juce::Colour sodium   { 0xffff8a1f };   // natriumorange: på, spelar, vald
const juce::Colour sodiumDk { 0xffc96a16 };
const juce::Colour cold     { 0xff4fd1e0 };   // turkos
const juce::Colour coldDk   { 0xff2ea7b5 };
const juce::Colour concrete { 0xffb9b6ae };
const juce::Colour steel    { 0xff8b8e91 };

// lådornas färger: en färg per låda så man ser vad som är vad
inline juce::Colour box (int b)
{
    static const juce::Colour c[] = { juce::Colour (0xffff8a1f), juce::Colour (0xff4fd1e0), juce::Colour (0xffffc24a),
                                      juce::Colour (0xff8fd19a), juce::Colour (0xffe58fb0), juce::Colour (0xffb0a4d6) };
    return c[(size_t) (((b % 6) + 6) % 6)];
}

// spårens färger: varje spår har sin egen
inline juce::Colour lane (int i)
{
    static const juce::Colour c[] = { juce::Colour (0xfff2efe8), juce::Colour (0xffff8a1f), juce::Colour (0xffffc24a), juce::Colour (0xff4fd1e0),
                                      juce::Colour (0xff8fd19a), juce::Colour (0xffe58fb0), juce::Colour (0xff7fa6c4), juce::Colour (0xffb0a4d6) };
    return c[(size_t) (((i % 8) + 8) % 8)];
}
} // namespace ArmColours

// All svensk text går genom utf8(): den blir engelsk när EN är valt.
inline juce::String utf8 (const char* s) { return sprak::tr (juce::String::fromUTF8 (s)); }

// Rattar, knappar, listor och typsnitt kommer från formspråket.
class ArmLnF : public mh::FormLookAndFeel
{
public:
    ArmLnF() { mh::Form::setAccent (ArmColours::sodium); }
};

// Ratt med namn under och värde som visas när man skruvar.
class Knob : public juce::Component
{
public:
    Knob()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.onValueChange = [this] { repaint(); };
        slider.setDoubleClickReturnValue (false, 0.0);
        addAndMakeVisible (slider);
    }

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& label)
    {
        attachment.reset();
        name = label;
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
        if (auto* p = state.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        repaint();
    }

    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); }

    void paint (juce::Graphics& g) override
    {
        using namespace ArmColours;
        auto r = getLocalBounds();
        auto bottom = r.removeFromBottom (26);
        g.setColour (mh::Form::ink3());
        g.setFont (mh::Form::text (10.0f, false, 0.08f));
        g.drawText (sprak::tr (name), bottom.removeFromTop (12), juce::Justification::centred, true);
        g.setColour (mh::Form::ink2());
        g.setFont (mh::Form::mono (10.5f));
        g.drawText (slider.getTextFromValue (slider.getValue()), bottom, juce::Justification::centred, true);
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (26)); }

    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Rubrik + hårlinje (formspråket): inga kort, inga rundade lådor.
inline void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& sub = {})
{
    if (title.isNotEmpty()) mh::Form::section (g, r.reduced (2, 2), sprak::tr (title).toUpperCase(), sprak::tr (sub));
}

// Liten ratt för spårremsorna: namn under, värdet visas medan man skruvar.
class MiniKnob : public juce::Component
{
public:
    MiniKnob()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.onValueChange = [this] { repaint(); };
        slider.onDragStart = [this] { dragging = true; repaint(); };
        slider.onDragEnd = [this] { dragging = false; repaint(); };
        addAndMakeVisible (slider);
    }
    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& label)
    {
        attachment.reset();
        name = label;
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
        if (auto* p = state.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (label);
        repaint();
    }
    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); }
    void paint (juce::Graphics& g) override
    {
        using namespace ArmColours;
        g.setColour (dragging ? mh::Form::ink() : mh::Form::ink3());
        g.setFont (dragging ? mh::Form::mono (9.5f) : mh::Form::text (9.5f, false, 0.08f));
        g.drawText (dragging ? slider.getTextFromValue (slider.getValue()) : sprak::tr (name), getLocalBounds().removeFromBottom (11), juce::Justification::centred, false);
    }
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (11)); }
    juce::Slider slider;
private:
    juce::String name;
    bool dragging = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Rullgardin kopplad till en val-parameter.
class ParamCombo : public juce::ComboBox
{
public:
    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    {
        att.reset();
        clear (juce::dontSendNotification);
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
            addItemList (p->choices, 1);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *this);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// Av/på-knapp kopplad till en bool-parameter.
class ParamToggle : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    {
        setClickingTogglesState (true);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *this);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Tärning: slumpknapp. Ritar en tärning med prickar (ingen font behövs).
class DiceButton : public juce::Button
{
public:
    explicit DiceButton (const juce::String& tip = {}) : juce::Button ("slump") { setTooltip (tip); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    juce::Colour accent = ArmColours::sodium;
    int face = 5;
    void clicked() override { face = 1 + juce::Random::getSystemRandom().nextInt (6); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using namespace ArmColours;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float s = std::min (r.getWidth(), r.getHeight());
        auto b = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
        // formspråket: bara en hårfin kontur och små prickar. Lyser upp när man håller över.
        b = b.reduced (s * 0.12f);
        g.setColour (over || down ? mh::Form::hair2().brighter (0.3f) : mh::Form::hair2());
        g.drawRect (b, 1.0f);
        g.setColour (down ? mh::Form::accent() : (over ? mh::Form::ink() : mh::Form::ink3()));
        const float d = s * 0.11f;
        auto dot = [&] (float x, float y) { g.fillEllipse (b.getX() + b.getWidth() * x - d * 0.5f, b.getY() + b.getHeight() * y - d * 0.5f, d, d); };
        const int f = face;
        if (f % 2 == 1) dot (0.5f, 0.5f);
        if (f >= 2) { dot (0.27f, 0.27f); dot (0.73f, 0.73f); }
        if (f >= 4) { dot (0.73f, 0.27f); dot (0.27f, 0.73f); }
        if (f == 6) { dot (0.27f, 0.5f); dot (0.73f, 0.5f); }
    }
};
