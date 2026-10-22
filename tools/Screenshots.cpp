// Renders the plug-in's own interface into PNGs, at 1x and 2x. Only built with -DFPH_SCREENSHOTS=ON,
// in its own build folder, never into the plug-ins.
//
//   FasePhuckerScreenshots <dir>   writes <dir>/main_100.png and <dir>/main_200.png

#include "../Source/PluginProcessor.h"
#include "../Source/ParamIDs.h"

namespace
{
    juce::Image render (float scale)
    {
        FasePhuckerAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        proc.apvts.state.setProperty (prop::uiScale, 1.0f, nullptr);

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        for (int i = 0; i < 4; ++i)   // let the editor's timers run once
        {
            juce::Thread::sleep (20);
            juce::Timer::callPendingTimersSynchronously();
        }
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        editor.reset();
        proc.releaseResources();
        return image;
    }

    void save (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("wrote %s (%d x %d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    }
}

int fphRenderScreenshots (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "screenshots"));
    dir.createDirectory();
    save (render (1.0f), dir.getChildFile ("main_100.png"));
    save (render (2.0f), dir.getChildFile ("main_200.png"));
    return 0;
}
