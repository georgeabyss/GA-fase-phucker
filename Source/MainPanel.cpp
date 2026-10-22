#include "MainPanel.h"

namespace
{
    // ---- layout (806 x 465 layout pixels = the mockup's pixels x 0.64), from the top of the content
    constexpr int kHeaderHeight = 31;                       // the title row
    constexpr int kPresetHeight = 24, kPresetOffset = -6;   // the preset bar (native height), nudged from the centre
    constexpr int kBandsTop = 51, kBandsHeader = 29;        // BANDS: its header line
    constexpr int kCardsTop = 80, kCardsHeight = 72;        // the band strip
    constexpr int kRow1Top = 170, kRow2Top = 287, kRowHeight = 117;   // Disperse | Env Follower, Distortion | Feedback
    constexpr int kColumns = 12, kSectionGap = 30;          // 7 | 5 columns, both rows split at the same column

    const char* bandName (int count, int index)
    {
        if (count == 1) return "FULL";
        if (count == 2) return index == 0 ? "LOW" : "HIGH";
        return index == 0 ? "LOW" : index == 1 ? "MID" : "HIGH";
    }

    juce::String hertz (float v)
    {
        return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz"
                           : (std::abs (v / 1000.0f - std::round (v / 1000.0f)) < 0.05f ? juce::String (juce::roundToInt (v / 1000.0f))
                                                                                         : juce::String (v / 1000.0f, 1)) + " kHz";
    }

    const juce::String& dash() { static const juce::String d = juce::String::fromUTF8 (" \xe2\x80\x93 "); return d; }   // " – "
    const juce::String& dot()  { static const juce::String d = juce::String::fromUTF8 (" \xc2\xb7 "); return d; }      // " · "
    const juce::String& notEqual() { static const juce::String d = juce::String::fromUTF8 (" \xe2\x89\xa0 "); return d; }   // " ≠ "
}

//==============================================================================
MainPanel::MainPanel (FasePhuckerAudioProcessor& p)
    : proc (p),
      header (p.presets, "FASE PHUCKER", "ALL-PASS DISPERSER")
{
    using namespace fp;
    selected = juce::jlimit (0, kMaxBands - 1, (int) p.apvts.state.getProperty (prop::selectedBand, 0));

    // header: the global dice next to the preset bar
    addAndMakeVisible (header);
    header.setPresetBar (kPresetHeight, kPresetOffset);
    header.setPresetExtras ({ &globalDice });
    header.presetBar.isMuted = [&p] { return p.isSafetyMuted(); };
    header.presetBar.resume = [&p] { p.resumeAfterSafetyMute(); };
    globalDice.setTooltip ("Randomise every band");
    globalDice.onClick = [this] { proc.randomiseAll(); };

    // BANDS: the band count, the cards and the splits
    bandsBox = std::make_unique<ga::StepBox> (*p.apvts.getParameter (pid::bands), true);
    bandsSection.setHeaderControls ({ bandsBox.get() });
    addAndMakeVisible (bandsSection);
    for (int i = 0; i < kMaxBands; ++i)
    {
        auto& c = cards[(size_t) i];
        const int b = i + 1;
        c.stereo = std::make_unique<ga::StepBox> (*p.apvts.getParameter (bandId (b, pid::band::stereo)), true);
        c.ledAttachment = std::make_unique<ButtonAttachment> (p.apvts, bandId (b, pid::band::on), c.led);
        c.soloAttachment = std::make_unique<ButtonAttachment> (p.apvts, bandId (b, pid::band::solo), c.solo);
        c.led.setTooltip ("Band on / off (off: the band passes unprocessed)");
        c.stereo->setTooltip ("What the band's two channels are: Left / Right or Mid / Side");
        c.card.setControls (&c.led, &c.solo, c.stereo.get());
        c.card.onSelect = [this, i] { select (i); };
    }
    split  = std::make_unique<ga::SplitHandle> (*p.apvts.getParameter (pid::split));
    split1 = std::make_unique<ga::SplitHandle> (*p.apvts.getParameter (pid::split1));
    split2 = std::make_unique<ga::SplitHandle> (*p.apvts.getParameter (pid::split2));
    split1->limit = [this] (float v) { return std::min (v, value (pid::split2) / 1.5f); };   // at least 1.5 times apart
    split2->limit = [this] (float v) { return std::max (v, value (pid::split1) * 1.5f); };
    addAndMakeVisible (strip);

    // the sections; their dice and the grip
    for (auto* s : { &disperse, &env, &distortion, &feedback })
    {
        addAndMakeVisible (s->view);
        s->dice.setTooltip ("Randomise this section");
        s->dice.onClick = [this, s] { proc.randomise (band(), s->id); };
    }
    grip.setTooltip ("Drag onto a knob in Disperse, Distortion or Feedback to modulate it");
    more.onClick = [this] { ++donutPage; donutsShown = 0xffffffff; updateDonuts(); };

    updateStrip();
    for (auto* s : { &disperse, &env, &distortion, &feedback })
        rebuild (*s);
    updateDonuts();
    updateCards();
    updateSections();
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
}

