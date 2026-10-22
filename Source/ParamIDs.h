#pragma once

#include <juce_core/juce_core.h>
#include "plugin/SharedIds.h"

// Parameter IDs, shared by the processor and the editor.
// Never change them: hosts and saved sessions refer to parameters by these strings.
// The scheme is in docs/design/spec.md; tests/reference/parameters.txt holds the full list.
namespace pid
{
    // global
    inline constexpr const char* bands  = "bands";    // Single, 2 Bands, 3 Bands
    inline constexpr const char* split  = "split";    // the 2-band crossover
    inline constexpr const char* split1 = "split1";   // the 3-band crossovers
    inline constexpr const char* split2 = "split2";

    // output and settings: shared by all GA plug-ins (framework/plugin/SharedIds.h)
    inline constexpr const char* dryWet       = ga::pid::dryWet;         // "mix"
    inline constexpr const char* bassMono     = ga::pid::bassMono;       // "bassmono"
    inline constexpr const char* bypass       = ga::pid::bypass;         // "bypass"
    inline constexpr const char* osFactor     = ga::pid::osFactor;       // "os"
    inline constexpr const char* osFilter     = ga::pid::osFilter;       // "osfilter"
    inline constexpr const char* phaseProtect = ga::pid::phaseProtect;   // "phaseguard"

    // per band and channel: b<band><channel>_<name>, e.g. "b2s_freq" (see channelId)
    namespace ch
    {
        // Disperse
        inline constexpr const char* stages   = "stages";
        inline constexpr const char* mode     = "mode";       // Behaved, Unhinged
        inline constexpr const char* jitter   = "jitter";
        inline constexpr const char* freq     = "freq";
        inline constexpr const char* pinch    = "pinch";
        inline constexpr const char* spread   = "spread";
        inline constexpr const char* dispMix  = "dispmix";
        inline constexpr const char* dispOn   = "dispon";

        // Env Follower (no on switch: it is active whenever it has destinations)
        inline constexpr const char* envGain  = "envgain";
        inline constexpr const char* envAmt   = "envamt";
        inline constexpr const char* attack   = "attack";
        inline constexpr const char* hold     = "hold";
        inline constexpr const char* release  = "release";

        // Distortion
        inline constexpr const char* type     = "type";       // Noise, Saturate, Clip, Fold, Rectify, Crush
        inline constexpr const char* order    = "order";      // Pre, Post
        inline constexpr const char* drive    = "drive";
        inline constexpr const char* bias     = "bias";
        inline constexpr const char* phat     = "phat";
        inline constexpr const char* distTone = "disttone";
        inline constexpr const char* distMix  = "distmix";
        inline constexpr const char* distOn   = "diston";

        // Feedback
        inline constexpr const char* invert   = "invert";
        inline constexpr const char* fbAmt    = "fbamt";
        inline constexpr const char* fbTime   = "fbtime";
        inline constexpr const char* fbTone   = "fbtone";
        inline constexpr const char* fbMix    = "fbmix";
        inline constexpr const char* fbOn     = "fbon";
    }

    // per band: b<band>_<name>, e.g. "b2_solo" (see bandId)
    namespace band
    {
        inline constexpr const char* on       = "on";
        inline constexpr const char* solo     = "solo";
        inline constexpr const char* stereo   = "stereo";      // L / R, M / S
        inline constexpr const char* dispLink = "disp_link";   // Link, 1st channel, 2nd channel
        inline constexpr const char* envLink  = "env_link";
        inline constexpr const char* distLink = "dist_link";
        inline constexpr const char* fbLink   = "fb_link";
    }
}

// Settings that are not parameters, saved as properties of the state tree
namespace prop
{
    // shared by all GA plug-ins (framework/plugin/SharedIds.h)
    inline constexpr const char* scopeOpen    = ga::prop::scopeOpen;      // "scopeOpen", editor shell
    inline constexpr const char* uiScale      = ga::prop::uiScale;        // "uiScale"
    inline constexpr const char* preset       = ga::prop::preset;         // "preset", preset manager
    inline constexpr const char* presetIsUser = ga::prop::presetIsUser;   // "presetUser"
    inline constexpr const char* lockDryWet   = ga::prop::lockDryWet;     // "lockDryWet"
    inline constexpr const char* lockBassMono = ga::prop::lockBassMono;   // "lockBassMono"

    // Fase Phucker's own
    inline constexpr const char* selectedBand = "selectedBand";   // the band the sections show (0..2)
}

namespace fp
{
    constexpr int kMaxBands = 3;   // bands 1..3 (b1, b2, b3: the 1st, 2nd, 3rd band of the current mode)
    constexpr int kChannels = 2;   // 0 = "m" (Mid, or Left), 1 = "s" (Side, or Right)

    // the four sections; their channel link parameter is b<band>_<token>_link
    enum class Section { disperse, envFollower, distortion, feedback };
    constexpr Section kSections[] = { Section::disperse, Section::envFollower, Section::distortion, Section::feedback };

    // "b2s_freq"
    inline juce::String channelId (int band, int channel, const char* name)
    {
        return "b" + juce::String (band) + (channel == 0 ? "m" : "s") + "_" + name;
    }

    // "b2_solo"
    inline juce::String bandId (int band, const char* name)
    {
        return "b" + juce::String (band) + "_" + name;
    }

    // A knob the Env Follower can drive: its depth is b<band>_mod_<section>_<param> (-100..+100 %) and
    // whether it is assigned is the same ID + "_on". Listed in the order the donuts show them.
    struct ModTarget
    {
        const char* section;   // d = Disperse, x = Distortion, f = Feedback
        const char* param;
        const char* knob;      // the per-channel parameter it drives (pid::ch)
    };
    inline constexpr ModTarget kModTargets[] = {
        { "d", "freq",   pid::ch::freq },    { "d", "pinch", pid::ch::pinch },  { "d", "spread", pid::ch::spread }, { "d", "mix", pid::ch::dispMix },
        { "x", "drive",  pid::ch::drive },   { "x", "bias",  pid::ch::bias },   { "x", "phat",   pid::ch::phat },
        { "x", "tone",   pid::ch::distTone }, { "x", "mix",  pid::ch::distMix },
        { "f", "amt",    pid::ch::fbAmt },   { "f", "time",  pid::ch::fbTime }, { "f", "tone",   pid::ch::fbTone }, { "f", "mix", pid::ch::fbMix },
    };

    // "b2_mod_d_freq"; assigned: "b2_mod_d_freq_on"
    inline juce::String modId (int band, const ModTarget& t)
    {
        return "b" + juce::String (band) + "_mod_" + t.section + "_" + t.param;
    }
    inline juce::String modOnId (int band, const ModTarget& t)
    {
        return modId (band, t) + "_on";
    }
}
