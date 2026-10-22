// The DSP test tool. Only built with -DFPH_RENDER=ON, in its own build folder (see
// tools/check_dsp.sh), never into the plug-ins.
//
//   FasePhuckerRender check [dir]   the measured checks, then every render compared with the
//                                   references in dir (tests/reference/renders); any difference fails
//   FasePhuckerRender write [dir]   write the references (only when the sound changes on purpose)
//   FasePhuckerRender listen <dir>  longer, musical renders to listen to (32-bit float WAV)
//
// The checks drive fp::dsp::Engine directly (exact settings, loudness match off) and measure it;
// the renders run fixed, generated test audio through the whole plug-in.

#include "../Source/PluginProcessor.h"
#include "../Source/ParamIDs.h"

namespace
{
    using namespace juce;
    using fp::dsp::Engine;
    using fp::dsp::Params;

    constexpr double kRate = 48000.0;

    // the same numbers on every platform and compiler
    struct Rng
    {
        uint32 s;
        explicit Rng (uint32 seed) : s (seed != 0 ? seed : 1) {}
        uint32 next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float bipolar() { return (float) (next() >> 8) * (2.0f / 16777216.0f) - 1.0f; }
    };

    //==========================================================================
    // test audio: 0-0.3 s silence | noise bursts | kicks | a sine sweep | silence for the tails
    AudioBuffer<float> testSignal (double seconds = 4.0)
    {
        const int n = (int) (seconds * kRate);
        AudioBuffer<float> a (2, n);
        a.clear();
        auto* l = a.getWritePointer (0);
        auto* r = a.getWritePointer (1);
        auto at = [] (double t) { return (int) (t * kRate); };
        Rng rng (4711);

        for (double start : { 0.3, 0.6 })   // noise bursts, partly decorrelated
        {
            float lpA = 0.0f, lpB = 0.0f;
            for (int i = 0; i < at (0.15); ++i)
            {
                lpA += 0.3f * (rng.bipolar() - lpA);
                lpB += 0.3f * (rng.bipolar() - lpB);
                const float env = std::sin (MathConstants<float>::pi * (float) i / (float) at (0.15));
                l[at (start) + i] = 0.5f * env * lpA;
                r[at (start) + i] = 0.5f * env * (0.7f * lpA + 0.3f * lpB);
            }
        }
        for (double start : { 1.0, 1.3 })   // kicks: a falling sine with a click
        {
            double phase = 0.0;
            for (int i = 0; i < at (0.25); ++i)
            {
                const double t = (double) i / kRate;
                phase += MathConstants<double>::twoPi * (45.0 + 80.0 * std::exp (-t / 0.03)) / kRate;
                float v = 0.8f * (float) (std::exp (-t / 0.08) * std::sin (phase));
                if (t < 0.002) v += 0.3f * rng.bipolar();
                l[at (start) + i] += v;
                r[at (start) + i] += v;
            }
        }
        {   // a log sine sweep, 30 Hz to 15 kHz
            double phase = 0.0;
            const int s0 = at (1.7), len = at (1.0);
            for (int i = 0; i < len; ++i)
            {
                const double f = 30.0 * std::pow (500.0, (double) i / len);
                phase += MathConstants<double>::twoPi * f / kRate;
                const float v = 0.4f * (float) std::sin (phase);
                l[s0 + i] = v;
                r[s0 + i] = 0.8f * v;
            }
        }
        return a;
    }

