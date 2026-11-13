#pragma once
// =============================================================================
//  Step response of the compressor, measured on the real engine (detector, law, timing and element included).
//  Silence → a constant level `overDb` above the threshold → silence again.
//  Attack time  = time from the step until GR reaches 63 % (90 %, 99 %) of its final value
//  Release time = time from the end of the step until GR has fallen to 37 % (1 %) of it
//  Measured at 1x with a clean element and without look-ahead, input gain, sidechain filters or transformers
//  (the test signal is a DC step, which those would block).
// =============================================================================
#include "ModCore.h"

namespace mod
{
struct StepResult
{
    bool valid = false;
    double finalGr = 0, idealGr = 0;       // steady GR reached / what the static law asks for
    double a63 = 0, a90 = 0, a99 = 0;      // ms from the step
    double r63 = 0, r99 = 0;               // ms from the end of the step
    std::vector<float> attack, release;    // GR (dB): attack from −preMs, release from the end of the step
    std::vector<float> slowRel;            // slow release component (dB) during the release, if the timing has one
    double preMs = 3.0, fs = 48000.0;
};

inline std::array<double, 19> stepKey (const Params& p)
{
    return { (double) p.det, p.rmsMs, p.feedback ? 1.0 : 0.0, (double) p.law, p.threshold, p.ratio, p.knee, (double) p.timing,
             p.attack, p.release, p.slowMult, p.slowAmt, p.holdMs, p.avgMs, (double) p.element, p.driveDb, p.relCurve, p.crestDb, p.grLimit };
}

inline StepResult measureStep (Params p, double overDb, double fs = 48000.0)
{
    StepResult r; r.fs = fs;
    p.input = 0; p.mix = 1; p.makeup = 0; p.lookaheadMs = 0; p.bypass = false; p.os = 1;
    p.inXfmr = false; p.scHpfHz = 0; p.scTilt = 0; p.band = BandOff; p.out = OutClean; p.link = 1;
    p.element = ElClean;                                  // FET / tube elements block DC, which would starve a feedback detector of the test step
    Core c (fs); c.set (p);
    const double level = p.threshold + overDb;
    r.idealGr = lawGr (level, p);
    const float quiet = 1.0e-6f, loud = (float) std::pow (10.0, level / 20.0);
    const int chunk = 256;
    std::vector<float> in ((size_t) chunk), outL ((size_t) chunk), outR ((size_t) chunk), gr ((size_t) chunk), sl ((size_t) chunk);
    Taps tap; tap.gr = gr.data(); tap.slow = sl.data();
    std::vector<float> pre, on, off, preS, onS, offS;
    auto feed = [&] (float v, int total, std::vector<float>& dst, std::vector<float>& dstS, bool untilSteady, bool untilQuiet, double ref)
    {
        int done = 0, sinceCheck = 0; double lastCheck = -1;
        while (done < total)
        {
            const int n = std::min (chunk, total - done);
            std::fill (in.begin(), in.begin() + n, v);
            c.run (in.data(), in.data(), outL.data(), outR.data(), n, nullptr, nullptr, tap);
            dst.insert (dst.end(), gr.begin(), gr.begin() + n); dstS.insert (dstS.end(), sl.begin(), sl.begin() + n);
            done += n; sinceCheck += n;
            if (sinceCheck >= (int) (fs * 0.05) && done > (int) (fs * 0.1))
            {
                sinceCheck = 0; const double g = gr[(size_t) (n - 1)];
                if (untilSteady && std::abs (g - lastCheck) < 2e-4) break;
                if (untilQuiet && g < ref * 0.0005) break;
                lastCheck = g;
            }
        }
    };
    feed (quiet, (int) (fs * 0.06), pre, preS, false, false, 0);
    feed (loud, (int) (fs * 8.0), on, onS, true, false, 0);
    r.finalGr = on.empty() ? 0.0 : on.back();
    if (r.finalGr < 0.05) return r;                       // nothing to measure (ratio ≈ 1, or the level is below the knee)
    feed (quiet, (int) (fs * 20.0), off, offS, false, true, r.finalGr);

    auto firstAbove = [&] (double f) { for (size_t i = 0; i < on.size(); ++i) if (on[i] >= f * r.finalGr) return (double) i / fs * 1000.0; return -1.0; };
    auto firstBelow = [&] (double f) { for (size_t i = 0; i < off.size(); ++i) if (off[i] <= f * r.finalGr) return (double) i / fs * 1000.0; return -1.0; };
    r.a63 = firstAbove (1.0 - std::exp (-1.0)); r.a90 = firstAbove (0.9); r.a99 = firstAbove (0.99);
    r.r63 = firstBelow (std::exp (-1.0)); r.r99 = firstBelow (0.01);
    const double a99 = r.a99 > 0 ? r.a99 : (double) on.size() / fs * 1000.0, r99 = r.r99 > 0 ? r.r99 : (double) off.size() / fs * 1000.0;
    const size_t preN = (size_t) (r.preMs * 0.001 * fs), nA = std::min (on.size(), (size_t) (a99 * 1.08 * 0.001 * fs) + 8), nR = std::min (off.size(), (size_t) (r99 * 1.08 * 0.001 * fs) + 8);
    r.attack.assign (preN, 0.0f);
    r.attack.insert (r.attack.end(), on.begin(), on.begin() + (std::ptrdiff_t) nA);
    r.release.assign (off.begin(), off.begin() + (std::ptrdiff_t) nR);
    if (p.timing == TimProg2 || p.timing == TimAuto || p.timing == TimOptoMem || p.det == DetCrest) r.slowRel.assign (offS.begin(), offS.begin() + (std::ptrdiff_t) nR);
    r.valid = true;
    return r;
}
} // namespace mod
