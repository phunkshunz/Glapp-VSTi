#include "PluginEditor.h"

using namespace gl;
using namespace ArmColours;

namespace
{
juce::String U (const char* s) { return sprak::tr (juce::String::fromUTF8 (s)); }

void label (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, juce::Colour c = muted, float size = 9.5f,
            juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour (c == muted ? mh::Form::ink3() : c);
    g.setFont (mh::Form::text (size + 0.5f, false, 0.1f));
    g.drawText (sprak::tr (t), r, j, false);
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
        // ordmärket: bara namnet, i Plex, med luft mellan bokstäverna. Inga ikoner.
        auto r = getLocalBounds();
        g.setColour (mh::Form::ink());
        g.setFont (mh::Form::text (30.0f, true, 0.22f));
        g.drawText ("GLAPP", r.removeFromTop (40), juce::Justification::centredLeft);
        g.setColour (mh::Form::ink3());
        g.setFont (mh::Form::text (9.5f, false, 0.2f));
        g.drawText ("MASKINHALLEN  /  PHUNKSHUNZ", r.removeFromTop (14), juce::Justification::centredLeft);
        // signalfärgen, en gång: en liten fyrkant efter namnet
        g.setColour (mh::Form::accent());
        const float wx = 6.0f + juce::GlyphArrangement::getStringWidth (mh::Form::text (30.0f, true, 0.22f), "GLAPP");
        g.fillRect (juce::Rectangle<float> (wx, 25.0f, 5.0f, 5.0f));
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
        const auto col = ArmColours::lane (t);   // spårets egen färg
        const float cw = r.getWidth() / (float) len;
        const int cur = (int) std::floor (head);

        for (int i = 0; i < len; ++i)
        {
            auto c = juce::Rectangle<float> (r.getX() + i * cw, r.getY(), cw, r.getHeight()).reduced (1.0f, 4.0f);
            const bool beat = (i % 4) == 0;
            g.setColour (beat ? mh::Form::cell() : mh::Form::raised());
            g.fillRect (c);
            const auto st = proc.pattern.get (t, i);
            if (st.vel > 0)
            {
                const float h = c.getHeight() * (0.18f + 0.82f * (float) st.vel / 127.0f);
                auto bar = c.withTop (c.getBottom() - h);
                g.setColour (st.vel < 40 ? col.withAlpha (0.45f) : col.withAlpha (0.95f));
                g.fillRect (bar);
                // var noten faktiskt landar: en tunn linje förskjuten med släpet
                const float ox = c.getX() + juce::jlimit (0.0f, 1.0f, offs[(size_t) i]) * cw;
                g.setColour (mh::Form::ink());
                g.fillRect (juce::Rectangle<float> (ox, c.getY() - 3.0f, 1.5f, 3.0f));
                if (st.pitch != 0 && cw > 11.0f)
                {
                    g.setColour (mh::Form::bg());
                    g.setFont (mh::Form::mono (8.5f));
                    g.drawText ((st.pitch > 0 ? "+" : "") + juce::String (st.pitch), bar.toNearestInt(), juce::Justification::centredBottom, false);
                }
            }
            if (playing && i == cur)
            {
                // spelhuvudet: orange slöja över rutan och en tydlig orange list under
                g.setColour (mh::Form::accent().withAlpha (0.28f));
                g.fillRect (c);
                g.setColour (mh::Form::accent());
                g.fillRect (juce::Rectangle<float> (c.getX(), c.getBottom() + 1.0f, c.getWidth(), 3.0f));
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
        nameBtn.setColour (juce::TextButton::textColourOffId, mh::Form::ink());
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
        // formspråket: ingen ruta. En hårlinje under, och det valda spåret får en tunn signalrand till vänster.
        auto b = getLocalBounds();
        if (sel) { g.setColour (mh::Form::panel().brighter (0.03f)); g.fillRect (b); g.setColour (mh::Form::accent()); g.fillRect (b.withWidth (2)); }
        g.setColour (mh::Form::hair());
        g.fillRect (b.withTop (b.getBottom() - 1));
        g.setColour (flash > 0.05f ? mh::Form::accent().withAlpha (0.25f + 0.75f * flash) : mh::Form::hair2());
        g.fillEllipse (ledArea.toFloat().reduced (1.0f));
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


        for (int t = 0; t < NT; ++t)
        {
            const auto& r = rings[(size_t) t];
            const float rad = rMin + (rMax - rMin) * ((float) t + 0.5f) / (float) NT;
            const auto col = ArmColours::lane (t).withMultipliedAlpha (r.mute ? 0.25f : 1.0f);
            g.setColour (mh::Form::hair());
            g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
            for (int i = 0; i < r.len; ++i)
            {
                const float a = tw * (float) i / (float) r.len - juce::MathConstants<float>::halfPi;
                const juce::Point<float> tick (c.x + std::cos (a) * rad, c.y + std::sin (a) * rad);
                g.setColour (mh::Form::hair2());
                g.fillEllipse (tick.x - 0.8f, tick.y - 0.8f, 1.6f, 1.6f);
                const int v = r.vel[(size_t) i];
                if (v <= 0) continue;
                const float aa = tw * ((float) i + r.off[(size_t) i]) / (float) r.len - juce::MathConstants<float>::halfPi;
                const float d = 2.5f + 3.0f * (float) v / 127.0f;
                g.setColour (col.withAlpha (v < 40 ? 0.5f : 0.95f));
                g.fillEllipse (c.x + std::cos (aa) * rad - d * 0.5f, c.y + std::sin (aa) * rad - d * 0.5f, d, d);
            }
            if (playing)
            {
                const float ah = tw * (float) (r.head / r.len) - juce::MathConstants<float>::halfPi;
                g.setColour (mh::Form::accent().withAlpha (0.7f));
                g.drawLine (c.x + std::cos (ah) * (rad - 3), c.y + std::sin (ah) * (rad - 3),
                            c.x + std::cos (ah) * (rad + 3), c.y + std::sin (ah) * (rad + 3), 1.5f);
            }
        }
        g.setColour (mh::Form::accent());
        g.fillEllipse (c.x - 2.5f, c.y - 2.5f, 5.0f, 5.0f);
    }

private:
    GlappProcessor& proc;
    struct Ring { int len = 16; bool mute = false; double head = 0.0; std::array<int, MAXS> vel {}; std::array<float, MAXS> off {}; };
    std::array<Ring, NT> rings;
    bool playing = false;
};

// ---------------------------------------------------------------- huvudfönstret
GlappPanel::GlappPanel (GlappProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);   // all text (även tooltips och menyer) får Plex
    auto& s = proc.apvts;

    auto* lg = new GlappLogo();
    lg->onClick = [this] { about.setVisible (true); about.toFront (false); };
    logo.reset (lg);
    addAndMakeVisible (*logo);

    prevBtn.onClick = [this] { const int c = proc.currentPreset.load(); proc.loadPreset (c <= 0 ? NUM_PRESETS - 1 : c - 1); };
    nextBtn.onClick = [this] { const int c = proc.currentPreset.load(); proc.loadPreset (c < 0 || c >= NUM_PRESETS - 1 ? 0 : c + 1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    presetBtn.setTooltip (U ("350 presets i sju stilar. Frysta spår lämnas i fred."));
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
    tomrum.attach (s, "tomrum", "TOMRUM");
    tomrum.slider.setTooltip (U ("Groovet byggs av det som inte spelas. Stryker slag: hat och klick först, spöknoter före huvudslag, kicken aldrig. Samma slag försvinner alltid på samma ställe."));
    provNiva.attach (s, "provniva", U ("NIVÅ"));
    provNiva.slider.setTooltip (U ("Hur högt GLAPP spelar sina egna samplingar."));
    kit.attach (s, "kit");
    kit.setTooltip (U ("Sex tunga, mörka kit. De hårda presetsen väljer kit själva."));
    muller.attach (s, "muller", "MULLER");
    muller.slider.setTooltip (U ("Kicken genom ett mörkt betongrum, bara basen kvar och duckad av kicken: mullret under hård techno."));
    tryck.attach (s, "tryck", "TRYCK");
    tryck.slider.setTooltip (U ("Kicken klipps hårt, allt annat parallellkomprimeras och mättas. Mer = fläskigare och tätare."));
    morker.attach (s, "morker", U ("MÖRKER"));
    morker.slider.setTooltip (U ("Allt utom kicken tappar sin gnistrande diskant."));
    for (auto* c : std::initializer_list<juce::Component*> { &kit, &muller, &tryck, &morker }) addAndMakeVisible (*c);
    folj.attach (s, "foljtempo");
    folj.setTooltip (U ("Släppet är inställt för 122 BPM och följer med tempot, så att känslan blir densamma."));
    interlock.attach (s, "interlock");
    interlock.setTooltip (U ("Två spår får aldrig slå exakt samtidigt. Spåret längre upp vinner, kicken vinner alltid."));
    ilMode.attach (s, "ilmode");
    ilMode.setTooltip (U ("Vad som händer med förloraren: SPÖKE = svag spöknot, FLYTTA = en sen 32-del, TYST = tas bort."));
    navBtn.attach (s, "nav");
    navBtn.setTooltip (U ("NAV: lyssna på BETONG. Alla spår svänger med BETONGs swing och släp, och INTERLOCK viker undan för BETONGs kick. Slå på NAV i BETONG också. Grön prick = GLAPP hör BETONG."));
    for (auto* c : std::initializer_list<juce::Component*> { &slap, &human, &tomrum, &provNiva, &folj, &interlock, &ilMode, &navBtn }) addAndMakeVisible (*c);

    rensa.setTooltip (U ("Töm mönstret på valt spår."));
    rensa.onClick = [this] { proc.clearLane (selected); };
    for (auto* c : std::initializer_list<juce::Component*> { &takt, &orbit, &orbTakt, &hand, &handTakt, &ackord, &note, &kanal, &niva, &chans, &gate, &rensa })
        addAndMakeVisible (*c);

    addChildComponent (about);
    selectLane (selected);

    setSize (1120, 680);
    startTimerHz (30);
}

GlappPanel::~GlappPanel()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GlappPanel::selectLane (int t)
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
    hand.setTooltip (U ("Vad som händer på sista slaget i frasen: TYST, HICKA (kastas sent), RATCHET (trippelslag), HOPP (slaget hoppas över) eller STUDS (en skiftnyckel som studsar nerför en trappa: fem slag, allt tätare och svagare)."));
    handTakt.setTooltip (U ("Frasens längd i takter. Händelsen sker i sista takten."));
    ackord.setTooltip (U ("Spela flera toner per träff. Bäst på ACKORD-spåret till en synt."));
    for (auto* k : { &note, &kanal, &niva, &chans, &gate }) k->setAccent (ArmColours::lane (t));
    for (int i = 0; i < (int) lanes.size(); ++i) lanes[(size_t) i]->setSelected (i == t);
    repaint (shopArea);
}

void GlappPanel::showPresetMenu()
{
    juce::PopupMenu m;
    const int cur = proc.currentPreset.load();
    for (int s = 0; s < NSTYLES; ++s)
    {
        juce::PopupMenu sub;
        const int a = s * PRESETS_PER_STYLE, b = a + PRESETS_PER_STYLE;
        for (int i = a; i < b; ++i)
            sub.addItem (i + 1, U (presetName (i).c_str()), true, i == cur);
        m.addSubMenu (U (styleNames()[(size_t) s].c_str()), sub, true, nullptr, cur >= a && cur < b);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn),
                     [this] (int r) { if (r > 0) proc.loadPreset (r - 1); });
}

void GlappPanel::timerCallback()
{
    if (const int ns = proc.navStatus.load(); ns != navShown) { navShown = ns; repaint (grooveArea); }
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

void GlappPanel::paint (juce::Graphics& g)
{
    g.fillAll (ground);

    // hårlinje under toppraden, i stället för varningsranden
    g.setColour (mh::Form::hair());
    g.fillRect (0, topArea.getBottom(), getWidth(), 1);

    drawPanel (g, urArea, "URVERKET", U ("hur spåren glider isär"));
    drawPanel (g, grooveArea, "GROOVE");
    if (navShown > 0)
    {
        g.setColour (navShown == 2 ? juce::Colour (0xff5fd36a) : juce::Colour (0xff7a6a3a));
        g.fillEllipse ((float) navBtn.getX() - 12.f, (float) navBtn.getBounds().getCentreY() - 3.5f, 7.f, 7.f);
    }
    drawPanel (g, ljudArea, "LJUD", U ("tunga samplingar"));
    label (g, { kit.getX(), kit.getY() - 13, 120, 12 }, "KIT");
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

void GlappPanel::resized()
{
    // 1120 x 680 (förut 1240 x 790)
    auto r = getLocalBounds().reduced (12);
    about.setBounds (getLocalBounds());

    // toppen
    topArea = r.removeFromTop (60);
    {
        auto t = topArea;
        logo->setBounds (t.removeFromLeft (168).withTrimmedTop (4));
        t.removeFromLeft (8);
        auto pr = t.removeFromLeft (296).withTrimmedTop (18).withHeight (30);
        prevBtn.setBounds (pr.removeFromLeft (28));
        pr.removeFromLeft (4);
        nextBtn.setBounds (pr.removeFromRight (28));
        pr.removeFromRight (4);
        presetBtn.setBounds (pr);
        t.removeFromLeft (12);
        stil.setBounds (t.removeFromLeft (118).withTrimmedTop (18).withHeight (30));
        t.removeFromLeft (12);
        auto db = t.removeFromLeft (196).withTrimmedTop (10).withHeight (42);
        bigDice.setBounds (db.removeFromLeft (42).reduced (2));
        db.removeFromLeft (6);
        diceAllBtn.setBounds (db);
        t.removeFromLeft (10);
        auto ob = t.removeFromLeft (108);
        orbitBtn.setBounds (ob.withTrimmedTop (10).withHeight (21));
        undoBtn.setBounds (ob.withTrimmedTop (34).withHeight (19));
        t.removeFromLeft (8);
        tempo.setBounds (t.removeFromRight (58));
        auto tg = t.removeFromRight (90);
        spela.setBounds (tg.withTrimmedTop (10).withHeight (21));
        prov.setBounds (tg.withTrimmedTop (34).withHeight (19));
    }
    r.removeFromTop (8);

    // verkstaden längst ner
    shopArea = r.removeFromBottom (104);
    {
        auto s = shopArea.reduced (12, 6).withTrimmedTop (22);
        auto combos = s.removeFromLeft (606);
        for (auto* c : { &takt, &orbit, &orbTakt, &hand, &handTakt, &ackord })
        {
            c->setBounds (combos.removeFromLeft (96).withTrimmedTop (18).withHeight (26));
            combos.removeFromLeft (6);
        }
        s.removeFromLeft (14);
        rensa.setBounds (s.removeFromRight (76).withTrimmedTop (18).withHeight (26));
        s.removeFromRight (8);
        const int kw = s.getWidth() / 5;
        for (auto* k : { &note, &kanal, &niva, &chans, &gate }) k->setBounds (s.removeFromLeft (kw).withSizeKeepingCentre (juce::jmin (64, kw), s.getHeight()));
    }
    r.removeFromBottom (8);

    // höger kolumn: URVERKET, LJUD, GROOVE
    rightArea = r.removeFromRight (292);
    grooveArea = rightArea.removeFromBottom (132);
    rightArea.removeFromBottom (8);
    ljudArea = rightArea.removeFromBottom (136);
    rightArea.removeFromBottom (8);
    urArea = rightArea;
    urverk->setBounds (urArea.reduced (8).withTrimmedTop (18));
    {
        auto l = ljudArea.reduced (10, 6).withTrimmedTop (20);
        kit.setBounds (l.removeFromTop (34).withTrimmedTop (12).withHeight (22).withWidth (150));
        l.removeFromTop (4);
        const int w = l.getWidth() / 4;
        for (auto* k : { &provNiva, &muller, &tryck, &morker }) k->setBounds (l.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (62, w), l.getHeight()));
    }
    {
        navBtn.setBounds (grooveArea.getRight() - 54, grooveArea.getY() + 5, 46, 17);
        auto g = grooveArea.reduced (10, 6).withTrimmedTop (20);
        auto knobs = g.removeFromLeft (174);
        for (auto* k : { &slap, &human, &tomrum }) k->setBounds (knobs.removeFromLeft (58).withSizeKeepingCentre (58, g.getHeight()));
        g.removeFromLeft (6);
        folj.setBounds (g.removeFromTop (22));
        g.removeFromTop (4);
        interlock.setBounds (g.removeFromTop (22));
        g.removeFromTop (16);
        ilMode.setBounds (g.removeFromTop (22));
    }

    // spåren
    r.removeFromRight (10);
    laneArea = r;
    const int h = r.getHeight() / NT;
    for (auto& l : lanes) l->setBounds (r.removeFromTop (h).reduced (0, 1));
}

//==============================================================================
// Ramen runt panelen: SV | EN-knappen bygger om panelen på valt språk.
GlappEditor::GlappEditor (GlappProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    sprak::laddaOm();
    byggPanel();
    sprakKnapp.accent = sodium;
    sprakKnapp.onClick = [this]
    {
        sprak::sattEngelska (! sprak::engelska());
        juce::Component::SafePointer<GlappEditor> self (this);
        juce::MessageManager::callAsync ([self] { if (self != nullptr) self->byggPanel(); });
    };
    addAndMakeVisible (sprakKnapp);
    setSize (1120, 680);
}

GlappEditor::~GlappEditor() = default;

void GlappEditor::byggPanel()
{
    panel.reset();
    panel = std::make_unique<GlappPanel> (proc);
    sprak::oversattAllt (*panel);
    addAndMakeVisible (*panel);
    sprakKnapp.toFront (false);
    sprakKnapp.repaint();
    resized();
}

void GlappEditor::resized()
{
    if (panel != nullptr) panel->setBounds (getLocalBounds());
    // under kugghjulen i loggan, där det är tomt
    sprakKnapp.setBounds (12 + 142, 14 + 52, 38, 14);
}
