#pragma once

// PROVLJUD: en liten inbyggd trummaskin så att du hör GLAPP direkt,
// utan att koppla den till något. Stäng av den när BETONG tar över.
//
// Ljudkvalitet: alla filter räknas i Hz (låter likadant i 44,1, 48 och 96 kHz),
// basen är bandbegränsad (polyBLEP, ingen digital fräs) och varje ljud tonas ut
// de sista millisekunderna i stället för att klippas av.

#include "Model.h"
#include <cmath>
#include <cstdint>

namespace gl
{
class Prov
{
public:
    void prepare (double sampleRate) { sr = sampleRate > 0 ? sampleRate : 48000.0; for (auto& v : voices) v.on = false; }

    void trigger (int role, int note, int vel)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.on) { v = &x; break; }
        if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.t > v->t) v = &x; }   // stjäl den äldsta
        *v = Voice {};
        v->on = true;
        v->role = role;
        // mjuk anslagskurva: svaga spöknoter hörs, men starka slag sticker inte iväg
        const float a = (float) vel / 127.0f;
        v->amp = a * (0.35f + 0.65f * a);
        v->noise = 0x1234567u + (uint32_t) note * 977u + (uint32_t) (counter++ * 131u);
        const int n = role == RAckord ? note + 24 : (role == RBas ? note + 12 : note);
        v->freq = 440.0 * std::pow (2.0, (n - 69) / 12.0);
        v->setup (sr);
    }

    // Lägger till ljud i L/R från sample start, num samples.
    void render (float* L, float* R, int start, int num, float level)
    {
        for (auto& v : voices)
        {
            if (! v.on) continue;
            const float pan = panOf (v.role);
            const float gl = std::sqrt (0.5f * (1.0f - pan)) * 1.4142f, gr = std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
            for (int i = start; i < start + num; ++i)
            {
                const float s = v.next() * v.amp * level;
                L[i] += s * gl;
                R[i] += s * gr;
                if (! v.on) break;
            }
        }
    }

