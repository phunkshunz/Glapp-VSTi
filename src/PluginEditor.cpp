#include "PluginEditor.h"

using namespace gl;
using namespace ArmColours;

namespace
{
juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

void label (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, juce::Colour c = muted, float size = 9.5f,
            juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour (c);
    g.setFont (juce::FontOptions (size, juce::Font::bold));
    g.drawText (t, r, j, false);
}

double stepMs (const Engine& e, int t) { return divPpq (e.tr[(size_t) t].div) * 60000.0 / std::max (20.0, e.bpm); }

// Var spelhuvudet står på ett spår, i steg (med decimaler), räknat inom mönstret.
double headPos (const Engine& e, int t, double ppq)
{
    const double sp = divPpq (e.tr[(size_t) t].div);
    const double pos = ppq / sp;
    const int64_t n = (int64_t) std::floor (pos);
    const int len = e.length (t);
    const int idx = e.stepIndex (t, n);
    return std::fmod ((double) idx + (pos - (double) n), (double) len);
}
} // namespace

// ---------------------------------------------------------------- loggan
class GlappLogo : public juce::Component
{
public:
    std::function<void()> onClick;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (sodium);
        g.setFont (juce::FontOptions (44.0f, juce::Font::bold).withHorizontalScale (0.85f));
        g.drawText ("GLAPP", r.removeFromTop (48), juce::Justification::centredLeft);
        g.setColour (muted);
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText ("MASKINHALLEN", r.removeFromTop (12), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("Phunkshunz AUDIO", r.removeFromTop (12), juce::Justification::centredLeft);
        // två kugghjul som inte riktigt griper i varandra
        auto cog = [&] (float cx, float cy, float rad, float rot, juce::Colour c)
        {
            juce::Path p;
            for (int i = 0; i < 8; ++i)
            {
                juce::Path tooth;
                tooth.addRectangle (-2.0f, -rad - 3.0f, 4.0f, 5.0f);
                p.addPath (tooth, juce::AffineTransform::rotation ((float) i * juce::MathConstants<float>::twoPi / 8.0f));
            }
            p.addEllipse (-rad, -rad, rad * 2.0f, rad * 2.0f);
            g.setColour (c);
            g.fillPath (p, juce::AffineTransform::rotation (rot).translated (cx, cy));
            g.setColour (juce::Colour (0xff111214));
            g.fillEllipse (cx - rad * 0.35f, cy - rad * 0.35f, rad * 0.7f, rad * 0.7f);
        };
        cog (146.0f, 22.0f, 9.0f, 0.2f, sodium.withAlpha (0.9f));
        cog (167.0f, 30.0f, 7.0f, 0.55f, cold.withAlpha (0.9f));
    }
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
};

// ---------------------------------------------------------------- stegrutnätet
// Klick = på/av. Dra upp/ner = velocity. Scrollhjul = tonhöjd. Shift-klick = spöknot.
class StepGrid : public juce::Component
{
public:
    StepGrid (GlappProcessor& p, int track) : proc (p), t (track) {}

