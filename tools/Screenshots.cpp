// Renders the plug-in's own interface into PNGs. Only built with -DFPH_SCREENSHOTS=ON, in its own
// build folder (see tools/make_screenshots.sh), never into the plug-ins.
//
//   FasePhuckerScreenshots <dir>            write every state into <dir>
//   FasePhuckerScreenshots check <dir>      compare with them; any differing pixel fails, the new
//                                           images go to build-tools/screenshot-diff/
//
// States: init (the defaults) and example (close to the mockup: three bands, the mid one selected).

#include "../Source/PluginProcessor.h"
#include "../Source/ParamIDs.h"

namespace
{
    using namespace juce;

    void set (FasePhuckerAudioProcessor& p, const String& id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // the mockup's mid band: Disperse and Distortion unlinked (M shown), two destinations
    void example (FasePhuckerAudioProcessor& p)
    {
        using namespace fp;
        using namespace pid::ch;
        set (p, pid::bands, 2.0f);
        auto both = [&p] (int b, const char* name, float v) { set (p, channelId (b, 0, name), v); set (p, channelId (b, 1, name), v); };
        // low and high, as in the mockup
        both (1, dispOn, 1.0f); both (1, stages, 64.0f); both (1, freq, 90.0f); both (1, pinch, 40.0f); both (1, spread, 20.0f);
        both (3, dispOn, 1.0f); both (3, stages, 16.0f); both (3, freq, 6000.0f); both (3, pinch, 30.0f); both (3, spread, 70.0f);
        set (p, bandId (3, pid::band::stereo), 0.0f);
        // mid
        for (auto s : { Section::disperse, Section::distortion, Section::feedback })
            p.setSectionLink (2, s, firstChannel);
        set (p, channelId (2, 0, dispOn), 1.0f); set (p, channelId (2, 0, stages), 256.0f); set (p, channelId (2, 0, mode), 1.0f);
        set (p, channelId (2, 0, jitter), 1.0f); set (p, channelId (2, 0, freq), 900.0f); set (p, channelId (2, 0, pinch), 65.0f);
        set (p, channelId (2, 0, spread), 45.0f);
        set (p, channelId (2, 1, dispOn), 1.0f); set (p, channelId (2, 1, stages), 32.0f); set (p, channelId (2, 1, freq), 1400.0f);
        set (p, channelId (2, 1, pinch), 30.0f); set (p, channelId (2, 1, spread), 80.0f);
        set (p, channelId (2, 0, envGain), 6.0f); set (p, channelId (2, 0, envAmt), -40.0f); set (p, channelId (2, 0, attack), 1.0f);
        set (p, channelId (2, 0, release), 250.0f);
        set (p, channelId (2, 0, distOn), 1.0f); set (p, channelId (2, 0, type), 3.0f); set (p, channelId (2, 0, order), 1.0f);
        set (p, channelId (2, 0, drive), 45.0f); set (p, channelId (2, 0, bias), 20.0f); set (p, channelId (2, 0, phat), 15.0f);
        set (p, channelId (2, 0, fbOn), 1.0f); set (p, channelId (2, 0, fbAmt), 35.0f); set (p, channelId (2, 0, fbTone), 60.0f);
        set (p, modOnId (2, kModTargets[0]), 1.0f); set (p, modId (2, kModTargets[0]), 60.0f);   // Freq
        set (p, modOnId (2, kModTargets[9]), 1.0f); set (p, modId (2, kModTargets[9]), 40.0f);   // Feedback Amount
        p.apvts.state.setProperty (prop::selectedBand, 1, nullptr);
    }

    struct Shot
    {
        String name;
        std::function<void (FasePhuckerAudioProcessor&)> setup;
        float scale;
    };

    const std::vector<Shot>& shots()
    {
        static const std::vector<Shot> all {
            { "init_100", [] (FasePhuckerAudioProcessor&) {}, 1.0f },
            { "example_100", example, 1.0f },
            { "example_200", example, 2.0f },
        };
        return all;
    }

    Image render (const Shot& shot)
    {
        FasePhuckerAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        shot.setup (proc);
        std::unique_ptr<AudioProcessorEditor> editor (proc.createEditor());
        for (int i = 0; i < 4; ++i)   // let the editor's timers apply the state
        {
            Thread::sleep (40);
            Timer::callPendingTimersSynchronously();
        }
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, shot.scale);
        editor.reset();
        proc.releaseResources();
        return image;
    }

    MemoryBlock png (const Image& image)
    {
        MemoryOutputStream out;
        PNGImageFormat().writeImageToStream (image, out);
        return out.getMemoryBlock();
    }
}

int fphRenderScreenshots (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const bool check = argc > 1 && String (argv[1]) == "check";
    const File dir (File::getCurrentWorkingDirectory().getChildFile (argc > (check ? 2 : 1) ? argv[check ? 2 : 1] : "tests/reference/screenshots"));
    const File diffDir (File::getCurrentWorkingDirectory().getChildFile ("build-tools/screenshot-diff"));
    dir.createDirectory();
    int failures = 0;
    for (const auto& shot : shots())
    {
        const auto data = png (render (shot));
        const auto file = dir.getChildFile (shot.name + ".png");
        if (! check)
        {
            file.replaceWithData (data.getData(), data.getSize());
            std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
            continue;
        }
        // compare after the same PNG round trip the reference went through
        const auto now = ImageFileFormat::loadFrom (data.getData(), data.getSize());
        const auto ref = ImageFileFormat::loadFrom (file);
        bool same = ref.isValid() && ref.getWidth() == now.getWidth() && ref.getHeight() == now.getHeight();
        for (int y = 0; same && y < now.getHeight(); ++y)
            for (int x = 0; same && x < now.getWidth(); ++x)
                same = ref.getPixelAt (x, y) == now.getPixelAt (x, y);
        std::printf ("%s  %s\n", same ? "ok  " : "FAIL", shot.name.toRawUTF8());
        if (! same)
        {
            diffDir.createDirectory();
            diffDir.getChildFile (shot.name + "_new.png").replaceWithData (data.getData(), data.getSize());
            ++failures;
        }
    }
    if (check)
        std::printf ("\n%s\n", failures == 0 ? "All screenshots match." : "Screenshots differ: see build-tools/screenshot-diff/");
    return failures == 0 ? 0 : 1;
}
