#pragma once

// MASKINHALLEN · formspråk
//
// Samma fil i alla maskiner. Minimal svart: nästan svart botten, tunna hårlinjer i stället för
// svävande kort, varmvit text, EN signalfärg per maskin som bara används för det som är på eller spelar.
// Typsnitt: IBM Plex Sans Condensed (text) och IBM Plex Mono (värden), SIL Open Font License (fonts/OFL.txt).
//
// Använd:
//   mh::Form::setAccent (färg);              en gång, i maskinens Gui.h
//   class MinLnF : public mh::FormLookAndFeel  rattar, knappar, listor och typsnitt
//   juce::LookAndFeel::setDefaultLookAndFeel (&lnf) i editorn, så att ALL text får Plex
//   mh::Form::section (g, r, "RUBRIK")       rubrik + hårlinje
//   mh::Form::text / mono (storlek, fet)     typsnitten

#include <juce_gui_basics/juce_gui_basics.h>
#if __has_include ("BinaryData.h")
 #include "BinaryData.h"
 #define MH_HAS_FONTS 1
#else
 #define MH_HAS_FONTS 0
#endif

namespace mh
{
struct Form
{
    // ---------------------------------------------------------------- färger
    static constexpr juce::uint32 kBg = 0xff121314, kPanel = 0xff1a1c1e, kRaised = 0xff232527, kCell = 0xff26292c,
                                  kHair = 0xff2c2f32, kHair2 = 0xff3d4246, kInk = 0xffe6e3dc, kInk2 = 0xffb9b6ae, kInk3 = 0xff8b8e91;
    static juce::Colour bg()     { return juce::Colour (kBg); }
    static juce::Colour panel()  { return juce::Colour (kPanel); }
    static juce::Colour raised() { return juce::Colour (kRaised); }
    static juce::Colour cell()   { return juce::Colour (kCell); }
    static juce::Colour hair()   { return juce::Colour (kHair); }
    static juce::Colour hair2()  { return juce::Colour (kHair2); }
    static juce::Colour ink()    { return juce::Colour (kInk); }
    static juce::Colour ink2()   { return juce::Colour (kInk2); }
    static juce::Colour ink3()   { return juce::Colour (kInk3); }
    static juce::Colour& accentRef() { static juce::Colour a { 0xffff8a1f }; return a; }
    static juce::Colour accent() { return accentRef(); }
    static void setAccent (juce::Colour c) { accentRef() = c; }

    // ---------------------------------------------------------------- typsnitt
    static juce::Typeface::Ptr load (const char* file)
    {
       #if MH_HAS_FONTS
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (juce::String (BinaryData::originalFilenames[i]) == file)
            {
                int size = 0;
                if (auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            }
       #endif
        juce::ignoreUnused (file);
        return nullptr;
    }
    static juce::Typeface::Ptr sans()     { static auto t = load ("IBMPlexSansCondensed-Medium.ttf"); return t; }
    static juce::Typeface::Ptr sansBold() { static auto t = load ("IBMPlexSansCondensed-SemiBold.ttf"); return t; }
    static juce::Typeface::Ptr monoFace() { static auto t = load ("IBMPlexMono-Medium.ttf"); return t; }

    // text: smalt, lugnt. Versaler får lite luft (tracking), gemener ingen.
    static juce::Font text (float size, bool bold = false, float tracking = 0.04f)
    {
        auto tf = bold ? sansBold() : sans();
        juce::FontOptions o = tf != nullptr ? juce::FontOptions (tf).withHeight (size) : juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
        return juce::Font (o.withKerningFactor (tracking));
    }
    static juce::Font mono (float size)
    {
        auto tf = monoFace();
        return juce::Font (tf != nullptr ? juce::FontOptions (tf).withHeight (size) : juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
    }

    // ---------------------------------------------------------------- byggstenar
    // Sektion: rubrik i små versaler och en hårlinje efter. Inga kort, inga rundade lådor.
    static void section (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& sub = {})
    {
        const auto t = r.removeFromTop (18);
        g.setFont (text (10.5f, true, 0.12f));
        g.setColour (ink2());
        g.drawText (title, t, juce::Justification::centredLeft, false);
        const int tw = (int) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title) + 10;
        int lineEnd = t.getRight();
        if (sub.isNotEmpty())
        {
            g.setFont (text (10.0f, false, 0.0f));
            g.setColour (ink3());
            const int sw = (int) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), sub) + 6;
            g.drawText (sub, t.withTrimmedLeft (t.getWidth() - sw), juce::Justification::centredRight, false);
            lineEnd -= sw + 10;
        }
        g.setColour (hair());
        if (lineEnd > t.getX() + tw) g.fillRect (t.getX() + tw, t.getCentreY(), lineEnd - t.getX() - tw, 1);
    }

