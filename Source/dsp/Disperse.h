#pragma once

#include <vector>
#include "Params.h"

namespace fp::dsp
{
// One channel's all-pass chain: up to kMaxStages second-order all-passes, spread around Freq.
//   Behaved   TPT state-variable all-passes in double, coefficients glide (~20 ms)
//   Unhinged  direct-form biquads in float, coefficients jump and are quantised (1/1024)
// Stage i sits at Freq * 2^(3 Spread (2 i / (N - 1) - 1)) octaves (±3 octaves at 100 %), plus, with
// Jitter, a slow random drift of up to ±1/4 octave. Q = 0.5 + 4 Pinch. A change of Stages or Mode
// crossfades (30 ms) from the old chain, frozen, to a new one. Mix is the section's dry/wet; off, the
// chain does not run at all.
class Disperse
{
public:
    void prepare (double sampleRate, unsigned seed);
    void reset();

    // once per sub-block, before process(): Stages, Mode, Jitter and on / off from the parameters,
    // the knobs the Env Follower can move (Freq, Pinch, Spread, Mix) as they are now
    void update (const ChannelParams&, float pinch, float spread, float mix, double freq);

    void process (float* x, int n);   // in place

private:
    struct Behaved
    {
        std::vector<double> s1, s2, cx, e1, e2, a1, a2, a3;   // state and coefficients per stage
        std::vector<double> logF;                             // the frequency each stage glides on (log2 Hz)
        double q = -1.0;                                      // the Q the coefficients were made for
        float run (double x, int count);
        void clear();   // empty, and the next coefficients jump to their targets
    };
    struct Unhinged
    {
        std::vector<float> z1, z2, c0, c1;   // coefficients jump: no smoothing
        float run (float x, int from, int to);
        void clear() { std::fill (z1.begin(), z1.end(), 0.0f); std::fill (z2.begin(), z2.end(), 0.0f); }
    };

    void targets (int stages, double freq, float spread);
    void setBehaved (int count, double q);
    void setUnhinged (int count, double q);
    float runChain (bool useUnhinged, float x, int count);   // the current chain of that mode

    double sr = 48000.0, glide = 0.5;
    Behaved b, bOld;                     // bOld, uOld: the frozen chain a Stages change fades out
    Unhinged u, uOld;
    std::vector<double> target;          // log2 Hz per stage, this sub-block
    std::vector<double> jitterPhase, jitterRate;

    // what runs now and what is fading out
    int stages = 0, oldStages = 0;       // oldStages != stages while the stage count crossfades
    bool unhinged = false, wasUnhinged = false;
    float stageFade = 1.0f, modeFade = 1.0f, fadeStep = 0.0f;
    float mix = 0.0f, mixTarget = 0.0f, mixStep = 0.0f;
    bool running = false;                // false while Mix is at 0: nothing to compute
    bool jitterOn = false;
    bool fresh = true;                   // after reset: the first update starts at its Mix, no ramp
    double jitterTime = 0.0;
};
}