void MainPanel::layout (juce::Rectangle<int> content, ga::BottomStrip& bottom)
{
    header.setArea (content.withHeight (kHeaderHeight));
    bottom.setArea (content.withTrimmedTop (content.getHeight() - ga::BottomStrip::heightForWidth (content.getWidth())));

    bandsSection.setBounds (content.withTrimmedTop (kBandsTop).withHeight (kBandsHeader));
    strip.setArea (content.withTrimmedTop (kCardsTop).withHeight (kCardsHeight));

    const ga::ColumnGrid row1 (content.withTrimmedTop (kRow1Top).withHeight (kRowHeight), kColumns, kSectionGap, { 7, 5 });
    const ga::ColumnGrid row2 (content.withTrimmedTop (kRow2Top).withHeight (kRowHeight), kColumns, kSectionGap, { 7, 5 });
    disperse.view.setBounds (row1.section (0));
    env.view.setBounds (row1.section (1));
    distortion.view.setBounds (row2.section (0));
    feedback.view.setBounds (row2.section (1));
}

//==============================================================================
float MainPanel::value (const juce::String& id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

void MainPanel::put (const juce::String& id, float v)
{
    auto* p = proc.apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (v));
}

int MainPanel::bandCount() const
{
    return juce::jlimit (1, fp::kMaxBands, juce::roundToInt (value (pid::bands)) + 1);
}

int MainPanel::shownChannel (fp::Section s) const
{
    return juce::roundToInt (value (fp::bandId (band(), fp::linkName (s)))) == fp::secondChannel ? 1 : 0;
}

juce::String MainPanel::channelLetter (int b, int channel) const
{
    const bool midSide = value (fp::bandId (b, pid::band::stereo)) > 0.5f;
    return midSide ? (channel == 0 ? "M" : "S") : (channel == 0 ? "L" : "R");
}

void MainPanel::select (int index)
{
    selected = juce::jlimit (0, bandCount() - 1, index);
    proc.apvts.state.setProperty (prop::selectedBand, selected, nullptr);
    for (auto* s : { &disperse, &env, &distortion, &feedback })
        rebuild (*s);
    donutsShown = 0xffffffff;
    donutPage = 0;
    updateDonuts();
    updateCards();
    updateSections();
}

//==============================================================================
juce::Component& MainPanel::addKnob (SectionView& s, const char* name, const juce::String& label, bool centreDetent)
{
    auto knob = std::make_unique<ga::Knob> (*proc.apvts.getParameter (fp::channelId (s.band, s.channel, name)), label, centreDetent);
    auto& ref = *knob;
    s.controls.push_back (std::move (knob));
    return ref;
}

juce::Component& MainPanel::addBox (SectionView& s, const char* name, bool names)
{
    auto box = std::make_unique<ga::StepBox> (*proc.apvts.getParameter (fp::channelId (s.band, s.channel, name)), names);
    auto& ref = *box;
    s.controls.push_back (std::move (box));
    return ref;
}

juce::Component& MainPanel::addLamp (SectionView& s, const char* name)
{
    auto lamp = std::make_unique<ga::LampButton>();
    s.attachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, fp::channelId (s.band, s.channel, name), *lamp));
    auto& ref = *lamp;
    s.controls.push_back (std::move (lamp));
    return ref;
}

