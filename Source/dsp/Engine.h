#pragma once

#include "Params.h"
#include "BandSplitter.h"
#include "Disperse.h"
#include "Distortion.h"
#include "Feedback.h"
#include "EnvFollower.h"
#include <juce_core/juce_core.h>

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

    // the ranges of the knobs the Env Follower drives (the parameters'), in ModTarget order; set once
    using ModRanges = std::array<juce::NormalisableRange<float>, kNumModTargets>;
    void setModRanges (const ModRanges* r) { modRanges = r; }

    // after process(): a channel's envelope (0..1) and a modulated knob's position (0..1 of its travel)
    float envelope (int band, int channel) const { return envelopes[(size_t) band][(size_t) channel].value(); }
    float modulated (int band, int channel, int target) const { return modPosition[(size_t) band][(size_t) channel][(size_t) target]; }
    void process (float* const* io, int numSamples);
    void reset();

    int latency() const { return distortion[0][0].latency(); }   // Distortion's oversampling, the same everywhere

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
    std::array<std::array<Distortion, 2>, kMaxBands> distortion;
    std::array<std::array<Feedback, 2>, kMaxBands> feedback;
    std::array<std::array<EnvFollower, 2>, kMaxBands> envelopes;

    // the knobs as the Env Follower moves them, this sub-block
    ChannelParams modulate (int band, int channel, const ChannelParams&);
    const ModRanges* modRanges = nullptr;
    std::array<std::array<std::array<float, kNumModTargets>, 2>, kMaxBands> modPosition {};

    // a fixed delay (Distortion's latency) for the dry paths: the output stage's, and a band that is off
    struct Delay
    {
        std::array<float, 4096> buf {};
        int w = 0, d = 0;
        float process (float x) { buf[(size_t) w] = x; const float y = buf[(size_t) ((w - d) & 4095)]; w = (w + 1) & 4095; return y; }
        void clear() { buf.fill (0.0f); w = 0; }
    };
    std::array<Delay, 2> inputDelay;
    std::array<std::array<Delay, 2>, kMaxBands> bandDelay;

    // where each channel's distortion sits. A switch restarts the channel's processing: fade its
    // output out, empty its sections, fade its input back in (the chain fills up from silence)
    std::array<std::array<bool, 2>, kMaxBands> post {};
    std::array<std::array<Ramp, 2>, kMaxBands> orderFade, inputFade;
    std::array<Ramp, kMaxBands> wet;      // 1 = processed, 0 = passed through (band off)
    std::array<Ramp, kMaxBands> level;    // 0 = muted by another band's solo
    int activeBands = 1;                  // the band count in use; a change fades out, switches, fades in
    Ramp topology;
};
}
