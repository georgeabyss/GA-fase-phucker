#pragma once

#include <array>
#include "dsp/OutputStage.h"

// What the DSP engine needs, gathered from the parameters once per block (see the processor). A
// linked section's 2nd channel already holds the 1st channel's values, so the engine never looks
// at links.
namespace fp::dsp
{
constexpr int kMaxBands = 3;
constexpr int kMaxStages = 1000;
constexpr int kSubBlock = 16;   // modulation and coefficient updates happen this often (samples)

enum class DistType { noise, saturate, clip, fold, rectify, crush };

struct ChannelParams
{
    // Disperse
    int   stages = 64;
    bool  unhinged = false, jitter = false, dispOn = true;
    float freq = 400.0f;          // Hz
    float pinch = 0.5f;           // 0..1
    float spread = 0.3f;          // 0..1
    float dispMix = 1.0f;         // 0..1

    // Env Follower
    float envGainDb = 0.0f;
    float envAmount = 0.0f;       // -1..1
    float attackMs = 5.0f, holdMs = 20.0f, releaseMs = 150.0f;

    // Distortion
    DistType type = DistType::noise;
    bool  post = false, distOn = true;
    float drive = 0.3f;           // 0..1
    float bias = 0.0f;            // -1..1
    float phat = 0.0f;            // 0..1
    float distTone = 0.5f;        // 0..1, 0.5 = neutral
    float distMix = 1.0f;         // 0..1

    // Feedback
    bool  invert = false, fbOn = true;
    float fbAmount = 0.0f;        // 0..0.95
    float fbTimeMs = 5.0f;
    float fbTone = 0.5f;          // 0..1, 0.5 = neutral
    float fbMix = 1.0f;           // 0..1
};

// The knobs the Env Follower can drive, in the order of fp::kModTargets
enum ModTarget { modFreq, modPinch, modSpread, modDispMix, modDrive, modBias, modPhat, modDistTone, modDistMix,
                 modFbAmount, modFbTime, modFbTone, modFbMix, kNumModTargets };

struct BandParams
{
    bool on = true, solo = false, midSide = true;
    std::array<ChannelParams, 2> ch;

    // modulation: depth (-1..1 of the knob's travel) of each assigned target
    std::array<bool, kNumModTargets> modOn {};
    std::array<float, kNumModTargets> modDepth {};
};

struct Params
{
    int bands = 1;                                   // 1, 2, 3
    bool osLinearPhase = true;                       // the anti-alias filter of Distortion's oversampling
    float split = 800.0f, split1 = 180.0f, split2 = 2400.0f;
    std::array<BandParams, kMaxBands> band;
    ga::dsp::OutputStage::Params output;
};
}
