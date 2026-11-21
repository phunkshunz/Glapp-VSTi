#pragma once

// MASKINHALLEN – NAV: BETONG berättar för de andra maskinerna hur det svänger.
// En liten minnesmappad fil i Application Support (samma sätt som SLINGAN), så att
// olika plugins i samma DAW kan läsa den, även när DAW:en kör dem i egna processer.
// BETONG skriver i ljudtråden utan lås. Läsarna (GRUND, GLAPP, KOLV) läser när de vill.
// Ett halvskrivet värde ibland gör inget: nästa block skriver om allt.

#include <juce_core/juce_core.h>
#include <atomic>
#include <cstring>

namespace mh
{
// Delarna i en låt, samma för alla maskiner
enum Fas : int32_t { FasIngen = 0, FasIntro, FasBygg, FasTopp, FasBreak, FasLyft, FasUtrull };

class Nav
{
public:
    static constexpr uint32_t kMagic = 0x4e415631;   // "NAV1"
    static constexpr int kLanes = 8;

    struct State
    {
        uint32_t magic;
        uint32_t version;
        std::atomic<uint64_t> seq;           // ökar varje skrivning
        std::atomic<int64_t> heartbeatMs;    // senaste skrivning
        double bpm;
        double ppq;                          // spelpositionen när det skrevs
        int32_t playing;
        float swing;                         // 0..0,75 (andel av en 16-del)
        float pocketMs;                      // hur långt bakom rutnätet allt ligger
        float laneOffsetMs[kLanes];          // spårens egna förskjutningar (friktion)
        double lastKickPpq;                  // senaste kicken, för sidechain
        uint32_t kickSteps;                  // takten som 16 bitar: var kicken slår
        float kickEnv;                       // 0..1, faller efter varje kick (sidechain-kurva)
        // LÅTLÄGE (NAV 2): motorn berättar var i låten vi är. Ryms i den gamla utfyllnaden,
        // så äldre läsare påverkas inte.
        int32_t motorOn;                     // 1 = motorn bygger låten just nu
        int32_t fas;                         // Fas nedan
        float energy;                        // 0..1
        float fasPos;                        // 0..1 genom delen
        float toNextBars;                    // takter till nästa del
        char pad[12];
    };

    static juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
           #if JUCE_MAC
            .getChildFile ("Application Support")
           #endif
            .getChildFile ("Phunkshunz").getChildFile ("Nav").getChildFile ("nav.map");
    }

    // Öppna (skapar filen om den saknas). Inte i ljudtråden.
    bool open()
    {
        if (map != nullptr) return true;
        const auto f = file();
        f.getParentDirectory().createDirectory();
        if (! f.existsAsFile() || f.getSize() != (juce::int64) sizeof (State))
        {
            juce::FileOutputStream out (f);
            if (! out.openedOk()) return false;
            out.setPosition (0); out.truncate();
            State zero; std::memset ((void*) &zero, 0, sizeof (State));
            out.write (&zero, sizeof (State));
        }
        map = std::make_unique<juce::MemoryMappedFile> (f, juce::MemoryMappedFile::readWrite, false);
        if (map->getData() == nullptr || map->getSize() < sizeof (State)) { map.reset(); return false; }
        auto* s = state();
        if (s->magic != kMagic) { std::memset ((void*) s, 0, sizeof (State)); s->magic = kMagic; s->version = 1; }
        return true;
    }

    bool isOpen() const { return map != nullptr; }
    State* state() const { return map ? static_cast<State*> (map->getData()) : nullptr; }

    // Lever det någon som skriver? (läsarna använder detta)
    bool alive (int64_t nowMs) const
    {
        auto* s = state();
        return s && s->magic == kMagic && nowMs - s->heartbeatMs.load() < 500;
    }

private:
    std::unique_ptr<juce::MemoryMappedFile> map;
};

// Läsarens hjälp: mjuka värden att styra ljudet med, i ljudtråden. Uppdatera en gång per block.
//   lift  0..1 stiger mot droppet (uppbyggnad och lyft)
//   brk   0..1 i breaket
//   drop  0..1 slår till när droppet kommer och klingar av på ett par takter
//   energy 0..1 låtens energi
// active = false när ingen motor skriver (då ska maskinen låta exakt som förut).
struct NavMotor
{
    bool active = false;
    int fas = FasIngen;
    float energy = 0.5f, lift = 0.0f, brk = 0.0f, drop = 0.0f;

    void update (const Nav& nav, int64_t nowMs, double blockSeconds) noexcept
    {
        const auto* s = nav.state();
        const bool on = s != nullptr && nav.alive (nowMs) && s->playing != 0 && s->motorOn != 0;
        float tLift = 0.0f, tBrk = 0.0f, tE = 0.5f;
        int f = FasIngen;
        if (on)
        {
            f = s->fas;
            tE = juce::jlimit (0.0f, 1.0f, s->energy);
            const float p = juce::jlimit (0.0f, 1.0f, s->fasPos);
            if (f == FasLyft) tLift = 0.3f + 0.7f * p * p;
            else if (f == FasBygg) tLift = 0.6f * p;
            if (f == FasBreak) tBrk = 1.0f;
            if (f == FasTopp && (fas == FasLyft || fas == FasBreak || fas == FasBygg)) drop = 1.0f;   // droppet!
        }
        active = on;
        fas = f;
        const float a = (float) juce::jlimit (0.0, 1.0, blockSeconds / 0.25);   // ~0,25 s glidning
        energy += (tE - energy) * a;
        lift += (tLift - lift) * a;
        brk += (tBrk - brk) * (float) juce::jlimit (0.0, 1.0, blockSeconds / 0.08);
        drop *= (float) std::exp (-blockSeconds / 1.5);   // klingar av på några sekunder
        if (! on) { lift = 0.0f; brk = 0.0f; drop = 0.0f; energy = 0.5f; }
    }
};
} // namespace mh