private:
    static float panOf (int role) { static const float p[] = { 0.0f, 0.1f, 0.25f, -0.3f, 0.35f, -0.2f, 0.0f, -0.1f }; return p[role & 7]; }

    // Tillståndsvariabelfilter (TPT), stabilt och samplingsfrekvensoberoende
    struct Svf
    {
        float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
        void set (double hz, float q, double sr)
        {
            hz = std::min (hz, sr * 0.45);
            const float g = (float) std::tan (3.14159265358979 * hz / sr);
            k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void tick (float x, float& lp, float& bp, float& hp)
        {
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
    };

    struct Voice
    {
        bool on = false;
        int role = 0;
        float amp = 1.0f;
        double t = 0.0, dt = 1.0 / 48000.0, sr = 48000.0, phase = 0.0, phase2 = 0.0, freq = 100.0, len = 0.5;
        uint32_t noise = 1;
        Svf f1, f2, f3;
        int counter = 0;

        float rnd() { noise = noise * 1664525u + 1013904223u; return (float) (noise >> 8) / 8388608.0f - 1.0f; }

        // bandbegränsad sågtand: tar bort vikningen vid vågens hopp
        static double polyBlep (double ph, double inc)
        {
            if (ph < inc) { const double x = ph / inc; return x + x - x * x - 1.0; }
            if (ph > 1.0 - inc) { const double x = (ph - 1.0) / inc; return x * x + x + x + 1.0; }
            return 0.0;
        }

        void setup (double sampleRate)
        {
            sr = sampleRate; dt = 1.0 / sr;
            switch (role)
            {
                case RKick:  len = 0.65; f1.set (2500.0, 0.7f, sr); break;          // klickets ton
                case RRim:   len = 0.09; f1.set (1900.0, 2.5f, sr); break;
                case RHat:   len = 0.11; f1.set (7600.0, 0.7f, sr); f2.set (10500.0, 1.6f, sr); f3.set (15500.0, 0.7f, sr); break;
                case RPerk1: len = 0.16; break;
                case RPerk2: len = 0.26; break;
                case RKlick: len = 0.03; f1.set (4200.0, 3.0f, sr); break;
                case RBas:   len = 0.48; break;
                default:     len = 0.7; f1.set (1800.0, 0.7f, sr); break;
            }
        }

        float next()
        {
            constexpr double tau = 6.283185307179586;
            float s = 0.0f, lp, bp, hp;
            switch (role)
            {
                case RKick:
                {
                    // tonhöjdssvep, rund kropp med lite mättning, och ett kort filtrerat klick överst
                    const double f = 44.0 + 125.0 * std::exp (-t * 30.0) + 30.0 * std::exp (-t * 300.0);
                    phase += tau * f * dt;
                    const float body = (float) (std::sin (phase) * std::exp (-t * 6.0));
                    f1.tick (rnd(), lp, bp, hp);
                    const float click = bp * (float) std::exp (-t * 450.0) * 0.6f;
                    s = std::tanh (1.6f * body) / std::tanh (1.6f) + click;
                    break;
                }
                case RRim:
                {
                    phase += tau * 1720.0 * dt; phase2 += tau * 830.0 * dt;
                    f1.tick (rnd(), lp, bp, hp);
                    s = (float) ((0.55 * std::sin (phase) + 0.35 * std::sin (phase2) + 0.6 * bp) * std::exp (-t * 95.0));
                    break;
                }
                case RHat:
                {
                    // metallisk hat: högpass + en resonans högt upp, som en analog trummaskin
                    const float n = rnd();
                    f1.tick (n, lp, bp, hp);
                    float l2, b2, h2;
                    f2.tick (hp, l2, b2, h2);
                    // toppen kapas vid 15,5 kHz så att hatten låter lika i 44,1 och 96 kHz
                    float l3, b3, h3;
                    f3.tick (hp * 0.5f + b2 * 0.7f, l3, b3, h3);
                    s = (float) (l3 * std::exp (-t * 62.0));
                    break;
                }
                case RPerk1:
                {
                    phase += tau * 540.0 * dt; phase2 += tau * 1242.0 * dt;
                    s = (float) ((std::sin (phase) + 0.3 * std::sin (phase2)) * 0.6 * std::exp (-t * 36.0));
                    break;
                }
                case RPerk2:
                {
                    const double f = 380.0 + 260.0 * std::exp (-t * 40.0);
                    phase += tau * f * dt;
                    s = (float) (std::sin (phase) * 0.6 * std::exp (-t * 21.0));
                    break;
                }
                case RKlick:
                {
                    phase += tau * 3300.0 * dt;
                    f1.tick (rnd(), lp, bp, hp);
                    s = (float) ((std::sin (phase) * 0.45 + bp * 0.25) * std::exp (-t * 320.0));
                    break;
                }
                case RBas:
                {
                    const double inc = freq * dt;
                    phase += inc; if (phase >= 1.0) phase -= 1.0;
                    const float saw = (float) (2.0 * phase - 1.0 - polyBlep (phase, inc));
                    // filtret stänger snabbt efter anslaget: plockigt, mörkt, minimalt
                    if ((counter++ & 15) == 0) f1.set (140.0 + 1900.0 * std::exp (-t * 16.0), 1.1f, sr);
                    f1.tick (saw, lp, bp, hp);
                    s = (float) (lp * 0.9 * std::exp (-t * 6.5));
                    break;
                }
                default:
                {
                    phase += freq * dt; if (phase >= 1.0) phase -= 1.0;
                    const float tri = (float) (4.0 * std::abs (phase - 0.5) - 1.0);
                    f1.tick (tri, lp, bp, hp);
                    s = (float) (lp * 0.24 * std::exp (-t * 4.5));
                    break;
                }
            }
            // mjuk start (0,3 ms) och uttoning de sista 6 ms, så att inget klickar
            const double fadeIn = std::min (1.0, t / 0.0003);
            const double fadeOut = std::min (1.0, (len - t) / 0.006);
            s *= (float) (role == RKick || role == RRim || role == RKlick ? std::max (0.0, fadeOut) : fadeIn * std::max (0.0, fadeOut));
            t += dt;
            if (t >= len) on = false;
            return s;
        }
    };

    double sr = 48000.0;
    uint32_t counter = 0;
    std::array<Voice, 32> voices {};
};
} // namespace gl
