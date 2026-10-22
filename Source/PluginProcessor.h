#pragma once

#include "plugin/ProcessorBase.h"
#include "Parameters.h"
#include "EngineParams.h"
#include "dsp/Engine.h"
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
// The effect itself is fp::dsp::Engine; its sections oversample themselves (only Distortion needs it).
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
    double getTailLengthSeconds() const override { return 3.0; }   // up to 2 s of dispersion, plus feedback

    static ga::ProcessorConfig processorConfig();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const fp::ParameterNames&);

    // Switch a section's channel link (the section header's dropdown). Unlinking copies the 1st
    // channel onto the 2nd once (while linked only the 1st counts); linking again copies the
    // channel that was shown onto the other. Message thread.
    void setSectionLink (int band, fp::Section, fp::Link);

    // The dice: new values within sensible ranges for one section of a band (band 1..3), or, without
    // a section, for every section of every band in use. Writes the channel being edited (the 1st
    // while linked). Message thread.
    void randomise (int band, std::optional<fp::Section>, juce::Random& = juce::Random::getSystemRandom());
    void randomiseAll (juce::Random& = juce::Random::getSystemRandom());

    ga::PresetManager presets { apvts, presetConfig (apvts) };
    fp::dsp::Engine engine;

private:
    void prepareWet (double sampleRate, int channels, int) override;
    void updateWetParams() override;
    void processWet (float* const* io, int numSamples) override;
    void resetWet() override { engine.reset(); }
    int wetLatency() const override { return engine.latency(); }

    // the band count or a stereo mode changed: the hosts ask for the parameter names again
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;

    fp::EngineParams engineParams { apvts };
    fp::dsp::Params params;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FasePhuckerAudioProcessor)
};
