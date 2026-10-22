#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"
#include "plugin/SharedParams.h"

ga::ProcessorConfig FasePhuckerAudioProcessor::processorConfig()
{
    ga::ProcessorConfig c;
    c.scope = false;           // no goniometer in Fase Phucker
    c.oversampleWet = false;   // only Distortion is oversampled, by the engine itself
    c.defaultScale = 1.172f;   // as Hyper! Hyper! Dimension: same layout units, same size on screen
    return c;
}

juce::AudioProcessorValueTreeState::ParameterLayout FasePhuckerAudioProcessor::createParameterLayout (const fp::ParameterNames& names)
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    // Fase Phucker's own parameters first, then the GA ones: Bass Mono, Dry/Wet, Bypass,
    // Oversampling, Anti-Alias Filter, Phase Protect
    fp::addParameters (p, names);
    ga::addSharedParameters (p, processorConfig());
    return { p.begin(), p.end() };
}

FasePhuckerAudioProcessor::FasePhuckerAudioProcessor()
    : ga::ProcessorBase (createParameterLayout (parameterNames), processorConfig())
{
    // the automation names follow the band count and the stereo modes
    parameterNames.bands = apvts.getRawParameterValue (pid::bands);
    apvts.addParameterListener (pid::bands, this);
    for (int b = 1; b <= fp::kMaxBands; ++b)
    {
        const auto id = fp::bandId (b, pid::band::stereo);
        parameterNames.stereo[(size_t) (b - 1)] = apvts.getRawParameterValue (id);
        apvts.addParameterListener (id, this);
    }
}

FasePhuckerAudioProcessor::~FasePhuckerAudioProcessor()
{
    apvts.removeParameterListener (pid::bands, this);
    for (int b = 1; b <= fp::kMaxBands; ++b)
        apvts.removeParameterListener (fp::bandId (b, pid::band::stereo), this);
    cancelPendingUpdate();
}

void FasePhuckerAudioProcessor::handleAsyncUpdate()
{
    updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));
}

void FasePhuckerAudioProcessor::setSectionLink (int band, fp::Section section, fp::Link link)
{
    auto* linkParam = apvts.getParameter (fp::bandId (band, fp::linkName (section)));
    const auto before = (fp::Link) juce::roundToInt (linkParam->convertFrom0to1 (linkParam->getValue()));
    if (link == before)
        return;

    // unlinking: both channels start from the 1st; linking: the channel that was shown wins
    const int from = before == fp::secondChannel ? 1 : 0;
    if (before == fp::linked || link == fp::linked)
        for (const char* name : fp::sectionParameters (section))
        {
            auto* src = apvts.getParameter (fp::channelId (band, from, name));
            auto* dst = apvts.getParameter (fp::channelId (band, 1 - from, name));
            dst->setValueNotifyingHost (src->getValue());
        }
    linkParam->setValueNotifyingHost (linkParam->convertTo0to1 ((float) link));
}

void FasePhuckerAudioProcessor::prepareWet (double sampleRate, int channels, int oversampling)
{
    engineParams.read (params);
    engine.setParams (params);
    engine.prepare (sampleRate, channels, oversampling);
}

void FasePhuckerAudioProcessor::updateWetParams()
{
    engineParams.read (params);
    params.output.loudnessMatch = getConfig().loudnessMatch;
    params.output.loudnessWindow = 1.0;   // longer than Hyper! Hyper! Dimension's: dispersion moves transients around
    engine.setParams (params);
}

void FasePhuckerAudioProcessor::processWet (float* const* io, int n)
{
    engine.process (io, n);
}

juce::AudioProcessorEditor* FasePhuckerAudioProcessor::createEditor()
{
    return new FasePhuckerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FasePhuckerAudioProcessor();
}
