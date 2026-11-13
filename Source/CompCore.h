#pragma once
// =============================================================================
//  Compressor core. A 1:1 port of CompCore in the web "コンプ解剖ラボ"
//  (Giannoulis, Massberg, Reiss 2012: log-domain gain computer, one-pole
//  attack / release smoothing in the dB domain, feed-forward / feedback).
//  No JUCE dependency, so the same file runs in the unit test and in the plugin.
// =============================================================================
#include <algorithm>
#include <cmath>
#include <vector>

namespace comp
{
enum Element { VCA = 0, FET = 1, Opto = 2, VariMu = 3 };

struct Params
{
    double input = 0, threshold = -18, ratio = 4, knee = 6, attack = 10, release = 120, makeup = 4;
    bool rms = false;                  // detector: false = peak, true = RMS
    double rmsMs = 10;
    bool feedback = false;             // topology
    double lookaheadMs = 0, mix = 1;   // mix 0..1
    bool bypass = false;
    int element = VCA;
    double scHpfHz = 0;                // detector high-pass (12 dB/oct), < 20 = off
};

struct Taps { float* gr = nullptr; float* memory = nullptr; float* inAbs = nullptr; float* detL = nullptr; float* detR = nullptr; };

class Core
{
public:
    explicit Core (double sampleRate) : fs (sampleRate)
    {
        size = (int) std::ceil (fs * 0.012) + 2;
        dL.assign ((size_t) size, 0.0f); dR.assign ((size_t) size, 0.0f);
        set (Params {});
    }

    void set (const Params& np)
    {
        p = np;
        auto tc = [this] (double ms) { return std::exp (-1.0 / (std::max (ms, 0.001) * 0.001 * fs)); };
        aA = tc (p.attack); aR = tc (p.release); aM = tc (p.rmsMs);
        oFA = tc (10); oFR = tc (60); oSA = tc (30); oMU = tc (1500); oMD = tc (4000);
        dcR = std::exp (-2.0 * M_PI * 8.0 / fs);
        hpfOn = p.scHpfHz >= 20.0;
        if (hpfOn)
        {
            const double w0 = 2.0 * M_PI * std::min (p.scHpfHz, fs * 0.45) / fs, c = std::cos (w0), al = std::sin (w0) / (2.0 * 0.70710678);
            const double a0 = 1.0 + al;
            hb0 = (1.0 + c) * 0.5 / a0; hb1 = -(1.0 + c) / a0; ha1 = -2.0 * c / a0; ha2 = (1.0 - al) / a0;
        }
        la = std::min (size - 1, (int) std::lround (p.lookaheadMs * 0.001 * fs));
        gIn = std::pow (10.0, p.input / 20.0); gMk = std::pow (10.0, p.makeup / 20.0);
    }
    const Params& params() const { return p; }
    int latencySamples() const { return la; }
    double grNow() const { return gs; }
    double memory() const { return mem; }

    void reset()
    {
        ms = gs = fbL = fbR = 0; gF = gS = mem = 0; dxL = dyL = dxR = dyR = 0; hz[0] = hz[1] = hz[2] = hz[3] = 0;
        std::fill (dL.begin(), dL.end(), 0.0f); std::fill (dR.begin(), dR.end(), 0.0f); w = 0;
    }

    // Static characteristic: output dB for input dB
    static double curve (double x, double T, double R, double W, int el)
    {
        if (el == VariMu)                                   // ratio rises with level, knee is built in
        {
            const double over = x - T + 6.0;
            return over <= 0 ? x : x - (1.0 - 1.0 / R) * over * over / (over + 12.0);
        }
        const double d = x - T;
        if (2 * d < -W) return x;
        if (W > 0 && 2 * std::abs (d) <= W) return x + (1.0 / R - 1.0) * (d + W / 2) * (d + W / 2) / (2 * W);
        return T + d / R;
    }

    struct Meters { float inPk = 0, outPk = 0, grMax = 0, lvlMax = -150, mem = 0; };

    // Optional per-sample outputs, each n long (nullptr = skip): GR in dB, Opto memory, |input| after input gain
    // (linear, max of L/R), and the detector signal after the high-pass (for "SC listen").