    // to listen to: a little drum and pluck loop
    AudioBuffer<float> musicLoop (double seconds = 8.0)
    {
        const int n = (int) (seconds * kRate);
        AudioBuffer<float> a (2, n);
        a.clear();
        auto* l = a.getWritePointer (0);
        auto* r = a.getWritePointer (1);
        Rng rng (99);
        const double beat = 60.0 / 120.0;
        auto add = [&] (double start, double len, auto&& voice)
        {
            const int s0 = (int) (start * kRate);
            for (int i = 0; i < (int) (len * kRate) && s0 + i < n; ++i)
            {
                float vl = 0.0f, vr = 0.0f;
                voice ((double) i / kRate, vl, vr);
                l[s0 + i] += vl; r[s0 + i] += vr;
            }
        };
        for (int step = 0; step < (int) (seconds / beat * 2.0); ++step)
        {
            const double t = step * beat * 0.5;
            if (step % 4 == 0)
                add (t, 0.35, [phase = 0.0] (double x, float& vl, float& vr) mutable
                {
                    phase += MathConstants<double>::twoPi * (48.0 + 90.0 * std::exp (-x / 0.025)) / kRate;
                    vl = vr = 0.75f * (float) (std::exp (-x / 0.09) * std::sin (phase));
                });
            if (step % 4 == 2)
                add (t, 0.2, [&rng, lp = 0.0f] (double x, float& vl, float& vr) mutable
                {
                    lp += 0.5f * (rng.bipolar() - lp);
                    const float e = (float) std::exp (-x / 0.045);
                    vl = 0.45f * e * lp; vr = 0.45f * e * rng.bipolar() * 0.8f;
                });
            add (t, 0.05, [&rng] (double x, float& vl, float& vr)
            {
                const float e = 0.12f * (float) std::exp (-x / 0.01) * rng.bipolar();
                vl = e; vr = -e;
            });
            if (step % 8 == 3 || step % 8 == 6)
            {
                const double f = step % 16 < 8 ? 220.0 : 196.0;
                add (t, 0.6, [f, ph = 0.0] (double x, float& vl, float& vr) mutable
                {
                    ph += MathConstants<double>::twoPi * f / kRate;
                    const float v = 0.25f * (float) (std::exp (-x / 0.18) * (std::sin (ph) + 0.4 * std::sin (2.0 * ph) + 0.2 * std::sin (3.01 * ph)));
                    vl = v; vr = 0.85f * v;
                });
            }
        }
        return a;
    }

    //==========================================================================
    // the engine with every section off (a clean signal path), loudness match off
    Params clean (int bands)
    {
        Params p;
        p.bands = bands;
        p.output.loudnessMatch = false;
        p.output.phaseProtect = false;
        for (auto& b : p.band)
            for (auto& c : b.ch)
            {
                c.dispOn = false;
                c.distOn = false;
                c.fbOn = false;
                c.fbAmount = 0.0f;
            }
        return p;
    }

    void run (Engine& e, AudioBuffer<float>& audio, int block = 512)
    {
        for (int done = 0; done < audio.getNumSamples(); done += block)
        {
            const int n = std::min (block, audio.getNumSamples() - done);
            float* io[2] = { audio.getWritePointer (0) + done, audio.getWritePointer (1) + done };
            e.process (io, n);
        }
    }

    // the left output's response to an impulse in both channels
    std::vector<float> impulseResponse (const Params& p, int length = 1 << 16)
    {
        Engine e;
        e.setParams (p);
        e.prepare (kRate, 2, 1);
        AudioBuffer<float> a (2, length);
        a.clear();
        a.setSample (0, 0, 1.0f);
        a.setSample (1, 0, 1.0f);
        run (e, a);
        return { a.getReadPointer (0), a.getReadPointer (0) + length };
    }

    // magnitude in dB at the FFT bins between lo and hi Hz: the largest deviation from 0 dB
    float worstDeviationDb (const std::vector<float>& ir, double lo, double hi)
    {
        const int order = (int) std::log2 ((double) ir.size());
        dsp::FFT fft (order);
        std::vector<float> data (ir.size() * 2, 0.0f);
        std::copy (ir.begin(), ir.end(), data.begin());
        fft.performFrequencyOnlyForwardTransform (data.data());
        float worst = 0.0f;
        for (size_t k = 1; k < ir.size() / 2; ++k)
        {
            const double f = (double) k * kRate / (double) ir.size();
            if (f >= lo && f <= hi)
                worst = std::max (worst, std::abs (Decibels::gainToDecibels (data[k], -200.0f)));
        }
        return worst;
    }

