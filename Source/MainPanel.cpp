#include "MainPanel.h"

namespace
{
    // ---- layout (806 x 465 layout pixels, the content inside the margins)
    constexpr int kHeaderHeight = 31;                       // the title row, at the top of the content
    constexpr int kPresetHeight = 24, kPresetOffset = -6;   // the preset bar (native height), nudged from the centre (as in the mockup)
}

MainPanel::MainPanel (FasePhuckerAudioProcessor& p)
    : header (p.presets, "FASE PHUCKER", "ALL-PASS DISPERSER")
{
    addAndMakeVisible (header);
    header.setPresetBar (kPresetHeight, kPresetOffset);
    header.presetBar.isMuted = [&p] { return p.isSafetyMuted(); };
    header.presetBar.resume = [&p] { p.resumeAfterSafetyMute(); };
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
}

void MainPanel::timerCallback()
{
    header.presetBar.refresh();
}

void MainPanel::layout (juce::Rectangle<int> content, ga::BottomStrip& strip)
{
    header.setArea (content.removeFromTop (kHeaderHeight));
    strip.setArea (content.removeFromBottom (ga::BottomStrip::heightForWidth (content.getWidth())));
}