    // Ratt: svart lock, en vit visare, skala med 11 prickar runt om. Prickarna upp till värdet lyser svagt,
    // och bara den sista i signalfärgen när ratten är långt ifrån noll. Ingen färgad ring.
    static void knob (juce::Graphics& g, juce::Rectangle<float> r, float pos, float a0, float a1, bool enabled, bool bipolar, juce::Colour acc)
    {
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();
        const float rad = size * 0.5f;
        const float capR = rad * 0.70f;
        const float angle = a0 + pos * (a1 - a0);
        // skalan
        const int ticks = 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float t = (float) i / (float) (ticks - 1);
            const float a = a0 + t * (a1 - a0);
            const bool lit = bipolar ? ((t >= 0.5f && t <= pos + 1.0e-4f) || (t <= 0.5f && t >= pos - 1.0e-4f)) : t <= pos + 1.0e-4f;
            const float dr = juce::jmax (1.0f, size * 0.022f);
            const auto p = c.getPointOnCircumference (rad - dr * 1.5f, a);
            g.setColour (! enabled ? hair2() : (lit ? acc : hair2()));
            g.fillEllipse (juce::Rectangle<float> (dr * 2, dr * 2).withCentre (p));
        }
        // locket: mattsvart med en hårfin kant och lite ljus uppifrån
        const auto cap = juce::Rectangle<float> (capR * 2, capR * 2).withCentre (c);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e3134), cap.getX(), cap.getY(), juce::Colour (0xff1b1d1f), cap.getX(), cap.getBottom(), false));
        g.fillEllipse (cap);
        g.setColour (juce::Colour (0xff4a4f54));
        g.drawEllipse (cap.reduced (0.5f), 1.0f);
        // visaren
        const auto p0 = c.getPointOnCircumference (capR * 0.30f, angle);
        const auto p1 = c.getPointOnCircumference (capR * 0.92f, angle);
        const bool hot = enabled && (bipolar ? std::abs (pos - 0.5f) > 0.35f : pos > 0.85f);
        g.setColour (! enabled ? ink3() : ink());
        g.drawLine ({ p0, p1 }, juce::jmax (1.6f, size * 0.045f));
    }

    // Knapp: hårlinje, text i ink2. På = text i ink och en liten lysdiod i signalfärgen. Aldrig fylld.
    static void button (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over, bool down, bool enabled)
    {
        r = r.reduced (0.5f);
        g.setColour (down ? raised().brighter (0.06f) : (over ? raised() : panel()));
        g.fillRect (r);
        if (on && enabled) { g.setColour (accent().withAlpha (0.16f)); g.fillRect (r); }
        g.setColour (on && enabled ? accent() : (over ? hair2().brighter (0.15f) : hair2()));
        g.drawRect (r, 1.0f);
        if (on && enabled)
        {
            const float d = 5.0f;
            g.setColour (accent());
            g.fillEllipse (r.getX() + 7.0f, r.getCentreY() - d * 0.5f, d, d);
        }
    }
};

