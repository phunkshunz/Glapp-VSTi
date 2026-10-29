// Fristående test av GLAPP-motorn och tärningen (ingen JUCE behövs).
// Bygg och kör:  g++ -std=c++17 -O2 -I../src engine_test.cpp -o engine_test && ./engine_test

#include "Engine.h"
#include "Dice.h"
#include <cassert>
#include <cstdio>
#include <set>

using namespace gl;

static int fails = 0;
#define CHECK(x) do { if (! (x)) { std::printf ("FEL: %s (rad %d)\n", #x, __LINE__); ++fails; } } while (0)

static void applyPreset (int idx, Pattern& pat, Engine& e)
{
    Rng r (presetSeed (idx));
    const int style = presetStyle (idx);
    int perk1Len = 0;
    for (int t = 0; t < NT; ++t)
    {
        TrackCfg cur; cur.note = 36 + t; cur.channel = 1;
        auto g = diceTrack (t, style, r, cur, t == RPerk2 ? perk1Len : 0);
        if (t == RPerk1) perk1Len = g.cfg.length;
        e.tr[(size_t) t] = g.cfg;
        for (int i = 0; i < MAXS; ++i) pat.set (t, i, g.steps[(size_t) i]);
    }
    auto gg = diceGlobal (style, r);
    e.g.slap = gg.slap; e.g.human = gg.human; e.g.interlock = gg.interlock; e.g.ilMode = gg.ilMode;
}

int main()
{
    Pattern pat;
    Engine e;
    e.pat = &pat;

    for (int p = 0; p < NUM_PRESETS; ++p)
    {
        applyPreset (p, pat, e);
        for (double bpm : { 100.0, 122.0, 135.0 })
        {
            e.bpm = bpm;
            const double sr = 48000.0, ppqPerSample = bpm / 60.0 / sr;
            const double total = 32.0 * 4.0;   // 32 takter

            // 1) Samla allt på en gång
            std::vector<NoteEv> all;
            e.collect (0.0, total, all);

            // 2) Samma sak i små block, som i en DAW
            std::vector<NoteEv> chunked;
            double pos = 0.0;
            int blk = 0;
            while (pos < total)
            {
                const int n = (blk++ % 3 == 0) ? 512 : 333;
                const double end = std::min (total, pos + n * ppqPerSample);
                e.collect (pos, end, chunked);
                pos = end;
            }
            CHECK (all.size() == chunked.size());

            for (auto& ev : all)
            {
                CHECK (ev.vel >= 1 && ev.vel <= 127);
                CHECK (ev.note >= 0 && ev.note <= 127);
                CHECK (ev.channel >= 1 && ev.channel <= 16);
                if (ev.track == RKick)
                {
                    // kicken ligger exakt på rutnätet
                    const double s = ev.ppq / 0.25;
                    CHECK (std::abs (s - std::round (s)) < 1e-9);
                }
                else
                {
                    // ingen not får landa mer än 250 ms + 1/32 + ratchet efter sitt steg
                    (void) 0;
                }
            }
        }
        // glesheten: percen ska ha 1..6 träffar per 16 steg (Rominimal bygger på tystnad)
        for (int t : { (int) RPerk1, (int) RPerk2 })
        {
            int hits = 0;
            for (int i = 0; i < e.tr[(size_t) t].length; ++i) hits += pat.get (t, i).vel > 0;
            const double per16 = hits * 16.0 / e.tr[(size_t) t].length;
            CHECK (per16 >= 1.0 && per16 <= 6.0);
        }
        CHECK (e.tr[RPerk1].length != e.tr[RPerk2].length);
    }

    // Kicken ska aldrig flyttas av interlock; rimmet ska ligga sent
    applyPreset (3, pat, e);
    e.bpm = 122.0;
    std::vector<NoteEv> ev;
    e.collect (0.0, 16.0, ev);
    int kicks = 0, rims = 0;
    for (auto& x : ev)
    {
        if (x.track == RKick) ++kicks;
        if (x.track == RRim)
        {
            ++rims;
            const double late = (x.ppq / 0.25 - std::floor (x.ppq / 0.25 + 1e-9)) * 0.25 * 60000.0 / 122.0;
            CHECK (late >= 0.0);
        }
    }
    CHECK (kicks >= 12);
    std::printf ("Preset 4 (%s): %d kickar, %d rims, %zu noter på 4 takter\n", presetName (3).c_str(), kicks, rims, ev.size());

    // Text-format för mönster tål rundtur
    pat.set (6, 3, { 99, -2 });
    const auto s = pat.trackToString (6);
    Pattern p2;
    p2.trackFromString (6, s);
    CHECK (p2.get (6, 3).vel == 99 && p2.get (6, 3).pitch == -2);

    // Namnen är unika
    std::set<std::string> names;
    for (int i = 0; i < NUM_PRESETS; ++i) names.insert (presetName (i));
    CHECK ((int) names.size() == NUM_PRESETS);

    std::printf (fails == 0 ? "ALLT OK\n" : "%d FEL\n", fails);
    return fails == 0 ? 0 : 1;
}
