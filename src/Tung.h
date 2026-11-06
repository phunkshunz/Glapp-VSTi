#pragma once

// GLAPP · TUNGT
// Inbyggd trummaskin med tunga, mörka samplingar ur BETONG-biblioteket: sex kit, åtta spår.
// Ersätter det gamla syntade provljudet. Samma gränssnitt: trigger (spår, not, styrka) och render.
//
// Ljudet:
//  - Samplingarna avkodas en gång (FLAC ur pluginet) och spelas med kubisk interpolation.
//  - Kick, hat, rim och klick är monofoniska per spår (nytt slag kväver det gamla, som på en riktig maskin).
//  - MÖRKERSTEGET på mastern, samma som i KOLV:
//      MULLER: kicken genom ett mörkt betongrum, bara basen kvar, duckad av kicken
//      TRYCK:  kicken klipps hårt, resten parallellkomprimeras och mättas
//      MÖRKER: allt utom kicken tappar sin gnistrande diskant

#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>
#include "Model.h"
#if __has_include ("BinaryData.h")
 #include "BinaryData.h"
 #define GLAPP_HAS_KIT 1
#else
 #define GLAPP_HAS_KIT 0
#endif

namespace gl
{
struct TungKit { const char* name; const char* file[NT]; };

// KICK, RIM, HAT, PERK 1, PERK 2, KLICK, BAS, ACKORD
inline constexpr TungKit kTungKits[] = {
    { "ÅNGVÄLT",     { "K11_Angvalt", "P22_Rim_Crack", "H13_Hat_Shuffle_Fat", "P20_Hammer_Clank", "P17_Metal_Pipe", "C10_Clap_Stack_Hard", "B13_Growl_Bass_F", "S11_Rave_Stab_Dist_Fm" } },
    { "STÅLPRESSEN", { "K13_Stampen", "C09_Snare_Rost", "H09_Hard_Closed_Dist", "P18_Anvil", "F05_Hammare_Stal", "C07_Clap_Smash", "B14_Dist_Saw_Bass_F", "S12_Industrial_Stab_Fm" } },
    { "BUNKERN",       { "K17_Bunker_Long_F", "C19_Rim_909", "H12_Hat_Tape_909", "P19_Tom_Dist_Low", "P13_Cowbell_Dark", "C08_Clap_Warehouse", "B05_Reese_Dist_F", "S14_Dub_Stab_Dirty_Fm" } },
    { "ROSTVERKET",    { "K14_Rost_Kick_F", "P22_Rim_Crack", "H06_Industrial_6bit", "F06_Slagga_Tung", "F15_Skrot", "C04_Snare_Noise_Industrial", "B15_Grind_FM_Bass_F", "S06_Crushed_Noise_Chord" } },
    { "VALSVERKET",    { "K18_Tunga_Vals", "C05_Rim", "H05_Closed_12bit", "P21_Tom_Dist_Mid", "X11_Metal_Slam", "C10_Clap_Stack_Hard", "B16_Hoover_Bass_F", "S13_Hoover_Stab_Fm" } },
    { "MULLRET",       { "K12_Rumble_Tunnel", "P22_Rim_Crack", "H02_Closed_Short", "P16_Spring_Thud", "F13_Ror_Stort", "C06_Clap_Layer_Verb", "B07_Rumble_Tone_F", "T08_Drone_Dark_F" } },
};
inline constexpr int kNumTungKits = (int) (sizeof (kTungKits) / sizeof (kTungKits[0]));

class Tung
{
public:
    struct Sample { std::vector<float> l, r; double rate = 48000.0; };

    Tung() { loadAll(); }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0 ? sampleRate : 48000.0;
        for (auto& v : voices) v.on = false;
        kickBuf.assign (kChunk * 2, 0.0f); busBuf.assign (kChunk * 2, 0.0f); duckBuf.assign (kChunk, 0.0f);
        static const double ms[4] = { 41.3, 53.9, 67.1, 89.7 };
        for (int k = 0; k < 4; ++k) { rumble[(size_t) k].assign ((size_t) (sr * ms[k] * 0.001) + 2, 0.0f); rumblePos[(size_t) k] = 0; }
        rbLpS.fill (0.0f); rb1 = rb2 = rbHpS = mk1L = mk2L = mk1R = mk2R = compEnv = dcL = dcR = 0.0f; duckEnv = 0.0f;
    }

