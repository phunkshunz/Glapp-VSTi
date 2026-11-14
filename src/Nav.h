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
        char pad[32];
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
} // namespace mh
