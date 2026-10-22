#pragma once

#include "plugin/ProcessorBase.h"
#include "dsp/OutputStage.h"
#include "Parameters.h"
#include "Presets.h"

namespace fp
{
// holds the automation names' context; a base class, so it exists before the parameters are made
struct NamesHolder
{
    ParameterNames parameterNames;
};
}

// Fase Phucker on the GA processor base (oversampling, bypass, safety mute, scope, state).
// Build step 2: every parameter exists; no effect yet, the audio only runs through the shared
// output stage.
class FasePhuckerAudioProcessor : private fp::NamesHolder,
                                  public ga::ProcessorBase,
                                  private juce::AudioProcessorValueTreeState::Listener,
                                  private juce::AsyncUpdater
{
public:
    FasePhuckerAudioProcessor();
    ~FasePhuckerAudioProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    const juce::String getName() const override { return "Fase Phucker"; }
    double getTailLengthSeconds() const override { return 0.0; }

    static ga::ProcessorConfig processorConfig();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const fp::ParameterNames&);

    // Switch a section's channel link (the section header's dropdown). Unlinking copies the 1st
    // channel onto the 2nd once (while linked only the 1st counts); linking again copies the
    // channel that was shown onto the other. Message thread.
    void setSectionLink (int band, fp::Section, fp::Link);

    ga::PresetManager presets { apvts, presetConfig (apvts) };
    ga::dsp::OutputStage output;   // dry/wet, Bass Mono, Phase Protect, loudness match

private:
    void prepareWet (double sampleRate, int channels, int) override;
    void updateWetParams() override;
    void processWet (float* const* io, int numSamples) override;
    void resetWet() override { output.reset(); }

    // the band count or a stereo mode changed: the hosts ask for the parameter names again
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;

    int numChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FasePhuckerAudioProcessor)
};
