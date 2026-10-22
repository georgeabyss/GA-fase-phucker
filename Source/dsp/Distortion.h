#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Params.h"

namespace fp::dsp
{
// One channel's distortion, oversampled by the factor the bottom strip picks (its own mono
// oversamplers, so it adds the same latency wherever it sits). At the oversampled rate:
//   shaper   Noise (noise ring-modulated by the signal), Saturate (tanh), Clip, Fold (sine), Rectify
//            (half- to full-wave with Drive), Crush (bits and sample rate); Bias shifts the curve
//   then     a DC blocker, Phat (extra saturation of 150 Hz - 2.5 kHz, added in parallel), Tone (a
//            tilt around 1 kHz, 50 % = neutral), Mix (dry/wet, with on / off)
class Distortion
{
public:
    // first call with the highest factor: creates every oversampler; later calls only pick one
    void prepare (double sampleRate, int factor, bool linearPhase, unsigned seed);
    void reset();
    int latency() const { return latencySamples; }

    // once per sub-block, before process(): the type and on / off from the parameters, the knobs
    // the Env Follower can move as they are now
    void update (const ChannelParams&, float drive, float bias, float phat, float tone, float mix);

    void process (float* x, int n);   // in place, n <= kSubBlock

private:
    void shape (float* x, int n);     // at the oversampled rate

    struct Smoothed
    {
        float current = 0.0f, target = 0.0f;
        bool fresh = true;
        void set (float t) { target = t; if (fresh) { current = t; fresh = false; } }
        float next (float step) { return current += (target - current) * step; }
    };

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 6> oversamplers;   // [linear phase 2x 4x 8x, IIR 2x 4x 8x]
    juce::dsp::Oversampling<float>* active = nullptr;
    int factor = 1, latencySamples = 0;
    double rate = 48000.0;   // the oversampled rate

    DistType type = DistType::noise;
    Smoothed drive, bias, phat, tone, mix;
    float smoothing = 0.01f;

    // state at the oversampled rate
    float dcX = 0.0f, dcY = 0.0f, dcCoef = 0.999f;
    float tiltLp = 0.0f, tiltCoef = 0.1f;
    ga::dsp::Biquad phatHp, phatLp;
    float held = 0.0f; int holdCount = 0;   // Crush
    juce::uint32 noise = 1;
};
}
