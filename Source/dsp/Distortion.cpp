#include "Distortion.h"

namespace fp::dsp
{
namespace
{
    constexpr double kTwoPi = 6.283185307179586;
}

void Distortion::prepare (double sampleRate, int f, bool linearPhase, unsigned seed)
{
    using OS = juce::dsp::Oversampling<float>;
    if (oversamplers[0] == nullptr)   // the first call: every factor and filter, so later calls never allocate
        for (size_t i = 0; i < oversamplers.size(); ++i)
        {
            oversamplers[i] = std::make_unique<OS> (1, i % 3 + 1, i < 3 ? OS::filterHalfBandFIREquiripple : OS::filterHalfBandPolyphaseIIR, true, true);
            oversamplers[i]->initProcessing ((size_t) kSubBlock);
        }

    factor = f >= 8 ? 8 : f >= 4 ? 4 : f >= 2 ? 2 : 1;
    const int stages = factor == 8 ? 3 : factor == 4 ? 2 : factor == 2 ? 1 : 0;
    active = stages > 0 ? oversamplers[(size_t) ((linearPhase ? 0 : 3) + stages - 1)].get() : nullptr;
    latencySamples = active != nullptr ? juce::roundToInt (active->getLatencyInSamples()) : 0;

    rate = sampleRate * factor;
    dcCoef = (float) std::exp (-kTwoPi * 10.0 / rate);
    tiltCoef = (float) (1.0 - std::exp (-kTwoPi * 1000.0 / rate));
    smoothing = (float) (1.0 - std::exp (-1.0 / (0.01 * rate)));
    phatHp.highpass (150.0, rate);
    phatLp.lowpass (2500.0, rate);
    noise = seed != 0 ? seed : 1;
    reset();
}

void Distortion::reset()
{
    if (active != nullptr)
        active->reset();
    dcX = dcY = tiltLp = held = 0.0f;
    delayLine.fill (0.0f);
    delayPos = 0;
    bypass = mix.target <= 0.0f ? 1.0f : 0.0f;
    holdCount = 0;
    phatHp.reset();
    phatLp.reset();
    for (auto* s : { &drive, &bias, &phat, &tone, &mix })
        s->fresh = true;
}

void Distortion::update (const ChannelParams& p, float d, float b, float ph, float t, float m)
{
    type = p.type;
    drive.set (d);
    bias.set (b);
    phat.set (ph);
    tone.set (t);
    mix.set (p.distOn ? m : 0.0f);
}

void Distortion::process (float* x, int n)
{
    if (active == nullptr)
    {
        shape (x, n);   // fully off, Mix 0 leaves it exactly as it was
        return;
    }

    // the input, delayed by the latency: the output while fully off
    float delayed[kSubBlock];
    for (int i = 0; i < n; ++i)
    {
        delayLine[(size_t) delayPos] = x[i];
        delayed[i] = delayLine[(size_t) ((delayPos - latencySamples) & 4095)];
        delayPos = (delayPos + 1) & 4095;
    }

    float* channels[1] = { x };
    juce::dsp::AudioBlock<float> block (channels, 1, (size_t) n);
    auto up = active->processSamplesUp (juce::dsp::AudioBlock<const float> (block));
    shape (up.getChannelPointer (0), (int) up.getNumSamples());
    active->processSamplesDown (block);

    // fully off (Mix has arrived at 0): the delayed input; switching on fades over to the oversampled path
    const bool off = mix.target <= 0.0f && mix.current < 1.0e-6f;
    const float step = 1.0f / (0.005f * (float) (rate / factor));
    for (int i = 0; i < n; ++i)
    {
        bypass = off ? std::min (1.0f, bypass + step) : std::max (0.0f, bypass - step);
        x[i] += (delayed[i] - x[i]) * bypass;
    }
}

void Distortion::shape (float* y, int n)
{
    const float s = smoothing;
    for (int i = 0; i < n; ++i)
    {
        const float d = drive.next (s), b = bias.next (s), p = phat.next (s), t = tone.next (s), w = mix.next (s);
        const float dry = y[i];
        const float g = std::pow (10.0f, 1.5f * d);        // 0 to +30 dB into the curve
        const float makeup = 1.0f / std::pow (g, 0.4f);
        const float bo = 0.5f * b;                         // Bias: the curve's offset
        float v = 0.0f;
        switch (type)
        {
            case DistType::saturate: v = (std::tanh (g * dry + bo) - std::tanh (bo)) * makeup; break;
            case DistType::clip:     v = (std::clamp (g * dry + bo, -1.0f, 1.0f) - std::clamp (bo, -1.0f, 1.0f)) * makeup; break;
            case DistType::fold:     v = (std::sin (1.5707964f * (g * dry + bo)) - std::sin (1.5707964f * bo)) * makeup; break;
            case DistType::rectify:
            {
                const float u = dry + bo, half = std::max (u, 0.0f);
                v = half + d * (std::abs (u) - half);      // half-wave, to full-wave with Drive
                break;
            }
            case DistType::crush:
            {
                if (--holdCount <= 0)   // sample rate: hold for 1 to 16 samples (at the base rate)
                {
                    const float levels = std::exp2 (15.0f - 13.0f * d);   // 16 bits down to 3
                    held = std::round ((dry + bo) * levels) / levels;
                    holdCount = factor * (1 + (int) (d * 15.0f));
                }
                v = held;
                break;
            }
            case DistType::noise:
            {
                noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
                const float r = (float) (noise >> 8) * (2.0f / 16777216.0f) - 1.0f;
                v = dry + 2.0f * d * r * (dry + bo);       // noise ring-modulated by the signal
                break;
            }
        }

        // DC blocker (Bias and Rectify make DC)
        const float hp = v - dcX + dcCoef * dcY;
        dcX = v; dcY = hp; v = hp;

        // Phat: a saturated copy of the low mids and mids, added in parallel
        const float band = phatLp.process (phatHp.process (v));   // always runs: no stale state when Phat comes up
        v += p * 0.5f * std::tanh (band * (1.0f + 9.0f * p));

        // Tone: a tilt around 1 kHz, neutral at 50 %
        tiltLp += tiltCoef * (v - tiltLp);
        const float tt = 2.0f * t - 1.0f;
        v = tiltLp * (1.0f - tt) + (v - tiltLp) * (1.0f + tt);

        y[i] = dry + (v - dry) * w;
    }
}
}
