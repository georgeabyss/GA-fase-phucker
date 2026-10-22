#pragma once

#include <algorithm>
#include <cmath>
#include "Params.h"

namespace fp::dsp
{
// One channel's envelope follower: the band's own signal (before any section), times Gain, followed
// at its peaks with Attack, kept up for Hold, falling with Release; 0..1.
class EnvFollower
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { env = 0.0f; holdLeft = 0; }

    void update (const ChannelParams& p)
    {
        gain = std::pow (10.0f, p.envGainDb / 20.0f);
        attack = coef (p.attackMs);
        release = coef (p.releaseMs);
        holdSamples = (int) (p.holdMs * 0.001f * (float) sr);
    }

    void process (const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float level = std::min (1.0f, std::abs (x[i]) * gain);
            if (level >= env)
            {
                env += (level - env) * attack;
                holdLeft = holdSamples;
            }
            else if (holdLeft > 0)
                --holdLeft;
            else
                env += (level - env) * release;
        }
    }

    float value() const { return env; }

private:
    float coef (float ms) const { return 1.0f - std::exp (-1.0f / (std::max (0.01f, ms) * 0.001f * (float) sr)); }

    double sr = 48000.0;
    float env = 0.0f, gain = 1.0f, attack = 0.1f, release = 0.01f;
    int holdSamples = 0, holdLeft = 0;
};
}
