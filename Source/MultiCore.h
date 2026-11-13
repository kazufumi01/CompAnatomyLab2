#pragma once
// =============================================================================
//  Single band, or three bands (Linkwitz-Riley 24 dB/oct) each with its own mod::Core.
//  In 3-band mode the low band is slower (×2) and the high band faster (×0.5), and each band has a gain trim.
//  `dry` receives the input as it sums back from the crossover (an all-pass of the input), so Delta and the
//  dry / wet mix stay phase-aligned.
// =============================================================================
#include "ModCore.h"
#include "Spectral.h"

namespace mod
{
class Multi
{
public:
    static constexpr int kMax = 1024;
    explicit Multi (double sampleRate) : fs (sampleRate), cores { Core (sampleRate), Core (sampleRate), Core (sampleRate) }, spec (sampleRate), preCore (sampleRate)
    {
        for (auto& b : buf) b.assign ((size_t) kMax, 0.0f);
        for (auto& b : msIn) b.assign ((size_t) kMax, 0.0f);
        for (auto& b : preBuf) b.assign ((size_t) kMax, 0.0f);
    }
    void set (const Params& pin)
    {
        Params p = pin; if (p.law == LawSpectral) p.mb = false;      // spectral compression already works per frequency
        if (p.mb != mbOn) { mbOn = p.mb; reset(); }
        prm = p;
        spec.set (p);
        if (p.pre)                                              // pre-stage: RMS, feedback, opto law at a fixed 2:1, opto timing (most of the release fast, the rest slow)
        {
            Params q; q.det = DetRMS; q.rmsMs = 10; q.feedback = true; q.law = LawOpto; q.threshold = p.preThr; q.ratio = 2;
            q.timing = TimOptoMem; q.attack = 15; q.release = 80; q.element = ElOpto; preCore.set (q);
        }
        if (! mbOn) { cores[0].set (p); return; }
        const double f1 = std::min (std::max (p.xLo, 30.0), 1000.0), f2 = std::min (std::max (p.xHi, f1 * 2.0), fs * 0.4);
        for (int k = 0; k < 2; ++k)
        {
            lp1[k].set (BandLP, f1, 0.70710678, fs); hp1[k].set (BandHP, f1, 0.70710678, fs);
            lp2[k].set (BandLP, f2, 0.70710678, fs); hp2[k].set (BandHP, f2, 0.70710678, fs);
            lpc[k].set (BandLP, f2, 0.70710678, fs); hpc[k].set (BandHP, f2, 0.70710678, fs);
        }
        static const double ts[3] { 2.0, 1.0, 0.5 };
        for (int b = 0; b < 3; ++b) { Params q = p; q.attack *= ts[b]; q.release *= ts[b]; q.makeup += p.bandGain[b]; cores[b].set (q); }
    }
    int latencySamples() const { return prm.law == LawSpectral ? Spectral::N : cores[0].latencySamples(); }
    const float* specBands() const { return spec.bands(); }
    void reset()
    {
        for (auto& c : cores) c.reset();
        spec.reset(); preCore.reset();
        for (int k = 0; k < 2; ++k) { lp1[k].reset(); hp1[k].reset(); lp2[k].reset(); hp2[k].reset(); lpc[k].reset(); hpc[k].reset(); }
    }

