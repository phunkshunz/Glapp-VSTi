#pragma once

// GLAPP-motorn: räknar ut exakt när varje not ska landa.
//
// Allt är tillståndslöst: en not bestäms bara av spåret, stegets nummer (räknat
// från låtens början) och inställningarna. Därför tål motorn att Bitwig hoppar,
// loopar eller spolar, och samma ställe i låten låter alltid likadant.

#include "Model.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace gl
{
struct NoteEv
{
    double ppq;       // när noten ska spelas, i kvartsnoter från låtens början
    int track;
    int note;
    int channel;      // 1..16
    int vel;          // 1..127
    double gatePpq;   // notlängd
};

class Engine
{
public:
    const Pattern* pat = nullptr;
    std::array<TrackCfg, NT> tr {};
    GlobalCfg g;
    double bpm = 122.0;
    double barPpq = 4.0;

    static int64_t floorDiv (int64_t a, int64_t b) { int64_t q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }
    static int mod (int64_t a, int b) { if (b <= 0) return 0; int r = (int) (a % b); return r < 0 ? r + b : r; }
    template <typename T> static T clampv (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

    int64_t barOf (double ppq) const { return (int64_t) std::floor (ppq / barPpq + 1e-9); }
    double msToPpq (double ms) const { return ms / 1000.0 * bpm / 60.0; }
    // FÖLJ TEMPO: släppet är inställt för 122 BPM och krymper/växer med tempot,
    // så att groovet känns lika oavsett tempo.
    double tempoScale() const { return g.followTempo ? 122.0 / std::max (20.0, bpm) : 1.0; }
    int length (int t) const { return clampv (tr[(size_t) t].length, 1, MAXS); }

    // ORBIT STEG: hur många steg mönstret har vandrat vid en viss takt.
    int orbitShift (int t, int64_t bar) const
    {
        const auto& c = tr[(size_t) t];
        if (c.orbit != OStegFram && c.orbit != OStegBak) return 0;
        const int64_t r = floorDiv (bar, barsValue (c.orbitBars));
        const int s = mod (r, length (t));
        return c.orbit == OStegFram ? s : -s;
    }

    int stepIndex (int t, int64_t n) const
    {
        const double ppq = (double) n * divPpq (tr[(size_t) t].div);
        return mod (n - orbitShift (t, barOf (ppq)), length (t));
    }

    bool inLastBeat (double ppq) const
    {
        const double inBar = ppq - (double) barOf (ppq) * barPpq;
        return inBar >= barPpq - 1.0 - 1e-9;
    }

    // Händelsetakten är sista takten i varje fras (t.ex. takt 8, 16, 24 ...).
    bool isEventBar (int t, int64_t bar) const
    {
        const auto& c = tr[(size_t) t];
        if (c.event == EAv) return false;
        const int eb = barsValue (c.eventBars);
        return mod (bar, eb) == eb - 1;
    }

    // Grundträff: mönster + CHANS + händelser som tar bort noter. Ingen tidsförskjutning.
    bool baseHit (int t, int64_t n, Step& st) const
    {
        const auto& c = tr[(size_t) t];
        if (c.mute || pat == nullptr) return false;
        const double ppq = (double) n * divPpq (c.div);
        st = pat->get (t, stepIndex (t, n));
        if (st.vel <= 0) return false;
        if (c.chance < 0.999f && hash01 (g.seed, t, n, 1) >= (double) c.chance) return false;
        if (g.tomrum > 0.001f && tomrumGone (t, n, st)) return false;
        if (isEventBar (t, barOf (ppq)))
        {
            if (c.event == ETyst) return false;
            if (c.event == EHopp && inLastBeat (ppq)) return false;
        }
        return true;
    }

    // TOMRUM: stryker slag efter roll. Spöknoter försvinner först, slag på
    // fjärdedelarna hänger kvar längst. Samma slag försvinner alltid på samma
    // ställe (hash), så tomrummet blir ett mönster, inte brus.
    bool tomrumGone (int t, int64_t n, const Step& st) const
    {
        const auto& c = tr[(size_t) t];
        double p = (double) g.tomrum * tomrumWeight (c.role) * 0.8;
        if (p <= 0.0) return false;
        if (st.vel < 60) p *= 1.35;
        const double stepPpq = divPpq (c.div);
        const double ppq = (double) n * stepPpq;
        if (std::abs (ppq - std::round (ppq)) < 1e-6) p *= 0.5;   // på slaget
        return hash01 (g.seed, t, n, 7) < std::min (0.92, p);
    }

    // STUDS: en skiftnyckel som studsar nerför en trappa. Varje studs kortare och svagare.
    static int studsCount() { return 5; }
    static double studsPos (int r)   // i steg efter första slaget
    {
        static const double v[] = { 0.0, 0.62, 1.0, 1.24, 1.39 };
        return v[clampv (r, 0, 4)];
    }
    static double studsVel (int r) { static const double v[] = { 1.0, 0.72, 0.52, 0.37, 0.26 }; return v[clampv (r, 0, 4)]; }

    // TRÖGHET: hur många träffar i rad som kom precis före det här steget.
    int runBefore (int t, int64_t n) const
    {
        int k = 0;
        Step s;
        while (k < 4 && baseHit (t, n - 1 - k, s)) ++k;
        return k;
    }
    static double inertiaMs (int k) { static const double v[] = { 0.0, 12.0, 24.0, 38.0, 54.0 }; return v[clampv (k, 0, 4)]; }

    // INTERLOCK: har ett spår med högre prioritet (lägre nummer) en träff exakt samtidigt?
    bool collides (int t, double ppq) const
    {
        for (int u = 0; u < t; ++u)
        {
            if (tr[(size_t) u].mute) continue;
            const double su = divPpq (tr[(size_t) u].div);
            const double m = std::round (ppq / su);
            if (std::abs (m * su - ppq) > 1e-6) continue;
            Step s;
            if (baseHit (u, (int64_t) m, s)) return true;
        }
        return false;
    }

    double glideMs (int t, int64_t bar) const
    {
        const auto& c = tr[(size_t) t];
        if (c.orbit != OGlid) return 0.0;
        const int ph = mod (floorDiv (bar, barsValue (c.orbitBars)), 8);
        return 3.0 * (ph <= 4 ? ph : 8 - ph);   // 0, 3, 6, 9, 12, 9, 6, 3 ms ...
    }

    // Alla noter som steg n på spår t ger upphov till.
    void stepEvents (int t, int64_t n, std::vector<NoteEv>& out) const
    {
        Step st;
        if (! baseHit (t, n, st)) return;
        const auto& c = tr[(size_t) t];
        const double step = divPpq (c.div);
        const double nom = (double) n * step;
        const int64_t bar = barOf (nom);
        const bool evBeat = isEventBar (t, bar) && inLastBeat (nom);

        double ms = c.drag;
        if (c.inertia > 0.0f) ms += (double) c.inertia * inertiaMs (runBefore (t, n));
        ms += glideMs (t, bar);
        if (evBeat && c.event == EHicka) ms += 45.0;
        ms *= (double) g.slap;
        ms += (hash01 (g.seed, t, n, 2) * 2.0 - 1.0) * (double) c.jitter * (double) g.human;
        ms = clampv (ms, 0.0, 250.0) * tempoScale();
        double off = msToPpq (ms);

        int vel = (int) std::lround ((double) st.vel * (double) c.level);
        if (g.interlock && c.role != RKick && collides (t, nom))
        {
            if (g.ilMode == IlTyst) return;
            if (g.ilMode == IlSpoke) vel = 18 + (int) (hash01 (g.seed, t, n, 3) * 17.0);   // spöknot 18..35
            else off += 0.125;                                                            // sen 32-del
        }
        vel = clampv (vel, 1, 127);

        const double gate = std::max (0.01, (double) c.gate * step);
        const bool studs = evBeat && c.event == EStuds;
        const int reps = (evBeat && c.event == ERatchet) ? 3 : (studs ? studsCount() : 1);
        const auto iv = chordIntervals (c.chord);
        for (int r = 0; r < reps; ++r)
        {
            double p, gg;
            int v;
            if (studs)
            {
                p = nom + off + studsPos (r) * step;
                v = std::max (1, (int) std::lround ((double) vel * studsVel (r)));
                gg = std::min (gate, step * 0.2);
            }
            else
            {
                p = nom + off + (double) r * step / 3.0;
                v = r == 0 ? vel : std::max (1, (int) ((double) vel * (r == 1 ? 0.7 : 0.5)));
                gg = reps > 1 ? std::min (gate, step / 3.0 * 0.8) : gate;
            }
            for (int i : iv)
                out.push_back ({ p, t, clampv (c.note + st.pitch + i, 0, 127), clampv (c.channel, 1, 16), v, gg });
        }
    }

    // Hur långt bakåt vi måste titta: största möjliga förskjutning + FLYTTA + ratchet/studs.
    double maxLookPpq() const { return msToPpq (260.0 * std::max (1.0, tempoScale())) + 0.125 + 1.5; }

    // Alla noter som ska spelas i [start, end), sorterade i tid.
    void collect (double start, double end, std::vector<NoteEv>& out) const
    {
        const double look = maxLookPpq();
        std::vector<NoteEv> tmp;
        tmp.reserve (64);
        for (int t = 0; t < NT; ++t)
        {
            if (tr[(size_t) t].mute) continue;
            const double step = divPpq (tr[(size_t) t].div);
            const int64_t n0 = (int64_t) std::floor ((start - look) / step);
            const int64_t n1 = (int64_t) std::ceil (end / step);
            tmp.clear();
            for (int64_t n = n0; n <= n1; ++n) stepEvents (t, n, tmp);
            for (auto& e : tmp)
                if (e.ppq >= start && e.ppq < end) out.push_back (e);
        }
        std::stable_sort (out.begin(), out.end(), [] (const NoteEv& a, const NoteEv& b) { return a.ppq < b.ppq; });
    }

    // För GUI:t: hur sent ett steg i mönstret ligger (DRAG + TRÖGHET), utan slump.
    double staticOffsetMs (int t, int idx) const
    {
        const auto& c = tr[(size_t) t];
        if (pat == nullptr) return 0.0;
        const int len = length (t);
        double ms = c.drag;
        if (c.inertia > 0.0f)
        {
            int k = 0;
            while (k < 4 && k < len - 1 && pat->get (t, mod (idx - 1 - k, len)).vel > 0) ++k;
            ms += (double) c.inertia * inertiaMs (k);
        }
        return clampv (ms * (double) g.slap, 0.0, 250.0) * tempoScale();
    }
};
} // namespace gl
