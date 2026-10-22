#include "EngineParams.h"
#include "Parameters.h"

namespace fp
{
EngineParams::EngineParams (juce::AudioProcessorValueTreeState& s)
{
    auto v = [&s] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
    bands = v (pid::bands);
    split = v (pid::split);
    split1 = v (pid::split1);
    split2 = v (pid::split2);
    dryWet = v (pid::dryWet);
    bassMono = v (pid::bassMono);
    phaseProtect = v (pid::phaseProtect);
    osFilter = v (pid::osFilter);

    for (int b = 0; b < dsp::kMaxBands; ++b)
    {
        auto& bd = band[(size_t) b];
        const int n = b + 1;
        bd.on = v (bandId (n, pid::band::on));
        bd.solo = v (bandId (n, pid::band::solo));
        bd.stereo = v (bandId (n, pid::band::stereo));
        for (size_t k = 0; k < 4; ++k)
            bd.link[k] = v (bandId (n, linkName (kSections[k])));
        for (int c = 0; c < 2; ++c)
        {
            auto id = [&] (const char* name) { return v (channelId (n, c, name)); };
            using namespace pid::ch;
            bd.ch[(size_t) c] = { id (stages), id (mode), id (jitter), id (freq), id (pinch), id (spread), id (dispMix), id (dispOn),
                                  id (envGain), id (envAmt), id (attack), id (hold), id (release),
                                  id (type), id (order), id (drive), id (bias), id (phat), id (distTone), id (distMix), id (distOn),
                                  id (invert), id (fbAmt), id (fbTime), id (fbTone), id (fbMix), id (fbOn) };
        }
        for (int t = 0; t < dsp::kNumModTargets; ++t)
        {
            bd.modDepth[(size_t) t] = v (modId (n, kModTargets[t]));
            bd.modOn[(size_t) t] = v (modOnId (n, kModTargets[t]));
        }
    }
    for (int t = 0; t < dsp::kNumModTargets; ++t)
        modRanges[(size_t) t] = s.getParameter (channelId (1, 0, kModTargets[t].knob))->getNormalisableRange();
}

void EngineParams::read (dsp::Params& p) const
{
    auto get = [] (Value x) { return x->load (std::memory_order_relaxed); };
    auto on = [&] (Value x) { return get (x) > 0.5f; };

    p.bands = std::clamp (juce::roundToInt (get (bands)) + 1, 1, dsp::kMaxBands);
    p.split = get (split);
    p.split1 = get (split1);
    p.split2 = get (split2);
    p.output.dryWet = get (dryWet) * 0.01f;
    p.output.bassMonoHz = get (bassMono);
    p.output.phaseProtect = on (phaseProtect);
    p.osLinearPhase = juce::roundToInt (get (osFilter)) == 0;

    for (size_t b = 0; b < (size_t) dsp::kMaxBands; ++b)
    {
        const auto& bd = band[b];
        auto& out = p.band[b];
        out.on = on (bd.on);
        out.solo = on (bd.solo);
        out.midSide = on (bd.stereo);
        for (size_t c = 0; c < 2; ++c)
        {
            const auto& in = bd.ch[c];
            auto& ch = out.ch[c];
            ch.stages = std::clamp (juce::roundToInt (get (in.stages)), 1, dsp::kMaxStages);
            ch.unhinged = on (in.mode);
            ch.jitter = on (in.jitter);
            ch.freq = get (in.freq);
            ch.pinch = get (in.pinch) * 0.01f;
            ch.spread = get (in.spread) * 0.01f;
            ch.dispMix = get (in.dispMix) * 0.01f;
            ch.dispOn = on (in.dispOn);
            ch.envGainDb = get (in.envGain);
            ch.envAmount = get (in.envAmt) * 0.01f;
            ch.attackMs = get (in.attack);
            ch.holdMs = get (in.hold);
            ch.releaseMs = get (in.release);
            ch.type = (dsp::DistType) std::clamp (juce::roundToInt (get (in.type)), 0, 5);
            ch.post = on (in.order);
            ch.drive = get (in.drive) * 0.01f;
            ch.bias = get (in.bias) * 0.01f;
            ch.phat = get (in.phat) * 0.01f;
            ch.distTone = get (in.distTone) * 0.01f;
            ch.distMix = get (in.distMix) * 0.01f;
            ch.distOn = on (in.distOn);
            ch.invert = on (in.invert);
            ch.fbAmount = get (in.fbAmt) * 0.01f;
            ch.fbTimeMs = get (in.fbTime);
            ch.fbTone = get (in.fbTone) * 0.01f;
            ch.fbMix = get (in.fbMix) * 0.01f;
            ch.fbOn = on (in.fbOn);
        }

        // linked sections: the 2nd channel takes the 1st channel's values
        auto& m = out.ch[0];
        auto& s = out.ch[1];
        if (juce::roundToInt (get (bd.link[0])) == linked)
        {
            s.stages = m.stages; s.unhinged = m.unhinged; s.jitter = m.jitter; s.freq = m.freq; s.pinch = m.pinch;
            s.spread = m.spread; s.dispMix = m.dispMix; s.dispOn = m.dispOn;
        }
        if (juce::roundToInt (get (bd.link[1])) == linked)
        {
            s.envGainDb = m.envGainDb; s.envAmount = m.envAmount; s.attackMs = m.attackMs; s.holdMs = m.holdMs; s.releaseMs = m.releaseMs;
        }
        if (juce::roundToInt (get (bd.link[2])) == linked)
        {
            s.type = m.type; s.post = m.post; s.drive = m.drive; s.bias = m.bias; s.phat = m.phat;
            s.distTone = m.distTone; s.distMix = m.distMix; s.distOn = m.distOn;
        }
        if (juce::roundToInt (get (bd.link[3])) == linked)
        {
            s.invert = m.invert; s.fbAmount = m.fbAmount; s.fbTimeMs = m.fbTimeMs; s.fbTone = m.fbTone; s.fbMix = m.fbMix; s.fbOn = m.fbOn;
        }

        for (size_t t = 0; t < (size_t) dsp::kNumModTargets; ++t)
        {
            out.modOn[t] = on (bd.modOn[t]);
            out.modDepth[t] = get (bd.modDepth[t]) * 0.01f;
        }
    }
}
}