    // n ≤ kMax. bandGr / bandUp (3 values each, may be nullptr) receive the peak reduction / boost per band in this call.
    // M/S modes: encode to mid / side, process, decode. `dry` comes back as left / right.
    Meters run (const float* iL, const float* iR, float* oL, float* oR, int n, const float* scL, const float* scR, const Taps& tap,
                float* dryL, float* dryR, float* bandGr = nullptr, float* bandUp = nullptr)
    {
        if (prm.stereo == 0) return runInner (iL, iR, oL, oR, n, scL, scR, tap, dryL, dryR, bandGr, bandUp);
        float* m = msIn[0].data(); float* s = msIn[1].data();
        for (int i = 0; i < n; ++i) { m[i] = 0.5f * (iL[i] + iR[i]); s[i] = 0.5f * (iL[i] - iR[i]); }
        auto r = runInner (m, s, oL, oR, n, scL, scR, tap, dryL, dryR, bandGr, bandUp);
        r.outPk = 0;
        for (int i = 0; i < n; ++i)
        {
            float a = oL[i], b = oR[i]; oL[i] = a + b; oR[i] = a - b; r.outPk = std::max (r.outPk, std::max (std::abs (oL[i]), std::abs (oR[i])));
            a = dryL[i]; b = dryR[i]; dryL[i] = a + b; dryR[i] = a - b;
        }
        return r;
    }

private:
    Meters runInner (const float* iL, const float* iR, float* oL, float* oR, int n, const float* scL, const float* scR, const Taps& tap,
                     float* dryL, float* dryR, float* bandGr, float* bandUp)
    {
        if (! mbOn)
        {
            for (int i = 0; i < n; ++i) { dryL[i] = iL[i]; dryR[i] = iR[i]; }             // dry = the input before any stage
            if (prm.pre)                                                                    // serial pre-stage (not in 3-band mode)
            {
                Taps t; t.gr = preBuf[2].data();
                preCore.run (iL, iR, preBuf[0].data(), preBuf[1].data(), n, nullptr, nullptr, t);
                if (tap.pre != nullptr) std::copy (preBuf[2].begin(), preBuf[2].begin() + n, tap.pre);
                iL = preBuf[0].data(); iR = preBuf[1].data();
            }
            else if (tap.pre != nullptr) std::fill (tap.pre, tap.pre + n, 0.0f);
            const auto m = prm.law == LawSpectral ? spec.run (iL, iR, oL, oR, n, tap) : cores[0].run (iL, iR, oL, oR, n, scL, scR, tap);
            if (bandGr != nullptr) { bandGr[0] = bandGr[1] = bandGr[2] = m.grMax; }
            if (bandUp != nullptr) { bandUp[0] = bandUp[1] = bandUp[2] = m.upMax; }
            return m;
        }
        if (tap.pre != nullptr) std::fill (tap.pre, tap.pre + n, 0.0f);
        float* bl[3] { buf[0].data(), buf[2].data(), buf[4].data() }; float* br[3] { buf[1].data(), buf[3].data(), buf[5].data() };
        for (int i = 0; i < n; ++i)
        {
            const double x[2] { iL[i], iR[i] }; double lo[2], mid[2], hi[2];
            for (int c = 0; c < 2; ++c)
            {
                const double l = lp1[1].run (c, lp1[0].run (c, x[c])), rest = hp1[1].run (c, hp1[0].run (c, x[c]));
                lo[c] = lpc[1].run (c, lpc[0].run (c, l)) + hpc[1].run (c, hpc[0].run (c, l));      // same phase as the upper split
                mid[c] = lp2[1].run (c, lp2[0].run (c, rest)); hi[c] = hp2[1].run (c, hp2[0].run (c, rest));
            }
            bl[0][i] = (float) lo[0]; bl[1][i] = (float) mid[0]; bl[2][i] = (float) hi[0];
            br[0][i] = (float) lo[1]; br[1][i] = (float) mid[1]; br[2][i] = (float) hi[1];
            dryL[i] = (float) (lo[0] + mid[0] + hi[0]); dryR[i] = (float) (lo[1] + mid[1] + hi[1]);
        }
        const double gIn = std::pow (10.0, prm.input / 20.0);
        if (tap.inAbs != nullptr) for (int i = 0; i < n; ++i) tap.inAbs[i] = (float) (gIn * std::max (std::abs (dryL[i]), std::abs (dryR[i])));
        Meters all;
        float* tg = buf[6].data(); float* tu = buf[7].data(); float* tsl = buf[8].data(); float* dl = buf[9].data(); float* dr = buf[10].data();
        for (int b = 0; b < 3; ++b)
        {
            Taps t; t.gr = tg; t.up = tu; t.slow = tsl; if (tap.detL != nullptr) { t.detL = dl; t.detR = dr; }
            const auto m = cores[b].run (bl[b], br[b], bl[b], br[b], n, scL, scR, t);
            all.grMax = std::max (all.grMax, m.grMax); all.upMax = std::max (all.upMax, m.upMax);
            if (bandGr != nullptr) bandGr[b] = m.grMax;
            if (bandUp != nullptr) bandUp[b] = m.upMax;
            for (int i = 0; i < n; ++i)
            {
                const bool first = b == 0;
                oL[i] = (first ? 0.0f : oL[i]) + bl[b][i]; oR[i] = (first ? 0.0f : oR[i]) + br[b][i];
                if (tap.gr != nullptr) tap.gr[i] = first ? tg[i] : std::max (tap.gr[i], tg[i]);
                if (tap.up != nullptr) tap.up[i] = first ? tu[i] : std::max (tap.up[i], tu[i]);
                if (tap.slow != nullptr) tap.slow[i] = first ? tsl[i] : std::max (tap.slow[i], tsl[i]);
                if (tap.detL != nullptr) { tap.detL[i] = (first ? 0.0f : tap.detL[i]) + dl[i]; tap.detR[i] = (first ? 0.0f : tap.detR[i]) + dr[i]; }
            }
        }
        for (int i = 0; i < n; ++i) all.outPk = std::max (all.outPk, std::max (std::abs (oL[i]), std::abs (oR[i])));
        return all;
    }

    double fs; bool mbOn = false; Params prm;
    std::vector<float> msIn[2];
    Core cores[3];
    Spectral spec; Core preCore; std::vector<float> preBuf[3];
    Biquad lp1[2], hp1[2], lp2[2], hp2[2], lpc[2], hpc[2];
    std::vector<float> buf[11];
};
} // namespace mod
