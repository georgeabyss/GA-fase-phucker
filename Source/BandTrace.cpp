#include "BandTrace.h"

namespace fp
{
const ga::BandCard::Trace& BandTrace::compute (const Inputs& in)
{
    if (valid && in == last)
        return trace;
    last = in;
    valid = true;

    constexpr double pi = 3.141592653589793;
    const int n = kSize;
    const double q = 0.5 + 4.0 * in.pinch;
    const double f0 = in.freq, mix = in.stages > 0 ? in.mix : 0.0;
    const double groupDelay = in.stages * 2.0 * q / (pi * f0);
    span = std::clamp (groupDelay * 1.4 + 0.002, 0.004, 2.0);
    const double fs = std::max ((double) n / span, 4.0 * std::max (f0, (double) in.low * 1.2));

    // one biquad all-pass at f0 (normalised: b = a reversed)
    const double w0 = 2.0 * pi * f0 / fs, alpha = std::sin (w0) / (2.0 * q), a0 = 1.0 + alpha;
    const double a1 = -2.0 * std::cos (w0) / a0, a2 = (1.0 - alpha) / a0;
    auto lg = [] (double f) { return std::log2 (std::max (f, 1.0)); };

    // the spectrum, bins 0 .. n/2, as JUCE's real-only inverse transform expects it
    std::fill (data.begin(), data.end(), 0.0f);
    for (int k = 0; k <= n / 2; ++k)
    {
        const double f = k * fs / n, w = 2.0 * pi * k / n;
        double g = 1.0;   // the band's window: half an octave of slope at each edge
        g *= std::clamp (lg (f) - lg (std::max ((double) in.low, 20.0)) + 0.5, 0.0, 1.0);
        g *= std::clamp (lg (std::min ((double) in.high, 20000.0)) - lg (f) + 0.5, 0.0, 1.0);
        const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
        const double nr = a2 + a1 * c1 + c2, ni = -(a1 * s1 + s2), dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        const double ph = in.stages * (std::atan2 (ni, nr) - std::atan2 (di, dr));
        data[(size_t) (2 * k)] = (float) (g * ((1.0 - mix) + mix * std::cos (ph)));
        data[(size_t) (2 * k + 1)] = (float) (g * mix * std::sin (ph));
    }
    fft.performRealOnlyInverseTransform (data.data());

    // min / max per column over the time shown, scaled to the peak
    const int shown = std::min (n, (int) std::lround (span * fs));
    const double per = (double) shown / kColumns;
    trace.low.assign (kColumns, 0.0f);
    trace.high.assign (kColumns, 0.0f);
    float peak = 1.0e-9f;
    for (int c = 0; c < kColumns; ++c)
    {
        const int from = (int) (c * per), to = std::max ((int) ((c + 1) * per), from + 1);
        float lo = 1.0e9f, hi = -1.0e9f;
        for (int i = from; i < to && i < n; ++i)
        {
            lo = std::min (lo, data[(size_t) i]);
            hi = std::max (hi, data[(size_t) i]);
        }
        trace.low[(size_t) c] = lo;
        trace.high[(size_t) c] = hi;
        peak = std::max ({ peak, std::abs (lo), std::abs (hi) });
    }
    for (int c = 0; c < kColumns; ++c)
    {
        trace.low[(size_t) c] /= peak;
        trace.high[(size_t) c] /= peak;
    }
    return trace;
}

juce::String BandTrace::formatSpan (double seconds)
{
    const double ms = seconds * 1000.0;
    return ms < 1000.0 ? juce::String (juce::roundToInt (ms)) + " ms" : juce::String (seconds, 1) + " s";
}
}