    float largestStep (const float* x, int n)   // the largest jump between neighbouring samples
    {
        float worst = 0.0f;
        for (int i = 1; i < n; ++i)
            worst = std::max (worst, std::abs (x[i] - x[i - 1]));
        return worst;
    }

    //==========================================================================
    int failures = 0;
    void expect (bool ok, const String& what)
    {
        std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
        failures += ok ? 0 : 1;
    }

    // ---- crossover and bands
    void checkBands()
    {
        for (int bands : { 1, 2, 3 })
        {
            const float dev = worstDeviationDb (impulseResponse (clean (bands)), 20.0, 20000.0);
            expect (dev < 0.01f, String (bands) + " band(s), sections off: flat within " + String (dev, 4) + " dB (20 Hz - 20 kHz)");
        }

        {   // a split moving during a sine: no clicks (no step larger than the sine's own)
            Params p = clean (3);
            Engine e;
            e.setParams (p);
            e.prepare (kRate, 2, 1);
            AudioBuffer<float> a (2, (int) kRate);
            for (int i = 0; i < a.getNumSamples(); ++i)
                a.setSample (0, i, 0.5f * (float) std::sin (MathConstants<double>::twoPi * 300.0 * i / kRate)), a.setSample (1, i, a.getSample (0, i));
            const float sineStep = largestStep (a.getReadPointer (0), a.getNumSamples());
            for (int done = 0; done < a.getNumSamples(); done += 256)
            {
                p.split1 = 100.0f * std::pow (20.0f, (float) done / (float) a.getNumSamples());   // 100 Hz -> 2 kHz in 1 s
                e.setParams (p);
                float* io[2] = { a.getWritePointer (0) + done, a.getWritePointer (1) + done };
                e.process (io, 256);
            }
            const float step = largestStep (a.getReadPointer (0) + 4800, a.getNumSamples() - 4800);
            expect (step < sineStep * 1.5f, "moving split: largest step " + String (step, 4) + " (the sine's own: " + String (sineStep, 4) + ")");
        }

        {   // band off and solo: click-free ramps, and solo leaves only that band
            Params p = clean (3);
            p.band[1].solo = true;
            const auto ir = impulseResponse (p);
            const float dev = worstDeviationDb (ir, 400.0, 1200.0);   // well inside the mid band (180 Hz - 2.4 kHz)
            const float low = worstDeviationDb (ir, 20.0, 40.0);
            expect (dev < 1.0f && low > 30.0f, "solo mid band: mid passes (" + String (dev, 2) + " dB), lows are " + String (low, 1) + " dB down");
        }
    }

    //==========================================================================
    // renders through the whole plug-in
    struct RenderDef
    {
        const char* name;
        std::function<void (FasePhuckerAudioProcessor&)> setup;
    };

    void set (FasePhuckerAudioProcessor& p, const String& id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    const std::vector<RenderDef>& renders()
    {
        using namespace fp;
        static const std::vector<RenderDef> all {
            { "init", [] (FasePhuckerAudioProcessor&) {} },
            { "3bands_ms_lr", [] (FasePhuckerAudioProcessor& p)
                {
                    set (p, pid::bands, 2.0f);
                    set (p, bandId (3, pid::band::stereo), 0.0f);
                    set (p, bandId (2, pid::band::on), 0.0f);
                } },
        };
        return all;
    }

    AudioBuffer<float> render (const RenderDef& def, const AudioBuffer<float>& input)
    {
        FasePhuckerAudioProcessor proc;
        def.setup (proc);
        proc.setPlayConfigDetails (2, 2, kRate, 512);
        proc.prepareToPlay (kRate, 512);
        AudioBuffer<float> audio (input);
        MidiBuffer midi;
        for (int done = 0; done < audio.getNumSamples(); done += 512)
        {
            const int n = std::min (512, audio.getNumSamples() - done);
            AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, done, n);
            proc.processBlock (block, midi);
        }
        proc.releaseResources();
        return audio;
    }

