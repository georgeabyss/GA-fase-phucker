#pragma once

#include "Disperse.h"
#include "dsp/DspBlocks.h"

namespace fp::dsp
{
// One channel's feedback loop, around the all-pass chain only (never around Distortion):
//   z = x + fb,  y = chain (z),  fb = 1.25 Amount * (Invert ? -1 : 1) * tanh (2 tone (y, Time ago)) / 2
// The loop gain is 1.25 times Amount: from about 80 % up it rings on by itself (self-oscillation);
// the tanh keeps that at about the signal's level (what comes back stays below ±0.6), a DC blocker keeps it audible (never latched onto DC). Time glides;
// Tone is a tilt around 1 kHz, normalised so it never adds
// gain (the onset stays at 80 % whatever the Tone). Mix is the section's dry/wet around the whole
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
    float dcCoef = 0.999f, dcX = 0.0f, dcY = 0.0f;
    bool fresh = true;
};
}
