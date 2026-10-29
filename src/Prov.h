#pragma once

// PROVLJUD: en liten inbyggd trummaskin så att du hör GLAPP direkt,
// utan att koppla den till något. Stäng av den när BETONG tar över.

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
        v->amp = (float) vel / 127.0f;
        v->noise = 0x1234567u + (uint32_t) note * 977u + (uint32_t) (counter++ * 131u);
        const int n = role == RAckord ? note + 24 : (role == RBas ? note + 12 : note);
        v->freq = 440.0 * std::pow (2.0, (n - 69) / 12.0);
    }

    // Lägger till ljud i L/R från sample start, num samples.
    void render (float* L, float* R, int start, int num, float level)
    {
        const double dt = 1.0 / sr;
        for (auto& v : voices)
        {
            if (! v.on) continue;
            for (int i = start; i < start + num; ++i)
            {
                const float s = v.next (dt, sr) * v.amp * level;
                const float pan = panOf (v.role);
                L[i] += s * (1.0f - pan);
                R[i] += s * (1.0f + pan);
                if (! v.on) break;
            }
        }
    }

private:
    static float panOf (int role) { static const float p[] = { 0.0f, 0.1f, 0.25f, -0.3f, 0.35f, -0.2f, 0.0f, -0.1f }; return p[role & 7]; }

    struct Voice
    {
        bool on = false;
        int role = 0;
        float amp = 1.0f;
        double t = 0.0, phase = 0.0, phase2 = 0.0, freq = 100.0;
        uint32_t noise = 1;
        float lp = 0.0f, lp2 = 0.0f;

        float rnd() { noise = noise * 1664525u + 1013904223u; return (float) (noise >> 8) / 8388608.0f - 1.0f; }

        float next (double dt, double sr)
        {
            constexpr double tau = 6.283185307179586;
            float s = 0.0f;
            double len = 0.5;
            switch (role)
            {
                case RKick:
                {
                    const double f = 46.0 + 120.0 * std::exp (-t * 32.0);
                    phase += tau * f * dt;
                    s = (float) (std::sin (phase) * std::exp (-t * 6.5) + 0.25 * std::exp (-t * 400.0) * rnd());
                    len = 0.6; break;
                }
                case RRim:
                {
                    phase += tau * 1720.0 * dt; phase2 += tau * 830.0 * dt;
                    s = (float) ((0.55 * std::sin (phase) + 0.35 * std::sin (phase2) + 0.3 * rnd()) * std::exp (-t * 95.0));
                    len = 0.09; break;
                }
                case RHat:
                {
                    const float n = rnd();
                    lp += 0.45f * (n - lp);
                    s = (float) ((n - lp) * 0.8 * std::exp (-t * 70.0));
                    len = 0.1; break;
                }
                case RPerk1:
                {
                    phase += tau * 540.0 * dt; phase2 += tau * 1242.0 * dt;
                    s = (float) ((std::sin (phase) + 0.3 * std::sin (phase2)) * 0.6 * std::exp (-t * 38.0));
                    len = 0.15; break;
                }
                case RPerk2:
                {
                    const double f = 380.0 + 260.0 * std::exp (-t * 40.0);
                    phase += tau * f * dt;
                    s = (float) (std::sin (phase) * 0.6 * std::exp (-t * 22.0));
                    len = 0.25; break;
                }
                case RKlick:
                {
                    phase += tau * 3300.0 * dt;
                    s = (float) (std::sin (phase) * 0.5 * std::exp (-t * 320.0));
                    len = 0.03; break;
                }
                case RBas:
                {
                    phase += freq * dt; if (phase >= 1.0) phase -= 1.0;
                    const float saw = (float) (2.0 * phase - 1.0);
                    const float cut = (float) (0.02 + 0.12 * std::exp (-t * 18.0));
                    lp += cut * (saw - lp); lp2 += cut * (lp - lp2);
                    s = (float) (lp2 * 1.4 * std::exp (-t * 7.0));
                    len = 0.45; break;
                }
                default:
                {
                    phase += freq * dt; if (phase >= 1.0) phase -= 1.0;
                    const float tri = (float) (4.0 * std::abs (phase - 0.5) - 1.0);
                    lp += 0.08f * (tri - lp);
                    s = (float) (lp * 0.22 * std::exp (-t * 5.0));
                    len = 0.6; break;
                }
            }
            (void) sr;
            t += dt;
            if (t > len) on = false;
            return s;
        }
    };

    double sr = 48000.0;
    uint32_t counter = 0;
    std::array<Voice, 32> voices {};
};
} // namespace gl