    void setView (const Engine& e, double ppq, bool isPlaying)
    {
        len = e.length (t);
        playing = isPlaying;
        head = headPos (e, t, ppq);
        const double sm = stepMs (e, t);
        for (int i = 0; i < len; ++i) offs[(size_t) i] = (float) (e.staticOffsetMs (t, i) / sm);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (ground);
        g.fillRoundedRectangle (r, 4.0f);
        const auto col = ArmColours::lane (t);
        const float cw = r.getWidth() / (float) len;
        const int cur = (int) std::floor (head);

        for (int i = 0; i < len; ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + i * cw, r.getY(), cw, r.getHeight()).reduced (1.0f, 3.0f);
            const bool beat = (i % 4) == 0;
            g.setColour (beat ? raised : panel);
            g.fillRoundedRectangle (c, 2.0f);
            const auto st = proc.pattern.get (t, i);
            if (st.vel > 0)
            {
                const float h = c.getHeight() * (0.18f + 0.82f * (float) st.vel / 127.0f);
                auto bar = c.withTop (c.getBottom() - h);
                g.setColour (col.withAlpha (st.vel < 40 ? 0.45f : 0.9f));
                g.fillRoundedRectangle (bar, 2.0f);
                // var noten faktiskt landar: en tunn linje förskjuten med släpet
                const float ox = c.getX() + juce::jlimit (0.0f, 1.0f, offs[(size_t) i]) * cw;
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.fillRect (juce::Rectangle<float> (ox, c.getY(), 1.5f, 5.0f));
                if (st.pitch != 0 && cw > 11.0f)
                {
                    g.setColour (ground);
                    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
                    g.drawText ((st.pitch > 0 ? "+" : "") + juce::String (st.pitch), bar.toNearestInt(), juce::Justification::centredBottom, false);
                }
            }
            if (playing && i == cur)
            {
                g.setColour (text.withAlpha (0.8f));
                g.drawRoundedRectangle (c, 2.0f, 1.2f);
            }
        }
    }

    int cellAt (juce::Point<float> p) const
    {
        const float cw = (float) getWidth() / (float) len;
        return juce::jlimit (0, len - 1, (int) (p.x / cw));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.position);
        auto st = proc.pattern.get (t, i);
        if (e.mods.isPopupMenu()) { st.vel = 0; st.pitch = 0; }
        else if (e.mods.isShiftDown()) st.vel = 30;
        else st.vel = st.vel > 0 ? 0 : 100;
        proc.setStep (t, i, st);
        dragCell = i;
        dragStartVel = st.vel;
        proc.currentPreset.store (-1);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragCell < 0 || dragStartVel == 0) return;
        auto st = proc.pattern.get (t, dragCell);
        st.vel = juce::jlimit (1, 127, dragStartVel - e.getDistanceFromDragStartY());
        proc.setStep (t, dragCell, st);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { dragCell = -1; }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int i = cellAt (e.position);
        auto st = proc.pattern.get (t, i);
        if (st.vel == 0) return;
        st.pitch = juce::jlimit (-24, 24, st.pitch + (w.deltaY > 0 ? 1 : -1));
        proc.setStep (t, i, st);
        repaint();
    }

private:
    GlappProcessor& proc;
    int t;
    int len = 16;
    double head = 0.0;
    bool playing = false;
    std::array<float, MAXS> offs {};
    int dragCell = -1, dragStartVel = 0;
};

// ---------------------------------------------------------------- en spårrad
class LaneRow : public juce::Component
{
public:
    LaneRow (GlappProcessor& p, int track, std::function<void (int)> onSelect) : proc (p), t (track), grid (p, track), select (std::move (onSelect))
    {
        auto& s = proc.apvts;
        nameBtn.setButtonText (U (roleName (t)));
        nameBtn.setColour (juce::TextButton::textColourOffId, ArmColours::lane (t));
        nameBtn.setTooltip (U ("Visa ") + U (roleName (t)) + U (" i verkstaden"));
        nameBtn.onClick = [this] { select (t); };
        addAndMakeVisible (nameBtn);
        addAndMakeVisible (grid);

        langd.attach (s, tid (t, "langd"), U ("LÄNGD"));
        drag.attach (s, tid (t, "drag"), "DRAG");
        trog.attach (s, tid (t, "trog"), U ("TRÖGHET"));
        jitter.attach (s, tid (t, "jitter"), "JITTER");
        for (auto* k : { &langd, &drag, &trog, &jitter }) { k->setAccent (ArmColours::lane (t)); addAndMakeVisible (*k); }
        langd.slider.setTooltip (U ("Antal steg. Udda längder (11, 13, 15, 17, 23) driver mot varandra i all evighet."));
        drag.slider.setTooltip (U ("Hur sent spåret slår, i millisekunder. Rimmet trivs runt 14–32 ms."));
        trog.slider.setTooltip (U ("Tröghet: slag tätt efter varandra halkar allt senare, som på en oljefläck."));
        jitter.slider.setTooltip (U ("Liten mänsklig slump i tid."));

        frys.attach (s, tid (t, "frys"));
        frys.setColour (juce::TextButton::buttonOnColourId, concrete);
        frys.setTooltip (U ("Lås spåret. DICE, ORBIT EVOLVE och presets lämnar det i fred."));
        tyst.attach (s, tid (t, "tyst"));
        tyst.setTooltip (U ("Tysta spåret."));
        addAndMakeVisible (frys);
        addAndMakeVisible (tyst);

        dice.accent = ArmColours::lane (t);
        dice.setTooltip (U ("Slumpa ") + U (roleName (t)) + U (" med reglerna för vald STIL."));
        dice.onClick = [this] { proc.diceLane (t); };
        addAndMakeVisible (dice);
    }