void MainPanel::rebuild (SectionView& s)
{
    using namespace fp;
    using namespace pid::ch;
    const int b = band();

    // the old controls stay alive until the new ones are in place
    auto oldControls = std::move (s.controls);
    auto oldAttachments = std::move (s.attachments);
    auto oldLink = std::move (s.link);
    auto oldLed = std::move (s.led);
    s.controls.clear();
    s.attachments.clear();
    s.band = b;
    s.channel = shownChannel (s.id);
    s.stereo = juce::roundToInt (value (bandId (b, pid::band::stereo)));

    // the channel drop-down: LINK / M / S (or L / R); a choice copies values as the spec says
    s.link = std::make_unique<ga::StepBox> (*proc.apvts.getParameter (bandId (b, linkName (s.id))), true);
    s.link->setItemNames ([this, b] (int v) { return v == linked ? juce::String ("LINK") : channelLetter (b, v - 1); });
    s.link->onChoose = [this, b, id = s.id] (int v) { proc.setSectionLink (b, id, (Link) v); };
    s.link->setTooltip ("LINK: both channels share this section. M / S or L / R: edit that channel alone.");

    // which of its knobs the Env Follower can drive (by ModTarget)
    auto modTarget = [this, &s] (int target, juce::Component& c)
    {
        auto* knob = static_cast<ga::Knob*> (&c);
        modKnobs[(size_t) target] = knob;
        knob->onModDrop = [this, target]
        {
            const auto& t = fp::kModTargets[target];
            if (value (modOnId (band(), t)) < 0.5f)
            {
                put (modId (band(), t), 50.0f);   // a new destination starts at +50 %
                put (modOnId (band(), t), 1.0f);
            }
        };
        juce::ignoreUnused (s);
    };

    std::vector<juce::Component*> headerControls;
    auto setLed = [this, &s, &headerControls] (const char* name)
    {
        s.led = std::make_unique<ga::SectionLed>();
        s.attachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, channelId (s.band, s.channel, name), *s.led));
        s.led->setTooltip ("Section on / off");
        headerControls = { s.link.get(), &s.dice, s.led.get() };
    };

    switch (s.id)
    {
        case Section::disperse:
        {
            auto& stages = static_cast<ga::StepBox&> (addBox (s, pid::ch::stages, false));
            stages.setWideNumber ([] (int v, int d) { return d > 0 ? (v >= 512 ? 1000 : v * 2) : (v == 1000 ? 512 : std::max (1, v / 2)); }, 3.0f);
            s.view.setControl (0, stages, "Stages");
            s.view.setControl (1, addBox (s, mode, true), "Mode");
            s.view.setControl (2, addLamp (s, jitter), "Jitter");
            auto& f = addKnob (s, freq, "Freq");      modTarget (dsp::modFreq, f);    s.view.setControl (3, f);
            auto& pi = addKnob (s, pinch, "Pinch");   modTarget (dsp::modPinch, pi);  s.view.setControl (4, pi);
            auto& sp = addKnob (s, spread, "Spread"); modTarget (dsp::modSpread, sp); s.view.setControl (5, sp);
            auto& m = addKnob (s, dispMix, "Mix");    modTarget (dsp::modDispMix, m); s.view.setControl (6, m);
            setLed (dispOn);
            break;
        }
        case Section::envFollower:
            s.view.setControl (0, addKnob (s, envGain, "Gain"));
            s.view.setControl (1, addKnob (s, envAmt, "Amount", true));
            s.view.setControl (2, addKnob (s, attack, "Attack"));
            s.view.setControl (3, addKnob (s, hold, "Hold"));
            s.view.setControl (4, addKnob (s, release, "Release"));
            break;
        case Section::distortion:
        {
            s.view.setControl (0, addBox (s, type, true), "Type");
            s.view.setControl (1, addBox (s, order, true), "Order");
            auto& d = addKnob (s, drive, "Drive");      modTarget (dsp::modDrive, d);    s.view.setControl (2, d);
            driveKnob = static_cast<ga::Knob*> (&d);
            auto& bi = addKnob (s, bias, "Bias", true); modTarget (dsp::modBias, bi);    s.view.setControl (3, bi);
            auto& ph = addKnob (s, phat, "Phat");       modTarget (dsp::modPhat, ph);    s.view.setControl (4, ph);
            auto& t = addKnob (s, distTone, "Tone");    modTarget (dsp::modDistTone, t); s.view.setControl (5, t);
            auto& m = addKnob (s, distMix, "Mix");      modTarget (dsp::modDistMix, m);  s.view.setControl (6, m);
            setLed (distOn);
            break;
        }
        case Section::feedback:
        {
            s.view.setControl (0, addLamp (s, invert), "Invert");
            auto& a = addKnob (s, fbAmt, "Amount");  modTarget (dsp::modFbAmount, a); s.view.setControl (1, a);
            auto& t = addKnob (s, fbTime, "Time");   modTarget (dsp::modFbTime, t);   s.view.setControl (2, t);
            auto& to = addKnob (s, fbTone, "Tone");  modTarget (dsp::modFbTone, to);  s.view.setControl (3, to);
            auto& m = addKnob (s, fbMix, "Mix");     modTarget (dsp::modFbMix, m);    s.view.setControl (4, m);
            setLed (fbOn);
            break;
        }
    }

    if (s.id == Section::envFollower)
    {
        donutsShown = 0xffffffff;   // the donuts and the drop-down: updateDonuts() puts them in the header
        updateDonuts();
    }
    else
        s.view.setHeaderControls (headerControls);
    juce::ignoreUnused (oldControls, oldAttachments, oldLink, oldLed);   // gone from here on
}