    // scL / scR: external detector signal (nullptr = detect on the input).
    Meters run (const float* iL, const float* iR, float* oL, float* oR, int n,
                const float* scL = nullptr, const float* scR = nullptr, const Taps& tap = {})
    {
        const double T = p.threshold, R = p.ratio, W = p.knee, mix = p.mix;
        const int el = p.element;
        const bool fb = p.feedback, byp = p.bypass, opto = el == Opto, colour = el == FET || el == VariMu;
        double inPk = 0, outPk = 0, grMax = 0, lvlMax = -150;
        for (int i = 0; i < n; ++i)
        {
            const double xl = (double) iL[i] * gIn, xr = (double) iR[i] * gIn;
            double sl = fb ? fbL : xl, sr = fb ? fbR : xr;
            if (scL != nullptr) { sl = scL[i]; sr = scR != nullptr ? scR[i] : scL[i]; }
            if (hpfOn)
            {
                auto bq = [this] (double x, double& z1, double& z2) { const double y = hb0 * x + z1; z1 = hb1 * x - ha1 * y + z2; z2 = hb0 * x - ha2 * y; return y; };
                sl = bq (sl, hz[0], hz[1]); sr = bq (sr, hz[2], hz[3]);
            }
            if (tap.detL != nullptr) { tap.detL[i] = (float) sl; tap.detR[i] = (float) sr; }
            double lvl;
            if (p.rms) { const double e = std::max (sl * sl, sr * sr); ms = aM * ms + (1 - aM) * e; lvl = std::sqrt (ms); }
            else lvl = std::max (std::abs (sl), std::abs (sr));
            const double xdB = lvl > 1e-7 ? 20.0 * std::log10 (lvl) : -140.0;
            const double gc = xdB - curve (xdB, T, R, W, el);
            if (opto)
            {
                // EL panel + CdS cell: fast half (10 / 60 ms) + slow half whose release grows with "memory"
                const double aF = gc > gF ? oFA : oFR;
                gF = aF * gF + (1 - aF) * gc;
                const double aS = gc > gS ? oSA : std::exp (-1.0 / ((0.5 + 4.5 * mem) * fs));
                gS = aS * gS + (1 - aS) * gc;
                const double tgt = std::min (1.0, gc / 12.0), aMm = tgt > mem ? oMU : oMD;
                mem = aMm * mem + (1 - aMm) * tgt;
                gs = 0.5 * gF + 0.5 * gS;
            }
            else
            {
                const double a = gc > gs ? aA : aR;
                gs = a * gs + (1 - a) * gc;
            }
            dL[(size_t) w] = (float) xl; dR[(size_t) w] = (float) xr;
            int r = w - la; if (r < 0) r += size;
            const double dl = dL[(size_t) r], dr = dR[(size_t) r];
            if (++w >= size) w = 0;
            const double g = std::pow (10.0, -gs / 20.0);
            double wl = dl * g, wr = dr * g;
            if (colour)
            {
                // FET: distortion grows with GR / Vari-Mu: steady even-order tube colour
                const double gn = std::min (gs, 20.0) / 20.0;
                const double dv = el == FET ? 0.5 + 1.5 * gn : 0.8 + 0.4 * gn;
                const double a2 = el == FET ? 0.05 + 0.2 * gn : 0.15 + 0.1 * gn;
                const double ul = std::tanh (dv * (wl + a2 * wl * wl)) / dv, ur = std::tanh (dv * (wr + a2 * wr * wr)) / dv;
                const double yl = ul - dxL + dcR * dyL, yr = ur - dxR + dcR * dyR;
                dxL = ul; dyL = yl; dxR = ur; dyR = yr;
                wl = yl; wr = yr;
            }
            fbL = wl; fbR = wr;
            double yl, yr;
            if (byp) { yl = dl; yr = dr; }
            else { const double k = mix * gMk; yl = wl * k + dl * (1 - mix); yr = wr * k + dr * (1 - mix); }
            oL[i] = (float) yl; oR[i] = (float) yr;
            const double ai = std::max (std::abs (xl), std::abs (xr)); if (ai > inPk) inPk = ai;
            const double ao = std::max (std::abs (yl), std::abs (yr)); if (ao > outPk) outPk = ao;
            if (gs > grMax) grMax = gs;
            if (xdB > lvlMax) lvlMax = xdB;
            if (tap.gr != nullptr) tap.gr[i] = (float) gs;
            if (tap.memory != nullptr) tap.memory[i] = (float) mem;
            if (tap.inAbs != nullptr) tap.inAbs[i] = (float) ai;
        }
        return { (float) inPk, (float) outPk, (float) grMax, (float) lvlMax, (float) mem };
    }

private:
    double fs;
    Params p;
    double ms = 0, gs = 0, fbL = 0, fbR = 0, gF = 0, gS = 0, mem = 0, dxL = 0, dyL = 0, dxR = 0, dyR = 0;
    double aA = 0, aR = 0, aM = 0, oFA = 0, oFR = 0, oSA = 0, oMU = 0, oMD = 0, dcR = 0, gIn = 1, gMk = 1;
    bool hpfOn = false; double hb0 = 1, hb1 = 0, ha1 = 0, ha2 = 0, hz[4] {};
    std::vector<float> dL, dR; int size = 0, w = 0, la = 0;
};
} // namespace comp
