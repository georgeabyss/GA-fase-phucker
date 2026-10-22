#include "Disperse.h"
#include <algorithm>
#include <cmath>

namespace fp::dsp
{
namespace
{
    constexpr double kPi = 3.141592653589793;
    constexpr double kFadeSeconds = 0.03;        // Stages and Mode changes
    constexpr double kMixSeconds = 0.02;
    constexpr float kQuantum = 1.0f / 1024.0f;   // Unhinged's coefficient grid
}

//==============================================================================
void Disperse::Behaved::clear()
{
    std::fill (s1.begin(), s1.end(), 0.0);
    std::fill (s2.begin(), s2.end(), 0.0);
    std::fill (logF.begin(), logF.end(), -1.0);
    q = -1.0;
}

float Disperse::Behaved::run (double x, int count)
{
    // TPT state-variable all-pass, x - 2k v1, written so that only cx * v waits on the previous stage
    double v = x;
    for (size_t i = 0; i < (size_t) count; ++i)
    {
        const double ap = cx[i] * v + e1[i] * s1[i] + e2[i] * s2[i];
        const double v3 = v - s2[i];
        const double v1 = a1[i] * s1[i] + a2[i] * v3;
        const double v2 = s2[i] + a2[i] * s1[i] + a3[i] * v3;
        s1[i] = 2.0 * v1 - s1[i];
        s2[i] = 2.0 * v2 - s2[i];
        v = ap;
    }
    return (float) v;
}

float Disperse::Unhinged::run (float x, int from, int to)
{
    // direct form II transposed: (c0 + c1 z^-1 + z^-2) / (1 + c1 z^-1 + c0 z^-2)
    float v = x;
    for (size_t i = (size_t) from; i < (size_t) to; ++i)
    {
        const float y = c0[i] * v + z1[i];
        z1[i] = c1[i] * (v - y) + z2[i];
        z2[i] = v - c0[i] * y;
        v = y;
    }
    return v;
}

//==============================================================================
void Disperse::prepare (double sampleRate, unsigned seed)
{
    sr = sampleRate;
    glide = 1.0 - std::exp (-(double) kSubBlock / (0.02 * sr));
    for (auto* x : { &b, &bOld })
        for (auto* v : { &x->s1, &x->s2, &x->cx, &x->e1, &x->e2, &x->a1, &x->a2, &x->a3, &x->logF })
            v->assign ((size_t) kMaxStages, 0.0);
    for (auto* x : { &u, &uOld })
        for (auto* v : { &x->z1, &x->z2, &x->c0, &x->c1 })
            v->assign ((size_t) kMaxStages, 0.0f);
    for (auto* v : { &target, &jitterPhase, &jitterRate })
        v->assign ((size_t) kMaxStages, 0.0);

    // every stage drifts at its own rate (0.3 to 3 Hz) and phase; the same on every run
    unsigned s = seed != 0 ? seed : 1;
    auto next = [&s] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double) (s >> 8) / 16777216.0; };
    for (size_t i = 0; i < (size_t) kMaxStages; ++i)
    {
        jitterRate[i] = 0.3 * std::pow (10.0, next());
        jitterPhase[i] = 2.0 * kPi * next();
    }
    reset();
}

void Disperse::reset()
{
    b.clear(); bOld.clear();
    u.clear(); uOld.clear();
    stages = oldStages = 0;
    stageFade = modeFade = 1.0f;
    mix = mixTarget;
    mixStep = 0.0f;
    running = false;
    fresh = true;
    jitterTime = 0.0;
}

void Disperse::targets (int count, double freq, float spread)
{
    const double hi = std::log2 (0.45 * sr), lo = std::log2 (10.0), centre = std::log2 (freq);
    for (int i = 0; i < count; ++i)
    {
        const double pos = count > 1 ? 2.0 * i / (count - 1) - 1.0 : 0.0;
        double oct = 3.0 * spread * pos;
        if (jitterOn)
            oct += 0.25 * std::sin (2.0 * kPi * jitterRate[(size_t) i] * jitterTime + jitterPhase[(size_t) i]);
        target[(size_t) i] = std::clamp (centre + oct, lo, hi);
    }
}

void Disperse::setBehaved (int count, double q)
{
    const bool newQ = q != b.q;
    b.q = q;
    const double k = 1.0 / q;
    for (size_t i = 0; i < (size_t) count; ++i)
    {
        double& f = b.logF[i];
        const double next = f < 0.0 ? target[i] : f + (target[i] - f) * glide;
        if (! newQ && std::abs (next - f) < 1.0e-7)
            continue;
        f = next;
        const double g = std::tan (kPi * std::exp2 (f) / sr);
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1;
        b.a1[i] = a1; b.a2[i] = a2; b.a3[i] = g * a2;
        b.cx[i] = 1.0 - 2.0 * k * a2;
        b.e1[i] = -2.0 * k * a1;
        b.e2[i] = 2.0 * k * a2;
    }
}

