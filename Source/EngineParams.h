#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Params.h"

namespace fp
{
// Reads every parameter the DSP needs into fp::dsp::Params (audio thread, no allocation). A linked
// section's 2nd channel gets the 1st channel's values: while linked, only the 1st counts.
class EngineParams
{
public:
    explicit EngineParams (juce::AudioProcessorValueTreeState&);

    void read (dsp::Params&) const;

    // the normalisable ranges of the knobs the Env Follower drives (dsp::ModTarget order)
    const juce::NormalisableRange<float>& modRange (int target) const { return modRanges[(size_t) target]; }

private:
    using Value = std::atomic<float>*;
    struct Channel
    {
        Value stages, mode, jitter, freq, pinch, spread, dispMix, dispOn;
        Value envGain, envAmt, attack, hold, release;
        Value type, order, drive, bias, phat, distTone, distMix, distOn;
        Value invert, fbAmt, fbTime, fbTone, fbMix, fbOn;
    };
    struct Band
    {
        Value on, solo, stereo;
        std::array<Value, 4> link;
        std::array<Channel, 2> ch;
        std::array<Value, dsp::kNumModTargets> modDepth, modOn;
    };

    Value bands, split, split1, split2, dryWet, bassMono, phaseProtect, osFilter;
    std::array<Band, dsp::kMaxBands> band;
    std::array<juce::NormalisableRange<float>, dsp::kNumModTargets> modRanges;
};
}
