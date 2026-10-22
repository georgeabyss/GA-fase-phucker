#include "Presets.h"
#include "Parameters.h"

namespace
{
    // the parameters a preset stores: all of Fase Phucker's own, plus Bass Mono and Dry/Wet
    std::vector<const char*> presetParameters()
    {
        std::vector<const char*> ids;
        for (const auto& id : fp::parameterIds())   // static, so the pointers stay valid
            ids.push_back (id.toRawUTF8());
        ids.push_back (pid::bassMono);
        ids.push_back (pid::dryWet);
        return ids;
    }

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
    c.parameters = presetParameters();
    c.lockable = { { pid::dryWet, prop::lockDryWet }, { pid::bassMono, prop::lockBassMono } };
    c.factory = factoryPresets();
    return c;
}