//==============================================================================
void MainPanel::updateStrip()
{
    const int count = bandCount();
    if (count == shownBands)
        return;
    shownBands = count;
    std::vector<ga::BandCard*> shown;
    for (int i = 0; i < count; ++i)
        shown.push_back (&cards[(size_t) i].card);
    std::vector<ga::SplitHandle*> splits;
    if (count == 2) splits = { split.get() };
    if (count == 3) splits = { split1.get(), split2.get() };
    for (auto* h : { split.get(), split1.get(), split2.get() })
        h->setVisible (std::find (splits.begin(), splits.end(), h) != splits.end());
    strip.setBands (shown, splits);
    if (selected >= count)
        select (count - 1);
}

void MainPanel::updateCards()
{
    using namespace fp;
    using namespace pid::ch;
    const int count = bandCount();
    std::vector<float> edges { 20.0f };
    if (count == 2) edges.push_back (value (pid::split));
    if (count == 3) { edges.push_back (value (pid::split1)); edges.push_back (value (pid::split2)); }
    edges.push_back (20000.0f);

    for (int i = 0; i < count; ++i)
    {
        auto& c = cards[(size_t) i];
        const int b = i + 1;
        const float lo = edges[(size_t) i], hi = edges[(size_t) i + 1];
        auto inputs = [&] (int ch)
        {
            BandTrace::Inputs in;
            in.stages = value (channelId (b, ch, dispOn)) > 0.5f ? juce::roundToInt (value (channelId (b, ch, stages))) : 0;
            in.freq = value (channelId (b, ch, freq));
            in.pinch = value (channelId (b, ch, pinch)) * 0.01f;
            in.mix = value (channelId (b, ch, dispMix)) * 0.01f;
            in.low = lo;
            in.high = hi;
            return in;
        };
        const bool dispLinked = juce::roundToInt (value (bandId (b, pid::band::dispLink))) == linked;
        const auto first = inputs (0);
        const auto second = dispLinked ? first : inputs (1);
        const auto& main = c.traces[0].compute (first);
        if (second == first)
            c.card.setTraces (main);
        else
            c.card.setTraces (main, c.traces[1].compute (second));

        const int s0 = juce::roundToInt (value (channelId (b, 0, stages)));
        const int s1 = juce::roundToInt (value (channelId (b, 1, stages)));
        c.card.setBandName (bandName (count, i));
        const bool allLinked = std::all_of (std::begin (kSections), std::end (kSections),
                                            [&] (Section s) { return juce::roundToInt (value (bandId (b, linkName (s)))) == linked; });
        c.card.setInfo ((dispLinked || s0 == s1 ? juce::String (s0) : juce::String (s0) + " / " + juce::String (s1)) + " stages",
                        hertz (lo) + dash() + hertz (hi) + dot() + BandTrace::formatSpan (c.traces[0].spanSeconds()),
                        allLinked ? juce::String ("LINKED") : channelLetter (b, 0) + notEqual() + channelLetter (b, 1));
        c.card.setSelected (i == selected);
        c.card.setActive (value (bandId (b, pid::band::on)) > 0.5f);
        c.led.setLit (c.led.getToggleState());
    }
}

