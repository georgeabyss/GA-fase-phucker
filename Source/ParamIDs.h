#pragma once

#include "plugin/SharedIds.h"

// Parameter IDs, shared by the processor and the editor.
// Never change them: hosts and saved sessions refer to parameters by these strings.
// Fase Phucker's own parameters follow the scheme in docs/design/spec.md (build step 2).
namespace pid
{
    // output and settings: shared by all GA plug-ins (framework/plugin/SharedIds.h)
    inline constexpr const char* dryWet       = ga::pid::dryWet;         // "mix"
    inline constexpr const char* bassMono     = ga::pid::bassMono;       // "bassmono"
    inline constexpr const char* bypass       = ga::pid::bypass;         // "bypass"
    inline constexpr const char* osFactor     = ga::pid::osFactor;       // "os"
    inline constexpr const char* osFilter     = ga::pid::osFilter;       // "osfilter"
    inline constexpr const char* phaseProtect = ga::pid::phaseProtect;   // "phaseguard"
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
}
