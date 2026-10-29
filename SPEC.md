# GLAPP – spec

MIDI-generator och sequencer för mikrotiming. Gör inget eget ljud (utom PROVLJUD), skickar MIDI till nästa instrument.

## Grundidé

Andra groove-generatorer slumpar *vilka* noter som spelas. GLAPP bestämmer *när* de landar. Rominimal och Berlin-minimal bor i millisekunder: rim som hänger 20 ms efter, perc i 13 steg som aldrig går jämnt upp med takten, spöknoter som bara finns för att två ljud ville ha samma plats.

## Motorn (src/Engine.h)

- **Tillståndslös.** En not bestäms bara av spår, stegnummer räknat från låtens början och inställningarna. Tål hopp, loopar och spolning. Samma ställe i låten låter alltid likadant (all slump är en hash av frö + spår + steg).
- **Sample-exakt.** Varje block räknar motorn ut alla noter i blockets tidsfönster, inklusive noter från tidigare steg vars släp landar i blocket.
- **Bara sent, aldrig tidigt.** En not kan inte skickas innan den inträffar. DRAG går 0–60 ms.
- **TRÖGHET:** tittar bakåt på hur många träffar i rad som föregick steget (max 4) och lägger på 12/24/38/54 ms × TRÖGHET.
- **FÖLJ TEMPO:** ms × 122 / BPM.
- **ORBIT STEG ±:** mönstrets index = (steg ∓ antal passerade perioder) mod längd. **GLID:** triangelvåg 0–12 ms i steg om 3 ms per period.
- **HÄNDELSE:** sista takten i varje fras; påverkar slag i sista slaget (TYST hela takten).
- **INTERLOCK:** jämför spårets nominella tid mot alla spår med lägre nummer. Kicken påverkas aldrig.
- **Noter:** note on + note off efter NOTLÄNGD × steg. Samma ton som redan ljuder släpps först. Stopp och hopp släpper allt.

## Spår

| # | Roll | Standardnot | Tärningens regler |
|---|---|---|---|
| 1 | KICK | C1 36 | 4/4 (ibland euklidisk 5/16 i ORGANISK), aldrig släp. |
| 2 | RIM | C#1 37 | Backbeat 4/12 (BERLIN varierar), 1–3 spöknoter, släp per stil (14–32 ms BUKAREST). |
| 3 | HAT | D1 38 | Offbeats + spöknoter, eller euklidisk 13/15 i BERLIN/ORGANISK. |
| 4 | PERK 1 | D#1 39 | Primtalslängder per stil, 2–5 träffar per 16 steg, euklidiskt, orbit ofta. |
| 5 | PERK 2 | E1 40 | Som PERK 1 men aldrig samma längd. |
| 6 | KLICK | F1 41 | 2–3 klick, ofta 1/32, GLID, RATCHET. |
| 7 | BAS | F#1 42 | Offbeat-stötar med tonhöjd {0, −2, 3, 5, 7, 12}. |
| 8 | ACKORD | G1 43 | 1–2 stötar, ackordtyp per stil; BUKAREST tystnar ibland. |

Tärningen rör aldrig NOT, KANAL, TYST eller NIVÅ.

## Presets

200 = 4 stilar × 50. Varje preset är ett fast frö till tärningen (`presetSeed`). Frysta spår lämnas i fred.

## Filer

- `src/Model.h` – data, listor, mönster (lås-fritt med atomics), hash, slumpgenerator
- `src/Engine.h` – tidsmotorn
- `src/Dice.h` – stilregler, ORBIT EVOLVE, presets
- `src/Prov.h` – provljudets lilla trumkit
- `src/Params.h` – alla parametrar
- `src/PluginProcessor.*` – DAW-koppling, MIDI ut, state, ångra
- `src/PluginEditor.*`, `src/Gui.h`, `src/About.h` – gränssnittet
- `tests/` – motortest (ren C++) och plugintest (JUCE)