    void setView (const Engine& e, double ppq, bool playing, int hits)
    {
        grid.setView (e, ppq, playing);
        if (hits != lastHits) { flash = 1.0f; lastHits = hits; }
        else flash *= 0.8f;
        repaint (ledArea);
    }

    void setSelected (bool b) { if (sel != b) { sel = b; repaint(); } }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 4);
        ledArea = r.removeFromLeft (8).withSizeKeepingCentre (6, 6);
        r.removeFromLeft (4);
        nameBtn.setBounds (r.removeFromLeft (70).reduced (0, 12));
        r.removeFromLeft (8);
        dice.setBounds (r.removeFromRight (26).withSizeKeepingCentre (22, 22));
        r.removeFromRight (6);
        auto tb = r.removeFromRight (46);
        frys.setBounds (tb.removeFromTop (tb.getHeight() / 2).reduced (0, 2));
        tyst.setBounds (tb.reduced (0, 2));
        r.removeFromRight (6);
        for (auto* k : { &jitter, &trog, &drag, &langd }) k->setBounds (r.removeFromRight (46));
        r.removeFromRight (8);
        grid.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (panel);
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (sel ? ArmColours::lane (t).withAlpha (0.7f) : line);
        g.drawRoundedRectangle (b, 5.0f, sel ? 1.4f : 1.0f);
        g.setColour (ArmColours::lane (t).withAlpha (0.15f + 0.85f * flash));
        g.fillEllipse (ledArea.toFloat());
    }

    void mouseDown (const juce::MouseEvent&) override { select (t); }

private:
    GlappProcessor& proc;
    int t;
    StepGrid grid;
    std::function<void (int)> select;
    juce::TextButton nameBtn;
    MiniKnob langd, drag, trog, jitter;
    ParamToggle frys { "FRYS" }, tyst { "TYST" };
    DiceButton dice;
    juce::Rectangle<int> ledArea;
    float flash = 0.0f;
    int lastHits = 0;
    bool sel = false;
};

// ---------------------------------------------------------------- urverket
// Åtta ringar, kicken innerst. Varje träff är en prick på sin ring, förskjuten med
// släpet. Spelhuvudena snurrar i olika hastighet, så du ser hur spåren glider isär.
class Urverket : public juce::Component
{
public:
    explicit Urverket (GlappProcessor& p) : proc (p) {}

