#include "Feedback.h"

namespace fp::dsp
{
void Feedback::prepare (double sampleRate)
{
    sr = sampleRate;
    line.init ((int) (0.051 * sr) + 4);
    smoothing = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));   // ~20 ms
    tiltCoef = (float) (1.0 - std::exp (-6.283185307179586 * 1000.0 / sr));
    reset();
}

void Feedback::reset()
{
    line.clear();
    tiltLp = 0.0f;
    fresh = true;
}

void Feedback::update (const ChannelParams& p, float amt, float timeMs, float tone, float m)
{
    amountTarget = p.fbOn ? std::clamp (amt, 0.0f, 0.95f) : 0.0f;
    mixTarget = p.fbOn ? m : 1.0f;   // off, the loop is just the chain: Mix has nothing to mix
    delayTarget = std::max (3.0f, timeMs * 0.001f * (float) sr) - 1.0f;   // the loop's own sample counts too
    tiltTarget = 2.0f * tone - 1.0f;
    sign = p.invert ? -1.0f : 1.0f;
    if (fresh)
    {
        amount = amountTarget; mix = mixTarget; delay = delayTarget; tilt = tiltTarget;
        fresh = false;
    }
}

void Feedback::process (float* x, int n, Disperse& chain)
{
    const float s = smoothing;
    for (int i = 0; i < n; ++i)
    {
        amount += (amountTarget - amount) * s;
        mix += (mixTarget - mix) * s;
        delay += (delayTarget - delay) * s;
        tilt += (tiltTarget - tilt) * s;

        // what comes back: Time ago, tilted (never louder), limited, scaled
        float back = line.read ((double) std::max (2.0f, delay));
        tiltLp += tiltCoef * (back - tiltLp);
        back = (tiltLp * (1.0f - tilt) + (back - tiltLp) * (1.0f + tilt)) / (1.0f + std::abs (tilt));
        const float fb = sign * amount * std::tanh (back);

        const float dry = x[i];
        const float y = chain.processSample (dry + fb);
        line.push (y);
        x[i] = dry + (y - dry) * mix;
    }
    chain.endBlock();
}
}
