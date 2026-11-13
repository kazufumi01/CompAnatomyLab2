#pragma once
// =============================================================================
//  Spectral compression: the signal is cut into overlapping frames, turned into a spectrum, and every
//  frequency gets its own compressor (threshold, ratio, knee, attack, release). The level each one sees is
//  the power in a third of an octave around it, so pink-ish music reads flat and single partials do not chatter.
//  2048-point frames, 75 % overlap → 2048 samples of latency (43 ms at 48 kHz). No JUCE dependency.
// =============================================================================
#include "ModCore.h"

namespace mod
{
class Spectral
{
public:
    static constexpr int N = 2048, H = 512, kBands = 48;
    explicit Spectral (double sampleRate) : fs (sampleRate)
    {
        for (int c = 0; c < 2; ++c) { inb[c].assign ((size_t) N, 0.0f); ola[c].assign ((size_t) N, 0.0f); dry[c].assign ((size_t) N, 0.0f); re[c].assign ((size_t) N, 0.0f); im[c].assign ((size_t) N, 0.0f); }
        win.resize ((size_t) N); cosT.resize ((size_t) N / 2); sinT.resize ((size_t) N / 2); rev.resize ((size_t) N);
        for (int n = 0; n < N; ++n) win[(size_t) n] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * n / N));
        for (int k = 0; k < N / 2; ++k) { cosT[(size_t) k] = (float) std::cos (2.0 * kPi * k / N); sinT[(size_t) k] = (float) std::sin (2.0 * kPi * k / N); }
        int bits = 0; while ((1 << bits) < N) ++bits;
        for (int i = 0; i < N; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[(size_t) i] = r; }
        const int nb = N / 2 + 1; grS.assign ((size_t) nb, 0.0f); gain.assign ((size_t) nb, 1.0f); pre.assign ((size_t) nb + 1, 0.0); lo.resize ((size_t) nb); hi.resize ((size_t) nb);
        const double w = std::pow (2.0, 1.0 / 6.0);
        for (int k = 0; k < nb; ++k)
        {
            lo[(size_t) k] = std::max (1, (int) std::floor (k / w)); hi[(size_t) k] = std::min (N / 2, (int) std::ceil (k * w));
        }
        for (int j = 0; j < kBands; ++j)                                 // display bands: the bins inside each, or the nearest one where bins are sparser than bands
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) j / kBands), f1 = 20.0 * std::pow (1000.0, (j + 1.0) / kBands);
            int a = std::max (1, (int) std::ceil (f0 * N / fs)), b = std::min (N / 2, (int) std::floor (f1 * N / fs));
            if (b < a) a = b = std::max (1, std::min (N / 2, (int) std::lround (std::sqrt (f0 * f1) * N / fs)));
            bLo[j] = a; bHi[j] = b;
        }
        set (Params {});
    }
    void set (const Params& p)
    {
        prm = p;
        aAtt = std::exp (-(double) H / (fs * std::max (p.attack, 0.1) * 0.001)); aRel = std::exp (-(double) H / (fs * std::max (p.release, 1.0) * 0.001));
        gIn = std::pow (10.0, p.input / 20.0); gMk = std::pow (10.0, p.makeup / 20.0);
    }
    int latencySamples() const { return N; }
    const float* bands() const { return grBand; }                   // gain reduction (dB) in kBands log-spaced bands, 20 Hz – 20 kHz
    void reset()
    {
        for (int c = 0; c < 2; ++c) { std::fill (inb[c].begin(), inb[c].end(), 0.0f); std::fill (ola[c].begin(), ola[c].end(), 0.0f); std::fill (dry[c].begin(), dry[c].end(), 0.0f); }
        std::fill (grS.begin(), grS.end(), 0.0f); std::fill (gain.begin(), gain.end(), 1.0f); for (auto& b : grBand) b = 0; k = 0; dw = 0; frameGr = 0;
    }

    Meters run (const float* iL, const float* iR, float* oL, float* oR, int n, const Taps& tap)
    {
        Meters m; const float* in[2] { iL, iR }; float* out[2] { oL, oR };
        for (int i = 0; i < n; ++i)
        {
            float ai = 0;
            for (int c = 0; c < 2; ++c)
            {
                const float x = (float) (in[c][i] * gIn);
                inb[c][(size_t) (N - H + k)] = x;
                const float d = dry[c][(size_t) dw]; dry[c][(size_t) dw] = x;                     // the dry signal, N samples late like the wet one
                const float wet = ola[c][(size_t) k];
                const float y = prm.bypass ? d : (float) (wet * gMk * prm.mix + d * (1.0 - prm.mix));
                out[c][i] = y; ai = std::max (ai, std::abs (d));                          // the input as the display sees it: late, like the output m.outPk = std::max (m.outPk, std::abs (y));
            }
            if (++dw >= N) dw = 0;
            m.inPk = std::max (m.inPk, ai); m.grMax = std::max (m.grMax, frameGr);
            if (tap.gr != nullptr) tap.gr[i] = frameGr;
            if (tap.up != nullptr) tap.up[i] = 0.0f;
            if (tap.slow != nullptr) tap.slow[i] = 0.0f;
            if (tap.inAbs != nullptr) tap.inAbs[i] = ai;
            if (tap.detL != nullptr) { tap.detL[i] = in[0][i]; tap.detR[i] = in[1][i]; }
            if (++k == H) { frame(); k = 0; }
        }
        return m;
    }

