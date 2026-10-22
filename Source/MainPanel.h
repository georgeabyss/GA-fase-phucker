#pragma once

#include "PluginProcessor.h"
#include "BandTrace.h"
#include "ui/Header.h"
#include "ui/EditorShell.h"
#include "ui/Section.h"
#include "ui/Bands.h"
#include "ui/Modulation.h"

// Fase Phucker's panel, in layout pixels (the mockup's x 0.64): the header with the global dice, the
// band strip, Disperse | Env Follower and Distortion | Feedback on a 12-column grid, the bottom strip.
// The sections show the selected band; each one the channel its link drop-down shows (the 1st while
// linked) and rebuilds its controls when either changes.
class MainPanel : public ga::Panel, private juce::Timer
{
public:
    static constexpr int kBaseWidth = 806, kBaseHeight = 465;    // the mockup's 1260 x 726, bottom strip included
    static constexpr int kMarginX = 28, kMarginY = 16;           // around the content: the strip gets its native 750

    explicit MainPanel (FasePhuckerAudioProcessor&);
    ~MainPanel() override;

    void layout (juce::Rectangle<int> content, ga::BottomStrip&) override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // a section and the controls it shows now (for one band and channel)
    struct SectionView
    {
        SectionView (fp::Section s, const char* title, int columns) : id (s), view (title, columns) {}
        fp::Section id;
        ga::Section view;
        ga::DiceButton dice;
        std::unique_ptr<ga::StepBox> link;
        std::unique_ptr<ga::SectionLed> led;
        std::vector<std::unique_ptr<juce::Component>> controls;
        std::vector<std::unique_ptr<ButtonAttachment>> attachments;
        int band = -1, channel = -1, stereo = -1;   // what it was built for
    };

    // a band's card and its own controls
    struct Card
    {
        ga::BandCard card;
        ga::SectionLed led;
        ga::SoloButton solo;
        std::unique_ptr<ga::StepBox> stereo;
        std::unique_ptr<ButtonAttachment> ledAttachment, soloAttachment;
        std::array<fp::BandTrace, 2> traces;
    };

    // the "+n" button that pages through the donuts
    struct MoreButton : public juce::Button
    {
        MoreButton() : juce::Button ("more") { setSize (24, 17); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
    };

    void timerCallback() override;

    int band() const { return selected + 1; }   // 1..3
    int bandCount() const;
    int shownChannel (fp::Section) const;
    juce::String channelLetter (int bandNumber, int channel) const;   // "M" / "S" or "L" / "R"
    float value (const juce::String& id) const;
    void put (const juce::String& id, float v);
    void select (int index);

    void rebuild (SectionView&);       // the controls for the band and channel it shows
    void updateStrip();                // which cards and splits, after a band count change
    void updateCards();
    void updateSections();             // dimming, labels, modulation arcs
    void updateDonuts();               // the Env Follower's destinations

    juce::Component& addKnob (SectionView&, const char* name, const juce::String& label, bool centreDetent = false);
    juce::Component& addBox (SectionView&, const char* name, bool names);
    juce::Component& addLamp (SectionView&, const char* name);

    FasePhuckerAudioProcessor& proc;
    int selected = 0, shownBands = -1;

    ga::Header header;
    ga::DiceButton globalDice;

    ga::Section bandsSection { "BANDS", 1 };
    std::unique_ptr<ga::StepBox> bandsBox;
    ga::BandStrip strip;
    std::array<Card, fp::kMaxBands> cards;
    std::unique_ptr<ga::SplitHandle> split, split1, split2;

    SectionView disperse { fp::Section::disperse, "DISPERSE", 7 };
    SectionView env { fp::Section::envFollower, "ENV FOLLOWER", 5 };
    SectionView distortion { fp::Section::distortion, "DISTORTION", 7 };
    SectionView feedback { fp::Section::feedback, "FEEDBACK", 5 };

    // modulation: the knobs it can drive (in the sections now), the grip, the donuts
    std::array<ga::Knob*, fp::dsp::kNumModTargets> modKnobs {};
    ga::Knob* driveKnob = nullptr;
    ga::ModSource grip;
    std::vector<std::unique_ptr<ga::ModDonut>> donuts;
    MoreButton more;
    int donutPage = 0, donutsBand = -1;
    juce::uint32 donutsShown = 0xffffffff;   // which targets the donuts were made for

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
