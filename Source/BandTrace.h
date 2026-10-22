#pragma once

#include <juce_dsp/juce_dsp.h>
#include "ui/Bands.h"

namespace fp
{
// A band's impulse response for its card (docs/design/spec.md, "Band trace"): H(w) = (1 - mix) +
// mix * A(w)^N, A one biquad all-pass at Freq with Q = 0.5 + 4 Pinch, windowed to the band's range
// (half-octave slopes), inverse FFT of 8192 points, min / max per column. The time shown is 1.4
// times the peak group delay N 2Q / (pi Freq), 4 ms to 2 s. Message thread; recomputes only when
// its inputs change.
class BandTrace
{
public:
    struct Inputs
    {
        int stages = 0;          // 0: Disperse off
        float freq = 400.0f, pinch = 0.5f, mix = 1.0f, low = 20.0f, high = 20000.0f;
        bool operator== (const Inputs& o) const
        {
            return stages == o.stages && freq == o.freq && pinch == o.pinch && mix == o.mix && low == o.low && high == o.high;
        }
    };

    // the trace for these inputs (cached: the last one is kept)
    const ga::BandCard::Trace& compute (const Inputs&);
    double spanSeconds() const { return span; }   // the time the trace shows

    static juce::String formatSpan (double seconds);   // "788 ms", "1.3 s"

private:
    static constexpr int kOrder = 13, kSize = 1 << kOrder, kColumns = 240;

    Inputs last;
    bool valid = false;
    ga::BandCard::Trace trace;
    double span = 0.0;
    juce::dsp::FFT fft { kOrder };
    std::vector<float> data = std::vector<float> ((size_t) kSize * 2);
};
}