private:
    void fft (float* r, float* q, bool inverse) const
    {
        for (int i = 0; i < N; ++i) { const int j = rev[(size_t) i]; if (j > i) { std::swap (r[i], r[j]); std::swap (q[i], q[j]); } }
        for (int len = 2; len <= N; len <<= 1)
        {
            const int half = len / 2, step = N / len;
            for (int i = 0; i < N; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const float wr = cosT[(size_t) (j * step)], wi = inverse ? sinT[(size_t) (j * step)] : -sinT[(size_t) (j * step)];
                    const int a = i + j, b = a + half; const float tr = r[b] * wr - q[b] * wi, ti = r[b] * wi + q[b] * wr;
                    r[b] = r[a] - tr; q[b] = q[a] - ti; r[a] += tr; q[a] += ti;
                }
        }
    }
    void frame()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (int n = 0; n < N; ++n) { re[c][(size_t) n] = inb[c][(size_t) n] * win[(size_t) n]; im[c][(size_t) n] = 0.0f; }
            fft (re[c].data(), im[c].data(), false);
        }
        const int nb = N / 2 + 1; const double norm = 1.0 / ((N / 4.0) * (N / 4.0));
        pre[0] = 0.0;
        for (int b = 0; b < nb; ++b)
        {
            const double p0 = (double) re[0][(size_t) b] * re[0][(size_t) b] + (double) im[0][(size_t) b] * im[0][(size_t) b], p1 = (double) re[1][(size_t) b] * re[1][(size_t) b] + (double) im[1][(size_t) b] * im[1][(size_t) b];
            pre[(size_t) b + 1] = pre[(size_t) b] + std::max (p0, p1) * norm;
        }
        float fg = 0; for (auto& b : grBand) b = 0;
        const bool active = prm.ratio > 1.0001;
        for (int b = 1; b < nb; ++b)
        {
            const double bp = pre[(size_t) hi[(size_t) b] + 1] - pre[(size_t) lo[(size_t) b]];                 // power in a third of an octave around this bin
            const float t = active ? (float) std::min (idealGr (10.0 * std::log10 (bp + 1e-20), prm.threshold, prm.ratio, prm.knee), prm.grLimit >= 39.95 ? 1.0e9 : prm.grLimit) : 0.0f;
            const float a = (float) (t > grS[(size_t) b] ? aAtt : aRel);
            grS[(size_t) b] = a * grS[(size_t) b] + (1.0f - a) * t;
            gain[(size_t) b] = grS[(size_t) b] > 1.0e-4f ? std::pow (10.0f, -grS[(size_t) b] / 20.0f) : 1.0f;
            fg = std::max (fg, grS[(size_t) b]);
        }
        frameGr = fg;
        for (int j = 0; j < kBands; ++j) for (int b = bLo[j]; b <= bHi[j]; ++b) grBand[j] = std::max (grBand[j], grS[(size_t) b]);
        for (int c = 0; c < 2; ++c)
        {
            float* r = re[c].data(); float* q = im[c].data();
            for (int b = 1; b < nb; ++b)
            {
                const float g = gain[(size_t) b]; r[b] *= g; q[b] *= g;
                if (b < N / 2) { r[N - b] *= g; q[N - b] *= g; }
            }
            fft (r, q, true);
            std::memmove (ola[c].data(), ola[c].data() + H, (size_t) (N - H) * sizeof (float)); std::fill (ola[c].begin() + (N - H), ola[c].end(), 0.0f);
            const float sc = (float) ((2.0 / 3.0) / N);                                                       // Hann² at 75 % overlap sums to 1.5
            for (int n = 0; n < N; ++n) ola[c][(size_t) n] += r[n] * win[(size_t) n] * sc;
            std::memmove (inb[c].data(), inb[c].data() + H, (size_t) (N - H) * sizeof (float));
        }
    }

    double fs; Params prm; double aAtt = 0, aRel = 0, gIn = 1, gMk = 1;
    std::vector<float> inb[2], ola[2], dry[2], re[2], im[2], win, cosT, sinT, grS, gain;
    std::vector<double> pre; std::vector<int> rev, lo, hi; int bLo[kBands] {}, bHi[kBands] {};
    float grBand[kBands] {}, frameGr = 0; int k = 0, dw = 0;
};
} // namespace mod
