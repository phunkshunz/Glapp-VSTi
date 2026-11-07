#pragma once

// Tärningen: slump med stilregler. Den spottar aldrig ut kaos, utan glesa
// mönster där varje roll följer sina regler (kicken står still, rimmet ligger
// sent, percen får udda längder som driver mot varandra).
//
// Tärningen rör aldrig NOT och KANAL: de är din koppling till BETONG eller
// trummaskinen.

#include "Model.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace gl
{
struct TrackGen
{
    TrackCfg cfg;
    std::array<Step, MAXS> steps {};
};

struct GlobalGen
{
    float slap = 1.0f, human = 1.0f;
    bool interlock = true;
    int ilMode = IlSpoke;
    float tomrum = 0.0f;
};

// Stilarna som är "maskinella" (inget släp, ingen jitter).
inline bool isRaster (int style) { return style == SRaster; }

// Euklidisk fördelning: k träffar jämnt utspridda över n steg, roterad rot steg.
inline bool euclidHit (int i, int k, int n, int rot)
{
    if (n <= 0 || k <= 0) return false;
    const int j = ((i + rot) % n + n) % n;
    return (j * k) % n < k;
}

// ================================================================ DE HÅRDA STILARNA
// HAMMARE  peak-time: fyra på golvet, 16-delshattar, rullande bas, clap på 2 och 4
// INDUSTRI galopp i kicken, flammade claps, metall som slår, bas på offbeat med oktavhopp
// HYPNOS   psykotiska slingor: udda längder (3, 5, 7, 11 ...) som driver mot 16 och vrider sig
// STÖK     funk som drar: synkoperat, spökslag, mycket släp på allt utom kicken
// Spåren: KICK, RIM (rim/virvel), HAT, PERK 1 (hammare, städ), PERK 2 (rör, metall), KLICK (clap), BAS, ACKORD (stab)
inline TrackGen diceHardTrack (int role, int style, Rng& r, const TrackCfg& cur, int avoidLen)
{
    TrackGen g;
    auto& c = g.cfg;
    c.role = role; c.note = cur.note; c.channel = cur.channel; c.mute = cur.mute; c.level = cur.level;
    c.div = D16; c.length = 16; c.chord = CEn; c.inertia = 0.0f; c.jitter = 0.5f; c.chance = 1.0f;
    auto put = [&] (int i, int v, int p = 0) { if (i >= 0 && i < MAXS) g.steps[(size_t) i] = { std::clamp (v, 1, 127), p }; };
    auto free = [&] (int i) { return i >= 0 && i < MAXS && g.steps[(size_t) i].vel == 0; };
    auto euclid = [&] (int len, int k, int rot, int accent, int lo, int hi)
    {
        bool first = true;
        for (int i = 0; i < len; ++i)
            if (euclidHit (i, k, len, rot)) { put (i, first ? accent : r.range (lo, hi)); first = false; }
    };
    const bool H = style == SHammare, I = style == SIndustri, Y = style == SHypnos, S = style == SStok;

    switch (role)
    {
        case RKick:
        {
            c.gate = 0.3f; c.drag = 0; c.jitter = 0;
            c.length = (H || I) && r.chance (0.4) ? 32 : 16;
            for (int b = 0; b < c.length / 4; ++b) put (b * 4, b % 4 == 0 ? 124 : r.range (114, 120));
            if (H && r.chance (0.5)) put (c.length - 1, r.range (48, 62));                         // rullande upptakt
            if (H && c.length == 32 && r.chance (0.5)) { put (30, r.range (70, 86)); put (31, r.range (80, 96)); }
            if (I && r.chance (0.6)) put (r.pick ({ 14, 10, 6 }), r.range (78, 96));                // galopp
            if (I && r.chance (0.3)) put (r.pick ({ 3, 11 }), r.range (50, 64));
            if (S && r.chance (0.6)) put (r.pick ({ 11, 3, 7 }), r.range (56, 72));                 // synkoperat spökslag
            if (r.chance (0.2)) { c.event = EHopp; c.eventBars = r.pick ({ 3, 4 }); }
            break;
        }

        case RKlick:   // clapen: ryggraden på 2 och 4
        {
            c.gate = 0.2f;
            c.drag = H ? r.rangef (0.0f, 4.0f) : (I ? r.rangef (0.0f, 6.0f) : (S ? r.rangef (10.0f, 22.0f) : r.rangef (4.0f, 12.0f)));
            if (Y)
            {
                c.length = r.pick ({ 32, 32, 24 });
                put (c.length == 24 ? 18 : 28, r.range (100, 118));                                 // en enda, sällan: suget
                if (r.chance (0.4)) put (12, r.range (84, 100));
            }
            else
            {
                put (4, r.range (110, 120)); put (12, r.range (112, 122));
                if (I) { put (3, r.range (44, 58)); if (r.chance (0.6)) put (11, r.range (44, 58)); }   // flam
                if (S) { if (r.chance (0.6)) put (r.pick ({ 7, 15 }), r.range (40, 56)); if (r.chance (0.4)) put (10, r.range (36, 50)); }
                if (H && r.chance (0.3)) put (15, r.range (60, 76));
            }
            if ((H || I) && r.chance (0.3)) { c.event = ERatchet; c.eventBars = r.pick ({ 3, 4 }); }
            break;
        }

        case RRim:     // rim och virvel: det som knackar mellan claparna
        {
            c.gate = 0.15f;
            if (H) { c.length = 16; euclid (16, r.pick ({ 3, 3, 5 }), r.range (1, 6), r.range (96, 110), 70, 92); c.drag = r.rangef (2.0f, 8.0f); }
            else if (I)
            {
                c.length = 16;
                for (int i : { 10, 11, 13, 14 }) put (i, 56 + (i - 10) * 14);                      // rullar upp mot clapen
                if (r.chance (0.5)) put (6, r.range (80, 96));
                c.drag = r.rangef (0.0f, 6.0f);
                if (r.chance (0.4)) { c.event = EStuds; c.eventBars = r.pick ({ 2, 4 }); }
            }
            else if (Y)
            {
                for (int t = 0; t < 8; ++t) { c.length = r.pick ({ 5, 6, 7, 9 }); if (c.length != avoidLen) break; }
                put (r.range (0, c.length - 1), r.range (92, 110));
                c.drag = r.rangef (6.0f, 16.0f);
                c.orbit = OStegFram; c.orbitBars = r.pick ({ 2, 4 });
            }
            else
            {
                c.length = 16;
                for (int i : { 3, 7, 10, 15 }) if (r.chance (0.75)) put (i, r.range (38, 72));
                put (r.pick ({ 6, 14 }), r.range (86, 104));
                c.drag = r.rangef (20.0f, 36.0f); c.inertia = r.rangef (0.3f, 0.6f); c.jitter = r.rangef (1.0f, 2.5f);
            }
            break;
        }

        case RHat:
        {
            c.gate = 0.08f;
            if (H)
            {
                c.length = 16;
                for (int i = 0; i < 16; ++i) put (i, (i % 4) == 2 ? r.range (108, 120) : ((i % 2) == 1 ? r.range (56, 74) : r.range (40, 54)));
                c.drag = r.rangef (2.0f, 6.0f); c.chance = r.rangef (0.92f, 1.0f);
            }
            else if (I)
            {
                c.length = 16;
                for (int i : { 2, 6, 10, 14 }) put (i, r.range (112, 124));
                for (int i = 1; i < 16; i += 2) if (r.chance (0.6)) put (i, r.range (50, 70));
                c.drag = r.rangef (0.0f, 4.0f);
                if (r.chance (0.5)) { c.event = ERatchet; c.eventBars = r.pick ({ 2, 4 }); }
            }
            else if (Y)
            {
                // varje 16-del spelas, men accenten går i 3, 5 eller 7: den vrider sig runt takten hela tiden
                const int acc = r.pick ({ 3, 5, 7 });
                c.length = acc * r.pick ({ 2, 3 });
                if (c.length == avoidLen) c.length += acc;
                for (int i = 0; i < c.length; ++i) put (i, (i % acc) == 0 ? r.range (104, 118) : r.range (34, 56));
                c.drag = r.rangef (2.0f, 8.0f);
            }
            else
            {
                c.length = 16;
                for (int i : { 2, 6, 10, 14 }) put (i, r.range (96, 112));
                for (int i : { 1, 3, 5, 7, 9, 11, 13, 15 }) if (r.chance (0.55)) put (i, r.range (32, 54));
                c.drag = r.rangef (10.0f, 22.0f); c.inertia = r.rangef (0.2f, 0.5f); c.jitter = r.rangef (1.5f, 3.0f);
            }
            if (! I && r.chance (0.25)) { c.orbit = OGlid; c.orbitBars = r.pick ({ 1, 2 }); }
            break;
        }

        case RPerk1:   // hammare, städ, slagg: metall som slår
        case RPerk2:   // rör, stål, skrot: det som svarar
        {
            const bool one = role == RPerk1;
            c.gate = 0.15f;
            if (H)
            {
                c.length = 16;
                euclid (16, one ? r.pick ({ 5, 7 }) : r.pick ({ 3, 4 }), r.range (0, 15), r.range (100, 116), 60, 92);
                c.drag = r.rangef (2.0f, 10.0f);
                if (r.chance (0.3)) { c.event = ERatchet; c.eventBars = r.pick ({ 2, 4 }); }
            }
            else if (I)
            {
                for (int t = 0; t < 8; ++t) { c.length = one ? r.pick ({ 16, 12 }) : r.pick ({ 5, 13, 11 }); if (c.length != avoidLen) break; }
                euclid (c.length, one ? std::max (2, c.length * 7 / 16) : std::max (2, c.length / 4), r.range (0, c.length - 1), r.range (104, 120), 70, 100);
                c.drag = r.rangef (0.0f, 8.0f);
                if (r.chance (0.4)) { c.event = r.chance (0.5) ? EStuds : ERatchet; c.eventBars = r.pick ({ 2, 3 }); }
            }
            else if (Y)
            {
                // psykosen: två korta slingor i olika längd som aldrig går ihop
                for (int t = 0; t < 8; ++t) { c.length = one ? r.pick ({ 3, 5, 7 }) : r.pick ({ 7, 9, 11, 13 }); if (c.length != avoidLen) break; }
                euclid (c.length, std::max (1, (c.length + 1) / 3), r.range (0, c.length - 1), r.range (98, 116), 50, 84);
                c.drag = r.rangef (4.0f, 18.0f);
                c.orbit = r.chance (0.5) ? OStegFram : OStegBak; c.orbitBars = r.pick ({ 1, 2, 4 });
                if (r.chance (0.25)) { c.event = EHicka; c.eventBars = 3; }
            }
            else
            {
                c.length = r.chance (0.6) ? 16 : 32;
                // tresillo 3-3-2 (och dess spegel): kroppen i funken
                const std::array<int, 6> a { 0, 3, 6, 8, 11, 14 }, b { 2, 5, 8, 10, 13, 15 };
                for (int i : (one ? a : b)) if (r.chance (one ? 0.85 : 0.6)) put (i + (c.length == 32 && r.chance (0.4) ? 16 : 0), r.range (one ? 70 : 54, one ? 108 : 92));
                c.drag = r.rangef (14.0f, 32.0f); c.inertia = r.rangef (0.3f, 0.7f); c.jitter = r.rangef (1.0f, 3.0f);
                if (r.chance (0.3)) { c.event = EHicka; c.eventBars = r.pick ({ 3, 4 }); }
            }
            break;
        }

        case RBas:
        {
            if (H)
            {
                // rullande bas: tre 16-delar efter varje kick, accent på offbeat
                c.length = r.pick ({ 16, 32 });
                for (int i = 0; i < c.length; ++i)
                    if (i % 4 != 0) put (i, (i % 4) == 2 ? r.range (104, 116) : r.range (78, 94), (i >= 28 && r.chance (0.5)) ? r.pick ({ 12, 3, -2 }) : 0);
                c.gate = r.rangef (0.3f, 0.45f); c.drag = r.rangef (0.0f, 4.0f);
            }
            else if (I)
            {
                c.length = 16;
                for (int i : { 2, 6, 10, 14 }) put (i, r.range (100, 118), r.chance (0.25) ? 12 : 0);
                if (r.chance (0.5)) put (15, r.range (70, 86), r.pick ({ 12, 7 }));
                c.gate = r.rangef (0.45f, 0.65f); c.drag = r.rangef (0.0f, 6.0f);
            }
            else if (Y)
            {
                for (int t = 0; t < 8; ++t) { c.length = r.pick ({ 3, 5, 7 }); if (c.length != avoidLen) break; }
                const int hits = std::max (2, c.length - 1);
                for (int i = 0, tries = 0; i < hits && tries < 30; ++tries)
                {
                    const int p = r.range (0, c.length - 1);
                    if (free (p)) { put (p, r.range (86, 112), r.pick ({ 0, 0, 0, 3, 7, -2, 12 })); ++i; }
                }
                c.gate = r.rangef (0.25f, 0.45f); c.drag = r.rangef (2.0f, 10.0f);
                c.orbit = OStegFram; c.orbitBars = r.pick ({ 2, 4 });
            }
            else
            {
                c.length = r.pick ({ 16, 32 });
                const int pos[] = { 3, 6, 10, 11, 14 }, pit[] = { 0, 12, 0, -2, 3 };
                for (int k = 0; k < 5; ++k)
                {
                    if (r.chance (0.8)) put (pos[k], r.range (88, 112), pit[k]);
                    if (c.length == 32 && r.chance (0.7)) put (16 + pos[k], r.range (84, 108), r.chance (0.3) ? r.pick ({ 7, 5, 12 }) : pit[k]);
                }
                c.gate = r.rangef (0.18f, 0.3f); c.drag = r.rangef (10.0f, 22.0f); c.inertia = r.rangef (0.2f, 0.4f);
            }
            break;
        }

        case RAckord:
        default:
        {
            c.chord = CEn;   // samplingarna är redan ackord
            if (H) { c.length = 32; put (r.pick ({ 6, 10, 14 }), r.range (90, 108)); if (r.chance (0.5)) put (r.pick ({ 22, 26, 30 }), r.range (80, 100)); c.gate = 0.25f; c.drag = r.rangef (2.0f, 10.0f); }
            else if (I) { c.length = 32; put (14, r.range (96, 112)); if (r.chance (0.6)) put (30, r.range (96, 112)); c.gate = 0.15f; c.drag = r.rangef (0.0f, 6.0f); }
            else if (Y)
            {
                for (int t = 0; t < 8; ++t) { c.length = r.pick ({ 7, 11, 13 }); if (c.length != avoidLen) break; }
                put (r.range (0, c.length - 1), r.range (88, 106));
                c.gate = r.rangef (0.15f, 0.4f); c.drag = r.rangef (6.0f, 20.0f);
                if (r.chance (0.4)) { c.orbit = OStegBak; c.orbitBars = 2; }
            }
            else { c.length = 16; put (3, r.range (86, 104)); if (r.chance (0.6)) put (11, r.range (76, 96)); c.gate = 0.12f; c.drag = r.rangef (14.0f, 28.0f); }
            break;
        }
    }
    return g;
}

inline TrackGen diceTrack (int role, int style, Rng& r, const TrackCfg& cur, int avoidLen = 0)
{
    if (isHard (style)) return diceHardTrack (role, style, r, cur, avoidLen);
    TrackGen g;
    auto& c = g.cfg;
    c.role = role;
    c.note = cur.note;
    c.channel = cur.channel;
    c.mute = cur.mute;
    c.level = cur.level;
    auto put = [&] (int i, int v, int p = 0)
    {
        if (i >= 0 && i < MAXS) g.steps[(size_t) i] = { std::clamp (v, 1, 127), p };
    };
    auto free = [&] (int i) { return i >= 0 && i < MAXS && g.steps[(size_t) i].vel == 0; };

    switch (role)
    {
        case RKick:
        {
            c.length = 16; c.div = D16; c.drag = 0; c.inertia = 0; c.jitter = 0; c.gate = 0.3f;
            if ((style == SOrganisk && r.chance (0.25)) || (style == SRaster && r.chance (0.3)))
            {
                const int rot = r.pick ({ 0, 3, 6 });
                for (int i = 0; i < 16; ++i)
                    if (euclidHit (i, 5, 16, rot)) put (i, i == 0 ? 120 : r.range (96, 112));
                put (0, 120);
            }
            else
            {
                for (int b = 0; b < 4; ++b) put (b * 4, b == 0 ? 122 : r.range (108, 118));
                if (style == SDub && r.chance (0.35)) put (r.pick ({ 14, 11 }), r.range (50, 70));
                if (style == SBerlin && r.chance (0.25)) put (r.pick ({ 3, 7, 15 }), r.range (40, 58));
                if (style == SKoln && r.chance (0.35)) put (r.pick ({ 14, 15 }), r.range (44, 60));   // schaffel-upptakt
            }
            if (r.chance (0.3)) { c.event = EHopp; c.eventBars = r.pick ({ 3, 4 }); }
            break;
        }

        case RRim:
        {
            c.div = D16;
            c.length = r.chance (0.6) ? 16 : 32;
            int a = 4, b = 12;
            if (style == SBerlin) { const int v = r.range (0, 2); a = v == 0 ? 4 : (v == 1 ? 6 : 4); b = v == 0 ? 12 : (v == 1 ? 14 : 11); }
            const bool onlyOne = (style == SDub && r.chance (0.5)) || (style == SKlinisk && r.chance (0.6));
            if (! onlyOne) put (a, r.range (88, 104));
            put (b, r.range (96, 112));
            if (c.length == 32)
            {
                if (! onlyOne) put (16 + a, r.range (84, 100));
                put (16 + b + (r.chance (0.3) ? 1 : 0), r.range (92, 108));
            }
            const int ghosts = (isRaster (style) ? 0 : (style == SKlinisk ? r.range (0, 1) : r.range (1, 3))) * (c.length == 32 ? 2 : 1);
            for (int i = 0, tries = 0; i < ghosts && tries < 40; ++tries)
            {
                const int p = r.range (0, c.length - 1);
                if (free (p) && free (p - 1) && free (p + 1)) { put (p, r.range (32, 58)); ++i; }
            }
            const float lo[NSTYLES] = { 14, 10, 20, 12, 18, 4, 0, 4, 6, 8, 20 }, hi[NSTYLES] = { 32, 24, 40, 30, 34, 10, 0, 10, 14, 18, 38 };
            const int si = std::clamp (style, 0, NSTYLES - 1);
            c.drag = r.rangef (lo[si], hi[si]);
            c.inertia = isRaster (style) ? 0.0f : r.rangef (0.1f, 0.3f);
            c.jitter = isRaster (style) ? 0.0f : r.rangef (1.0f, 2.0f);
            c.gate = 0.2f;
            c.chance = r.rangef (0.9f, 1.0f);
            if (r.chance (0.2)) { c.event = EHicka; c.eventBars = r.pick ({ 3, 4 }); }
            break;
        }

        case RHat:
        {
            c.div = D16;
            if (style == SKoln)
            {
                // Schaffel: 8-delstrioler, hatten på sista trioldelen av varje slag.
                c.div = D8T;
                c.length = 12;
                for (int b = 0; b < 4; ++b) put (b * 3 + 2, r.range (78, 98));
                const int ghosts = r.range (1, 2);
                for (int i = 0, tries = 0; i < ghosts && tries < 40; ++tries)
                {
                    const int p = r.range (0, 3) * 3 + 1;
                    if (free (p)) { put (p, r.range (30, 48)); ++i; }
                }
            }
            else if (style == SKlinisk)
            {
                c.length = r.pick ({ 16, 13, 19 });
                const int k = r.range (2, 3), rot = r.range (0, c.length - 1);
                for (int i = 0; i < c.length; ++i)
                    if (euclidHit (i, k, c.length, rot)) put (i, r.range (55, 90));
            }
            else if (isRaster (style))
            {
                c.div = r.chance (0.5) ? D32 : D16;
                c.length = r.pick ({ 7, 9, 11, 13 });
                const int k = r.range (3, 5), rot = r.range (0, c.length - 1);
                for (int i = 0; i < c.length; ++i)
                    if (euclidHit (i, k, c.length, rot)) put (i, r.chance (0.3) ? 110 : 70);
            }
            else if ((style == SBerlin && r.chance (0.5)) || (style == SOrganisk && r.chance (0.3)))
            {
                c.length = r.pick ({ 15, 13 });
                const int k = r.range (4, 6), rot = r.range (0, c.length - 1);
                for (int i = 0; i < c.length; ++i)
                    if (euclidHit (i, k, c.length, rot)) put (i, r.range (60, 96));
            }
            else
            {
                c.length = (style == SRominimal && r.chance (0.35)) ? 32 : 16;
                for (int i = 2; i < c.length; i += 4) put (i, r.range (80, 100));
                const int ghosts = r.range (1, 2) * (c.length / 16);
                for (int i = 0, tries = 0; i < ghosts && tries < 40; ++tries)
                {
                    const int p = r.range (0, c.length - 1) | 1;
                    if (p < c.length && free (p)) { put (p, r.range (30, 50)); ++i; }
                }
            }
            c.drag = style == SKoln ? r.rangef (6.0f, 16.0f) : (style == SKlinisk ? r.rangef (2.0f, 6.0f) : r.rangef (4.0f, 14.0f));
            c.inertia = r.rangef (0.0f, 0.2f);
            c.jitter = r.rangef (1.0f, 3.0f);
            c.chance = style == SKlinisk ? r.rangef (0.7f, 0.9f) : r.rangef (0.85f, 1.0f);
            c.gate = style == SKlinisk ? 0.05f : 0.15f;
            if (isRaster (style)) { c.drag = 0; c.inertia = 0; c.jitter = 0; c.chance = 1.0f; c.gate = 0.08f; }
            else if (r.chance (0.25)) { c.orbit = OGlid; c.orbitBars = r.pick ({ 1, 2 }); }
            break;
        }

        case RPerk1:
        case RPerk2:
        {
            for (int tries = 0; tries < 8; ++tries)
            {
                switch (style)
                {
                    case SRominimal: c.length = r.pick ({ 15, 23, 13 }); break;
                    case SBerlin:    c.length = r.pick ({ 11, 13, 17 }); break;
                    case SDub:       c.length = r.pick ({ 13, 15, 11 }); break;
                    case SKoln:      c.length = r.pick ({ 12, 9, 15 }); break;
                    case SKlinisk:   c.length = r.pick ({ 13, 17, 19 }); break;
                    case SRaster:    c.length = r.pick ({ 5, 7, 9, 11 }); break;
                    default:         c.length = r.pick ({ 11, 13, 17, 23 }); break;
                }
                if (c.length != avoidLen) break;
            }
            c.div = D16;
            if ((style == SBerlin && r.chance (0.25)) || (style == SOrganisk && r.chance (0.2))) c.div = D16T;
            if (style == SKoln) c.div = r.chance (0.5) ? D8T : D16T;
            if (isRaster (style) && r.chance (0.4)) c.div = D32;
            const int dens = style == SDub ? r.range (2, 3) : (style == SKlinisk ? r.range (1, 2) : (isRaster (style) ? r.range (2, 4) : r.range (2, 5)));   // träffar per 16 steg
            const int hits = std::max (1, (int) std::lround (c.length * dens / 16.0));
            const int rot = r.range (0, c.length - 1);
            bool first = true;
            for (int i = 0; i < c.length; ++i)
                if (euclidHit (i, hits, c.length, rot)) { put (i, first ? r.range (90, 110) : r.range (45, 85)); first = false; }
            c.drag = style == SDub ? r.rangef (18.0f, 40.0f) : r.rangef (8.0f, 40.0f);
            c.inertia = r.rangef (0.3f, 0.7f);
            c.jitter = r.rangef (1.0f, 3.0f);
            c.chance = r.rangef (0.8f, 1.0f);
            c.gate = 0.2f;
            const double o = r.uni();
            if (o < 0.5) { c.orbit = r.chance (0.5) ? OStegFram : OStegBak; c.orbitBars = r.pick ({ 2, 3 }); }
            else if (o < 0.75) { c.orbit = OGlid; c.orbitBars = r.pick ({ 1, 2 }); }
            if (r.chance (0.25)) { c.event = r.chance (0.5) ? ERatchet : EHicka; c.eventBars = r.pick ({ 3, 4 }); }
            if ((style == SKoln || style == SOrganisk) && r.chance (0.3)) { c.event = EStuds; c.eventBars = r.pick ({ 2, 3 }); }
            if (style == SKlinisk) { c.gate = 0.06f; c.drag = r.rangef (3.0f, 12.0f); }
            if (isRaster (style)) { c.drag = 0; c.inertia = 0; c.jitter = 0; c.chance = 1.0f; c.gate = 0.08f; if (c.orbit == OGlid) c.orbit = OStegFram; }
            break;
        }

        case RKlick:
        {
            c.div = r.chance (0.5) ? D32 : D16;
            c.length = r.pick ({ 16, 32, 12 });
            if (isRaster (style))
            {
                c.div = D32;
                c.length = r.pick ({ 11, 13, 16, 19 });
                const int k = r.range (3, 6), rot = r.range (0, c.length - 1);
                for (int i = 0; i < c.length; ++i)
                    if (euclidHit (i, k, c.length, rot)) put (i, r.range (60, 100));
            }
            else
            {
                const int hits = style == SKlinisk ? r.range (1, 2) : r.range (2, 3);
                for (int i = 0, tries = 0; i < hits && tries < 40; ++tries)
                {
                    const int p = r.range (0, c.length - 1);
                    if (free (p)) { put (p, r.range (40, 90)); ++i; }
                }
            }
            c.gate = style == SKlinisk ? 0.05f : 0.1f;
            c.drag = r.rangef (0.0f, 20.0f);
            c.jitter = r.rangef (2.0f, 5.0f);
            c.chance = r.rangef (0.75f, 1.0f);
            if (r.chance (0.5)) { c.orbit = OGlid; c.orbitBars = r.pick ({ 0, 1, 2 }); }
            if (r.chance (0.35)) { c.event = ERatchet; c.eventBars = r.pick ({ 2, 3 }); }
            if (style == SKoln && r.chance (0.4)) { c.event = EStuds; c.eventBars = r.pick ({ 2, 3 }); }
            if (isRaster (style)) { c.drag = 0; c.jitter = 0; c.chance = 1.0f; c.orbit = r.chance (0.5) ? OStegBak : OAv; }
            break;
        }

        case RBas:
        {
            c.div = D16;
            c.length = (style == SDub && r.chance (0.3)) ? 12 : r.pick ({ 16, 32 });
            const int slots[] = { 2, 3, 6, 7, 10, 11, 14, 15 };
            const int hits = (style == SKlinisk ? r.range (1, 2) : r.range (2, 4)) * (c.length == 32 ? 2 : 1);
            for (int i = 0, tries = 0; i < hits && tries < 60; ++tries)
            {
                const int p = slots[r.range (0, 7)] + (c.length == 32 && r.chance (0.5) ? 16 : 0);
                if (p < c.length && free (p)) { put (p, r.range (85, 110), r.pick ({ 0, 0, 0, 0, -2, 3, 5, 7, 12 })); ++i; }
            }
            c.gate = r.rangef (0.35f, 0.6f);
            c.drag = r.rangef (6.0f, 20.0f);
            c.inertia = r.rangef (0.1f, 0.3f);
            c.chance = r.rangef (0.9f, 1.0f);
            if (isRaster (style)) { c.drag = 0; c.inertia = 0; c.gate = 0.15f; }
            break;
        }

        case RAckord:
        default:
        {
            c.div = D16;
            c.length = r.pick ({ 16, 32 });
            if ((style == SRominimal && r.chance (0.3)) || (style == SKlinisk && r.chance (0.5))) { c.length = 16; break; }   // tystnad är också ett val
            const int hits = r.range (1, 2);
            for (int i = 0, tries = 0; i < hits && tries < 40; ++tries)
            {
                const int p = r.pick ({ 3, 7, 10, 6, 14 }) + (c.length == 32 && r.chance (0.5) ? 16 : 0);
                if (free (p)) { put (p, r.range (70, 95)); ++i; }
            }
            switch (style)
            {
                case SDub:       c.chord = r.chance (0.6) ? CDub : CMoll7; break;
                case SBerlin:    c.chord = r.chance (0.5) ? CKvart : CMoll9; break;
                case SRominimal: c.chord = r.chance (0.6) ? CMoll7 : CEn; break;
                case SKoln:      c.chord = r.chance (0.6) ? CDub : CMoll9; break;
                case SKlinisk:   c.chord = CEn; break;
                case SRaster:    c.chord = CKvart; break;
                default:         c.chord = r.chance (0.5) ? CMoll9 : CKvart; break;
            }
            c.gate = (style == SDub || style == SKoln) ? 0.5f : r.rangef (0.2f, 0.4f);
            c.drag = r.rangef (10.0f, 30.0f);
            c.inertia = 0.1f;
            if ((style == SDub || style == SKoln) && r.chance (0.4)) { c.event = EHicka; c.eventBars = 3; }
            if (isRaster (style)) { c.drag = 0; c.inertia = 0; c.gate = 0.1f; }
            break;
        }
    }
    return g;
}

// ORBIT EVOLVE: nya rörelser, samma mönster.
inline void diceOrbit (int role, Rng& r, TrackCfg& c)
{
    c.orbit = OAv; c.event = EAv;
    if (role == RKick) { if (r.chance (0.4)) { c.event = EHopp; c.eventBars = r.pick ({ 3, 4 }); } return; }
    const double o = r.uni();
    if (role == RPerk1 || role == RPerk2 || role == RKlick)
    {
        if (o < 0.45) { c.orbit = r.chance (0.5) ? OStegFram : OStegBak; c.orbitBars = r.pick ({ 1, 2, 3 }); }
        else if (o < 0.8) { c.orbit = OGlid; c.orbitBars = r.pick ({ 0, 1, 2 }); }
    }
    else if (o < 0.35) { c.orbit = OGlid; c.orbitBars = r.pick ({ 1, 2 }); }
    if (r.chance (0.4)) { c.event = r.pick ({ (int) ERatchet, (int) EHicka, (int) EHopp, (int) ETyst, (int) EStuds }); c.eventBars = r.pick ({ 2, 3, 4 }); }
}

inline GlobalGen diceGlobal (int style, Rng& r)
{
    GlobalGen g;
    switch (style)
    {
        case SRominimal: g.slap = r.rangef (1.0f, 1.2f); g.ilMode = IlSpoke; break;
        case SBerlin:    g.slap = r.rangef (0.8f, 1.0f); g.ilMode = r.chance (0.5) ? IlFlytta : IlSpoke; break;
        case SDub:       g.slap = r.rangef (1.1f, 1.4f); g.ilMode = IlSpoke; break;
        case SKoln:      g.slap = r.rangef (1.0f, 1.3f); g.ilMode = IlSpoke; break;
        case SKlinisk:   g.slap = r.rangef (0.7f, 1.0f); g.ilMode = IlTyst; break;
        case SRaster:    g.slap = 1.0f; g.ilMode = IlTyst; break;
        case SHammare:   g.slap = r.rangef (0.6f, 0.85f); g.ilMode = IlSpoke; break;
        case SIndustri:  g.slap = r.rangef (0.7f, 0.95f); g.ilMode = IlSpoke; break;
        case SHypnos:    g.slap = r.rangef (0.9f, 1.15f); g.ilMode = IlFlytta; break;
        case SStok:      g.slap = r.rangef (1.3f, 1.7f); g.ilMode = IlSpoke; break;
        default:         g.slap = r.rangef (0.9f, 1.2f); g.ilMode = r.pick ({ (int) IlSpoke, (int) IlFlytta }); break;
    }
    g.human = isRaster (style) ? 0.0f : r.rangef (0.8f, 1.3f);
    g.interlock = ! (style == SHammare || style == SIndustri);   // hamrar = slagen får krocka och stapla sig
    if (isHard (style)) g.human = r.rangef (0.4f, style == SStok ? 1.3f : 0.9f);
    switch (style)
    {
        case SHammare:
        case SIndustri: g.tomrum = r.rangef (0.0f, 0.06f); break;
        case SHypnos:
        case SStok:     g.tomrum = r.rangef (0.03f, 0.12f); break;
        case SKlinisk: g.tomrum = r.rangef (0.45f, 0.7f); break;
        case SRaster:  g.tomrum = r.rangef (0.2f, 0.45f); break;
        case SBerlin:
        case SDub:     g.tomrum = r.rangef (0.15f, 0.35f); break;
        default:       g.tomrum = r.rangef (0.05f, 0.25f); break;
    }
    return g;
}

// ---------------------------------------------------------------- 550 presets
// 11 stilar x 50. Varje preset är ett fast frö till tärningen, så de är alltid desamma.
// Nummer 0-349 är desamma som i GLAPP 2, 0-199 som i GLAPP 1. 350-549 är de hårda stilarna.
constexpr int PRESETS_PER_STYLE = 50;
constexpr int NUM_PRESETS = NSTYLES * PRESETS_PER_STYLE;
inline int presetStyle (int i) { return std::clamp (i / PRESETS_PER_STYLE, 0, NSTYLES - 1); }
inline uint64_t presetSeed (int i) { return (uint64_t) 0x6C61707000 + (uint64_t) i * (uint64_t) 7919; }

inline std::string presetName (int i)
{
    static const char* w[NSTYLES][25] = {
        { "TRÄ", "RULLE", "NATTBUSS", "GRUSVÄG", "VARV", "KÄLLARE", "MORGON", "SKUGGA", "LÖV", "BALKONG",
          "SPÅRVAGN", "KORK", "LÅNG NATT", "TRÄDGÅRD", "SOLUPPGÅNG", "VÄRMEN", "KLOCKA", "RÖKEN", "BRUNNEN", "DIMMAN",
          "VINDEN", "SLÄPET", "GÅRDEN", "FLODEN", "EFTERFEST" },
        { "KLICK", "BETONG", "FASDRIFT", "S-BAHN", "TUNNEL", "SKEV", "KAKEL", "RÖR", "LAGER", "PUMP",
          "STÅL", "NEON", "KORRIDOR", "SPÄRR", "GLAS", "SIGNAL", "RELÄ", "KABEL", "VÅNING", "TRAPPA",
          "GALLER", "LYKTA", "VENTIL", "DRIFT", "SÖNDAG" },
        { "EKO", "SLÄPIG", "TÅKA", "HAMN", "DJUP", "NÅL", "FRUSEN", "BÅTVARV", "KAJ", "SKÄR",
          "MÖRKER", "RUM", "BÖLJA", "SÄNKT", "DAGG", "LÅG", "SVALL", "MOLN", "REGN", "ÄNGEL",
          "KABLAR", "FJÄDER", "MOSSA", "SIMMA", "TYSTNAD" },
        { "GONG", "BAMBU", "SNUBBEL", "KLAPPER", "SKAL", "TRUMMA", "FRÖ", "BI", "BARK", "STEN",
          "PÄRLA", "SNÄCKA", "KVIST", "SKOG", "BÄCK", "RAMSA", "DANS", "HÄNDER", "ELD", "SAND",
          "RIS", "KOTTE", "GLÄNTA", "BLAD", "FLÖJT" },
        { "DOMEN", "SCHAFFEL", "RHEN", "VARV", "KATEDRAL", "UNDERVATTEN", "KUPOL", "PELARE", "VALV", "KRYPTA",
          "DIMHORN", "BRO", "KÄLLARVÅNING", "FYRA I FYRA", "MÄSSING", "TRIOL", "FASFEL", "TÄCKE", "STOFT", "SVANSEN",
          "HAMNKRAN", "VÄGG", "STRÖM", "GRÅ", "MORGONTÅG" },
        { "PINCETT", "SKALPELL", "VIT", "STERIL", "NÅLSTICK", "LABB", "SPRUTA", "KLINIK", "KLORIN", "GAS",
          "KAKELVÄGG", "LÄKARE", "SPÅR", "PUNKT", "STRECK", "MILLIMETER", "LUPP", "SÖMN", "LJUSRÖR", "SKALA",
          "TOM", "NOLL", "ETT", "KYLA", "SKÄRVA" },
        { "RUTNÄT", "PIXEL", "SINUS", "BINÄR", "FREKVENS", "RASTER", "LINJE", "PULS", "PRIMTAL", "MODUL",
          "KVADRAT", "VEKTOR", "BIT", "KOD", "SKANNA", "MATRIS", "PROTOKOLL", "ÖVERTON", "FÄLT", "IMPULS",
          "SERIE", "INDEX", "TAKTIL", "SYNK", "NOD" },
        { "SLÄGGA", "STÄDET", "KOLVSLAG", "PRESSEN", "SMEDJAN", "NITAR", "GJUTERI", "SKIFT", "VALSEN", "BALK",
          "SPIK", "RAMMEN", "KRAN", "STAMP", "MASUGN", "HYTTA", "GLÖD", "STÅLVERK", "LOK", "BORR",
          "SKROTGÅRD", "KOKS", "GÖT", "SVETS", "NATTSKIFT" },
        { "ROST", "SKORSTEN", "LYFTKRAN", "KOMPRESSOR", "RÖRGATA", "FLÄKT", "TRANSFORMATOR", "SILO", "SPÅRVÄXEL", "SLAM",
          "AVLOPP", "KORROSION", "PUMPHUS", "STÄLLVERK", "GENERATOR", "BULTAR", "TÄTNING", "LÄCKA", "KYLTORN", "TANK",
          "MUDDER", "SKRÄP", "KEDJOR", "HAMNKRANAR", "RIVNING" },
        { "SPIRAL", "VIRVEL", "TRANS", "PENDEL", "FEBER", "SÖMNLÖS", "LOOP", "MANTRA", "KRAMP", "SPEGEL",
          "LABYRINT", "UPPREPNING", "TICK", "MAGNET", "HJULET", "VÄXLING", "SNURR", "DRÖM", "PARANOIA", "SVÄRM",
          "HALLUCINATION", "RYCK", "PUPILL", "EKOT", "GRYNING" },
        { "SVETT", "HÖFTEN", "SLÄPA", "GUNG", "STUDS", "SNEDSTEG", "KNYCK", "VAGGA", "SVAJ", "SLINK",
          "FJÄDRING", "LUTNING", "SLANG", "KROKEN", "SLÄNG", "SNUBBLA", "VRICK", "SKUMPA", "TRÅNG", "SVÄNG",
          "GOLVET", "NACKEN", "GALEJ", "KNÄNA", "SISTA DANSEN" } };
    const int s = presetStyle (i), k = i % 50;
    std::string num = std::to_string (k + 1);
    if (num.size() < 2) num = "0" + num;
    return styleNames()[(size_t) s] + " " + num + " " + w[s][k % 25] + (k >= 25 ? " II" : "");
}
} // namespace gl
