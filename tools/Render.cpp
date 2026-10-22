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
#include <complex>

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

    // ---- Disperse
    Params disperse (int stages, bool unhinged, float freq = 1000.0f, float pinch = 0.5f, float spread = 0.0f)
    {
        Params p = clean (1);
        for (auto& c : p.band[0].ch)
        {
            c.dispOn = true; c.stages = stages; c.unhinged = unhinged; c.freq = freq; c.pinch = pinch; c.spread = spread; c.dispMix = 1.0f;
        }
        return p;
    }

    // group delay (seconds) at f, from the phase difference of the neighbouring FFT bins
    double groupDelay (const std::vector<float>& ir, double f)
    {
        const int order = (int) std::log2 ((double) ir.size());
        dsp::FFT fft (order);
        std::vector<float> data (ir.size() * 2, 0.0f);
        std::copy (ir.begin(), ir.end(), data.begin());
        fft.performRealOnlyForwardTransform (data.data());
        const double df = kRate / (double) ir.size();
        const auto k = (size_t) std::lround (f / df);
        const std::complex<double> a (data[2 * (k - 1)], data[2 * (k - 1) + 1]), b (data[2 * (k + 1)], data[2 * (k + 1) + 1]);
        return -std::arg (b * std::conj (a)) / (2.0 * MathConstants<double>::twoPi * df);
    }

    // a 300 Hz sine through the engine while change() alters the settings at 0.5 s: largest step
    float stepDuring (Params p, const std::function<void (Params&)>& change)
    {
        Engine e;
        e.setParams (p);
        e.prepare (kRate, 2, 1);
        AudioBuffer<float> a (2, (int) kRate);
        for (int i = 0; i < a.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c)
                a.setSample (c, i, 0.5f * (float) std::sin (MathConstants<double>::twoPi * 300.0 * i / kRate));
        for (int done = 0; done < a.getNumSamples(); done += 256)
        {
            if (done == 24064) { change (p); e.setParams (p); }
            float* io[2] = { a.getWritePointer (0) + done, a.getWritePointer (1) + done };
            e.process (io, 256);
        }
        return largestStep (a.getReadPointer (0) + 9600, a.getNumSamples() - 9600);   // after the chain has filled
    }

    void checkDisperse()
    {
        for (bool unhinged : { false, true })
        {
            const String mode = unhinged ? "Unhinged" : "Behaved";
            const auto ir = impulseResponse (disperse (256, unhinged), 1 << 17);
            const float dev = worstDeviationDb (ir, 20.0, 20000.0);
            expect (dev < 0.05f, mode + ", 256 stages: flat within " + String (dev, 4) + " dB");
            if (! unhinged)
            {
                const double expected = 256.0 * 2.0 * 2.5 / (MathConstants<double>::pi * 1000.0);
                const double measured = groupDelay (ir, 1000.0);
                expect (std::abs (measured / expected - 1.0) < 0.03,
                        mode + ": group delay at Freq " + String (measured * 1000.0, 1) + " ms (N 2Q / (pi Freq): " + String (expected * 1000.0, 1) + " ms)");
            }
        }

        {   // Mix 0: untouched
            Params p = disperse (256, false);
            p.band[0].ch[0].dispMix = p.band[0].ch[1].dispMix = 0.0f;
            const auto ir = impulseResponse (p, 1 << 12);
            expect (std::abs (ir[0] - 1.0f) < 1.0e-6f && std::all_of (ir.begin() + 1, ir.end(), [] (float v) { return std::abs (v) < 1.0e-6f; }),
                    "Disperse Mix 0: the signal passes untouched");
        }

        const float sineStep = 0.5f * (float) (MathConstants<double>::twoPi * 300.0 / kRate);
        for (bool unhinged : { false, true })
        {
            const String mode = unhinged ? "Unhinged" : "Behaved";
            const float a = stepDuring (disperse (64, unhinged, 400.0f, 0.3f, 0.3f), [] (Params& p) { for (auto& c : p.band[0].ch) c.stages = 256; });
            const float b = stepDuring (disperse (256, unhinged, 400.0f, 0.3f, 0.3f), [] (Params& p) { for (auto& c : p.band[0].ch) c.stages = 16; });
            expect (std::max (a, b) < sineStep * 2.0f, mode + ": Stages crossfade, largest step 64 -> 256: " + String (a, 4) + ", 256 -> 16: " + String (b, 4)
                                                       + " (the sine's own: " + String (sineStep, 4) + ")");
        }
        {
            const float m = stepDuring (disperse (128, false, 400.0f, 0.3f, 0.3f), [] (Params& p) { for (auto& c : p.band[0].ch) c.unhinged = true; });
            expect (m < sineStep * 2.0f, "Behaved -> Unhinged crossfade, largest step " + String (m, 4));
        }

        // CPU: 3 bands x 2 channels x 1000 stages, the worst case
        for (bool unhinged : { false, true })
        {
            Params p = clean (3);
            for (auto& b : p.band)
                for (auto& c : b.ch)
                {
                    c.dispOn = true; c.stages = 1000; c.unhinged = unhinged; c.jitter = true; c.freq = 800.0f; c.spread = 0.5f;
                }
            Engine e;
            e.setParams (p);
            e.prepare (kRate, 2, 1);
            AudioBuffer<float> a (2, (int) kRate * 2);
            Rng rng (5);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < a.getNumSamples(); ++i)
                    a.setSample (c, i, 0.1f * rng.bipolar());
            const auto t0 = Time::getMillisecondCounterHiRes();
            run (e, a);
            const double load = (Time::getMillisecondCounterHiRes() - t0) / 2000.0;
            expect (load < 1.0, String (unhinged ? "Unhinged" : "Behaved") + ", jitter, 3 bands x 2 channels x 1000 stages: "
                                    + String (load * 100.0, 0) + " % of one core in real time");
        }
    }

    // ---- Distortion
    std::vector<float> impulseResponseAt (const Params& p, int factor, int length, int* reportedLatency = nullptr, float amplitude = 1.0f)
    {
        Engine e;
        e.setParams (p);
        e.prepare (kRate, 2, factor);
        if (reportedLatency != nullptr)
            *reportedLatency = e.latency();
        constexpr int settle = 9600;   // 200 ms of silence first: every glide has arrived
        AudioBuffer<float> a (2, settle + length);
        a.clear();
        a.setSample (0, settle, amplitude);
        a.setSample (1, settle, amplitude);
        run (e, a);
        return { a.getReadPointer (0) + settle, a.getReadPointer (0) + settle + length };
    }

    // the share of the output's power that is not at a multiple of f (aliasing), in dB
    float aliasingDb (const Params& p, int factor, double f)
    {
        Engine e;
        e.setParams (p);
        e.prepare (kRate, 2, factor);
        const int n = 1 << 15;
        AudioBuffer<float> a (2, n + 8192);
        for (int i = 0; i < a.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c)
                a.setSample (c, i, 0.5f * (float) std::sin (MathConstants<double>::twoPi * f * i / kRate));
        run (e, a);
        dsp::FFT fft (15);
        std::vector<float> data ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n; ++i)   // after it settled, Hann window
            data[(size_t) i] = a.getSample (0, 8192 + i) * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) n));
        fft.performFrequencyOnlyForwardTransform (data.data());
        double harmonic = 0.0, other = 0.0;
        for (int k = 1; k < n / 2; ++k)
        {
            const double fk = k * kRate / n, ratio = fk / f;
            const double power = (double) data[(size_t) k] * data[(size_t) k];
            (std::abs (ratio - std::round (ratio)) * f < 4.0 * kRate / n ? harmonic : other) += power;
        }
        return (float) (10.0 * std::log10 (other / (harmonic + other) + 1.0e-30));
    }

    Params distorted (fp::dsp::DistType type, float drive)
    {
        Params p = clean (1);
        for (auto& c : p.band[0].ch)
        {
            c.distOn = true; c.type = type; c.drive = drive; c.distMix = 1.0f;
        }
        return p;
    }

    void checkDistortion()
    {
        using fp::dsp::DistType;

        // the latency it reports is the delay it has; the dry paths line up with it
        for (bool linear : { true, false })
        {
            Params p = clean (3);
            p.osLinearPhase = linear;
            int reported = 0;
            const auto ir = impulseResponseAt (p, 2, 1 << 14, &reported);
            const auto peak = (int) (std::max_element (ir.begin(), ir.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) - ir.begin());
            const String filter = linear ? "linear phase" : "low latency";
            if (linear)
                expect (peak == reported, "2x " + filter + ": reports " + String (reported) + " samples latency, the impulse comes out at " + String (peak));
            const float flat = worstDeviationDb (ir, 20.0, 18000.0);
            expect (flat < 0.3f, "2x " + filter + ", Distortion off: flat within " + String (flat, 3) + " dB (20 Hz - 18 kHz)");

            // the dry paths, delayed like the wet: a band that is off, and (with one band, where the wet
            // path is just the delayed input; three bands' crossovers are an all-pass) Dry/Wet at 50 %
            Params off = p;
            off.band[1].on = false;
            Params half = clean (1);
            half.osLinearPhase = linear;
            half.output.dryWet = 0.5f;
            const float a = worstDeviationDb (impulseResponseAt (off, 2, 1 << 14), 20.0, 18000.0);
            const float b = worstDeviationDb (impulseResponseAt (half, 2, 1 << 14), 20.0, 18000.0);
            if (linear)
                expect (std::max (a, b) < 0.3f, "2x " + filter + ", a band off: flat within " + String (a, 3) + " dB; Dry/Wet 50 %: within " + String (b, 3) + " dB");
        }

        // oversampling keeps aliasing down
        {
            const auto p = distorted (DistType::clip, 0.8f);
            const float at1 = aliasingDb (p, 1, 7000.0), at8 = aliasingDb (p, 8, 7000.0);
            expect (at1 - at8 > 20.0f, "Clip, 7 kHz: aliasing " + String (at1, 1) + " dB without oversampling, " + String (at8, 1) + " dB at 8x");
        }

        // every type stays finite and bounded, at the extremes of every knob
        for (int t = 0; t < 6; ++t)
        {
            float worst = 0.0f;
            bool finite = true;
            for (float bias : { -1.0f, 1.0f })
                for (float tone : { 0.0f, 1.0f })
                {
                    Params p = distorted ((DistType) t, 1.0f);
                    for (auto& c : p.band[0].ch) { c.bias = bias; c.phat = 1.0f; c.distTone = tone; }
                    Engine e;
                    e.setParams (p);
                    e.prepare (kRate, 2, 2);
                    auto a = testSignal();
                    run (e, a);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < a.getNumSamples(); ++i)
                        {
                            finite = finite && std::isfinite (a.getSample (c, i));
                            worst = std::max (worst, std::abs (a.getSample (c, i)));
                        }
                }
            const char* names[] = { "Noise", "Saturate", "Clip", "Fold", "Rectify", "Crush" };
            expect (finite && worst < 8.0f, String (names[t]) + " at Drive 100, Bias -100 / +100, Phat 100, Tone 0 / 100: peak " + String (worst, 2));
        }

        // Pre <-> Post: fades out and in, no click
        {
            Params p = distorted (DistType::saturate, 0.3f);
            for (auto& c : p.band[0].ch) { c.dispOn = true; c.stages = 32; c.freq = 400.0f; }
            const float m = stepDuring (p, [] (Params& q) { for (auto& c : q.band[0].ch) c.post = true; });
            Params postAll = p;
            for (auto& c : postAll.band[0].ch) c.post = true;
            const float still = std::max (stepDuring (p, [] (Params&) {}), stepDuring (postAll, [] (Params&) {}));
            expect (m < still * 1.25f, "Distortion Pre -> Post, largest step " + String (m, 4) + " (Pre or Post without a switch: " + String (still, 4) + ")");
        }
    }

    // ---- Feedback
    Params looped (float amount, float timeMs, float tone, bool invert, bool withDisperse)
    {
        Params p = clean (1);
        for (auto& c : p.band[0].ch)
        {
            c.fbOn = true; c.fbAmount = amount; c.fbTimeMs = timeMs; c.fbTone = tone; c.invert = invert; c.fbMix = 1.0f;
            c.dispOn = withDisperse; c.stages = 256; c.freq = 400.0f; c.spread = 0.4f;
        }
        return p;
    }

    void checkFeedback()
    {
        // an echo: Disperse off, 50 %, 1 ms (48 samples); Invert flips it
        for (bool invert : { false, true })
        {
            const auto ir = impulseResponseAt (looped (0.5f, 1.0f, 0.5f, invert, false), 1, 256, nullptr, 0.01f);
            const float first = ir[48] / 0.01f, second = ir[96] / 0.01f;
            const float want = invert ? -0.5f : 0.5f;
            expect (std::abs (first - want) < 0.02f && std::abs (second - 0.25f) < 0.02f,
                    String ("echo after 48 samples at ") + String (first, 3) + ", then " + String (second, 3) + (invert ? " (Invert)" : ""));
        }

        // Feedback off: the same as Amount 0
        {
            Params off = looped (0.0f, 5.0f, 0.5f, false, true);
            for (auto& c : off.band[0].ch) { c.fbOn = false; c.fbMix = 0.3f; }   // Mix does nothing while off
            const auto a = impulseResponseAt (off, 1, 1 << 14), b = impulseResponseAt (looped (0.0f, 5.0f, 0.5f, false, true), 1, 1 << 14);
            float worst = 0.0f;
            for (size_t i = 0; i < a.size(); ++i) worst = std::max (worst, std::abs (a[i] - b[i]));
            expect (worst < 1.0e-6f, "Feedback off (Mix 30 %) = Amount 0: largest difference " + String (worst));
        }

        // stable and decaying at 95 %, at the extremes of Tone and Time, with and without Disperse
        {
            float peak = 0.0f, worstRatio = -100.0f;
            bool finite = true;
            for (float tone : { 0.0f, 1.0f })
                for (float time : { 0.1f, 50.0f })
                    for (bool invert : { false, true })
                        for (bool withDisperse : { false, true })
                        {
                            Engine e;
                            e.setParams (looped (0.95f, time, tone, invert, withDisperse));
                            e.prepare (kRate, 2, 1);
                            AudioBuffer<float> a (2, (int) kRate * 4);
                            a.clear();
                            Rng rng (77);
                            for (int i = 0; i < (int) kRate * 2; ++i)
                                a.setSample (0, i, 0.5f * rng.bipolar()), a.setSample (1, i, 0.5f * rng.bipolar());
                            run (e, a);
                            for (int c = 0; c < 2; ++c)
                                for (int i = 0; i < a.getNumSamples(); ++i)
                                {
                                    finite = finite && std::isfinite (a.getSample (c, i));
                                    peak = std::max (peak, std::abs (a.getSample (c, i)));
                                }
                            const int half = (int) (kRate * 0.5);
                            const float early = a.getRMSLevel (0, (int) kRate * 2, half), late = a.getRMSLevel (0, (int) kRate * 4 - half, half);
                            worstRatio = std::max (worstRatio, Decibels::gainToDecibels (late, -200.0f) - Decibels::gainToDecibels (early, -200.0f));
                        }
            expect (finite && peak < 4.0f && worstRatio < -3.0f,
                    "Amount 95 %, Tone 0 / 100, Time 0.1 / 50 ms, Invert, with and without Disperse: peak " + String (peak, 2)
                        + ", the slowest tail falls " + String (-worstRatio, 1) + " dB in 1.5 s");
        }

        // switching Feedback on: no click
        {
            Params p = looped (0.6f, 3.0f, 0.5f, false, true);
            for (auto& c : p.band[0].ch) c.fbOn = false;
            Params on = p;
            for (auto& c : on.band[0].ch) c.fbOn = true;
            const float m = stepDuring (p, [] (Params& q) { for (auto& c : q.band[0].ch) c.fbOn = true; });
            const float still = std::max (stepDuring (p, [] (Params&) {}), stepDuring (on, [] (Params&) {}));
            expect (m < still * 1.25f, "Feedback on: largest step " + String (m, 4) + " (off or on without a switch: " + String (still, 4) + ")");
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
            { "disperse_256", [] (FasePhuckerAudioProcessor& p)
                {
                    set (p, channelId (1, 0, pid::ch::stages), 256.0f);
                    set (p, channelId (1, 0, pid::ch::freq), 900.0f);
                    set (p, channelId (1, 0, pid::ch::pinch), 65.0f);
                } },
            { "disperse_unhinged_jitter", [] (FasePhuckerAudioProcessor& p)
                {
                    set (p, channelId (1, 0, pid::ch::mode), 1.0f);
                    set (p, channelId (1, 0, pid::ch::jitter), 1.0f);
                    set (p, channelId (1, 0, pid::ch::stages), 128.0f);
                    set (p, channelId (1, 0, pid::ch::spread), 80.0f);
                } },
            { "dist_noise",    [] (FasePhuckerAudioProcessor& p) { set (p, channelId (1, 0, pid::ch::drive), 60.0f); } },
            { "dist_saturate", [] (FasePhuckerAudioProcessor& p) { set (p, channelId (1, 0, pid::ch::type), 1.0f); set (p, channelId (1, 0, pid::ch::drive), 60.0f); } },
            { "dist_clip",     [] (FasePhuckerAudioProcessor& p) { set (p, channelId (1, 0, pid::ch::type), 2.0f); set (p, channelId (1, 0, pid::ch::drive), 60.0f); } },
            { "dist_fold",     [] (FasePhuckerAudioProcessor& p) { set (p, channelId (1, 0, pid::ch::type), 3.0f); set (p, channelId (1, 0, pid::ch::drive), 60.0f); } },
            { "dist_rectify",  [] (FasePhuckerAudioProcessor& p) { set (p, channelId (1, 0, pid::ch::type), 4.0f); set (p, channelId (1, 0, pid::ch::drive), 60.0f); } },
            { "dist_crush",    [] (FasePhuckerAudioProcessor& p) { set (p, channelId (1, 0, pid::ch::type), 5.0f); set (p, channelId (1, 0, pid::ch::drive), 60.0f); } },
            { "dist_post_phat_bias_tone", [] (FasePhuckerAudioProcessor& p)
                {
                    set (p, channelId (1, 0, pid::ch::type), 1.0f);
                    set (p, channelId (1, 0, pid::ch::order), 1.0f);
                    set (p, channelId (1, 0, pid::ch::drive), 45.0f);
                    set (p, channelId (1, 0, pid::ch::phat), 70.0f);
                    set (p, channelId (1, 0, pid::ch::bias), 30.0f);
                    set (p, channelId (1, 0, pid::ch::distTone), 70.0f);
                } },
            { "feedback_long", [] (FasePhuckerAudioProcessor& p)
                {
                    set (p, channelId (1, 0, pid::ch::distOn), 0.0f);
                    set (p, channelId (1, 0, pid::ch::fbAmt), 85.0f);
                    set (p, channelId (1, 0, pid::ch::fbTime), 20.0f);
                } },
            { "feedback_invert_tone", [] (FasePhuckerAudioProcessor& p)
                {
                    set (p, channelId (1, 0, pid::ch::distOn), 0.0f);
                    set (p, channelId (1, 0, pid::ch::fbAmt), 70.0f);
                    set (p, channelId (1, 0, pid::ch::fbTime), 2.0f);
                    set (p, channelId (1, 0, pid::ch::invert), 1.0f);
                    set (p, channelId (1, 0, pid::ch::fbTone), 80.0f);
                } },
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
    checkDisperse();
    checkDistortion();
    checkFeedback();
    runRenders (dir, false);
    std::printf ("\n%s\n", failures == 0 ? "All DSP checks passed." : "DSP checks failed.");
    return failures == 0 ? 0 : 1;
}
