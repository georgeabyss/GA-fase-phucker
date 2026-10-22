#pragma once

#include "Params.h"
#include "BandSplitter.h"
#include "Disperse.h"

namespace fp::dsp
{
// The effect: the input split into 1, 2 or 3 bands; each band in L / R or M / S through its own
// sections; the bands summed into the shared output stage (dry/wet, Bass Mono, Phase Protect,
// loudness match). Everything runs in sub-blocks of kSubBlock samples.
class Engine
{
public:
    void prepare (double sampleRate, int channels, int oversampling);
    void setParams (const Params& p) { prm = p; output.setParams (p.output); }
    void process (float* const* io, int numSamples);
    void reset();

    int latency() const { return 0; }

    ga::dsp::OutputStage output;

private:
    void processSubBlock (float* const* io, int offset, int n);

    // a band's level ramps (click-free on / off and solo)
    struct Ramp
    {
        float current = 1.0f, target = 1.0f, step = 0.0f;
        void set (float t, int samples) { if (t != target) { target = t; step = (target - current) / (float) samples; } }
        float next()
        {
            if (current != target)
            {
                current += step;
                if ((step > 0.0f && current >= target) || (step < 0.0f && current <= target)) current = target;
            }
            return current;
        }
        void jump (float v) { current = target = v; step = 0.0f; }
    };

    Params prm;
    double sr = 48000.0;
    int numCh = 2;

    std::array<BandSplitter, 2> splitters;
    std::array<std::array<Disperse, 2>, kMaxBands> disperse;   // [band][channel]
    std::array<Ramp, kMaxBands> wet;      // 1 = processed, 0 = passed through (band off)
    std::array<Ramp, kMaxBands> level;    // 0 = muted by another band's solo
    int activeBands = 1;                  // the band count in use; a change fades out, switches, fades in
    Ramp topology;
};
}
