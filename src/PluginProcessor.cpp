#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace gl;

namespace
{
const char* kKeys[17] = { "tyst", "frys", "langd", "takt", "not", "kanal", "niva", "drag", "trog",
                          "jitter", "chans", "gate", "orbit", "orbtakt", "hand", "handtakt", "ackord" };
enum K { KTyst, KFrys, KLangd, KTakt, KNot, KKanal, KNiva, KDrag, KTrog, KJitter, KChans, KGate, KOrbit, KOrbTakt, KHand, KHandTakt, KAckord };
}

GlappProcessor::GlappProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GLAPP", createLayout())
{
    for (int t = 0; t < NT; ++t)
        for (int k = 0; k < 17; ++k)
            raw[(size_t) t].p[k] = apvts.getRawParameterValue (tid (t, kKeys[k]));
    pSlap = apvts.getRawParameterValue ("slap");
    pHuman = apvts.getRawParameterValue ("human");
    pTomrum = apvts.getRawParameterValue ("tomrum");
    pFolj = apvts.getRawParameterValue ("foljtempo");
    pIl = apvts.getRawParameterValue ("interlock");
    pIlMode = apvts.getRawParameterValue ("ilmode");
    pProv = apvts.getRawParameterValue ("prov");
    pProvNiva = apvts.getRawParameterValue ("provniva");
    pSpela = apvts.getRawParameterValue ("spela");
    pTempo = apvts.getRawParameterValue ("tempo");

    pending.reserve (512);
    events.reserve (512);
    engine.pat = &pattern;

    // Startläge: första Bukarest-presetet, så att det låter direkt.
    loadPreset (0);
    undoStack.clear();

    // Som fristående app finns ingen DAW: då startar GLAPP med eget ljud och egen klocka.
    if (wrapperType == wrapperType_Standalone)
    {
        setParam ("prov", 1.0f);
        setParam ("spela", 1.0f);
    }
}

bool GlappProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
}

void GlappProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    prov.prepare (sr);
    pending.clear();
    wasRunning = false;
    expectedPpq = -1.0e9;
}

// ---------------------------------------------------------------- inställningar -> motor
void GlappProcessor::readConfig (Engine& e) const
{
    for (int t = 0; t < NT; ++t)
    {
        const auto& r = raw[(size_t) t];
        auto& c = e.tr[(size_t) t];
        c.role = t;
        c.mute = r.p[KTyst]->load() > 0.5f;
        c.length = juce::jlimit (1, MAXS, juce::roundToInt (r.p[KLangd]->load()));
        c.div = juce::roundToInt (r.p[KTakt]->load());
        c.note = juce::roundToInt (r.p[KNot]->load());
        c.channel = juce::roundToInt (r.p[KKanal]->load());
        c.level = r.p[KNiva]->load();
        c.drag = r.p[KDrag]->load();
        c.inertia = r.p[KTrog]->load();
        c.jitter = r.p[KJitter]->load();
        c.chance = r.p[KChans]->load();
        c.gate = r.p[KGate]->load();
        c.orbit = juce::roundToInt (r.p[KOrbit]->load());
        c.orbitBars = juce::roundToInt (r.p[KOrbTakt]->load());
        c.event = juce::roundToInt (r.p[KHand]->load());
        c.eventBars = juce::roundToInt (r.p[KHandTakt]->load());
        c.chord = juce::roundToInt (r.p[KAckord]->load());
    }
    e.g.slap = pSlap->load();
    e.g.human = pHuman->load();
    e.g.tomrum = pTomrum->load();
    e.g.followTempo = pFolj->load() > 0.5f;
    e.g.interlock = pIl->load() > 0.5f;
    e.g.ilMode = juce::roundToInt (pIlMode->load());
    e.g.seed = seed.load();
    e.pat = &pattern;
}

