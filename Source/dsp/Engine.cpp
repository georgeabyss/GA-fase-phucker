#include "Engine.h"

namespace fp::dsp
{
namespace
{
    constexpr double kRampSeconds = 0.01;   // band on / off and solo
}

void Engine::prepare (double sampleRate, int channels, int)
{
    sr = sampleRate;
    numCh = std::clamp (channels, 1, 2);
    for (auto& s : splitters)
        s.prepare (sr);
    for (int b = 0; b < kMaxBands; ++b)
        for (int c = 0; c < 2; ++c)
            disperse[(size_t) b][(size_t) c].prepare (sr, 1000u + 17u * (unsigned) b + 7u * (unsigned) c);
    output.prepare (sr, numCh);
    reset();
}

void Engine::reset()
{
    for (auto& s : splitters)
        s.reset();
    for (auto& band : disperse)
        for (auto& d : band)
            d.reset();
    output.reset();
    activeBands = prm.bands;
    topology.jump (1.0f);
    const bool anySolo = std::any_of (prm.band.begin(), prm.band.end(), [] (const BandParams& b) { return b.solo; });
    for (int b = 0; b < kMaxBands; ++b)
    {
        wet[(size_t) b].jump (prm.band[(size_t) b].on ? 1.0f : 0.0f);
        level[(size_t) b].jump (! anySolo || prm.band[(size_t) b].solo ? 1.0f : 0.0f);
    }
}

void Engine::process (float* const* io, int n)
{
    output.beginBlock();
    for (int done = 0; done < n; done += kSubBlock)
        processSubBlock (io, done, std::min (kSubBlock, n - done));
    output.endBlock();
}

void Engine::processSubBlock (float* const* io, int offset, int n)
{
    // band count: fade out, switch, fade in
    const int rampSamples = std::max (1, (int) (kRampSeconds * sr));
    if (prm.bands != activeBands && topology.target != 0.0f)
        topology.set (0.0f, rampSamples);
    if (topology.current == 0.0f && prm.bands != activeBands)
    {
        activeBands = prm.bands;
        for (auto& s : splitters)
            s.reset();
        topology.set (1.0f, rampSamples);
    }

    for (int c = 0; c < numCh; ++c)
        splitters[(size_t) c].update (activeBands, prm.split, prm.split1, prm.split2);

    const bool anySolo = std::any_of (prm.band.begin(), prm.band.begin() + activeBands, [] (const BandParams& b) { return b.solo; });
    for (int b = 0; b < kMaxBands; ++b)
    {
        const auto& bp = prm.band[(size_t) b];
        wet[(size_t) b].set (bp.on ? 1.0f : 0.0f, rampSamples);
        level[(size_t) b].set (! anySolo || bp.solo ? 1.0f : 0.0f, rampSamples);
    }

    // split every channel into its bands
    float band[kMaxBands][2][kSubBlock] {};
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i)
        {
            float out[kMaxBands];
            splitters[(size_t) c].process (io[c][offset + i], out);
            for (int b = 0; b < kMaxBands; ++b)
                band[b][c][i] = out[b];
        }

    // each band in L / R or M / S through its sections, then summed
    float sum[2][kSubBlock] {};
    for (int b = 0; b < activeBands; ++b)
    {
        const auto& bp = prm.band[(size_t) b];
        float* l = band[b][0];
        float* r = band[b][numCh > 1 ? 1 : 0];
        float dry[2][kSubBlock];
        std::copy (l, l + n, dry[0]);
        std::copy (r, r + n, dry[1]);

        const bool ms = bp.midSide && numCh > 1;
        if (ms)
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]);
                l[i] = m; r[i] = s;
            }

        // the sections, per channel
        for (int c = 0; c < numCh; ++c)
        {
            const auto& ch = bp.ch[(size_t) c];
            float* x = c == 0 ? l : r;
            auto& d = disperse[(size_t) b][(size_t) c];
            d.update (ch, ch.pinch, ch.spread, ch.dispMix, (double) ch.freq);
            d.process (x, n);
        }

        if (ms)
            for (int i = 0; i < n; ++i)
            {
                const float m = l[i], s = r[i];
                l[i] = m + s; r[i] = m - s;
            }

        for (int i = 0; i < n; ++i)
        {
            const float w = wet[(size_t) b].next(), g = level[(size_t) b].next();
            sum[0][i] += g * (dry[0][i] + (l[i] - dry[0][i]) * w);
            sum[1][i] += g * (dry[1][i] + (r[i] - dry[1][i]) * w);
        }
    }
    for (int b = activeBands; b < kMaxBands; ++b)   // keep the unused bands' ramps moving
        for (int i = 0; i < n; ++i) { wet[(size_t) b].next(); level[(size_t) b].next(); }

    // the shared output stage, against the dry input
    for (int i = 0; i < n; ++i)
    {
        const float t = topology.next();
        const float l = io[0][offset + i];
        const float r = numCh > 1 ? io[1][offset + i] : l;
        float outL = sum[0][i] * t, outR = sum[1][i] * t;
        output.process (l, r, outL, outR);
        io[0][offset + i] = outL;
        if (numCh > 1) io[1][offset + i] = outR;
    }
}
}
