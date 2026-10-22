// The parameter tool. Only built with -DFPH_PARAMS=ON, in its own build folder (see
// tools/check_parameters.sh), never into the plug-ins.
//
//   FasePhuckerParams list   every parameter, one per line: ID, name, kind, range, default,
//                            automatable. tests/reference/parameters.txt holds the reference:
//                            an ID must never change or disappear.
//   FasePhuckerParams test   the automation names, the channel links and loading a session

#include "../Source/PluginProcessor.h"

namespace
{
    juce::String describe (juce::RangedAudioParameter& p)
    {
        juce::String kind, range;
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&p))
        {
            kind = "choice";
            range = c->choices.joinIntoString (" | ");
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (&p) != nullptr)
        {
            kind = "bool";
            range = "off | on";
        }
        else
        {
            kind = "float";
            const auto& r = p.getNormalisableRange();
            range = p.getText (p.convertTo0to1 (r.start), 32) + " .. " + p.getText (p.convertTo0to1 (r.end), 32);
        }
        return p.getParameterID() + "\t" + p.getName (128) + "\t" + kind + "\t" + range + "\t"
               + p.getText (p.getDefaultValue(), 32) + "\t" + (p.isAutomatable() ? "automatable" : "-");
    }

    int list()
    {
        FasePhuckerAudioProcessor proc;
        for (auto* p : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                std::printf ("%s\n", describe (*r).toRawUTF8());
        return 0;
    }

    int failures = 0;
    void expect (bool ok, const juce::String& what)
    {
        std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
        failures += ok ? 0 : 1;
    }

    void set (FasePhuckerAudioProcessor& proc, const juce::String& id, float value)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
    float get (FasePhuckerAudioProcessor& proc, const juce::String& id)
    {
        return proc.apvts.getRawParameterValue (id)->load();
    }
    juce::String name (FasePhuckerAudioProcessor& proc, const juce::String& id)
    {
        return proc.apvts.getParameter (id)->getName (128);
    }

    int test()
    {
        using namespace fp;
        const auto freq1m = channelId (1, 0, pid::ch::freq), freq1s = channelId (1, 1, pid::ch::freq);

        // IDs: unique, and the counts the scheme gives
        {
            const auto& ids = parameterIds();
            std::set<juce::String> unique (ids.begin(), ids.end());
            expect (unique.size() == ids.size(), "all " + juce::String ((int) ids.size()) + " IDs are unique");
            expect (ids.size() == 4 + 3 * (7 + 2 * 27 + 2 * 13), "4 global + 3 bands x (7 + 2 x 27 + 2 x 13)");
            expect (channelId (2, 1, pid::ch::freq) == "b2s_freq" && bandId (2, pid::band::distLink) == "b2_dist_link"
                        && modId (2, kModTargets[0]) == "b2_mod_d_freq", "the spec's examples: b2s_freq, b2_dist_link, b2_mod_d_freq");
        }

        // automation names follow the band count and the stereo mode
        {
            FasePhuckerAudioProcessor proc;
            const auto id = channelId (2, 1, pid::ch::freq);
            expect (name (proc, id) == juce::String::fromUTF8 ("Band 2 \xc2\xb7 Side \xc2\xb7 Freq"), "Single: " + name (proc, id));
            set (proc, pid::bands, 2.0f);
            set (proc, bandId (2, pid::band::stereo), 0.0f);
            expect (name (proc, id) == juce::String::fromUTF8 ("Mid Band \xc2\xb7 Right \xc2\xb7 Freq"), "3 Bands, L / R: " + name (proc, id));
            set (proc, pid::bands, 1.0f);
            expect (name (proc, id).startsWith ("High Band"), "2 Bands: " + name (proc, id));
        }

        // channel links
        {
            FasePhuckerAudioProcessor proc;
            set (proc, freq1m, 1000.0f);
            set (proc, freq1s, 200.0f);
            const auto drive1s = channelId (1, 1, pid::ch::drive);
            set (proc, drive1s, 77.0f);

            proc.setSectionLink (1, Section::disperse, firstChannel);
            expect (std::abs (get (proc, freq1s) - 1000.0f) < 0.5f, "unlinking copies the 1st channel onto the 2nd");
            expect (std::abs (get (proc, drive1s) - 77.0f) < 0.5f, "... only in that section");

            proc.setSectionLink (1, Section::disperse, secondChannel);
            set (proc, freq1s, 3000.0f);
            expect (std::abs (get (proc, freq1m) - 1000.0f) < 0.5f, "switching the shown channel copies nothing");

            proc.setSectionLink (1, Section::disperse, linked);
            expect (std::abs (get (proc, freq1m) - 3000.0f) < 1.0f, "linking while the 2nd is shown copies the 2nd onto the 1st");

            proc.setSectionLink (1, Section::distortion, firstChannel);
            set (proc, drive1s, 12.0f);
            proc.setSectionLink (1, Section::distortion, linked);
            expect (std::abs (get (proc, drive1s) - 30.0f) < 0.5f, "linking while the 1st is shown copies the 1st onto the 2nd");
        }

        // a session with unlinked, different channels comes back as it was saved
        {
            juce::MemoryBlock state;
            {
                FasePhuckerAudioProcessor proc;
                proc.setSectionLink (1, Section::disperse, secondChannel);
                set (proc, freq1m, 500.0f);
                set (proc, freq1s, 5000.0f);
                proc.getStateInformation (state);
            }
            FasePhuckerAudioProcessor proc;
            proc.setStateInformation (state.getData(), (int) state.getSize());
            expect (std::abs (get (proc, freq1m) - 500.0f) < 0.5f && std::abs (get (proc, freq1s) - 5000.0f) < 1.0f
                        && juce::roundToInt (get (proc, bandId (1, pid::band::dispLink))) == secondChannel,
                    "loading a session keeps both channels and the link");
        }

        std::printf ("\n%s\n", failures == 0 ? "All parameter tests passed." : "Parameter tests failed.");
        return failures == 0 ? 0 : 1;
    }
}

// called from tools/ParametersMain.cpp
int fphParameters (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String mode (argc > 1 ? argv[1] : "list");
    return mode == "test" ? test() : list();
}