// ---------------------------------------------------------------- ljudtråden
void GlappProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    readConfig (engine);

    // Tid: följ DAW:en när den spelar, annars den egna klockan (SPELA).
    double bpm = (double) pTempo->load();
    double ppq = 0.0;
    double barPpq = 4.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto ts = pos->getTimeSignature())
                if (ts->numerator > 0 && ts->denominator > 0) barPpq = ts->numerator * 4.0 / ts->denominator;
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) { hostPlaying = true; ppq = *p; }
        }
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);

    const bool spela = pSpela->load() > 0.5f;
    if (! hostPlaying)
    {
        if (spela && ! internalWasOn) internalPpq = 0.0;
        ppq = internalPpq;
    }
    internalWasOn = spela;
    const bool running = hostPlaying || spela;

    engine.bpm = bpm;
    engine.barPpq = barPpq;
    const double ppqPerSample = bpm / 60.0 / sr;
    const double end = ppq + n * ppqPerSample;

    // Stopp eller hopp i tiden: släpp alla hängande noter.
    if (! running || ! wasRunning || std::abs (ppq - expectedPpq) > 1.0e-3)
    {
        for (auto& p : pending) midi.addEvent (juce::MidiMessage::noteOff (p.channel, p.note), 0);
        pending.clear();
    }

    const bool provOn = pProv->load() > 0.5f;
    const float provLevel = pProvNiva->load();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    int rendered = 0;

    if (running)
    {
        events.clear();
        engine.collect (ppq, end, events);

        for (auto& e : events)
        {
            const int s = juce::jlimit (0, n - 1, (int) std::floor ((e.ppq - ppq) / ppqPerSample));
            const int64_t on = sampleClock + s;

            // samma ton spelas redan: släpp den först
            for (size_t i = 0; i < pending.size(); ++i)
                if (pending[i].note == e.note && pending[i].channel == e.channel)
                {
                    midi.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), (int) juce::jlimit<int64_t> (0, s, pending[i].at - sampleClock));
                    pending.erase (pending.begin() + (long) i);
                    break;
                }

            midi.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.vel), s);
            const int64_t len = juce::jmax<int64_t> (1, (int64_t) (e.gatePpq / ppqPerSample));
            if (pending.size() < 500) pending.push_back ({ e.note, e.channel, on + len });

            if (provOn)
            {
                if (s > rendered) { prov.render (L, R, rendered, s - rendered, provLevel); rendered = s; }
                prov.trigger (e.track, e.note, e.vel);
            }
            uiHits[(size_t) e.track].fetch_add (1, std::memory_order_relaxed);
        }
    }

    // noter vars längd tar slut i det här blocket
    for (size_t i = 0; i < pending.size();)
    {
        if (pending[i].at < sampleClock + n)
        {
            midi.addEvent (juce::MidiMessage::noteOff (pending[i].channel, pending[i].note), (int) juce::jmax<int64_t> (0, pending[i].at - sampleClock));
            pending.erase (pending.begin() + (long) i);
        }
        else ++i;
    }

    if (provOn && rendered < n) prov.render (L, R, rendered, n - rendered, provLevel);
    if (provOn)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] = std::tanh (d[i]);
        }

    sampleClock += n;
    expectedPpq = end;
    wasRunning = running;
    if (! hostPlaying && spela) internalPpq = end;

    uiPpq.store (ppq);
    uiBpm.store (bpm);
    uiBarPpq.store (barPpq);
    uiPlaying.store (running);
}

// ---------------------------------------------------------------- tärning, presets, ångra
void GlappProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void GlappProcessor::applyTrack (int t, const TrackGen& g)
{
    const auto& c = g.cfg;
    setParam (tid (t, "langd"), (float) c.length);
    setParam (tid (t, "takt"), (float) c.div);
    setParam (tid (t, "drag"), c.drag);
    setParam (tid (t, "trog"), c.inertia);
    setParam (tid (t, "jitter"), c.jitter);
    setParam (tid (t, "chans"), c.chance);
    setParam (tid (t, "gate"), c.gate);
    setParam (tid (t, "orbit"), (float) c.orbit);
    setParam (tid (t, "orbtakt"), (float) c.orbitBars);
    setParam (tid (t, "hand"), (float) c.event);
    setParam (tid (t, "handtakt"), (float) c.eventBars);
    setParam (tid (t, "ackord"), (float) c.chord);
    for (int i = 0; i < MAXS; ++i) pattern.set (t, i, g.steps[(size_t) i]);
}

void GlappProcessor::diceAll()
{
    pushUndo();
    Engine cur;
    readConfig (cur);
    const int style = juce::roundToInt (apvts.getRawParameterValue ("stil")->load());
    Rng r ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    int perk1Len = cur.tr[RPerk1].length;
    for (int t = 0; t < NT; ++t)
    {
        const bool frozen = apvts.getRawParameterValue (tid (t, "frys"))->load() > 0.5f;
        if (frozen) { if (t == RPerk1) perk1Len = cur.tr[RPerk1].length; continue; }
        auto g = diceTrack (t, style, r, cur.tr[(size_t) t], t == RPerk2 ? perk1Len : (t == RPerk1 ? cur.tr[RPerk2].length : 0));
        if (t == RPerk1) perk1Len = g.cfg.length;
        applyTrack (t, g);
    }
    auto gg = diceGlobal (style, r);
    setParam ("slap", gg.slap);
    setParam ("human", gg.human);
    setParam ("ilmode", (float) gg.ilMode);
    setParam ("tomrum", gg.tomrum);
    seed.store ((uint32_t) r.next());
    currentPreset.store (-1);
}

