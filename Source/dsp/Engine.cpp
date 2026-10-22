#include "Engine.h"

namespace fp::dsp
{
namespace
{
    constexpr double kRampSeconds = 0.01;   // band on / off and solo
}

void Engine::prepare (double sampleRate, int channels, int oversampling)
{
    sr = sampleRate;
    numCh = std::clamp (channels, 1, 2);
    for (auto& s : splitters)
        s.prepare (sr);
    for (int b = 0; b < kMaxBands; ++b)
        for (int c = 0; c < 2; ++c)
        {
            disperse[(size_t) b][(size_t) c].prepare (sr, 1000u + 17u * (unsigned) b + 7u * (unsigned) c);
            distortion[(size_t) b][(size_t) c].prepare (sr, oversampling, prm.osLinearPhase, 2000u + 13u * (unsigned) b + 5u * (unsigned) c);
        }
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
    for (auto& band : distortion)
        for (auto& d : band)
            d.reset();
    for (auto& d : inputDelay) { d.clear(); d.d = latency(); }
    for (auto& band : bandDelay)
        for (auto& d : band) { d.clear(); d.d = latency(); }
    for (int b = 0; b < kMaxBands; ++b)
        for (int c = 0; c < 2; ++c)
        {
            post[(size_t) b][(size_t) c] = prm.band[(size_t) b].ch[(size_t) c].post;
            orderFade[(size_t) b][(size_t) c].jump (1.0f);
            inputFade[(size_t) b][(size_t) c].jump (1.0f);
        }
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
        float dry[2][kSubBlock];   // the band unprocessed (band off), delayed like the processed one
        for (int i = 0; i < n; ++i)
        {
            dry[0][i] = bandDelay[(size_t) b][0].process (l[i]);
            dry[1][i] = numCh > 1 ? bandDelay[(size_t) b][1].process (r[i]) : dry[0][i];
        }

        const bool ms = bp.midSide && numCh > 1;
        if (ms)
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]);
                l[i] = m; r[i] = s;
            }

        // the sections, per channel: Distortion (Pre) -> Disperse -> Distortion (Post)
        for (int c = 0; c < numCh; ++c)
        {
            const auto& ch = bp.ch[(size_t) c];
            float* x = c == 0 ? l : r;
            auto& disp = disperse[(size_t) b][(size_t) c];
            auto& dist = distortion[(size_t) b][(size_t) c];
            auto& order = orderFade[(size_t) b][(size_t) c];
            auto& in = inputFade[(size_t) b][(size_t) c];
            bool& isPost = post[(size_t) b][(size_t) c];

            // Pre / Post switched: fade the channel out, empty its sections, move the distortion,
            // and fade the input in (the chain fills up again from silence: no step to smear out)
            const int fade = std::max (1, (int) (0.005 * sr));
            if (ch.post != isPost && order.target != 0.0f)
                order.set (0.0f, fade);
            if (ch.post != isPost && order.current == 0.0f)
            {
                isPost = ch.post;
                disp.reset();
                dist.reset();
                order.jump (1.0f);
                in.jump (0.0f);
                in.set (1.0f, 2 * fade);
            }
            for (int i = 0; i < n; ++i)
                x[i] *= in.next();

            disp.update (ch, ch.pinch, ch.spread, ch.dispMix, (double) ch.freq);
            dist.update (ch, ch.drive, ch.bias, ch.phat, ch.distTone, ch.distMix);
            if (! isPost) dist.process (x, n);
            disp.process (x, n);
            if (isPost) dist.process (x, n);
            for (int i = 0; i < n; ++i)
                x[i] *= order.next();
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
        const float l = inputDelay[0].process (io[0][offset + i]);   // the dry input, delayed like the wet
        const float r = numCh > 1 ? inputDelay[1].process (io[1][offset + i]) : l;
        float outL = sum[0][i] * t, outR = sum[1][i] * t;
        output.process (l, r, outL, outR);
        io[0][offset + i] = outL;
        if (numCh > 1) io[1][offset + i] = outR;
    }
}
}