    // 32-bit float WAV, written and read here so the test build needs no extra JUCE module
    void writeWav (const File& file, const AudioBuffer<float>& audio)
    {
        MemoryOutputStream m;
        const int n = audio.getNumSamples(), ch = audio.getNumChannels();
        const int dataBytes = n * ch * 4;
        m.write ("RIFF", 4); m.writeInt (36 + dataBytes); m.write ("WAVE", 4);
        m.write ("fmt ", 4); m.writeInt (16); m.writeShort (3); m.writeShort ((short) ch);   // 3 = IEEE float
        m.writeInt ((int) kRate); m.writeInt ((int) kRate * ch * 4); m.writeShort ((short) (ch * 4)); m.writeShort (32);
        m.write ("data", 4); m.writeInt (dataBytes);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c)
                m.writeFloat (audio.getSample (c, i));
        file.getParentDirectory().createDirectory();
        file.replaceWithData (m.getData(), m.getDataSize());
    }

    AudioBuffer<float> readWav (const File& file)
    {
        MemoryBlock data;
        if (! file.loadFileAsData (data) || data.getSize() < 44)
            return {};
        MemoryInputStream in (data, false);
        char id[5] {};
        in.read (id, 4); in.readInt(); in.read (id, 4);
        int channels = 0;
        while (! in.isExhausted())
        {
            in.read (id, 4);
            const int size = in.readInt();
            const auto next = in.getPosition() + size;
            if (String (id) == "fmt ")
            {
                in.readShort();
                channels = in.readShort();
            }
            else if (String (id) == "data" && channels > 0)
            {
                AudioBuffer<float> a (channels, size / (channels * 4));
                for (int i = 0; i < a.getNumSamples(); ++i)
                    for (int c = 0; c < channels; ++c)
                        a.setSample (c, i, in.readFloat());
                return a;
            }
            in.setPosition (next);
        }
        return {};
    }

    int runRenders (const File& dir, bool write)
    {
        const auto input = testSignal();
        dir.createDirectory();
        for (const auto& def : renders())
        {
            const auto out = render (def, input);
            const auto file = dir.getChildFile (String (def.name) + ".wav");
            if (write)
            {
                writeWav (file, out);
                std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
                continue;
            }
            const auto ref = readWav (file);
            float worst = 0.0f;
            const bool sameSize = ref.getNumSamples() == out.getNumSamples() && ref.getNumChannels() == 2;
            if (sameSize)
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < out.getNumSamples(); ++i)
                        worst = std::max (worst, std::abs (out.getSample (c, i) - ref.getSample (c, i)));
            const bool ok = sameSize && worst == 0.0f;
            expect (ok, String ("render ") + def.name + (sameSize ? (worst == 0.0f ? ": identical" : ": differs by " + String (Decibels::gainToDecibels (worst), 1) + " dB")
                                                                  : ": reference missing"));
        }
        return failures;
    }

    void listen (const File& dir)
    {
        dir.createDirectory();
        const auto loop = musicLoop();
        writeWav (dir.getChildFile ("00_dry.wav"), loop);
        int i = 1;
        for (const auto& def : renders())
        {
            const auto file = dir.getChildFile (String (i++).paddedLeft ('0', 2) + "_" + def.name + ".wav");
            writeWav (file, render (def, loop));
            std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
        }
    }
}

// called from tools/RenderMain.cpp
int fphRender (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const String mode (argc > 1 ? argv[1] : "check");
    const File dir (File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "tests/reference/renders"));
    if (mode == "listen")
    {
        listen (dir);
        return 0;
    }
    if (mode == "write")
        return runRenders (dir, true);

    checkBands();
    runRenders (dir, false);
    std::printf ("\n%s\n", failures == 0 ? "All DSP checks passed." : "DSP checks failed.");
    return failures == 0 ? 0 : 1;
}