void MainPanel::updateSections()
{
    using namespace fp;
    const int b = band();
    const bool off = proc.isBypassedNow();
    const bool bandOn = value (bandId (b, pid::band::on)) > 0.5f;
    auto sectionOn = [&] (SectionView& s, const char* onName) { return value (channelId (b, s.channel, onName)) > 0.5f; };
    bool anyMod = false;
    for (const auto& t : kModTargets)
        anyMod = anyMod || value (modOnId (b, t)) > 0.5f;

    bandsSection.setDimmed (off, off);
    for (auto* v : { &disperse, &distortion, &feedback })   // the LEDs show the section's own switch
        if (v->led != nullptr)
            v->led->setLit (v->led->getToggleState());
    disperse.view.setDimmed (off, off || ! bandOn || ! sectionOn (disperse, pid::ch::dispOn));
    env.view.setDimmed (off, off || ! bandOn || ! anyMod);   // active whenever it has destinations
    distortion.view.setDimmed (off, off || ! bandOn || ! sectionOn (distortion, pid::ch::distOn));
    feedback.view.setDimmed (off, off || ! bandOn || ! sectionOn (feedback, pid::ch::fbOn));

    // Drive is the noise level for Noise
    if (driveKnob != nullptr)
        driveKnob->setLabel (juce::roundToInt (value (channelId (b, distortion.channel, pid::ch::type))) == 0 ? "Noise" : "Drive");

    // the arcs: from the value to value + depth on every assigned knob
    for (int t = 0; t < dsp::kNumModTargets; ++t)
        if (auto* k = modKnobs[(size_t) t])
            k->setModulation (value (modOnId (b, kModTargets[t])) > 0.5f, value (modId (b, kModTargets[t])) * 0.01f);
}

void MainPanel::updateDonuts()
{
    using namespace fp;
    const int b = band();
    juce::uint32 assigned = 0;
    std::vector<int> targets;   // fixed knob order: Disperse, Distortion, Feedback
    for (int t = 0; t < dsp::kNumModTargets; ++t)
        if (value (modOnId (b, kModTargets[t])) > 0.5f)
        {
            assigned |= 1u << t;
            targets.push_back (t);
        }
    if (assigned == donutsShown && donutsBand == b)
        return;
    donutsShown = assigned;
    donutsBand = b;

    // five fit; with six or more, four and a "+n" that pages through the rest
    const bool paged = targets.size() > 5;
    const int perPage = paged ? 4 : 5;
    const int pages = std::max (1, ((int) targets.size() + perPage - 1) / perPage);
    donutPage %= pages;

    auto old = std::move (donuts);
    donuts.clear();
    std::vector<juce::Component*> headerControls { &grip };
    for (int k = donutPage * perPage; k < std::min ((int) targets.size(), (donutPage + 1) * perPage); ++k)
    {
        const int t = targets[(size_t) k];
        auto donut = std::make_unique<ga::ModDonut> (*proc.apvts.getParameter (modId (b, kModTargets[t])));
        donut->setTooltip (juce::String (proc.apvts.getParameter (modId (b, kModTargets[t]))->getName (64)) + ". Drag up/down: depth. Double-click: remove.");
        donut->onRemove = [this, t] { put (modOnId (band(), kModTargets[t]), 0.0f); };
        donut->onHover = [this, t] (bool over)
        {
            if (auto* knob = modKnobs[(size_t) t])
                knob->setModHighlight (over ? ga::Knob::ModHighlight::target : ga::Knob::ModHighlight::none);
        };
        headerControls.push_back (donut.get());
        donuts.push_back (std::move (donut));
    }
    if (paged)
    {
        more.setButtonText ("+" + juce::String ((int) targets.size() - perPage));
        more.setTooltip ("Show the other destinations");
        headerControls.push_back (&more);
    }
    more.setVisible (paged);
    if (env.link != nullptr)
        headerControls.push_back (env.link.get());
    env.view.setHeaderControls (headerControls);
    juce::ignoreUnused (old);
}

void MainPanel::MoreButton::paintButton (juce::Graphics& g, bool isOver, bool)
{
    const auto& t = ga::themeOf (*this);
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    ga::ui::drawRecess (g, t, r);
    if (isOver)
        ga::ui::drawHoverWash (g, t, r);
    g.setFont (t.font (t.type.boxText));
    g.setColour (t.colours.boxText);
    g.drawText (getButtonText(), r, juce::Justification::centred, false);
}

//==============================================================================
void MainPanel::timerCallback()
{
    header.presetBar.refresh();
    updateStrip();

    // rebuild a section whose band, shown channel or channel names changed
    for (auto* s : { &disperse, &env, &distortion, &feedback })
        if (s->band != band() || s->channel != shownChannel (s->id) || s->stereo != juce::roundToInt (value (fp::bandId (band(), pid::band::stereo))))
            rebuild (*s);

    updateDonuts();
    updateCards();
    updateSections();
}
