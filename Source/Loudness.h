#pragma once
// =============================================================================
//  Loudness after ITU-R BS.1770 / EBU R 128: K-weighting, 100 ms steps.
//  M = last 400 ms, S = last 3 s, I = gated mean since the reset (absolute gate -70 LUFS, relative gate -10 LU).
//  Peak is the true peak: 4x oversampled with a 48-tap windowed-sinc interpolator (BS.1770-4 Annex 2 style). No JUCE dependency.
// =============================================================================
#include <cmath>
#include <algorithm>

namespace mod
{
class LoudMeter
{
public:
    struct Vals { float m = -120, s = -120, i = -120, pk = -120, pkMax = -120; };     // LUFS, LUFS, LUFS, dBFS (last 3 s), dBFS (since reset)
    void prepare (double sampleRate)
    {
        fs = sampleRate; hop = std::max (1, (int) std::lround (fs * 0.1));
        const double pi = 3.14159265358979323846;
        { const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196, K = std::tan (pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K;
          sb[0] = (Vh + Vb * K / Q + K * K) / a0; sb[1] = 2.0 * (K * K - Vh) / a0; sb[2] = (Vh - Vb * K / Q + K * K) / a0; sa[0] = 2.0 * (K * K - 1.0) / a0; sa[1] = (1.0 - K / Q + K * K) / a0; }
        { const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan (pi * f0 / fs), a0 = 1.0 + K / Q + K * K;
          ha[0] = 2.0 * (K * K - 1.0) / a0; ha[1] = (1.0 - K / Q + K * K) / a0; }
        for (int ph = 0; ph < 4; ++ph)
            for (int k = 0; k < kTaps; ++k)
            {
                const double t = (double) (k - kTaps / 2) + (double) ph / 4.0 + 0.5, x = pi * t;                   // sample positions around the interpolated point
                const double sinc = std::abs (t) < 1e-9 ? 1.0 : std::sin (x) / x, w = 0.5 + 0.5 * std::cos (pi * t / (kTaps / 2 + 1));
                fir[ph][k] = sinc * w;
            }
        reset();
    }
    void reset()
    {
        for (auto& c : z) for (auto& v : c) v = 0;
        for (auto& v : e) v = 0; for (auto& v : pk) v = 0; for (auto& v : hist) v = 0; for (auto& c : tp) for (auto& v : c) v = 0; tpPos = 0;
        acc = 0; pkAcc = 0; cnt = 0; pos = 0; filled = 0; pkMax = 0; out = Vals {};
    }
    void process (const float* L, const float* R, int n)
    {
        const float* in[2] { L, R };
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                const double x = in[c][i];
                const double y1 = sb[0] * x + z[c][0]; z[c][0] = sb[1] * x - sa[0] * y1 + z[c][1]; z[c][1] = sb[2] * x - sa[1] * y1;       // high shelf
                const double y2 = y1 + z[c][2]; z[c][2] = -2.0 * y1 - ha[0] * y2 + z[c][3]; z[c][3] = y1 - ha[1] * y2;                       // high-pass
                acc += y2 * y2; pkAcc = std::max (pkAcc, std::abs (x));
                tp[c][tpPos] = x; tp[c][tpPos + kTaps] = x;                                          // doubled ring: a straight read of the last kTaps samples
                const double* h = &tp[c][tpPos + 1];
                for (int ph = 0; ph < 4; ++ph) { double s = 0; for (int k = 0; k < kTaps; ++k) s += h[k] * fir[ph][k]; pkAcc = std::max (pkAcc, std::abs (s)); }
            }
            if (++tpPos >= kTaps) tpPos = 0;
            if (++cnt >= hop) step();
        }
    }
    Vals vals() const { return out; }

private:
    static double lufs (double meanSquare) { return meanSquare > 1e-12 ? -0.691 + 10.0 * std::log10 (meanSquare) : -120.0; }
    static double db (double a) { return a > 1e-6 ? 20.0 * std::log10 (a) : -120.0; }
    void step()
    {
        e[pos] = acc / hop; pk[pos] = pkAcc; pkMax = std::max (pkMax, pkAcc); pos = (pos + 1) % kRing; filled = std::min (kRing, filled + 1);
        acc = 0; pkAcc = 0; cnt = 0;
        double m4 = 0, s30 = 0, p30 = 0; const int nm = std::min (filled, 4);
        for (int k = 0; k < filled; ++k) { const int j = (pos - 1 - k + 2 * kRing) % kRing; s30 += e[j]; p30 = std::max (p30, pk[j]); if (k < 4) m4 += e[j]; }
        m4 /= nm; s30 /= filled;
        if (filled >= 4) { const double lm = lufs (m4); if (lm > -70.0) ++hist[std::min (kBins - 1, (int) ((lm + 70.0) * 10.0))]; }
        auto gated = [this] (int from) { double sum = 0; long cntb = 0; for (int b = from; b < kBins; ++b) if (hist[b] > 0) { sum += hist[b] * std::pow (10.0, ((b + 0.5) * 0.1 - 70.0 + 0.691) / 10.0); cntb += hist[b]; }
                                         return cntb > 0 ? sum / (double) cntb : 0.0; };
        double integ = -120.0;
        if (const double a = gated (0); a > 0) { const double rel = lufs (a) - 10.0; const double g = gated (std::max (0, (int) std::ceil ((rel + 70.0) * 10.0 - 0.5))); integ = lufs (g > 0 ? g : a); }
        out.m = (float) lufs (m4); out.s = (float) lufs (s30); out.i = (float) integ; out.pk = (float) db (p30); out.pkMax = (float) db (pkMax);
    }
    static constexpr int kRing = 30, kBins = 800, kTaps = 48;
    double fir[4][kTaps] {}, tp[2][2 * kTaps] {}; int tpPos = 0;
    double fs = 48000, sb[3] {}, sa[2] {}, ha[2] {}, z[2][4] {}, e[kRing] {}, pk[kRing] {}, acc = 0, pkAcc = 0, pkMax = 0;
    long hist[kBins] {}; int hop = 4800, cnt = 0, pos = 0, filled = 0; Vals out;
};
} // namespace mod
