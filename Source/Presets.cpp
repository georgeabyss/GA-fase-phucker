#include "Presets.h"
#include "ParamIDs.h"

namespace
{
    // the parameters a preset stores (Fase Phucker's own ones join in build step 2)
    constexpr const char* kPresetParams[] = { pid::bassMono, pid::dryWet };

    // Starting points; values not listed are the defaults. The first one is the default.
    std::vector<ga::PresetManager::FactoryPreset> factoryPresets()
    {
        return {
            { "Init", {} },
        };
    }
}

ga::PresetManager::Config presetConfig (juce::AudioProcessorValueTreeState&)
{
    ga::PresetManager::Config c;
    c.folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("GA").getChildFile ("Fase Phucker").getChildFile ("Presets");
    c.extension = ".fphpreset";
    c.fileTag = "FPHPRESET";
    c.parameters.assign (std::begin (kPresetParams), std::end (kPresetParams));
    c.lockable = { { pid::dryWet, prop::lockDryWet }, { pid::bassMono, prop::lockBassMono } };
    c.factory = factoryPresets();
    return c;
}
