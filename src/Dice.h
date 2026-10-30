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

inline TrackGen diceTrack (int role, int style, Rng& r, const TrackCfg& cur, int avoidLen = 0)
{
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
            const float lo[] = { 14, 10, 20, 12, 18, 4, 0 }, hi[] = { 32, 24, 40, 30, 34, 10, 0 };
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
        default:         g.slap = r.rangef (0.9f, 1.2f); g.ilMode = r.pick ({ (int) IlSpoke, (int) IlFlytta }); break;
    }
    g.human = isRaster (style) ? 0.0f : r.rangef (0.8f, 1.3f);
    g.interlock = true;
    switch (style)
    {
        case SKlinisk: g.tomrum = r.rangef (0.45f, 0.7f); break;
        case SRaster:  g.tomrum = r.rangef (0.2f, 0.45f); break;
        case SBerlin:
        case SDub:     g.tomrum = r.rangef (0.15f, 0.35f); break;
        default:       g.tomrum = r.rangef (0.05f, 0.25f); break;
    }
    return g;
}

// ---------------------------------------------------------------- 350 presets
// 7 stilar x 50. Varje preset är ett fast frö till tärningen, så de är alltid desamma.
// Nummer 0-199 är desamma som i GLAPP 1.
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
          "SERIE", "INDEX", "TAKTIL", "SYNK", "NOD" } };
    const int s = presetStyle (i), k = i % 50;
    std::string num = std::to_string (k + 1);
    if (num.size() < 2) num = "0" + num;
    return styleNames()[(size_t) s] + " " + num + " " + w[s][k % 25] + (k >= 25 ? " II" : "");
}
} // namespace gl
