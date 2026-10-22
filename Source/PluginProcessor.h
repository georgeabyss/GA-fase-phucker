#pragma once

#include "plugin/ProcessorBase.h"
#include "dsp/OutputStage.h"
#include "Presets.h"

// Fase Phucker on the GA processor base (oversampling, bypass, safety mute, scope, state).
// Build step 1: no effect yet; the audio only runs through the shared output stage.
class FasePhuckerAudioProcessor : public ga::ProcessorBase
{
public:
    FasePhuckerAudioProcessor();

    juce::AudioProcessorEditor* createEditor() override;

    const juce::String getName() const override { return "Fase Phucker"; }
    double getTailLengthSeconds() const override { return 0.0; }

    static ga::ProcessorConfig processorConfig();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ga::PresetManager presets { apvts, presetConfig (apvts) };
    ga::dsp::OutputStage output;   // dry/wet, Bass Mono, Phase Protect, loudness match

private:
    void prepareWet (double sampleRate, int channels, int) override;
    void updateWetParams() override;
    void processWet (float* const* io, int numSamples) override;
    void resetWet() override { output.reset(); }

    int numChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FasePhuckerAudioProcessor)
};
