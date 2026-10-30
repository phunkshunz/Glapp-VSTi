#pragma once

// GLAPP: grunddata. Ren C++ utan JUCE, så att motorn kan testas fristående.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

namespace gl
{
constexpr int NT = 8;      // antal spår
constexpr int MAXS = 32;   // max steg per spår

// Spårens roller. Rollen styr tärningens regler och interlock (KICK är ankaret).
enum Role { RKick, RRim, RHat, RPerk1, RPerk2, RKlick, RBas, RAckord };

inline const char* roleName (int r)
{
    static const char* n[] = { "KICK", "RIM", "HAT", "PERK 1", "PERK 2", "KLICK", "BAS", "ACKORD" };
    return n[(r < 0 || r >= NT) ? 0 : r];
}

// ---------------------------------------------------------------- val-listor
// Klockdelare: längd på ett steg i kvartsnoter (ppq).
inline const std::vector<std::string>& divNames() { static const std::vector<std::string> v { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; return v; }
inline double divPpq (int d)
{
    static const double v[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return v[(d < 0 || d > 5) ? 3 : d];
}
enum Div { D4, D8, D8T, D16, D16T, D32 };

enum Orbit { OAv, OStegFram, OStegBak, OGlid };
inline const std::vector<std::string>& orbitNames() { static const std::vector<std::string> v { "AV", "STEG +", "STEG -", "GLID" }; return v; }

inline const std::vector<std::string>& barsNames() { static const std::vector<std::string> v { "1", "2", "4", "8", "16" }; return v; }
inline int barsValue (int i) { static const int v[] = { 1, 2, 4, 8, 16 }; return v[(i < 0 || i > 4) ? 2 : i]; }

enum Event { EAv, ETyst, EHicka, ERatchet, EHopp, EStuds };
inline const std::vector<std::string>& eventNames() { static const std::vector<std::string> v { "AV", "TYST", "HICKA", "RATCHET", "HOPP", "STUDS" }; return v; }

enum Chord { CEn, CMoll7, CMoll9, CKvart, CDub };
inline const std::vector<std::string>& chordNames() { static const std::vector<std::string> v { "EN TON", "MOLL 7", "MOLL 9", "KVART", "DUB" }; return v; }
inline std::vector<int> chordIntervals (int c)
{
    switch (c)
    {
        case CMoll7: return { 0, 3, 7, 10 };
        case CMoll9: return { 0, 3, 7, 10, 14 };
        case CKvart: return { 0, 5, 10, 15 };
        case CDub:   return { 0, 7, 10, 15 };
        default:     return { 0 };
    }
}

enum IlMode { IlSpoke, IlFlytta, IlTyst };
inline const std::vector<std::string>& ilNames() { static const std::vector<std::string> v { "SPOKE", "FLYTTA", "TYST" }; return v; }

// De fyra första är GLAPP 1. KÖLN, KLINISK och RASTER kom i v2 (sist i listan, så att gamla projekt laddar rätt).
enum Style { SRominimal, SBerlin, SDub, SOrganisk, SKoln, SKlinisk, SRaster };
constexpr int NSTYLES = 7;
inline const std::vector<std::string>& styleNames() { static const std::vector<std::string> v { "BUKAREST", "BERLIN", "MIKRODUB", "ORGANISK", "KÖLN", "KLINISK", "RASTER" }; return v; }

// ---------------------------------------------------------------- mönstret
// Ett steg: velocity (0 = av) och tonhöjd i halvtoner (för BAS/ACKORD).
// Lagras packat i en atomic så att GUI och ljudtråd kan dela utan lås.
struct Step { int vel = 0; int pitch = 0; };

struct Pattern
{
    std::array<std::array<std::atomic<uint16_t>, MAXS>, NT> cell {};

    Pattern() { clear(); }
    void clear() { for (auto& t : cell) for (auto& c : t) c.store (pack ({}), std::memory_order_relaxed); }

    static uint16_t pack (Step s)
    {
        const int v = s.vel < 0 ? 0 : (s.vel > 127 ? 127 : s.vel);
        const int p = (s.pitch < -24 ? -24 : (s.pitch > 24 ? 24 : s.pitch)) + 64;
        return (uint16_t) (v | (p << 8));
    }
    static Step unpack (uint16_t c) { return { (int) (c & 0xff), (int) (c >> 8) - 64 }; }

    Step get (int t, int i) const { return unpack (cell[(size_t) t][(size_t) i].load (std::memory_order_relaxed)); }
    void set (int t, int i, Step s) { cell[(size_t) t][(size_t) i].store (pack (s), std::memory_order_relaxed); }

    // Text: ett spår per rad, "vel:pitch" kommaseparerat. Används i state och presets.
    std::string trackToString (int t) const
    {
        std::string s;
        for (int i = 0; i < MAXS; ++i)
        {
            auto st = get (t, i);
            if (i) s += ',';
            s += std::to_string (st.vel);
            if (st.pitch != 0) { s += ':'; s += std::to_string (st.pitch); }
        }
        return s;
    }
    void trackFromString (int t, const std::string& s)
    {
        int i = 0;
        size_t pos = 0;
        while (i < MAXS)
        {
            size_t comma = s.find (',', pos);
            std::string tok = s.substr (pos, comma == std::string::npos ? std::string::npos : comma - pos);
            Step st;
            if (! tok.empty())
            {
                size_t colon = tok.find (':');
                st.vel = std::atoi (tok.substr (0, colon).c_str());
                if (colon != std::string::npos) st.pitch = std::atoi (tok.substr (colon + 1).c_str());
            }
            set (t, i++, st);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        for (; i < MAXS; ++i) set (t, i, {});
    }
};

// ---------------------------------------------------------------- inställningar
// Allt motorn behöver om ett spår. Läses från parametrarna varje block.
struct TrackCfg
{
    int role = 0;
    bool mute = false;
    int length = 16;      // 1..32
    int div = D16;
    int note = 36;
    int channel = 1;      // 1..16
    float level = 1.0f;   // 0..1.5
    float drag = 0.0f;    // ms, 0..60
    float inertia = 0.0f; // 0..1
    float jitter = 0.0f;  // ms, 0..5
    float chance = 1.0f;  // 0..1
    float gate = 0.4f;    // del av steget, 0.05..1
    int orbit = OAv;
    int orbitBars = 2;    // index i barsNames
    int event = EAv;
    int eventBars = 3;    // index i barsNames
    int chord = CEn;
};

struct GlobalCfg
{
    float slap = 1.0f;    // skalar allt släp, 0..2
    float human = 1.0f;   // skalar jitter, 0..2
    bool followTempo = true;
    bool interlock = true;
    int ilMode = IlSpoke;
    float tomrum = 0.0f;  // 0..1, stryker slag (kicken rörs aldrig)
    uint32_t seed = 1;
};

// TOMRUM: hur gärna en roll tappar slag. Hat och klick går först, kicken aldrig.
inline double tomrumWeight (int role)
{
    static const double w[] = { 0.0, 0.35, 1.0, 0.8, 0.8, 0.9, 0.45, 0.6 };
    return w[(role < 0 || role >= NT) ? 0 : role];
}

// Snabb deterministisk slump: samma steg ger alltid samma värde, så att
// sökning, loopar och export blir stabila.
inline double hash01 (uint32_t seed, int track, int64_t n, int salt)
{
    uint64_t x = (uint64_t) seed * 0x9E3779B97F4A7C15ull ^ ((uint64_t) (uint32_t) track << 48) ^ (uint64_t) n * 0xBF58476D1CE4E5B9ull ^ ((uint64_t) salt << 56);
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return (double) (x >> 11) * (1.0 / 9007199254740992.0);
}

// Liten egen slumpgenerator för tärningen (inte beroende av JUCE).
struct Rng
{
    uint64_t s;
    explicit Rng (uint64_t seed) : s (seed * 2654435761ull + 0x632BE59BD9B4E019ull) { next(); }
    uint64_t next() { s += 0x9E3779B97F4A7C15ull; uint64_t z = s; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
    double uni() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    int range (int lo, int hi) { return lo + (int) (uni() * (double) (hi - lo + 1)); }   // lo..hi inklusive
    float rangef (float lo, float hi) { return lo + (float) uni() * (hi - lo); }
    bool chance (double p) { return uni() < p; }
    template <typename T> T pick (std::initializer_list<T> l) { auto i = (size_t) range (0, (int) l.size() - 1); return *(l.begin() + i); }
};
} // namespace gl