void GlappProcessor::diceLane (int t)
{
    pushUndo();
    Engine cur;
    readConfig (cur);
    const int style = juce::roundToInt (apvts.getRawParameterValue ("stil")->load());
    Rng r ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    const int avoid = t == RPerk1 ? cur.tr[RPerk2].length : (t == RPerk2 ? cur.tr[RPerk1].length : 0);
    applyTrack (t, diceTrack (t, style, r, cur.tr[(size_t) t], avoid));
    currentPreset.store (-1);
}

void GlappProcessor::orbitEvolve()
{
    pushUndo();
    Engine cur;
    readConfig (cur);
    Rng r ((uint64_t) juce::Random::getSystemRandom().nextInt64());
    for (int t = 0; t < NT; ++t)
    {
        if (apvts.getRawParameterValue (tid (t, "frys"))->load() > 0.5f) continue;
        auto c = cur.tr[(size_t) t];
        diceOrbit (t, r, c);
        setParam (tid (t, "orbit"), (float) c.orbit);
        setParam (tid (t, "orbtakt"), (float) c.orbitBars);
        setParam (tid (t, "hand"), (float) c.event);
        setParam (tid (t, "handtakt"), (float) c.eventBars);
    }
    currentPreset.store (-1);
}

void GlappProcessor::clearLane (int t)
{
    pushUndo();
    for (int i = 0; i < MAXS; ++i) pattern.set (t, i, {});
}

void GlappProcessor::loadPreset (int index)
{
    index = juce::jlimit (0, NUM_PRESETS - 1, index);
    pushUndo();
    Engine cur;
    readConfig (cur);
    Rng r (presetSeed (index));
    const int style = presetStyle (index);
    int perk1Len = 0;
    for (int t = 0; t < NT; ++t)
    {
        // frysta spår lämnas i fred även vid preset-byte
        const bool frozen = apvts.getRawParameterValue (tid (t, "frys"))->load() > 0.5f;
        auto g = diceTrack (t, style, r, cur.tr[(size_t) t], t == RPerk2 ? perk1Len : 0);
        if (t == RPerk1) perk1Len = g.cfg.length;
        if (! frozen) applyTrack (t, g);
    }
    auto gg = diceGlobal (style, r);
    setParam ("stil", (float) style);
    setParam ("slap", gg.slap);
    setParam ("human", gg.human);
    setParam ("interlock", gg.interlock ? 1.0f : 0.0f);
    setParam ("ilmode", (float) gg.ilMode);
    setParam ("tomrum", gg.tomrum);
    seed.store ((uint32_t) (presetSeed (index) & 0xffffffffu));
    currentPreset.store (index);
}

juce::String GlappProcessor::presetLabel() const
{
    const int p = currentPreset.load();
    return p >= 0 ? juce::String::fromUTF8 (presetName (p).c_str()) : juce::String::fromUTF8 ("EGET (tärnat)");
}

juce::ValueTree GlappProcessor::snapshot()
{
    auto tree = apvts.copyState();
    juce::ValueTree pt ("PATTERN");
    for (int t = 0; t < NT; ++t) pt.setProperty ("t" + juce::String (t), juce::String (pattern.trackToString (t)), nullptr);
    pt.setProperty ("seed", (int) seed.load(), nullptr);
    pt.setProperty ("preset", currentPreset.load(), nullptr);
    tree.removeChild (tree.getChildWithName ("PATTERN"), nullptr);
    tree.appendChild (pt, nullptr);
    return tree;
}

void GlappProcessor::restore (const juce::ValueTree& tree)
{
    if (! tree.isValid()) return;
    auto pt = tree.getChildWithName ("PATTERN");
    auto copy = tree.createCopy();
    copy.removeChild (copy.getChildWithName ("PATTERN"), nullptr);
    apvts.replaceState (copy);
    if (pt.isValid())
    {
        for (int t = 0; t < NT; ++t)
            pattern.trackFromString (t, pt.getProperty ("t" + juce::String (t)).toString().toStdString());
        seed.store ((uint32_t) (int) pt.getProperty ("seed", 1));
        currentPreset.store ((int) pt.getProperty ("preset", -1));
    }
}

void GlappProcessor::pushUndo()
{
    undoStack.push_back (snapshot());
    if (undoStack.size() > 40) undoStack.erase (undoStack.begin());
}

bool GlappProcessor::undo()
{
    if (undoStack.empty()) return false;
    auto t = undoStack.back();
    undoStack.pop_back();
    restore (t);
    return true;
}

void GlappProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = snapshot().createXml()) copyXmlToBinary (*xml, destData);
}

void GlappProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restore (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* GlappProcessor::createEditor() { return new GlappEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlappProcessor(); }
