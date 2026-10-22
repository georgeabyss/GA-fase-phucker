#pragma once

#include "presets/PresetManager.h"

// Fase Phucker's presets: factory presets, the parameters a preset stores, the locks, and the preset
// folder (~/Music/GA/Fase Phucker/Presets, .fphpreset files).
ga::PresetManager::Config presetConfig (juce::AudioProcessorValueTreeState&);
