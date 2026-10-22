#pragma once

#include "Disperse.h"
#include "dsp/DspBlocks.h"

namespace fp::dsp
{
// One channel's feedback loop, around the all-pass chain only (never around Distortion):
//   z = x + fb,  y = chain (z),  fb = Amount * (Invert ? -1 : 1) * limit (tone (y, Time ago))
// Time glides; Tone is a tilt around 1 kHz, normalised so it never adds gain (with Amount at most
// 95 % the loop always decays); the limiter is a tanh. Mix is the section's dry/wet around the whole
// loop, only while Feedback is on.
class Feedback
{
public:
    void prepare (double sampleRate);
    void reset();

    // once per sub-block: Invert and on / off from the parameters, the knobs the Env Follower can move
    void update (const ChannelParams&, float amount, float timeMs, float tone, float mix);

    // the loop around `chain`, in place
    void process (float* x, int n, Disperse& chain);

private:
    double sr = 48000.0;
    ga::dsp::DelayLine line;
    float amount = 0.0f, amountTarget = 0.0f;
    float mix = 1.0f, mixTarget = 1.0f;
    float delay = 0.0f, delayTarget = 0.0f;   // samples
    float tilt = 0.0f, tiltTarget = 0.0f;
    float sign = 1.0f;
    float smoothing = 0.01f, tiltCoef = 0.1f, tiltLp = 0.0f;
    bool fresh = true;
};
}