    // inställningar (läses av ljudtråden)
    std::atomic<int> kit { 0 };
    std::atomic<float> muller { 0.35f }, tryck { 0.5f }, morker { 0.35f };

    void trigger (int role, int note, int vel)
    {
        if (role < 0 || role >= NT) return;
        const int k = juce::jlimit (0, kNumTungKits - 1, kit.load (std::memory_order_relaxed));
        const auto* smp = samples[(size_t) k][(size_t) role].get();
        if (smp == nullptr || smp->l.empty()) return;
        const bool mono = role == RKick || role == RHat || role == RRim || role == RKlick;
        if (mono)
            for (auto& x : voices) if (x.on && x.role == role && x.fade < 0) x.fade = (int) (sr * 0.004);   // kväv det gamla slaget
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.on) { v = &x; break; }
        if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
        *v = Voice {};
        v->on = true; v->role = role; v->smp = smp;
        const float a = (float) vel / 127.0f;
        v->amp = a * (0.3f + 0.7f * a);
        // tonhöjd relativt spårets standardnot: standard = samplingen orörd, scrollhjulet stämmer om
        const int semis = juce::jlimit (-24, 24, note - (36 + role));
        v->step = smp->rate / sr * std::pow (2.0, semis / 12.0);
        v->pan = panOf (role);
        if (role == RKick) duckEnv = 1.0f;
    }

    // Lägger till ljud i L/R från start, num sampel. level = PROVLJUD-nivån.
    void render (float* L, float* R, int start, int num, float level)
    {
        while (num > 0)
        {
            const int n = std::min (num, kChunk);
            renderChunk (L + start, R + start, n, level);
            start += n; num -= n;
        }
    }

    static const char* kitName (int k) { return kTungKits[juce::jlimit (0, kNumTungKits - 1, k)].name; }