void Disperse::setUnhinged (int count, double q)
{
    auto quantise = [] (float c) { return std::round (c / kQuantum) * kQuantum; };
    for (size_t i = 0; i < (size_t) count; ++i)
    {
        const double w = 2.0 * kPi * std::exp2 (target[i]) / sr;
        const double alpha = std::sin (w) / (2.0 * q);
        // quantised, and kept inside the stable triangle (|c1| < 1 + c0, c0 < 1)
        const float c0 = std::min (quantise ((float) ((1.0 - alpha) / (1.0 + alpha))), 1.0f - kQuantum);
        const float limit = 1.0f + c0 - kQuantum;
        u.c0[i] = c0;
        u.c1[i] = std::clamp (quantise ((float) (-2.0 * std::cos (w) / (1.0 + alpha))), -limit, limit);
    }
}

void Disperse::update (const ChannelParams& p, float pinch, float spread, float wetMix, double freq)
{
    jitterOn = p.jitter;
    if (jitterOn)
        jitterTime += (double) kSubBlock / sr;

    // Mix (and the section's on / off) glides; at 0 the chain stops
    const float newTarget = p.dispOn ? wetMix : 0.0f;
    if (fresh)
    {
        mix = mixTarget = newTarget;
        mixStep = 0.0f;
        fresh = false;
    }
    else if (newTarget != mixTarget)
    {
        mixTarget = newTarget;
        mixStep = (mixTarget - mix) / (float) (kMixSeconds * sr);
    }
    if (! running)
    {
        if (mixTarget <= 0.0f)
            return;
        running = true;   // a fresh start: an empty chain, coefficients straight to their targets
        b.clear();
        u.clear();
        stages = oldStages = p.stages;
        unhinged = wasUnhinged = p.unhinged;
        stageFade = modeFade = 1.0f;
    }

    // Stages or Mode changed: the old chain goes on frozen, a new one starts empty, and they crossfade
    // (one change at a time: another one waits for the next update after the fade)
    if (stageFade >= 1.0f && modeFade >= 1.0f)
    {
        const float step = (float) (1.0 / (kFadeSeconds * sr));
        if (p.stages != stages)
        {
            if (unhinged) { uOld = u; u.clear(); }
            else          { bOld = b; b.clear(); }
            oldStages = stages;
            stages = p.stages;
            stageFade = 0.0f;
            fadeStep = step;
        }
        else if (p.unhinged != unhinged)
        {
            wasUnhinged = unhinged;
            unhinged = p.unhinged;
            if (unhinged) u.clear(); else b.clear();
            modeFade = 0.0f;
            fadeStep = step;
        }
    }

    // only the current chain of each running mode follows the knobs; a frozen old chain keeps its own
    const double q = 0.5 + 4.0 * (double) pinch;
    targets (stages, freq, spread);
    if (! unhinged || (modeFade < 1.0f && ! wasUnhinged))
        setBehaved (stages, q);
    if (unhinged || (modeFade < 1.0f && wasUnhinged))
        setUnhinged (stages, q);
}

float Disperse::runChain (bool useUnhinged, float x, int count)
{
    return useUnhinged ? u.run (x, 0, count) : b.run ((double) x, count);
}

float Disperse::processSample (float dry)
{
    if (! running)
        return dry;

    float wetOut = runChain (unhinged, dry, stages);
    if (stageFade < 1.0f)
    {
        const float before = unhinged ? uOld.run (dry, 0, oldStages) : bOld.run ((double) dry, oldStages);
        wetOut = before + (wetOut - before) * stageFade;
    }
    else if (modeFade < 1.0f)
    {
        const float before = runChain (wasUnhinged, dry, stages);
        wetOut = before + (wetOut - before) * modeFade;
    }

    const float out = dry + (wetOut - dry) * mix;

    if (mix != mixTarget)
    {
        mix += mixStep;
        if ((mixStep > 0.0f && mix > mixTarget) || (mixStep < 0.0f && mix < mixTarget)) mix = mixTarget;
    }
    if (stageFade < 1.0f && (stageFade += fadeStep) >= 1.0f) { stageFade = 1.0f; oldStages = stages; }
    if (modeFade < 1.0f && (modeFade += fadeStep) >= 1.0f)   { modeFade = 1.0f; wasUnhinged = unhinged; }
    return out;
}

void Disperse::endBlock()
{
    if (running && mix <= 0.0f && mixTarget <= 0.0f)
        running = false;
}

void Disperse::process (float* x, int n)
{
    for (int i = 0; i < n; ++i)
        x[i] = processSample (x[i]);
    endBlock();
}
}
