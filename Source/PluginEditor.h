#pragma once

#include "PluginProcessor.h"
#include "MainPanel.h"
#include "ui/EditorShell.h"

// The GA editor shell (scaling, bottom strip) with Fase Phucker's panel on it.
class FasePhuckerAudioProcessorEditor : public ga::EditorShell
{
public:
    explicit FasePhuckerAudioProcessorEditor (FasePhuckerAudioProcessor&);

private:
    MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FasePhuckerAudioProcessorEditor)
};
