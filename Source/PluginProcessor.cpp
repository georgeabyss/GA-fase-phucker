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