    void setView (const Engine& e, double ppq, bool isPlaying)
    {
        playing = isPlaying;
        for (int t = 0; t < NT; ++t)
        {
            auto& r = rings[(size_t) t];
            r.len = e.length (t);
            r.mute = e.tr[(size_t) t].mute;
            r.head = headPos (e, t, ppq);
            const double sm = stepMs (e, t);
            for (int i = 0; i < r.len; ++i)
            {
                r.vel[(size_t) i] = proc.pattern.get (t, i).vel;
                r.off[(size_t) i] = (float) juce::jlimit (0.0, 1.0, e.staticOffsetMs (t, i) / sm);
            }
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const float size = std::min (area.getWidth(), area.getHeight()) - 8.0f;
        auto c = area.getCentre();
        const float rMax = size * 0.5f, rMin = size * 0.12f;
        const float tw = juce::MathConstants<float>::twoPi;

        g.setColour (ground);
        g.fillEllipse (c.x - rMax - 3, c.y - rMax - 3, (rMax + 3) * 2, (rMax + 3) * 2);

        for (int t = 0; t < NT; ++t)
        {
            const auto& r = rings[(size_t) t];
            const float rad = rMin + (rMax - rMin) * ((float) t + 0.5f) / (float) NT;
            const auto col = ArmColours::lane (t).withMultipliedAlpha (r.mute ? 0.25f : 1.0f);
            g.setColour (line);
            g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
            for (int i = 0; i < r.len; ++i)
            {
                const float a = tw * (float) i / (float) r.len - juce::MathConstants<float>::halfPi;
                const juce::Point<float> tick (c.x + std::cos (a) * rad, c.y + std::sin (a) * rad);
                g.setColour (line2);
                g.fillEllipse (tick.x - 1.0f, tick.y - 1.0f, 2.0f, 2.0f);
                const int v = r.vel[(size_t) i];
                if (v <= 0) continue;
                const float aa = tw * ((float) i + r.off[(size_t) i]) / (float) r.len - juce::MathConstants<float>::halfPi;
                const float d = 3.0f + 4.0f * (float) v / 127.0f;
                g.setColour (col.withAlpha (v < 40 ? 0.5f : 0.95f));
                g.fillEllipse (c.x + std::cos (aa) * rad - d * 0.5f, c.y + std::sin (aa) * rad - d * 0.5f, d, d);
            }
            if (playing)
            {
                const float ah = tw * (float) (r.head / r.len) - juce::MathConstants<float>::halfPi;
                g.setColour (col.withAlpha (0.35f));
                g.drawLine (c.x + std::cos (ah) * (rad - 4), c.y + std::sin (ah) * (rad - 4),
                            c.x + std::cos (ah) * (rad + 4), c.y + std::sin (ah) * (rad + 4), 2.5f);
            }
        }
        g.setColour (sodium);
        g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
    }

private:
    GlappProcessor& proc;
    struct Ring { int len = 16; bool mute = false; double head = 0.0; std::array<int, MAXS> vel {}; std::array<float, MAXS> off {}; };
    std::array<Ring, NT> rings;
    bool playing = false;
};

// ---------------------------------------------------------------- huvudfönstret
GlappEditor::GlappEditor (GlappProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    auto* lg = new GlappLogo();
    lg->onClick = [this] { about.setVisible (true); about.toFront (false); };
    logo.reset (lg);
    addAndMakeVisible (*logo);

    prevBtn.onClick = [this] { const int c = proc.currentPreset.load(); proc.loadPreset (c <= 0 ? NUM_PRESETS - 1 : c - 1); };
    nextBtn.onClick = [this] { const int c = proc.currentPreset.load(); proc.loadPreset (c < 0 || c >= NUM_PRESETS - 1 ? 0 : c + 1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    presetBtn.setTooltip (U ("200 presets i fyra stilar. Frysta spår lämnas i fred."));
    for (auto* b : { &prevBtn, &nextBtn, &presetBtn }) addAndMakeVisible (*b);

    stil.attach (s, "stil");
    stil.setTooltip (U ("Stilen styr tärningens regler: hur sent, hur glest, vilka längder."));
    addAndMakeVisible (stil);

    diceAllBtn.setColour (juce::TextButton::buttonColourId, sodiumDk);
    diceAllBtn.setColour (juce::TextButton::textColourOffId, text);
    diceAllBtn.setTooltip (U ("Nytt groove på alla spår som inte är frysta. Slumpen följer stilens regler, aldrig kaos."));
    diceAllBtn.onClick = [this] { proc.diceAll(); };
    bigDice.setTooltip (diceAllBtn.getTooltip());
    bigDice.onClick = [this] { proc.diceAll(); };
    orbitBtn.setTooltip (U ("Nya rörelser över tid (ORBIT och HÄNDELSER), samma mönster."));
    orbitBtn.onClick = [this] { proc.orbitEvolve(); };
    undoBtn.setTooltip (U ("Tillbaka till hur det var innan senaste tärning eller preset."));
    undoBtn.onClick = [this] { proc.undo(); };
    for (auto* b : { &diceAllBtn, &orbitBtn, &undoBtn }) addAndMakeVisible (*b);
    addAndMakeVisible (bigDice);

    spela.attach (s, "spela");
    spela.setTooltip (U ("Egen klocka när DAW:en står still. Spelar DAW:en följer GLAPP den."));
    prov.attach (s, "prov");
    prov.setTooltip (U ("Inbyggda provljud så att du hör GLAPP direkt. Stäng av när BETONG tar över."));
    prov.setColour (juce::TextButton::buttonOnColourId, cold);
    tempo.attach (s, "tempo", "TEMPO");
    for (auto* c : std::initializer_list<juce::Component*> { &spela, &prov, &tempo }) addAndMakeVisible (*c);

    for (int t = 0; t < NT; ++t)
    {
        lanes.push_back (std::make_unique<LaneRow> (proc, t, [this] (int i) { selectLane (i); }));
        addAndMakeVisible (*lanes.back());
    }

    urverk = std::make_unique<Urverket> (proc);
    addAndMakeVisible (*urverk);

    slap.attach (s, "slap", U ("SLÄP"));
    slap.slider.setTooltip (U ("Skalar allt släp på alla spår. 0 % = rakt, 200 % = riktigt fullt."));
    human.attach (s, "human", "HUMAN");
    human.slider.setTooltip (U ("Skalar all jitter."));
    provNiva.attach (s, "provniva", U ("PROV NIVÅ"));
    provNiva.setAccent (cold);
    folj.attach (s, "foljtempo");
    folj.setTooltip (U ("Släppet är inställt för 122 BPM och följer med tempot, så att känslan blir densamma."));
    interlock.attach (s, "interlock");
    interlock.setTooltip (U ("Två spår får aldrig slå exakt samtidigt. Spåret längre upp vinner, kicken vinner alltid."));
    ilMode.attach (s, "ilmode");
    ilMode.setTooltip (U ("Vad som händer med förloraren: SPÖKE = svag spöknot, FLYTTA = en sen 32-del, TYST = tas bort."));
    for (auto* c : std::initializer_list<juce::Component*> { &slap, &human, &provNiva, &folj, &interlock, &ilMode }) addAndMakeVisible (*c);

    rensa.setTooltip (U ("Töm mönstret på valt spår."));
    rensa.onClick = [this] { proc.clearLane (selected); };
    for (auto* c : std::initializer_list<juce::Component*> { &takt, &orbit, &orbTakt, &hand, &handTakt, &ackord, &note, &kanal, &niva, &chans, &gate, &rensa })
        addAndMakeVisible (*c);

    addChildComponent (about);
    selectLane (selected);

    setSize (1240, 790);
    startTimerHz (30);
}

GlappEditor::~GlappEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GlappEditor::selectLane (int t)
{
    selected = t;
    auto& s = proc.apvts;
    takt.attach (s, tid (t, "takt"));
    orbit.attach (s, tid (t, "orbit"));
    orbTakt.attach (s, tid (t, "orbtakt"));
    hand.attach (s, tid (t, "hand"));
    handTakt.attach (s, tid (t, "handtakt"));
    ackord.attach (s, tid (t, "ackord"));
    note.attach (s, tid (t, "not"), "NOT");
    kanal.attach (s, tid (t, "kanal"), "KANAL");
    niva.attach (s, tid (t, "niva"), U ("NIVÅ"));
    chans.attach (s, tid (t, "chans"), "CHANS");
    gate.attach (s, tid (t, "gate"), U ("NOTLÄNGD"));
    takt.setTooltip (U ("Stegets längd. Trioler (T) skaver mot raka spår."));
    orbit.setTooltip (U ("Rörelse över tid. STEG +/− vrider mönstret ett steg, GLID låter släppet andas fram och tillbaka."));
    orbTakt.setTooltip (U ("Hur ofta orbit flyttar, i takter."));
    hand.setTooltip (U ("Vad som händer på sista slaget i frasen: TYST, HICKA (kastas sent), RATCHET (trippelslag) eller HOPP (slaget hoppas över)."));
    handTakt.setTooltip (U ("Frasens längd i takter. Händelsen sker i sista takten."));
    ackord.setTooltip (U ("Spela flera toner per träff. Bäst på ACKORD-spåret till en synt."));
    for (auto* k : { &note, &kanal, &niva, &chans, &gate }) k->setAccent (ArmColours::lane (t));
    for (int i = 0; i < (int) lanes.size(); ++i) lanes[(size_t) i]->setSelected (i == t);
    repaint (shopArea);
}

void GlappEditor::showPresetMenu()
{
    juce::PopupMenu m;
    const int cur = proc.currentPreset.load();
    for (int s = 0; s < 4; ++s)
    {
        juce::PopupMenu sub;
        for (int i = s * 50; i < s * 50 + 50; ++i)
            sub.addItem (i + 1, U (presetName (i).c_str()), true, i == cur);
        m.addSubMenu (U (styleNames()[(size_t) s].c_str()), sub, true, nullptr, cur >= s * 50 && cur < s * 50 + 50);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn),
                     [this] (int r) { if (r > 0) proc.loadPreset (r - 1); });
}

void GlappEditor::timerCallback()
{
    proc.readConfig (view);
    view.bpm = proc.uiBpm.load();
    view.barPpq = proc.uiBarPpq.load();
    const double ppq = proc.uiPpq.load();
    const bool playing = proc.uiPlaying.load();
    for (int t = 0; t < NT; ++t)
        lanes[(size_t) t]->setView (view, ppq, playing, proc.uiHits[(size_t) t].load());
    urverk->setView (view, ppq, playing);

    const auto lbl = proc.presetLabel();
    if (presetBtn.getButtonText() != lbl) presetBtn.setButtonText (lbl);
    undoBtn.setEnabled (proc.canUndo());
}

void GlappEditor::paint (juce::Graphics& g)
{
    g.fillAll (ground);

    // varningsrand längst upp
    auto stripe = getLocalBounds().removeFromTop (4).toFloat();
    g.saveState();
    g.reduceClipRegion (stripe.toNearestInt());
    for (float x = -20.0f; x < stripe.getRight(); x += 20.0f)
    {
        juce::Path p;
        p.addQuadrilateral (x, stripe.getBottom(), x + 10.0f, stripe.getY(), x + 20.0f, stripe.getY(), x + 10.0f, stripe.getBottom());
        g.setColour (sodium.withAlpha (0.8f));
        g.fillPath (p);
    }
    g.restoreState();

    drawPanel (g, urArea, "URVERKET", U ("hur spåren glider isär"));
    drawPanel (g, grooveArea, "GROOVE");
    drawPanel (g, shopArea, U ("VERKSTAD  ·  ") + U (roleName (selected)), U ("klick = på/av · dra = velocity · scroll = tonhöjd · shift = spöknot · högerklick = rensa steg"));

    label (g, { stil.getX(), stil.getY() - 13, 120, 12 }, "STIL");
    label (g, { presetBtn.getX(), presetBtn.getY() - 13, 120, 12 }, "PRESET");
    for (auto* c : std::initializer_list<juce::Component*> { &takt, &orbit, &orbTakt, &hand, &handTakt, &ackord })
    {
        static const char* names[] = { "TAKT", "ORBIT", "ORBIT VAR (TAKTER)", "HÄNDELSE", "FRAS (TAKTER)", "ACKORD" };
        const int i = (int) (c == &takt ? 0 : c == &orbit ? 1 : c == &orbTakt ? 2 : c == &hand ? 3 : c == &handTakt ? 4 : 5);
        label (g, { c->getX(), c->getY() - 13, c->getWidth() + 40, 12 }, U (names[i]));
    }
    label (g, { ilMode.getX(), ilMode.getY() - 13, 140, 12 }, U ("INTERLOCK LÄGE"));
}

void GlappEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (2);
    about.setBounds (getLocalBounds());

    // toppen
    topArea = r.removeFromTop (74);
    {
        auto t = topArea;
        logo->setBounds (t.removeFromLeft (190));
        t.removeFromLeft (10);
        auto pr = t.removeFromLeft (330).withTrimmedTop (22).withHeight (32);
        prevBtn.setBounds (pr.removeFromLeft (30));
        pr.removeFromLeft (4);
        nextBtn.setBounds (pr.removeFromRight (30));
        pr.removeFromRight (4);
        presetBtn.setBounds (pr);
        t.removeFromLeft (14);
        stil.setBounds (t.removeFromLeft (130).withTrimmedTop (22).withHeight (32));
        t.removeFromLeft (14);
        auto db = t.removeFromLeft (230).withTrimmedTop (14).withHeight (46);
        bigDice.setBounds (db.removeFromLeft (46).reduced (2));
        db.removeFromLeft (6);
        diceAllBtn.setBounds (db);
        t.removeFromLeft (10);
        auto ob = t.removeFromLeft (120);
        orbitBtn.setBounds (ob.withTrimmedTop (14).withHeight (22));
        undoBtn.setBounds (ob.withTrimmedTop (40).withHeight (20));
        t.removeFromLeft (10);
        tempo.setBounds (t.removeFromRight (64));
        auto tg = t.removeFromRight (100);
        spela.setBounds (tg.withTrimmedTop (14).withHeight (22));
        prov.setBounds (tg.withTrimmedTop (40).withHeight (20));
    }
    r.removeFromTop (10);

    // verkstaden längst ner
    shopArea = r.removeFromBottom (112);
    {
        auto s = shopArea.reduced (14, 10).withTrimmedTop (26);
        auto combos = s.removeFromLeft (660);
        const int cw = 104;
        for (auto* c : { &takt, &orbit, &orbTakt, &hand, &handTakt, &ackord })
        {
            c->setBounds (combos.removeFromLeft (cw).withTrimmedTop (16).withHeight (26));
            combos.removeFromLeft (6);
        }
        s.removeFromLeft (16);
        rensa.setBounds (s.removeFromRight (80).withTrimmedTop (16).withHeight (26));
        s.removeFromRight (10);
        const int kw = s.getWidth() / 5;
        for (auto* k : { &note, &kanal, &niva, &chans, &gate }) k->setBounds (s.removeFromLeft (kw).withSizeKeepingCentre (70, 76));
    }
    r.removeFromBottom (10);

    // höger kolumn
    rightArea = r.removeFromRight (330);
    grooveArea = rightArea.removeFromBottom (150);
    rightArea.removeFromBottom (10);
    urArea = rightArea;
    urverk->setBounds (urArea.reduced (12).withTrimmedTop (22));
    {
        auto g = grooveArea.reduced (12, 8).withTrimmedTop (22);
        auto knobs = g.removeFromLeft (200);
        for (auto* k : { &slap, &human, &provNiva }) k->setBounds (knobs.removeFromLeft (66).withSizeKeepingCentre (64, 80));
        g.removeFromLeft (8);
        folj.setBounds (g.removeFromTop (24));
        g.removeFromTop (4);
        interlock.setBounds (g.removeFromTop (24));
        g.removeFromTop (18);
        ilMode.setBounds (g.removeFromTop (24));
    }

    // spåren
    r.removeFromRight (10);
    laneArea = r;
    const int h = r.getHeight() / NT;
    for (auto& l : lanes) l->setBounds (r.removeFromTop (h).reduced (0, 2));
}