// ---------------------------------------------------------------- utseendet
class FormLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FormLookAndFeel()
    {
        using F = Form;
        setColour (juce::ResizableWindow::backgroundColourId, F::bg());
        setColour (juce::TextButton::buttonColourId, F::panel());
        setColour (juce::TextButton::buttonOnColourId, F::panel());
        setColour (juce::TextButton::textColourOffId, F::ink2());
        setColour (juce::TextButton::textColourOnId, F::ink());
        setColour (juce::ComboBox::backgroundColourId, F::panel());
        setColour (juce::ComboBox::textColourId, F::ink());
        setColour (juce::ComboBox::outlineColourId, F::hair2());
        setColour (juce::ComboBox::arrowColourId, F::ink3());
        setColour (juce::PopupMenu::backgroundColourId, F::raised());
        setColour (juce::PopupMenu::textColourId, F::ink());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, F::hair2());
        setColour (juce::PopupMenu::highlightedTextColourId, F::ink());
        setColour (juce::PopupMenu::headerTextColourId, F::ink3());
        setColour (juce::Slider::textBoxTextColourId, F::ink());
        setColour (juce::Slider::textBoxBackgroundColourId, F::panel());
        setColour (juce::Slider::textBoxOutlineColourId, F::hair2());
        setColour (juce::Label::textColourId, F::ink());
        setColour (juce::ListBox::backgroundColourId, F::panel());
        setColour (juce::ScrollBar::thumbColourId, F::hair2());
        setColour (juce::TooltipWindow::backgroundColourId, F::raised());
        setColour (juce::TooltipWindow::textColourId, F::ink());
        setColour (juce::TooltipWindow::outlineColourId, F::hair2());
        setColour (juce::TextEditor::backgroundColourId, F::panel());
        setColour (juce::TextEditor::outlineColourId, F::hair2());
        setColour (juce::TextEditor::textColourId, F::ink());
    }

    // ALL text i fönstret blir Plex: vanliga typsnitt -> Plex Sans Condensed, monospace -> Plex Mono
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        if (f.getTypefaceName() == juce::Font::getDefaultMonospacedFontName())
            if (auto t = Form::monoFace()) return t;
        if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() || f.getTypefaceName() == juce::Font::getDefaultSerifFontName())
            if (auto t = f.isBold() ? Form::sansBold() : Form::sans()) return t;
        return juce::LookAndFeel_V4::getTypefaceForFont (f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const bool bip = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        Form::knob (g, juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.0f), pos, a0, a1, s.isEnabled(), bip, Form::accent());
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        Form::button (g, b.getLocalBounds().toFloat(), b.getToggleState(), over, down, b.isEnabled());
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds();
        if (b.getToggleState()) r.removeFromLeft (8);   // plats för lysdioden
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (! b.isEnabled() ? Form::ink3() : (b.getToggleState() ? Form::ink() : Form::ink2()));
        g.drawFittedText (b.getButtonText(), r.reduced (4, 0), juce::Justification::centred, 1, 0.85f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Form::text (juce::jmin (12.0f, (float) h * 0.48f), true, 0.08f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Form::text (12.5f, false, 0.02f); }
    juce::Font getPopupMenuFont() override { return Form::text (13.5f, false, 0.0f); }
    juce::Font getLabelFont (juce::Label&) override { return Form::text (12.0f); }

    // Lista: bara en hårlinje under, och en liten pil. Som ett fält på en apparat.
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (Form::panel());
        g.fillRect (r);
        g.setColour (box.hasKeyboardFocus (true) ? Form::ink3() : Form::hair2());
        g.fillRect (r.removeFromBottom (1.0f));
        juce::Path arrow;
        const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 3.5f, ay - 1.5f); arrow.lineTo (ax, ay + 2.0f); arrow.lineTo (ax + 3.5f, ay - 1.5f);
        g.setColour (Form::ink3());
        g.strokePath (arrow, juce::PathStrokeType (1.2f));
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 20, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (Form::raised());
        g.setColour (Form::hair2());
        g.drawRect (0, 0, w, h, 1);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h) override
    {
        g.fillAll (Form::raised());
        g.setColour (Form::hair2());
        g.drawRect (0, 0, w, h, 1);
        g.setColour (Form::ink());
        g.setFont (Form::text (12.5f, false, 0.0f));
        g.drawFittedText (t, juce::Rectangle<int> (w, h).reduced (8, 5), juce::Justification::centredLeft, 6);
    }
    juce::Rectangle<int> getTooltipBounds (const juce::String& t, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (360, 24 + (int) juce::GlyphArrangement::getStringWidth (Form::text (12.5f, false, 0.0f), t));
        const int lines = 1 + (int) juce::GlyphArrangement::getStringWidth (Form::text (12.5f, false, 0.0f), t) / 340;
        const int h = 12 + lines * 16;
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - w - 10 : pos.x + 14,
                                     pos.y > parent.getCentreY() ? pos.y - h - 8 : pos.y + 16, w, h).constrainedWithin (parent);
    }
};
} // namespace mh
