#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"
#include "plugin/SharedParams.h"

ga::ProcessorConfig FasePhuckerAudioProcessor::processorConfig()
{
    ga::ProcessorConfig c;
    c.scope = false;   // no goniometer in Fase Phucker
    c.defaultScale = 1.172f;   // as Hyper! Hyper! Dimension: same layout units, same size on screen
    return c;
}

juce::AudioProcessorValueTreeState::ParameterLayout FasePhuckerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    // Fase Phucker's own parameters come first (build step 2), then the GA ones: Bass Mono, Dry/Wet,
    // Bypass, Oversampling, Anti-Alias Filter, Phase Protect
    ga::addSharedParameters (p, processorConfig());
    return { p.begin(), p.end() };
}

FasePhuckerAudioProcessor::FasePhuckerAudioProcessor()
    : ga::ProcessorBase (createParameterLayout(), processorConfig())
{
}

void FasePhuckerAudioProcessor::prepareWet (double sampleRate, int channels, int)
{
    numChannels = channels;
    output.prepare (sampleRate, channels);
}

void FasePhuckerAudioProcessor::updateWetParams()
{
    ga::dsp::OutputStage::Params o;
    o.dryWet = param (pid::dryWet) * 0.01f;
    o.bassMonoHz = param (pid::bassMono);
    o.phaseProtect = param (pid::phaseProtect) > 0.5f;
    o.loudnessMatch = getConfig().loudnessMatch;
    output.setParams (o);
}

void FasePhuckerAudioProcessor::processWet (float* const* io, int n)
{
    output.beginBlock();
    for (int i = 0; i < n; ++i)
    {
        const float l = io[0][i];
        const float r = numChannels > 1 ? io[1][i] : l;

        float outL = l, outR = r;   // the effect: nothing yet
        output.process (l, r, outL, outR);

        io[0][i] = outL;
        if (numChannels > 1) io[1][i] = outR;
    }
    output.endBlock();
}

juce::AudioProcessorEditor* FasePhuckerAudioProcessor::createEditor()
{
    return new FasePhuckerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FasePhuckerAudioProcessor();
}