private:
    static constexpr int kChunk = 512;
    static float panOf (int role) { static const float p[] = { 0.0f, 0.12f, 0.22f, -0.28f, 0.3f, -0.15f, 0.0f, -0.08f }; return p[role & 7]; }

    struct Voice
    {
        bool on = false;
        int role = 0, fade = -1, age = 0;
        const Sample* smp = nullptr;
        double pos = 0.0, step = 1.0;
        float amp = 1.0f, pan = 0.0f;
    };

    void loadAll()
    {
       #if GLAPP_HAS_KIT
        juce::FlacAudioFormat flac;
        for (int k = 0; k < kNumTungKits; ++k)
            for (int r = 0; r < NT; ++r)
            {
                const juce::String want = juce::String (kTungKits[k].file[r]) + ".flac";
                // samma fil i flera kit: dela den
                std::shared_ptr<Sample> found;
                for (int k2 = 0; k2 < k && found == nullptr; ++k2)
                    for (int r2 = 0; r2 < NT; ++r2)
                        if (juce::String (kTungKits[k2].file[r2]) + ".flac" == want && samples[(size_t) k2][(size_t) r2] != nullptr)
                        { found = samples[(size_t) k2][(size_t) r2]; break; }
                if (found == nullptr)
                    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
                        if (want == BinaryData::originalFilenames[i])
                        {
                            int size = 0;
                            const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
                            std::unique_ptr<juce::AudioFormatReader> rd (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
                            if (rd == nullptr) break;
                            auto s = std::make_shared<Sample>();
                            const int len = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 8.0));
                            juce::AudioBuffer<float> b ((int) juce::jmax (1u, rd->numChannels), len);
                            rd->read (&b, 0, len, 0, true, true);
                            s->rate = rd->sampleRate;
                            s->l.assign (b.getReadPointer (0), b.getReadPointer (0) + len);
                            s->r.assign (b.getReadPointer (b.getNumChannels() > 1 ? 1 : 0), b.getReadPointer (b.getNumChannels() > 1 ? 1 : 0) + len);
                            // 3 ms uttoning i slutet: inget klick om samplingen är klippt hårt
                            const int f = std::min (len, (int) (s->rate * 0.003));
                            for (int j = 0; j < f; ++j) { const float g = (float) j / (float) f; s->l[(size_t) (len - 1 - j)] *= g; s->r[(size_t) (len - 1 - j)] *= g; }
                            found = s;
                            break;
                        }
                samples[(size_t) k][(size_t) r] = found;
            }
       #endif
    }

    static float hermite (const std::vector<float>& d, double pos)
    {
        const int i = (int) pos;
        const float t = (float) (pos - i);
        const int n = (int) d.size();
        const float y0 = d[(size_t) juce::jmax (0, i - 1)], y1 = d[(size_t) i], y2 = d[(size_t) juce::jmin (n - 1, i + 1)], y3 = d[(size_t) juce::jmin (n - 1, i + 2)];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void renderChunk (float* L, float* R, int n, float level)
    {
        float* kl = kickBuf.data(); float* kr = kl + kChunk;
        float* bl = busBuf.data();  float* br = bl + kChunk;
        std::fill (kickBuf.begin(), kickBuf.end(), 0.0f);
        std::fill (busBuf.begin(), busBuf.end(), 0.0f);
        const float fadeIn = (float) (sr * 0.0005);
        for (auto& v : voices)
        {
            if (! v.on || v.smp == nullptr) continue;
            const auto& s = *v.smp;
            const int len = (int) s.l.size();
            float* oL = v.role == RKick ? kl : bl;
            float* oR = v.role == RKick ? kr : br;
            const float gl = v.amp * std::sqrt (0.5f * (1.0f - v.pan)) * 1.41f, gr = v.amp * std::sqrt (0.5f * (1.0f + v.pan)) * 1.41f;
            const int fadeLen = (int) (sr * 0.004);
            for (int i = 0; i < n; ++i)
            {
                if (v.pos >= len - 2) { v.on = false; break; }
                float g = v.pos < fadeIn ? (float) (v.pos / fadeIn) : 1.0f;   // ingen klick i början
                if (v.fade >= 0)
                {
                    g *= (float) v.fade / (float) fadeLen;
                    if (--v.fade < 0) { v.on = false; break; }
                }
                oL[i] += hermite (s.l, v.pos) * g * gl;
                oR[i] += hermite (s.r, v.pos) * g * gr;
                v.pos += v.step;
            }
            ++v.age;
        }

        // ---------------------------------------------------------------- MÖRKERSTEGET
        const float fsr = (float) sr;
        const float mu = muller.load (std::memory_order_relaxed), tr = tryck.load (std::memory_order_relaxed), mo = morker.load (std::memory_order_relaxed);
        const float mkLp = 1.0f - std::exp (-6.2831853f * (20000.0f * std::pow (2200.0f / 20000.0f, mo)) / fsr);
        const float rbLp = 1.0f - std::exp (-6.2831853f * 900.0f / fsr), rbOut = 1.0f - std::exp (-6.2831853f * 150.0f / fsr);
        const float rbHp = 1.0f - std::exp (-6.2831853f * 32.0f / fsr);
        const float compAtk = 1.0f - std::exp (-1.0f / (0.0008f * fsr)), compRel = 1.0f - std::exp (-1.0f / (0.07f * fsr));
        const float duckDec = std::exp (-1.0f / (0.18f * fsr));
        const float kickDrive = 1.0f + 3.0f * tr, kickNorm = 1.0f / std::tanh (kickDrive);
        for (int i = 0; i < n; ++i)
        {
            const float duck = duckEnv; duckEnv *= duckDec;
            float a = kl[i], b = kr[i], c = bl[i], d = br[i];
            if (mo > 0.001f)
            {
                mk1L += (c - mk1L) * mkLp; mk2L += (mk1L - mk2L) * mkLp;
                mk1R += (d - mk1R) * mkLp; mk2R += (mk1R - mk2R) * mkLp;
                c = mk2L * (1.0f + 0.25f * mo); d = mk2R * (1.0f + 0.25f * mo);
            }
            if (tr > 0.001f)
            {
                a = std::tanh (a * kickDrive) * kickNorm; b = std::tanh (b * kickDrive) * kickNorm;
                const float lev = std::max (std::fabs (c), std::fabs (d));
                compEnv += (lev - compEnv) * (lev > compEnv ? compAtk : compRel);
                const float th = 0.12f, gr = compEnv > th ? th / compEnv : 1.0f, par = tr * 0.9f;
                c += (std::tanh (c * gr * 3.1f) * 0.8f - c) * par * 0.6f + c * gr * 2.2f * par * 0.25f;
                d += (std::tanh (d * gr * 3.1f) * 0.8f - d) * par * 0.6f + d * gr * 2.2f * par * 0.25f;
                // osymmetrisk mättning (jämna övertoner), med DC-spärr
                const float s1 = c + 0.09f * tr * c * c, s2 = d + 0.09f * tr * d * d;
                dcL += (s1 - dcL) * 0.0005f; dcR += (s2 - dcR) * 0.0005f;
                c = s1 - dcL; d = s2 - dcR;
            }
            float l = a + c, r = b + d;
            if (mu > 0.001f)
            {
                const float in = (kl[i] + kr[i]) * 0.5f;
                std::array<float, 4> o {};
                for (int k = 0; k < 4; ++k) o[(size_t) k] = rumble[(size_t) k][(size_t) rumblePos[(size_t) k]];
                const float h1 = o[0] + o[1], h2 = o[0] - o[1], h3 = o[2] + o[3], h4 = o[2] - o[3];
                const std::array<float, 4> mx { (h1 + h3) * 0.5f, (h2 + h4) * 0.5f, (h1 - h3) * 0.5f, (h2 - h4) * 0.5f };
                const float fb = 0.80f + 0.12f * mu;
                for (int k = 0; k < 4; ++k)
                {
                    rbLpS[(size_t) k] += (mx[(size_t) k] - rbLpS[(size_t) k]) * rbLp;
                    rumble[(size_t) k][(size_t) rumblePos[(size_t) k]] = std::tanh (in * 0.6f + rbLpS[(size_t) k] * fb);
                    if (++rumblePos[(size_t) k] >= (int) rumble[(size_t) k].size()) rumblePos[(size_t) k] = 0;
                }
                const float wet = (o[0] + o[1] + o[2] + o[3]) * 0.25f;
                rb1 += (wet - rb1) * rbOut; rb2 += (rb1 - rb2) * rbOut;
                rbHpS += (rb2 - rbHpS) * rbHp;
                const float sub = (rb2 - rbHpS) * (1.0f - 0.95f * duck) * mu * 2.6f;
                l += sub; r += sub;
            }
            const float drive = level * 0.9f * (1.0f + 0.6f * tr);
            l = std::tanh (l * drive) * 0.9f; r = std::tanh (r * drive) * 0.9f;
            if (! std::isfinite (l)) l = 0.0f;
            if (! std::isfinite (r)) r = 0.0f;
            L[i] += l; R[i] += r;
        }
    }

    double sr = 48000.0;
    std::array<std::array<std::shared_ptr<Sample>, NT>, kNumTungKits> samples {};
    std::array<Voice, 32> voices {};
    std::vector<float> kickBuf, busBuf, duckBuf;
    std::array<std::vector<float>, 4> rumble;
    std::array<int, 4> rumblePos {};
    std::array<float, 4> rbLpS {};
    float rb1 = 0, rb2 = 0, rbHpS = 0, mk1L = 0, mk2L = 0, mk1R = 0, mk2R = 0, compEnv = 0, dcL = 0, dcR = 0, duckEnv = 0;
};
} // namespace gl
