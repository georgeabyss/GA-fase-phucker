#pragma once

#include "PluginProcessor.h"
#include "ui/Header.h"
#include "ui/EditorShell.h"

// Header and the bottom strip, at 100 %; the band strip and the four sections follow in build step 5.
// It sits on the GA editor shell's faceplate, which adds the scaling.
class MainPanel : public ga::Panel, private juce::Timer
{
public:
    // In Hyper! Hyper! Dimension's layout units: the mockup's pixels x 0.64 (750 / 1172, its strip).
    static constexpr int kBaseWidth = 806, kBaseHeight = 465;    // the mockup's 1260 x 726, bottom strip included
    static constexpr int kMarginX = 28, kMarginY = 16;           // around the content: the strip gets its native 750

    explicit MainPanel (FasePhuckerAudioProcessor&);
    ~MainPanel() override;

    void layout (juce::Rectangle<int> content, ga::BottomStrip&) override;

private:
    void timerCallback() override;

    ga::Header header;   // title, preset bar and the GA logo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
