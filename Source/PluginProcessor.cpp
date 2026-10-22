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
    engine.setModRanges (&engineParams.ranges());
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

void FasePhuckerAudioProcessor::randomise (int band, std::optional<fp::Section> section, juce::Random& rng)
{
    using namespace fp;
    auto value = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto put = [this] (const juce::String& id, float v)
    {
        auto* p = apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    auto between = [&rng] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto logBetween = [&rng] (float lo, float hi) { return lo * std::pow (hi / lo, rng.nextFloat()); };
    auto chance = [&rng] (float p) { return rng.nextFloat() < p ? 1.0f : 0.0f; };

    // the band's own range, for Freq
    const int count = juce::roundToInt (value (pid::bands)) + 1;
    std::vector<float> edges { 20.0f };
    if (count == 2) edges.push_back (value (pid::split));
    if (count == 3) { edges.push_back (value (pid::split1)); edges.push_back (value (pid::split2)); }
    edges.push_back (20000.0f);
    const float lo = edges[(size_t) juce::jlimit (0, count - 1, band - 1)], hi = edges[(size_t) juce::jlimit (1, count, band)];

    for (auto s : kSections)
    {
        if (section.has_value() && *section != s)
            continue;
        // the channel being edited: the 1st while linked
        const int link = juce::roundToInt (value (bandId (band, linkName (s))));
        auto id = [band, link] (const char* name) { return channelId (band, link == secondChannel ? 1 : 0, name); };
        using namespace pid::ch;
        switch (s)
        {
            case Section::disperse:
            {
                const int choices[] = { 4, 8, 16, 32, 64, 128, 256, 512, 1000 };
                put (id (stages), (float) choices[rng.nextInt (9)]);
                put (id (freq), juce::roundToInt (logBetween (lo * 1.15f, hi / 1.15f)));
                put (id (pinch), std::round (between (0.0f, 100.0f)));
                put (id (spread), std::round (between (0.0f, 100.0f)));
                put (id (dispMix), std::round (between (40.0f, 100.0f)));
                put (id (mode), chance (0.5f));
                put (id (jitter), chance (0.35f));
                break;
            }
            case Section::envFollower:
                put (id (envGain), std::round (between (-12.0f, 24.0f)));
                put (id (envAmt), std::round (between (-100.0f, 100.0f)));
                put (id (attack), logBetween (0.1f, 100.0f));
                put (id (hold), std::round (between (0.0f, 200.0f)));
                put (id (release), std::round (logBetween (5.0f, 2000.0f)));
                break;
            case Section::distortion:
                put (id (type), (float) rng.nextInt (6));
                put (id (drive), std::round (between (0.0f, 100.0f)));
                put (id (bias), std::round (between (-60.0f, 60.0f)));
                put (id (phat), std::round (between (0.0f, 100.0f)));
                put (id (distTone), std::round (between (0.0f, 100.0f)));
                put (id (distMix), std::round (between (30.0f, 100.0f)));
                put (id (order), chance (0.5f));
                break;
            case Section::feedback:
                put (id (fbAmt), std::round (between (0.0f, 70.0f)));   // below 80 %, where the loop starts to ring on
                put (id (fbTime), logBetween (0.1f, 50.0f));
                put (id (fbTone), std::round (between (0.0f, 100.0f)));
                put (id (invert), chance (0.3f));
                put (id (fbMix), std::round (between (40.0f, 100.0f)));
                break;
        }
    }
}

void FasePhuckerAudioProcessor::randomiseAll (juce::Random& rng)
{
    const int count = juce::roundToInt (apvts.getRawParameterValue (pid::bands)->load()) + 1;
    for (int b = 1; b <= count; ++b)
        randomise (b, std::nullopt, rng);
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
