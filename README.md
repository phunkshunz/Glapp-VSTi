# GLAPP

MIDI-maskin för mikrosläp och fasdrift, som VST3 och AU för macOS (VST3 för Windows). Phunkshunz AUDIO, serien **MASKINHALLEN**. Syskon till BETONG, FUKT, FRÄS, SMIDET, SMUTS, KOLV, SPÅN och VENTIL.

GLAPP är spelet i en sliten maskin, där kuggarna inte riktigt griper i varandra. Den gör inget ljud själv. Den skickar MIDI till BETONG, en trummaskin eller en synt, och bestämmer **när** varje slag landar: rimmet hänger efter, percen driver i udda längder och ingenting loopar exakt likadant två gånger. Kicken står still. Den är ankaret.

Hela specen finns i [SPEC.md](SPEC.md). Manualen (engelska, PDF) ligger i zip-filen.

## Ladda ner

Mac: https://github.com/phunkshunz/Glapp-VSTi/releases/latest/download/GLAPP-macOS.zip

Windows (VST3): https://github.com/phunkshunz/Glapp-VSTi/releases/latest/download/GLAPP-Windows.zip

Ladda inte ner via den gröna knappen **Code**. Den ger bara källkoden, inte pluginet.

## Installera

**Mac:** dubbelklicka på zip-filen i Hämtade filer. Öppna sedan Terminal och kör:

```
bash ~/Downloads/GLAPP/installera.command
```

**Stäng Bitwig helt och starta igen.** GLAPP finns bland instrumenten. Kräver macOS 11 eller nyare, Intel eller Apple Silicon.

**Windows:** packa upp och lägg `GLAPP.vst3` i `C:\Program Files\Common Files\VST3`.

## Koppla in den i Bitwig

1. Lägg **GLAPP** på ett instrumentspår.
2. Lägg **BETONG** (eller din trummaskin) direkt efter GLAPP i samma kedja. Bitwig skickar GLAPP:s noter vidare till nästa enhet.
3. Spår 1–8 spelar C1–G1 (not 36–43) på kanal 1, alltså pad 1–8 i BETONG. Byt NOT och KANAL i VERKSTAD om du vill något annat.

Kommer inga noter fram? Lägg BETONG på ett eget spår och välj GLAPP-spåret som notkälla (eller använd en Note Receiver).

Vill du bara höra GLAPP direkt: slå på **PROVLJUD**. Då spelar ett litet inbyggt trumkit med.

## Kom igång på en minut

1. Tryck play i Bitwig. GLAPP följer tempot och play/stop. Ingen DAW? Tryck **SPELA**.
2. Bläddra bland presets med pilarna, eller klicka på namnet och välj bland alla 200.
3. Välj **STIL** och tryck **DICE · MUTERA** tills något fastnar.
4. **FRYS** spåren du gillar. Slå igen: bara de andra ändras.
5. **ORBIT EVOLVE** behåller mönstret men ändrar hur det rör sig över tid.
6. Gick det för långt? **ÅNGRA** (upp till 40 steg).

## Vad finns i maskinen

| Del | Vad den gör |
|---|---|
| **8 spår** | KICK (ankaret), RIM, HAT, PERK 1, PERK 2, KLICK, BAS och ACKORD. Egen längd (1–32 steg) och egen takt (1/4 till 1/32, med trioler) per spår. |
| **DRAG** | Hur sent spåret slår, 0–60 ms, sample-exakt. |
| **TRÖGHET** | Slag tätt i rad halkar allt senare (+12, +24, +38 ms), som på en oljefläck. |
| **JITTER** | Liten mänsklig slump, 0–5 ms. Samma varje gång låten passerar stället, så loopar och export blir stabila. |
| **ORBIT** | STEG +/−: mönstret vandrar ett steg var N:e takt. GLID: släppet andas fram och tillbaka. |
| **HÄNDELSE** | På sista slaget i frasen (2/4/8/16 takter): TYST, HICKA (kastas 45 ms sent), RATCHET (trippelslag) eller HOPP (slaget hoppas över). |
| **INTERLOCK** | Två spår slår aldrig exakt samtidigt. Spåret högre upp vinner, kicken vinner alltid. Förloraren blir SPÖKE (velocity 18–35), FLYTTAS en sen 32-del eller tystas. |
| **SLÄP / HUMAN** | Skalar allt släp och all jitter på en gång. |
| **FÖLJ TEMPO** | Släppet är inställt för 122 BPM och följer tempot, så känslan blir densamma. |
| **ACKORD** | Flera toner per träff: MOLL 7, MOLL 9, KVART, DUB. |
| **Tärningar** | En stor (DICE · MUTERA), en per spår och ORBIT EVOLVE. Slumpen följer stilens regler och rör aldrig NOT eller KANAL. |
| **4 stilar** | BUKAREST, BERLIN, MIKRODUB och ORGANISK. |
| **200 presets** | 50 per stil. Frysta spår lämnas i fred vid preset-byte. |
| **URVERKET** | Ringar som visar hur spåren glider isär i realtid. |
| **PROVLJUD** | Litet inbyggt trumkit, så att du hör GLAPP utan att koppla något. |

**Stegrutnätet:** klick = på/av, dra = velocity, scroll = tonhöjd, shift-klick = spöknot, högerklick = rensa steg.

## Bygga själv

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --config Release
```

JUCE 8 hämtas automatiskt. GitHub Actions bygger varje push till `main` och lägger den som senaste release.

Tester: `g++ -std=c++17 -Isrc tests/engine_test.cpp -o engine_test && ./engine_test` testar motorn och alla 200 presets. `cmake -B build -DGLAPP_TESTS=ON` bygger också ett test av hela pluginet.
