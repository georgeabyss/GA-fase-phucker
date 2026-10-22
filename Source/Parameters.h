#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamIDs.h"

namespace fp
{
// What the automation names depend on: the band count (which band is Low, Mid or High) and each
// band's stereo mode (Mid / Side or Left / Right). Until the processor connects it, the names are
// those of the defaults (Single, M / S).
struct ParameterNames
{
    std::atomic<float>* bands = nullptr;
    std::array<std::atomic<float>*, kMaxBands> stereo {};

    juce::String band (int band) const;                // "Mid Band"
    juce::String channel (int band, int channel) const;   // "Side"
};

// Fase Phucker's own parameters, in the order hosts list them: global, then per band (band, 1st
// channel, 2nd channel, modulation). The names follow the band count and stereo modes.
void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>&, const ParameterNames&);

// every ID addParameters() creates, in that order
const std::vector<juce::String>& parameterIds();

// a section's per-channel parameter names (pid::ch), e.g. for copying one channel onto the other
const std::vector<const char*>& sectionParameters (Section);
const char* linkName (Section);   // pid::band::dispLink etc.

// link choices
enum Link { linked = 0, firstChannel = 1, secondChannel = 2 };
}
