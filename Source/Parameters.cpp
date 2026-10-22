#include "Parameters.h"

namespace fp
{
namespace
{
    using namespace juce;
    using Params = std::vector<std::unique_ptr<RangedAudioParameter>>;
    using NameFn = std::function<String()>;

    // between the parts of a name: "Mid Band · Side · Freq"
    const String& dot()
    {
        static const String d = String::fromUTF8 (" \xc2\xb7 ");
        return d;
    }

    // a parameter whose name follows the band count and stereo mode (asked again when the host is
    // told the parameter info changed)
    template <class Base>
    class Named : public Base
    {
    public:
        template <typename... Args>
        Named (const String& id, NameFn n, Args&&... args)
            : Base (ParameterID { id, 1 }, n(), std::forward<Args> (args)...), nameOf (std::move (n)) {}

        String getName (int maximumLength) const override { return nameOf().substring (0, maximumLength); }

    private:
        NameFn nameOf;
    };

    // ---- ranges
    NormalisableRange<float> linear (float lo, float hi, float step = 0.1f)
    {
        return { lo, hi, step };
    }

    // equal steps per octave; integer: whole numbers only (Stages)
    NormalisableRange<float> logarithmic (float lo, float hi, bool integer = false)
    {
        return { lo, hi,
                 [] (float s, float e, float x) { return s * std::pow (e / s, x); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [integer] (float s, float e, float v) { v = jlimit (s, e, v); return integer ? std::round (v) : v; } };
    }

    // ---- value text, as the interface shows it
    String number (float v, int decimals = 1)   // "2.4", "20"
    {
        return std::abs (v - std::round (v)) < 0.05f ? String (roundToInt (v)) : String (v, decimals);
    }
    String percent (float v, int)    { return String (roundToInt (v)) + " %"; }
    String bipolar (float v, int)    { return (roundToInt (v) > 0 ? "+" : "") + String (roundToInt (v)) + " %"; }
    String decibels (float v, int)   { return (v >= 0.05f ? "+" : "") + number (v) + " dB"; }
    String hertz (float v, int)      { return v < 1000.0f ? String (roundToInt (v)) + " Hz" : number (v / 1000.0f) + " kHz"; }
    String millis (float v, int)     { return (v < 10.0f ? String (v, 1) : String (roundToInt (v))) + " ms"; }
    String whole (float v, int)      { return String (roundToInt (v)); }

    float parse (const String& t)    { return t.getFloatValue() * (t.containsIgnoreCase ("k") ? 1000.0f : 1.0f); }

    struct Maker
    {
        Params& p;
        const ParameterNames& names;

        void real (const String& id, NameFn name, NormalisableRange<float> range, float def,
                   String (*text) (float, int), bool automatable = true)
        {
            p.push_back (std::make_unique<Named<AudioParameterFloat>> (id, std::move (name), range, def,
                                                                          AudioParameterFloatAttributes()
                                                                              .withStringFromValueFunction (text)
                                                                              .withValueFromStringFunction (parse)
                                                                              .withAutomatable (automatable)));
        }
        void choice (const String& id, NameFn name, StringArray choices, int def, bool automatable = true)
        {
            p.push_back (std::make_unique<Named<AudioParameterChoice>> (id, std::move (name), choices, def,
                                                                           AudioParameterChoiceAttributes().withAutomatable (automatable)));
        }
        void toggle (const String& id, NameFn name, bool def, bool automatable = true)
        {
            p.push_back (std::make_unique<Named<AudioParameterBool>> (id, std::move (name), def,
                                                                         AudioParameterBoolAttributes().withAutomatable (automatable)));
        }
    };

    void addChannel (Maker& m, int b, int c)
    {
        const auto* names = &m.names;
        auto id = [b, c] (const char* n) { return channelId (b, c, n); };
        auto name = [names, b, c] (const char* label) -> NameFn
        {
            return [names, b, c, label] { return names->band (b) + dot() + names->channel (b, c) + dot() + label; };
        };
        using namespace pid::ch;

        // Disperse
        m.real   (id (stages),  name ("Stages"), logarithmic (1.0f, 1000.0f, true), 64.0f, whole);
        m.choice (id (mode),    name ("Mode"), { "Behaved", "Unhinged" }, 0);
        m.toggle (id (jitter),  name ("Jitter"), false);
        m.real   (id (freq),    name ("Freq"), logarithmic (20.0f, 20000.0f), 400.0f, hertz);
        m.real   (id (pinch),   name ("Pinch"), linear (0.0f, 100.0f), 50.0f, percent);
        m.real   (id (spread),  name ("Spread"), linear (0.0f, 100.0f), 30.0f, percent);
        m.real   (id (dispMix), name ("Disperse Mix"), linear (0.0f, 100.0f), 100.0f, percent);
        m.toggle (id (dispOn),  name ("Disperse On"), true);

        // Env Follower
        m.real   (id (envGain), name ("Env Gain"), linear (-12.0f, 24.0f), 0.0f, decibels);
        m.real   (id (envAmt),  name ("Env Amount"), linear (-100.0f, 100.0f), 0.0f, bipolar);
        m.real   (id (attack),  name ("Attack"), logarithmic (0.1f, 100.0f), 5.0f, millis);
        m.real   (id (hold),    name ("Hold"), NormalisableRange<float> (0.0f, 500.0f, 0.1f, 0.5f), 20.0f, millis);
        m.real   (id (release), name ("Release"), logarithmic (5.0f, 2000.0f), 150.0f, millis);

        // Distortion
        m.choice (id (type),     name ("Distortion Type"), { "Noise", "Saturate", "Clip", "Fold", "Rectify", "Crush" }, 0);
        m.choice (id (order),    name ("Distortion Order"), { "Pre", "Post" }, 0);
        m.real   (id (drive),    name ("Drive"), linear (0.0f, 100.0f), 30.0f, percent);
        m.real   (id (bias),     name ("Bias"), linear (-100.0f, 100.0f), 0.0f, bipolar);
        m.real   (id (phat),     name ("Phat"), linear (0.0f, 100.0f), 0.0f, percent);
        m.real   (id (distTone), name ("Distortion Tone"), linear (0.0f, 100.0f), 50.0f, percent);
        m.real   (id (distMix),  name ("Distortion Mix"), linear (0.0f, 100.0f), 100.0f, percent);
        m.toggle (id (distOn),   name ("Distortion On"), true);

        // Feedback
        m.toggle (id (invert), name ("Feedback Invert"), false);
        m.real   (id (fbAmt),  name ("Feedback Amount"), linear (0.0f, 95.0f), 0.0f, percent);
        m.real   (id (fbTime), name ("Feedback Time"), logarithmic (0.1f, 50.0f), 5.0f, millis);
        m.real   (id (fbTone), name ("Feedback Tone"), linear (0.0f, 100.0f), 50.0f, percent);
        m.real   (id (fbMix),  name ("Feedback Mix"), linear (0.0f, 100.0f), 100.0f, percent);
        m.toggle (id (fbOn),   name ("Feedback On"), true);
    }

    const char* modLabel (const ModTarget& t)
    {
        static const std::map<String, const char*> labels {
            { pid::ch::freq, "Freq" }, { pid::ch::pinch, "Pinch" }, { pid::ch::spread, "Spread" }, { pid::ch::dispMix, "Disperse Mix" },
            { pid::ch::drive, "Drive" }, { pid::ch::bias, "Bias" }, { pid::ch::phat, "Phat" }, { pid::ch::distTone, "Distortion Tone" },
            { pid::ch::distMix, "Distortion Mix" }, { pid::ch::fbAmt, "Feedback Amount" }, { pid::ch::fbTime, "Feedback Time" },
            { pid::ch::fbTone, "Feedback Tone" }, { pid::ch::fbMix, "Feedback Mix" } };
        return labels.at (t.knob);
    }

    void addBand (Maker& m, int b)
    {
        const auto* names = &m.names;
        auto name = [names, b] (String label) -> NameFn { return [names, b, label] { return names->band (b) + dot() + label; }; };
        using namespace pid::band;

        m.toggle (bandId (b, on),     name ("On"), true);
        m.toggle (bandId (b, solo),   name ("Solo"), false);
        m.choice (bandId (b, stereo), name ("Stereo"), { "L / R", "M / S" }, 1, false);
        const StringArray links { "Linked", "1st Channel", "2nd Channel" };   // not automatable: switching copies values
        m.choice (bandId (b, dispLink), name ("Disperse Link"), links, linked, false);
        m.choice (bandId (b, envLink),  name ("Env Follower Link"), links, linked, false);
        m.choice (bandId (b, distLink), name ("Distortion Link"), links, linked, false);
        m.choice (bandId (b, fbLink),   name ("Feedback Link"), links, linked, false);

        addChannel (m, b, 0);
        addChannel (m, b, 1);

        // the Env Follower's destinations: depth, and whether the knob is assigned at all
        for (const auto& t : kModTargets)
        {
            const String label = "Env Mod" + dot() + modLabel (t);
            m.real   (modId (b, t),   name (label), linear (-100.0f, 100.0f), 50.0f, bipolar);
            m.toggle (modOnId (b, t), name (label + " Assigned"), false, false);
        }
    }
}

String ParameterNames::band (int b) const
{
    const int count = bands != nullptr ? roundToInt (bands->load()) + 1 : 1;
    if (count == 1) return b == 1 ? "Band" : "Band " + String (b);
    if (count == 2) return b == 1 ? "Low Band" : b == 2 ? "High Band" : "Band 3";
    return b == 1 ? "Low Band" : b == 2 ? "Mid Band" : "High Band";
}

String ParameterNames::channel (int b, int c) const
{
    const auto* s = stereo[(size_t) (b - 1)];
    const bool midSide = s == nullptr || s->load() > 0.5f;
    return midSide ? (c == 0 ? "Mid" : "Side") : (c == 0 ? "Left" : "Right");
}

void addParameters (Params& p, const ParameterNames& names)
{
    Maker m { p, names };
    m.choice (pid::bands, [] { return String ("Bands"); }, { "Single", "2 Bands", "3 Bands" }, 0, false);
    m.real (pid::split,  [] { return String ("Split"); },   logarithmic (20.0f, 20000.0f), 800.0f, hertz);
    m.real (pid::split1, [] { return String ("Split 1"); }, logarithmic (20.0f, 20000.0f), 180.0f, hertz);
    m.real (pid::split2, [] { return String ("Split 2"); }, logarithmic (20.0f, 20000.0f), 2400.0f, hertz);

    for (int b = 1; b <= kMaxBands; ++b)
        addBand (m, b);
}

const std::vector<String>& parameterIds()
{
    static const std::vector<String> ids = []
    {
        ParameterNames names;
        Params p;
        addParameters (p, names);
        std::vector<String> out;
        for (auto& param : p)
            out.push_back (param->getParameterID());
        return out;
    }();
    return ids;
}

const std::vector<const char*>& sectionParameters (Section s)
{
    using namespace pid::ch;
    static const std::vector<const char*> disperse { stages, mode, jitter, freq, pinch, spread, dispMix, dispOn },
                                          env { envGain, envAmt, attack, hold, release },
                                          distortion { type, order, drive, bias, phat, distTone, distMix, distOn },
                                          feedback { invert, fbAmt, fbTime, fbTone, fbMix, fbOn };
    switch (s)
    {
        case Section::disperse:    return disperse;
        case Section::envFollower: return env;
        case Section::distortion:  return distortion;
        case Section::feedback:    break;
    }
    return feedback;
}

const char* linkName (Section s)
{
    switch (s)
    {
        case Section::disperse:    return pid::band::dispLink;
        case Section::envFollower: return pid::band::envLink;
        case Section::distortion:  return pid::band::distLink;
        case Section::feedback:    break;
    }
    return pid::band::fbLink;
}
}
