#pragma once

#include <cmath>
#include "dsp/DspBlocks.h"

namespace fp::dsp
{
// Splits one channel into 1, 2 or 3 bands with Linkwitz-Riley 4th order crossovers. With three
// bands the low band also runs through the upper crossover's low + high (an all-pass), so the bands
// sum back to the same all-pass response as the mid and high bands: flat magnitude. The splits glide
// to new values (about 30 ms) and the upper one stays at least 1.5 times above the lower one.
class BandSplitter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        glide = 1.0 - std::exp (-(double) kUpdate / (0.03 * sr));
        reset();
    }

    void reset()
    {
        for (auto* x : { &a, &b, &comp })
            x->reset();
        current[0] = current[1] = -1.0;   // set at the next update
    }

    // the band count and the splits to glide to (Hz); call every kUpdate samples
    void update (int bandCount, double split, double split1, double split2)
    {
        bands = bandCount;
        const double hi = 0.45 * sr;
        double lo = 0.0, up = 0.0;
        if (bands == 2)
            lo = std::clamp (split, 20.0, hi);
        else if (bands == 3)
        {
            lo = std::clamp (split1, 20.0, hi / 1.5);
            up = std::clamp (split2, lo * 1.5, hi);
        }
        glideTo (0, lo, a);
        if (bands == 3)
        {
            glideTo (1, up, b);
            for (int k = 0; k < 2; ++k)   // the upper crossover's coefficients, the compensation's own state
            {
                copyCoefficients (comp.lp[k], b.lp[k]);
                copyCoefficients (comp.hp[k], b.hp[k]);
            }
        }
    }

    // one sample in, bands out (out[0] = lowest); unused bands are 0
    void process (float x, float* out)
    {
        if (bands <= 1)
        {
            out[0] = x; out[1] = out[2] = 0.0f;
            return;
        }
        float low, high;
        a.split (x, low, high);
        if (bands == 2)
        {
            out[0] = low; out[1] = high; out[2] = 0.0f;
            return;
        }
        float mid, top, cl, ch;
        b.split (high, mid, top);
        compensate (low, cl, ch);
        out[0] = cl + ch;
        out[1] = mid;
        out[2] = top;
    }

    static constexpr int kUpdate = 16;   // samples between coefficient updates

private:
    void glideTo (int i, double target, ga::dsp::Crossover& x)
    {
        if (target <= 0.0)
            return;
        double& f = current[(size_t) i];
        const double next = f < 0.0 ? target : std::exp (std::log (f) + (std::log (target) - std::log (f)) * glide);
        if (std::abs (next - f) > 1.0e-6 * next)
        {
            f = next;
            const auto state = x;   // keep the filter state, change only the coefficients
            x.set (f, sr);
            for (int k = 0; k < 2; ++k)
            {
                x.lp[k].z1 = state.lp[k].z1; x.lp[k].z2 = state.lp[k].z2;
                x.hp[k].z1 = state.hp[k].z1; x.hp[k].z2 = state.hp[k].z2;
            }
        }
    }

    static void copyCoefficients (ga::dsp::Biquad& to, const ga::dsp::Biquad& from)
    {
        to.b0 = from.b0; to.b1 = from.b1; to.b2 = from.b2; to.a1 = from.a1; to.a2 = from.a2;
    }

    void compensate (float x, float& low, float& high)
    {
        low  = comp.lp[1].process (comp.lp[0].process (x));
        high = comp.hp[1].process (comp.hp[0].process (x));
    }

    double sr = 48000.0, glide = 0.5;
    int bands = 1;
    ga::dsp::Crossover a, b, comp;
    double current[2] { -1.0, -1.0 };
};
}
