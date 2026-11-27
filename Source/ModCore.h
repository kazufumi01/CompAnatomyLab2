#pragma once
// =============================================================================
//  Modular compressor engine. Seven stages, each with a selectable method:
//    ① input (gain, transformer)          ② sidechain (ext, HPF, tilt, band, stereo link)
//    ③ detector (peak / RMS / diode+RC, feed-forward / feedback)
//    ④ gain law (ideal / FET / opto / vari-mu / diode)
//    ⑤ timing (one-pole / 2-stage program / opto memory / auto / precise look-ahead / dual-average)
//    ⑥ gain element (clean / FET / push-pull tube / opto cell / diode bridge), oversampled 1x / 2x / 4x
//    ⑦ output (clean / transformer / tube / 2nd / 3rd harmonic), makeup, dry-wet, band-only
//  A hardware "model" is just a set of these choices plus constants (Models.h).
//  No JUCE dependency. With every stage on its ideal method at 1x this equals comp::Core.
// =============================================================================
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace mod
{
constexpr double kPi = 3.14159265358979323846;
enum Det { DetPeak = 0, DetRMS, DetDiode, DetCrest };
enum Law { LawIdeal = 0, LawFET, LawOpto, LawVariMu, LawDiode, LawLimit, LawRide, LawSpectral, LawClip };
enum DynMode { DynComp = 0, DynExp, DynGate };
enum Timing { TimOne = 0, TimProg2, TimOptoMem, TimAuto, TimPrecise, TimDualAvg };
enum Element { ElClean = 0, ElFET, ElTube, ElOpto, ElDiode };
enum OutStage { OutClean = 0, OutXfmr, OutTube, OutH2, OutH3, OutNickel, OutIron, OutSteel };
enum Band { BandOff = 0, BandLP, BandHP, BandBP };

struct Params
{
    // ① input
    double input = 0; bool inXfmr = false;
    // ② sidechain
    double scHpfHz = 0;                 // < 20 = off
    int scTilt = 0;                     // 0 off, 1 medium, 2 loud (detector hears more highs, less lows)
    int band = BandOff; double bandHz = 6000, bandQ = 1.0;   // detector band filter
    bool bandOnly = false;              // apply the gain to that band only (de-esser style)
    double link = 1.0;                  // 0 = dual mono … 1 = fully linked
    bool scMb = false; double scSens[3] { 0, 0, 0 };   // multiband detection: the detector looks at three bands (split at xLo / xHi), each with its own sensitivity (dB); one gain for the whole signal
    int stereo = 0;                     // 0 L/R, 1 M/S, 2 mid only, 3 side only (encoding is done by mod::Multi; Core only skips the other channel for 2 / 3)
    // ③ detector
    int det = DetPeak; double rmsMs = 10; bool feedback = false;
    double crestDb = 4;                 // Peak + RMS detector: the peak path's threshold sits this much above the RMS path's
    // ④ gain law
    int law = LawIdeal; double threshold = -18, ratio = 4, knee = 6;
    bool upOn = false; double upThreshold = -40, upRatio = 2, upRange = 18;   // upward: raise what is below upThreshold, by at most upRange dB
    // ⑤ timing
    int timing = TimOne; double attack = 10, release = 120;
    double slowMult = 8, slowAmt = 0.5; // 2-stage / dual-average: slow release = release × slowMult
    double holdMs = 0, lookaheadMs = 0, avgMs = 100;   // lookaheadMs < 0 = look-behind: the gain change is delayed, so transients pass
    double relCurve = 0;                // release shape: 0 = exponential (fast first, then slow), 1 = linear in dB (constant dB per second)
    double grLimit = 40;                // most gain reduction allowed, dB (40 = off)
    // ⑥ element
    int element = ElClean; double driveDb = 0, analogPct = 100; int os = 1;
    // ⑦ output
    int out = OutClean; double makeup = 0, mix = 1; bool bypass = false;
    // serial pre-stage (used by mod::Multi): an opto leveller, fixed 2:1, in front of the main compressor
    bool pre = false; double preThr = -20;
    // 3-band mode (used by mod::Multi, ignored by Core)
    bool mb = false; double xLo = 120, xHi = 2500, bandGain[3] { 0, 0, 0 };
    // Auto release and dynamics mode
    bool autoRel = false;
    int dynMode = DynComp;
    // Precision digital & analog extensions (appended; defaults match legacy behaviour)
    bool tpDet = false;                 // 4x Hermite true-peak detector
    double attCurve = 0.0;              // 0 = exponential attack, 1 = linear-dB / transient-preserving ramp
    double crestRel = 0.0;              // 0..1 continuous crest-factor adaptive release
    bool fbPost = false;                // true = feedback taps after output stage & tone IR
    int toneIr = 0;                     // 0 Off, 1 76 Blue IR, 2 76 All-Btn IR, 3 Warm Tube, 4 Console Xfmr, 5 User IR

    bool operator== (const Params& o) const
    {
        return input == o.input && inXfmr == o.inXfmr && scHpfHz == o.scHpfHz && scTilt == o.scTilt
            && band == o.band && bandHz == o.bandHz && bandQ == o.bandQ && bandOnly == o.bandOnly
            && link == o.link && scMb == o.scMb && scSens[0] == o.scSens[0] && scSens[1] == o.scSens[1] && scSens[2] == o.scSens[2]
            && stereo == o.stereo && det == o.det && rmsMs == o.rmsMs && feedback == o.feedback
            && crestDb == o.crestDb && law == o.law && threshold == o.threshold && ratio == o.ratio && knee == o.knee
            && upOn == o.upOn && upThreshold == o.upThreshold && upRatio == o.upRatio && upRange == o.upRange
            && timing == o.timing && attack == o.attack && release == o.release
            && slowMult == o.slowMult && slowAmt == o.slowAmt && holdMs == o.holdMs && lookaheadMs == o.lookaheadMs
            && avgMs == o.avgMs && relCurve == o.relCurve && grLimit == o.grLimit
            && element == o.element && driveDb == o.driveDb && analogPct == o.analogPct && os == o.os
            && out == o.out && makeup == o.makeup && mix == o.mix && bypass == o.bypass
            && pre == o.pre && preThr == o.preThr
            && mb == o.mb && xLo == o.xLo && xHi == o.xHi
            && bandGain[0] == o.bandGain[0] && bandGain[1] == o.bandGain[1] && bandGain[2] == o.bandGain[2]
            && autoRel == o.autoRel && dynMode == o.dynMode
            && tpDet == o.tpDet && attCurve == o.attCurve && crestRel == o.crestRel && fbPost == o.fbPost && toneIr == o.toneIr;
    }
    bool operator!= (const Params& o) const { return ! (*this == o); }
};

// ---- ④ static gain law: gain reduction (dB, ≥ 0) for a detector level (dB) ----
inline double idealGr (double x, double T, double R, double W)
{
    const double d = x - T;
    if (2 * d < -W) return 0.0;
    if (W > 0 && 2 * std::abs (d) <= W) return (1.0 - 1.0 / R) * (d + W / 2) * (d + W / 2) / (2 * W);
    return d - d / R;
}
// FET: the ratio buttons also move the threshold and harden the knee (calibrated to 1176 measurements)
struct FetStep { double ratio, dT, knee; };
inline FetStep fetStep (double ratio)
{
    if (ratio >= 30) return { 10.0, 4.0, 2.0 };      // all buttons
    if (ratio >= 16) return { 15.0, 6.0, 2.5 };      // 20:1
    if (ratio >= 10) return { 9.5, 4.0, 4.0 };       // 12:1
    if (ratio >= 6)  return { 6.5, 2.0, 5.0 };       // 8:1
    return { 4.0, 0.0, 6.0 };                        // 4:1
}
inline double lawGrRaw (double x, const Params& p)
{
    switch (p.law)
    {
        // gain riding: move the level to the target (threshold), by at most ±upRange; input below upThreshold is idle (no change). Negative = gain up.
        case LawRide:   return x < p.upThreshold ? 0.0 : -std::max (-p.upRange, std::min (p.upRange, p.threshold - x));
        case LawFET:    { const auto s = fetStep (p.ratio);
                          const double g = idealGr (x, p.threshold + s.dT, s.ratio, std::max (0.0, s.knee + (p.knee - 6.0)));
                          return p.feedback ? s.ratio * g : g; }
        case LawOpto:   { const double o = x - p.threshold + 4.0; const double k = std::max (0.1, 8.0 * (p.knee / 6.0));
                          const double g = o <= 0 ? 0.0 : (1.0 - 1.0 / p.ratio) * o * o / (o + k);
                          return p.feedback ? std::max (1.0, p.ratio) * g : g; }
        case LawVariMu: { const double o = x - p.threshold + 6.0; const double k = std::max (0.1, 12.0 * (p.knee / 6.0));
                          const double g = o <= 0 ? 0.0 : (1.0 - 1.0 / p.ratio) * o * o / (o + k);
                          return p.feedback ? std::max (1.0, p.ratio) * g : g; }
        case LawDiode:  { const double g = idealGr (x, p.threshold, p.ratio, std::max (0.2, 8.0 + (p.knee - 6.0)));
                          return p.feedback ? std::max (1.0, p.ratio) * g : g; }
        case LawLimit:  return p.knee <= 0.0 ? std::max (0.0, x - p.threshold) : idealGr (x, p.threshold, 1.0e6, p.knee);
        case LawClip:   return (p.ratio >= 99.5 && p.knee <= 0.0) ? std::max (0.0, x - p.threshold) : idealGr (x, p.threshold, p.ratio >= 99.5 ? 1.0e6 : p.ratio, p.knee);
        default:        return idealGr (x, p.threshold, p.ratio, p.knee);
    }
}
inline double lawGr (double x, const Params& p) { return std::min (lawGrRaw (x, p), p.grLimit >= 39.95 ? 1.0e9 : p.grLimit); }

// Real-time effective knee width (dB) across all laws / elements
inline double effectiveKnee (const Params& p)
{
    switch (p.law)
    {
        case LawIdeal:    return p.knee;
        case LawFET:      { const auto s = fetStep (p.ratio); return std::max (0.0, s.knee + (p.knee - 6.0)); }
        case LawOpto:     return std::max (0.1, 8.0 * (p.knee / 6.0)) * 1.25;
        case LawVariMu:   return std::max (0.1, 12.0 * (p.knee / 6.0)) * 1.20;
        case LawDiode:    return std::max (0.2, 8.0 + (p.knee - 6.0));
        case LawLimit:    return p.knee;
        case LawClip:     return p.knee;
        case LawSpectral: return p.knee;
        default:          return p.knee;
    }
}

// Downward Expander: attenuates signals below threshold with smooth knee transition
inline double expanderGr (double x, const Params& p)
{
    const double T = p.threshold, R = std::max (1.0, p.ratio), W = effectiveKnee (p);
    const double d = T - x; // amount below threshold (dB)
    if (d <= -W / 2.0) return 0.0;
    double gr = 0.0;
    if (W > 0.0 && std::abs (d) <= W / 2.0)
        gr = (R - 1.0) * (d + W / 2.0) * (d + W / 2.0) / (2.0 * W);
    else
        gr = (R - 1.0) * d;
    return std::min (p.grLimit >= 39.95 ? 60.0 : p.grLimit, std::max (0.0, gr));
}

// Noise Gate: steep attenuation below threshold with smooth transition
inline double gateGr (double x, const Params& p)
{
    const double T = p.threshold, W = std::max (0.5, effectiveKnee (p));
    const double floorDb = std::min (p.grLimit >= 39.95 ? 60.0 : p.grLimit, 60.0);
    const double d = T - x;
    if (d <= -W / 2.0) return 0.0;
    if (d >= W / 2.0) return floorDb;
    const double t = (d + W / 2.0) / W;
    const double smooth = t * t * (3.0 - 2.0 * t);
    return smooth * floorDb;
}

// Combined dynamics gain reduction (Compressor / Expander / Gate)
inline double dynLawGr (double x, const Params& p)
{
    if (p.dynMode == DynExp)  return expanderGr (x, p);
    if (p.dynMode == DynGate) return gateGr (x, p);
    return lawGr (x, p);
}
// upward compression: boost (dB, ≥ 0) for a detector level below upThreshold. Very quiet input (< −70 dB) is not lifted.
inline double lawUp (double x, const Params& p)
{
    if (! p.upOn || p.law == LawRide || p.upRatio <= 1.0 || x >= p.upThreshold) return 0.0;
    double b = std::min (p.upRange, (p.upThreshold - x) * (1.0 - 1.0 / p.upRatio));
    if (x < -70.0) b *= std::max (0.0, (x + 90.0) / 20.0);
    return b;
}
inline double steadyGr (double x, const Params& p)
{
    const double ff = dynLawGr (x, p);
    if (! p.feedback || p.law == LawRide || p.law == LawLimit || p.law == LawClip || p.law == LawSpectral || p.dynMode != DynComp)
        return ff;
    double lo = 0.0, hi = std::max (0.0, ff);
    for (int i = 0; i < 28; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        if (lawGr (x - mid, p) > mid) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}
inline double lawOut (double x, const Params& p) { return x - steadyGr (x, p) + lawUp (x, p); }

// ---- small parts ----
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z[2][2] {};
    void reset() { z[0][0] = z[0][1] = z[1][0] = z[1][1] = 0; }
    void bypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    double run (int c, double x) { const double y = b0 * x + z[c][0]; z[c][0] = b1 * x - a1 * y + z[c][1]; z[c][1] = b2 * x - a2 * y; return y; }
    void set (int type, double f, double q, double fs)                    // BandLP / BandHP / BandBP (RBJ)
    {
        const double w = 2.0 * kPi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * std::max (0.1, q)), a0 = 1.0 + al;
        if (type == BandLP)      { b0 = (1 - c) * 0.5 / a0; b1 = (1 - c) / a0; b2 = b0; }
        else if (type == BandHP) { b0 = (1 + c) * 0.5 / a0; b1 = -(1 + c) / a0; b2 = b0; }
        else                     { b0 = al / a0; b1 = 0; b2 = -al / a0; }
        a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
    }
    void peak (double f, double gainDb, double q, double fs)
    {
        const double w0 = 2.0 * kPi * std::min (f, fs * 0.45) / fs, c = std::cos (w0), s = std::sin (w0);
        const double A = std::pow (10.0, gainDb / 40.0), al = s / (2.0 * std::max (0.1, q)), a0 = 1.0 + al / A;
        b0 = (1.0 + al * A) / a0; b1 = (-2.0 * c) / a0; b2 = (1.0 - al * A) / a0; a1 = (-2.0 * c) / a0; a2 = (1.0 - al / A) / a0;
    }
    void shelf (bool high, double f, double gainDb, double q, double fs)
    {
        const double w0 = 2.0 * kPi * std::min (f, fs * 0.45) / fs, c = std::cos (w0), s = std::sin (w0);
        const double A = std::pow (10.0, gainDb / 40.0), al = s / (2.0 * std::max (0.1, q)), sq = 2.0 * std::sqrt (A) * al;
        if (! high)
        {
            const double a0 = (A + 1.0) + (A - 1.0) * c + sq;
            b0 = A * ((A + 1.0) - (A - 1.0) * c + sq) / a0; b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * c) / a0;
            b2 = A * ((A + 1.0) - (A - 1.0) * c - sq) / a0; a1 = -2.0 * ((A - 1.0) + (A + 1.0) * c) / a0; a2 = ((A + 1.0) + (A - 1.0) * c - sq) / a0;
        }
        else
        {
            const double a0 = (A + 1.0) - (A - 1.0) * c + sq;
            b0 = A * ((A + 1.0) + (A - 1.0) * c + sq) / a0; b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * c) / a0;
            b2 = A * ((A + 1.0) + (A - 1.0) * c - sq) / a0; a1 = 2.0 * ((A - 1.0) - (A + 1.0) * c) / a0; a2 = ((A + 1.0) - (A - 1.0) * c - sq) / a0;
        }
    }
    void tilt (double lowDb, double highDb, double fs)                    // first-order shelf centred on 1 kHz
    {
        const double gL = std::pow (10.0, lowDb / 20.0), gH = std::pow (10.0, highDb / 20.0);
        const double K = std::tan (kPi * std::min (1000.0 * std::sqrt (gH / gL), fs * 0.45) / fs);
        b0 = (gH + gL * K) / (1 + K); b1 = (gL * K - gH) / (1 + K); b2 = 0; a1 = (K - 1) / (1 + K); a2 = 0;
    }
};

struct Fir                                                                // windowed-sinc low-pass (Kaiser)
{
    std::vector<double> h, buf; int w = 0, n = 0;
    static double i0 (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; }
    void design (int taps, double fc, double gain)
    {
        n = taps; h.assign ((size_t) n, 0.0); buf.assign ((size_t) n, 0.0); w = 0;
        const double beta = 9.0, m = (n - 1) * 0.5; double sum = 0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i - m, s = std::abs (t) < 1e-12 ? 2.0 * fc : std::sin (2.0 * kPi * fc * t) / (kPi * t);
            const double r = t / m; h[(size_t) i] = s * i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta); sum += h[(size_t) i];
        }
        for (auto& v : h) v *= gain / sum;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0); w = 0; }
    double run (double x)
    {
        buf[(size_t) w] = x; double y = 0; int k = w;
        for (int i = 0; i < n; ++i) { y += h[(size_t) i] * buf[(size_t) k]; if (--k < 0) k = n - 1; }
        if (++w >= n) w = 0;
        return y;
    }
};

struct SlideMax                                                           // running maximum over the last `win` samples
{
    std::vector<double> v; std::vector<long long> ix; int head = 0, count = 0, cap = 4, win = 1; long long t = 0;
    void setup (int maxWin) { cap = maxWin + 4; v.assign ((size_t) cap, 0.0); ix.assign ((size_t) cap, 0); reset(); }
    void reset() { head = count = 0; t = 0; }
    double run (double x)
    {
        while (count > 0 && v[(size_t) ((head + count - 1) % cap)] <= x) --count;
        const int tail = (head + count) % cap; v[(size_t) tail] = x; ix[(size_t) tail] = t; ++count;
        while (count > 1 && ix[(size_t) head] <= t - win) { head = (head + 1) % cap; --count; }
        ++t; return v[(size_t) head];
    }
};
struct Boxcar                                                             // moving average over `len` samples
{
    std::vector<double> b; int w = 0, len = 1; double sum = 0;
    void setup (int maxLen) { b.assign ((size_t) maxLen + 2, 0.0); reset(); }
    void reset() { std::fill (b.begin(), b.end(), 0.0); w = 0; sum = 0; }
    void setLen (int l) { l = std::max (1, std::min (l, (int) b.size() - 1)); if (l != len) { len = l; reset(); } }
    double run (double x)
    {
        const int size = (int) b.size(); int r = w - len; if (r < 0) r += size;
        sum += x - b[(size_t) r]; b[(size_t) w] = x;
        if (++w >= size) { w = 0; sum = 0; for (int i = 0; i < len; ++i) sum += b[(size_t) (size - 1 - i)]; }   // stop drift
        return sum / len;
    }
};

struct Taps { float* pre = nullptr; float* gr = nullptr; float* up = nullptr; float* slow = nullptr; float* inAbs = nullptr; float* detL = nullptr; float* detR = nullptr; };
struct Meters { float inPk = 0, outPk = 0, grMax = 0, upMax = 0; };

// =============================================================================
class Core
{
public:
    explicit Core (double sampleRate) : fsBase (sampleRate)
    {
        const int maxD = (int) std::ceil (sampleRate * 4 * 0.012) + 8, maxW = (int) std::ceil (sampleRate * 4 * 0.065) + 8;
        for (int c = 0; c < 2; ++c)
        {
            dly[c].assign ((size_t) maxD, 0.0); gdl[c].assign ((size_t) maxD, 0.0); smax[c].setup (maxW); box[c].setup (maxD);
            up1[c].design (63, 0.25, 2.0); dn1[c].design (63, 0.25, 1.0); up2[c].design (17, 0.25, 2.0); dn2[c].design (17, 0.25, 1.0);
        }
        dsize = maxD; set (Params {});
    }

    void set (const Params& np)
    {
        const bool osChanged = np.os != p.os;
        p = np; os = p.os >= 4 ? 4 : p.os >= 2 ? 2 : 1; fs = fsBase * os;
        if (osChanged) reset();
        auto tc = [this] (double ms) { return std::exp (-1.0 / (std::max (ms, 0.001) * 0.001 * fs)); };
        aA = tc (p.attack); aR = tc (p.release); aM = tc (p.rmsMs); aAvg = tc (p.avgMs);
        const bool autoT = p.timing == TimAuto || p.autoRel;
        aSlowUp = tc (autoT ? 350.0 : 350.0);
        aSlowDn = tc (autoT ? (p.autoRel ? std::max (p.release * 2.5, 600.0) : 1200.0) : p.release * p.slowMult);
        aFastDn = autoT ? tc (p.autoRel ? std::min (p.release * 0.5, 80.0) : 75.0) : aR;
        slowAmt = autoT ? 0.65 : p.slowAmt;
        oSA = tc (30); oMU = tc (1500); oMD = tc (4000);
        aDiodeA = tc (std::max (p.attack * 2.5, 1.4)); aDiodeR = tc (26.0);
        aCrPk = tc (8.0); aCrRms = tc (60.0);
        dcR = std::exp (-2.0 * kPi * 8.0 / fs); xfK = 1.0 - std::exp (-2.0 * kPi * 120.0 / fs);
        xfTone1.peak (115.0, 0.2, 0.9, fs);
        xfTone2.peak (14000.0, 0.3, 1.3, fs);
        for (auto& b : irBq) b.bypass();
        if (p.toneIr == 1)      // 1176 Blue Stripe IR profile (8 8 4 a4 r4)
        {
            irBq[0].shelf (false, 75.5, -3.7, 0.95, fs); irBq[1].peak (53.3, 1.8, 0.96, fs);
            irBq[2].peak (14300.0, 2.0, 1.35, fs);       irBq[3].shelf (true, 18400.0, -3.2, 1.1, fs);
        }
        else if (p.toneIr == 2) // 1176 All-Buttons IR profile
        {
            irBq[0].shelf (false, 30.0, -9.5, 0.68, fs); irBq[1].peak (116.7, 4.3, 0.95, fs);
            irBq[2].peak (423.6, 1.1, 1.03, fs);         irBq[3].shelf (true, 2200.0, 2.2, 0.5, fs);
        }
        else if (p.toneIr == 3) // Warm Tube (Opto / Vari-Mu character)
        {
            irBq[0].shelf (false, 32.0, -1.5, 0.7, fs);  irBq[1].peak (62.0, 0.9, 0.85, fs);
            irBq[2].peak (240.0, 0.4, 0.8, fs);          irBq[3].shelf (true, 11500.0, -1.3, 0.7, fs);
        }
        else if (p.toneIr == 4) // Console Transformer (British / American iron)
        {
            irBq[0].shelf (false, 35.0, -1.8, 0.75, fs); irBq[1].peak (68.0, 1.0, 1.0, fs);
            irBq[2].peak (3200.0, 0.35, 0.9, fs);        irBq[3].shelf (true, 14500.0, 0.9, 0.8, fs);
        }
        laBase = (int) std::lround (std::max (0.0, p.lookaheadMs) * 0.001 * fsBase); la = std::min (dsize - 2, laBase * os); laBase = la / os;
        lb = std::min (dsize - 2, (int) std::lround (std::max (0.0, -p.lookaheadMs) * 0.001 * fs));
        attLinStep = 6.3 / (std::max (p.attack, 0.02) * 0.001 * fs);
        linStep = 6.3 / (std::max (p.release, 0.001) * 0.001 * fs);          // linear release: 6.3 dB per release time (= the exponential's first 63 % of 10 dB)
        aCrest = tc (p.release * p.slowMult);
        kHf = 1.0 - std::exp (-2.0 * kPi * std::min (9000.0, fs * 0.4) / fs); kLo = 1.0 - std::exp (-2.0 * kPi * 140.0 / fs);
        const int holdN = (int) std::lround (p.holdMs * 0.001 * fs);
        for (int c = 0; c < 2; ++c) { smax[c].win = std::max (1, std::min (smax[c].cap - 4, la + holdN + 1)); box[c].setLen (la + 1); }
        thrLin = std::pow (10.0, p.threshold / 20.0);
        gIn = std::pow (10.0, p.input / 20.0); gMk = std::pow (10.0, p.makeup / 20.0); drv = std::pow (10.0, p.driveDb / 20.0);
        if (p.scMb)
        {
            const double f1 = std::min (std::max (p.xLo, 30.0), 1000.0), f2 = std::min (std::max (p.xHi, f1 * 2.0), fs * 0.4);
            scLp.set (BandLP, f1, 0.70710678, fs); scHp.set (BandHP, f2, 0.70710678, fs);
            for (int b = 0; b < 3; ++b) scW[b] = std::pow (10.0, p.scSens[b] / 20.0);
        }
        hpfOn = p.scHpfHz >= 20.0; if (hpfOn) hpf.set (BandHP, p.scHpfHz, 0.7071, fs);
        if (p.scTilt > 0) tilt.tilt (p.scTilt == 2 ? -10.0 : -4.0, p.scTilt == 2 ? 10.0 : 4.0, fs);
        if (p.band != BandOff) { bandSc.set (p.band, p.bandHz, p.bandQ, fs); bandAu.b0 = bandSc.b0; bandAu.b1 = bandSc.b1; bandAu.b2 = bandSc.b2; bandAu.a1 = bandSc.a1; bandAu.a2 = bandSc.a2; }
    }
    const Params& params() const { return p; }
    int latencySamples() const { return laBase + (os == 4 ? 35 : os == 2 ? 31 : 0); }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill (dly[c].begin(), dly[c].end(), 0.0); std::fill (gdl[c].begin(), gdl[c].end(), 0.0); smax[c].reset(); box[c].reset();
            up1[c].reset(); dn1[c].reset(); up2[c].reset(); dn2[c].reset();
            ms[c] = y1[c] = y2[c] = gS[c] = mem[c] = avg[c] = gs[c] = fb[c] = yUp[c] = yRms[c] = diodeEnv[c] = diodeEnv2[c] = crPk[c] = crRm[c] = 0;
            xfIn[c] = xfOut[c] = xfLp[c] = adaaEl[c] = adaaElNeg[c] = adaaOut[c] = 0;
            for (int k = 0; k < 4; ++k) tpH[c][k] = 0;
            for (auto& d : dc[c]) d[0] = d[1] = 0;
        }
        scLp.reset(); scHp.reset(); xfTone1.reset(); xfTone2.reset(); for (auto& b : irBq) b.reset();
        for (auto& b : msB) b[0] = b[1] = 0; yRide[0] = yRide[1] = 0; yEnv[0] = yEnv[1] = -140.0;
        hpf.reset(); tilt.reset(); bandSc.reset(); bandAu.reset(); w = 0;
    }

    Meters run (const float* iL, const float* iR, float* oL, float* oR, int n,
                const float* scL = nullptr, const float* scR = nullptr, const Taps& tap = {})
    {
        Meters m;
        for (int i = 0; i < n; ++i)
        {
            double x[2] { iL[i], iR[i] }, y[2] { 0, 0 }, e[2] { 0, 0 };
            const bool ext = scL != nullptr;
            if (ext) { e[0] = scL[i]; e[1] = scR != nullptr ? scR[i] : scL[i]; }
            tGr = 0; tSlow = 0; tIn = 0; tUp = 0;
            if (os == 1) tick (x, e, ext, y);
            else
            {
                double a[2][2];
                for (int c = 0; c < 2; ++c) { a[c][0] = up1[c].run (x[c]); a[c][1] = up1[c].run (0.0); }
                for (int k = 0; k < 2; ++k)
                {
                    double v[2] { a[0][k], a[1][k] }, o[2];
                    if (os == 2) tick (v, e, ext, o);
                    else
                    {
                        double b[2][2], q[2];
                        for (int c = 0; c < 2; ++c) { b[c][0] = up2[c].run (v[c]); b[c][1] = up2[c].run (0.0); }
                        for (int j = 0; j < 2; ++j)
                        {
                            double u[2] { b[0][j], b[1][j] }; tick (u, e, ext, q);
                            for (int c = 0; c < 2; ++c) { const double d = dn2[c].run (q[c]); if (j == 0) o[c] = d; }
                        }
                    }
                    for (int c = 0; c < 2; ++c) { const double d = dn1[c].run (o[c]); if (k == 0) y[c] = d; }
                }
            }
            oL[i] = (float) y[0]; oR[i] = (float) y[1];
            m.inPk = std::max (m.inPk, (float) tIn); m.outPk = std::max (m.outPk, (float) std::max (std::abs (y[0]), std::abs (y[1]))); m.grMax = std::max (m.grMax, (float) tGr); m.upMax = std::max (m.upMax, (float) tUp);
            if (tap.up != nullptr) tap.up[i] = (float) tUp;
            if (tap.gr != nullptr) tap.gr[i] = (float) tGr;
            if (tap.slow != nullptr) tap.slow[i] = (float) tSlow;
            if (tap.inAbs != nullptr) tap.inAbs[i] = (float) tIn;
            if (tap.detL != nullptr) { tap.detL[i] = (float) tDet[0]; tap.detR[i] = (float) tDet[1]; }
        }
        return m;
    }

private:
    double dcBlock (int c, int k, double u) { auto& d = dc[c][k]; const double y = u - d[0] + dcR * d[1]; d[0] = u; d[1] = y; return y; }
    double analogMix (double dry, double modeled) const
    {
        const double amount = std::max (0.0, std::min (2.0, p.analogPct * 0.01));
        return dry + (modeled - dry) * amount;
    }
    // 1st-order ADAA for tanh(x)
    static double adaaTanh (double x, double& prev)
    {
        auto F = [] (double u) { const double a = std::abs (u); return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; };
        const double d = x - prev;
        const double y = std::abs (d) < 1e-5 ? std::tanh (0.5 * (x + prev)) : (F (x) - F (prev)) / d;
        prev = x;
        return y;
    }
    // 4x Catmull-Rom inter-sample true-peak amplitude estimator
    double truePeakAbs (int c, double s)
    {
        auto* h = tpH[c];
        h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = s;
        double pk = std::abs (s);
        const double a0 = -0.5 * h[0] + 1.5 * h[1] - 1.5 * h[2] + 0.5 * h[3];
        const double a1 = h[0] - 2.5 * h[1] + 2.0 * h[2] - 0.5 * h[3];
        const double a2 = -0.5 * h[0] + 0.5 * h[2];
        const double a3 = h[1];
        for (double t : { 0.25, 0.5, 0.75 })
            pk = std::max (pk, std::abs (((a0 * t + a1) * t + a2) * t + a3));
        return pk;
    }
    // transformer: only the lows saturate (flux grows as frequency falls) + magnetic hysteresis & saturation detail
    double xfmr (double x, double& st, double d, double asym = 0.020)
    {
        st += xfK * (x - st);
        const double sat = std::tanh (d * st + asym * st * st) / d;
        return (x - st) + sat;
    }

    // attack / release step. Blends attack (attCurve) and release (relCurve) between exponential and linear-in-dB.
    double follow (double y, double u, double aAtt, double aRel) const
    {
        if (u > y)
        {
            const double e = aAtt * y + (1.0 - aAtt) * u;
            if (p.attCurve <= 0.0) return e;
            return e + p.attCurve * (std::min (u, y + attLinStep) - e);
        }
        const double e = aRel * y + (1.0 - aRel) * u;
        if (p.relCurve <= 0.0) return e;
        return e + p.relCurve * (std::max (u, y - linStep) - e);
    }
    double effectiveRelCoef (int c, double baseCoef, double baseMs) const
    {
        if (p.crestRel <= 0.0) return baseCoef;
        const double crestDb = 20.0 * std::log10 (std::max (crPk[c], 1e-7) / std::sqrt (std::max (crRm[c], 1e-14)));
        // High crest (> 8 dB transient) -> faster release (down to 0.35x); low crest (< 4 dB sustain) -> slower (up to 2.4x)
        const double norm = std::max (-1.0, std::min (1.0, (6.0 - crestDb) / 6.0));
        const double mult = norm >= 0.0 ? (1.0 + 1.4 * norm * p.crestRel) : (1.0 + 0.65 * norm * p.crestRel);
        return std::exp (-1.0 / (std::max (baseMs * mult, 0.5) * 0.001 * fs));
    }
    // ⑤ timing: follows the target u (GR in dB)
    double ballistics (int c, double u, double grNow)
    {
        const double rCoef = effectiveRelCoef (c, aR, p.release);
        const double rFast = effectiveRelCoef (c, aFastDn, p.timing == TimAuto ? 75.0 : p.release);
        if (p.autoRel && p.timing != TimAuto && p.timing != TimProg2)
        {
            y1[c] = follow (y1[c], u, aA, rFast);
            const double s = u > y2[c] ? aSlowUp : aSlowDn; y2[c] = s * y2[c] + (1 - s) * u;
            slowNow[c] = slowAmt * y2[c];
            return std::max (y1[c], slowNow[c]);
        }
        switch (p.timing)
        {
            case TimProg2:
            {
                y1[c] = follow (y1[c], u, aA, rFast);
                const double s = u > y2[c] ? aSlowUp : aSlowDn; y2[c] = s * y2[c] + (1 - s) * u;
                const double slowPeak = std::max (y1[c], y2[c]);
                slowNow[c] = slowAmt * slowPeak;
                return (1.0 - slowAmt) * y1[c] + slowAmt * slowPeak;
            }
            case TimAuto:
            {
                y1[c] = follow (y1[c], u, aA, rFast);
                const double s = u > y2[c] ? aSlowUp : aSlowDn; y2[c] = s * y2[c] + (1 - s) * u;
                slowNow[c] = slowAmt * y2[c];
                return std::max (y1[c], slowNow[c]);
            }
            case TimOptoMem:                                        // EL panel + CdS cell: fast half + slow half with memory
            {
                const double aF = u > y1[c] ? aA : rCoef; y1[c] = aF * y1[c] + (1 - aF) * u;
                const double aS = u > gS[c] ? oSA : std::exp (-1.0 / ((0.5 + 4.5 * mem[c]) * fs)); gS[c] = aS * gS[c] + (1 - aS) * u;
                const double tgt = std::min (1.0, (p.det == DetDiode ? grNow : u) / 12.0), aMm = tgt > mem[c] ? oMU : oMD;
                mem[c] = aMm * mem[c] + (1 - aMm) * tgt; slowNow[c] = 0.5 * gS[c];
                return 0.5 * y1[c] + 0.5 * gS[c];
            }
            case TimPrecise:                                        // hold the peak, ramp linearly over the look-ahead, then release
            {
                const double b = box[c].run (smax[c].run (u));
                y1[c] = follow (y1[c], b, aA, rCoef); slowNow[c] = 0;
                return y1[c];
            }
            case TimDualAvg:                                        // sustained reduction → slow release, transients → fast release
            {
                const double on = grNow > 0.1 ? 1.0 : 0.0; avg[c] = aAvg * avg[c] + (1 - aAvg) * on;
                const double rel = p.release * (1.0 + (p.slowMult - 1.0) * avg[c]);
                const double a = u > y1[c] ? aA : effectiveRelCoef (c, std::exp (-1.0 / (std::max (rel, 0.001) * 0.001 * fs)), rel);
                y1[c] = a * y1[c] + (1 - a) * u; slowNow[c] = avg[c];
                return y1[c];
            }
            default: { y1[c] = follow (y1[c], u, aA, rCoef); slowNow[c] = 0; return y1[c]; }
        }
    }

    // ⑥ gain element: where the level is actually turned down (1st-order ADAA calibrated to hardware measurements)
    double element (int c, double wv, double g)
    {
        if (p.analogPct <= 0.0) return wv;
        const double gn = std::min (g, 20.0) / 20.0;
        switch (p.element)
        {
            case ElFET:   { const double allBtn = (p.law == LawFET && p.ratio >= 30.0) ? 5.5 : 1.0;
                            const double dv = (0.21 + 0.45 * gn) * drv, a2 = (0.0052 + 0.012 * gn) * allBtn;
                            const double shaped = adaaTanh (dv * (wv + a2 * wv * wv), adaaEl[c]) / dv;
                            const double wet = dcBlock (c, 0, shaped);
                            return analogMix (wv, wet); }
            case ElTube:  { const double dv = (0.28 + 0.35 * gn) * drv, a2 = 0.16, mis = 0.025;    // push-pull Vari-Mu (Fairchild 670 H2/H3 balance)
                            const double pos = adaaTanh (dv * (wv + a2 * wv * wv), adaaEl[c]);
                            const double neg = adaaTanh (dv * (-wv + a2 * wv * wv), adaaElNeg[c]);
                            const double wet = dcBlock (c, 0, 0.5 * ((1.0 + mis) * pos - (1.0 - mis) * neg) / dv);
                            return analogMix (wv, wet); }
            case ElOpto:  { const double dv = (0.12 + 0.20 * gn) * drv;
                            const double wet = dcBlock (c, 0, wv - 0.015 * dv * wv * wv * wv);
                            return analogMix (wv, wet); }
            case ElDiode: { const double dv = (0.18 + 0.38 * gn) * drv, a2 = 0.006 + 0.012 * gn;   // diode bridge (DMG Zener / 2254 calibration)
                            const double shaped = adaaTanh (dv * (wv + a2 * wv * wv), adaaEl[c]) / dv;
                            const double wet = dcBlock (c, 0, shaped);
                            return analogMix (wv, wet); }
            default: return wv;
        }
    }
    double outStage (int c, double v)
    {
        double u = v;
        if (p.analogPct > 0.0)
        {
            switch (p.out)
            {
                case OutXfmr:   { double wet = dcBlock (c, 1, xfmr (v, xfOut[c], 0.55, 0.014)); wet = xfTone2.run (c, xfTone1.run (c, wet)); u = analogMix (v, wet); break; }
                case OutTube:   { const double wet = dcBlock (c, 1, adaaTanh (0.42 * (v + 0.042 * v * v), adaaOut[c]) / 0.42); u = analogMix (v, wet); break; }
                case OutH2:     { const double wet = dcBlock (c, 1, adaaTanh (0.52 * (v + 0.058 * v * v), adaaOut[c]) / 0.52); u = analogMix (v, wet); break; }
                case OutH3:     { const double wet = adaaTanh (0.65 * v, adaaOut[c]) / 0.65; u = analogMix (v, wet); break; }
                case OutNickel: { const double x = xfmr (v, xfOut[c], 0.6); xfLp[c] += kHf * (x - xfLp[c]); u = analogMix (v, x + 0.07 * (x - xfLp[c])); break; }
                case OutIron:   { double x = xfmr (v, xfOut[c], 1.2); x = dcBlock (c, 1, adaaTanh (0.55 * (x + 0.08 * x * x), adaaOut[c]) / 0.55);
                                  xfLp[c] += kLo * (x - xfLp[c]); u = analogMix (v, x + 0.10 * xfLp[c]); break; }
                case OutSteel:  { double x = xfmr (v, xfOut[c], 2.2); x = adaaTanh (0.95 * x, adaaOut[c]) / 0.95; xfLp[c] += kLo * 0.6 * (x - xfLp[c]); u = analogMix (v, x + 0.08 * xfLp[c]); break; }
                default: break;
            }
            if (p.toneIr >= 1 && p.toneIr <= 4)
            {
                double t = u;
                for (auto& b : irBq) t = b.run (c, t);
                u = analogMix (u, t);
            }
        }
        return u;
    }

    void tick (const double* in, const double* e, bool ext, double* out)
    {
        double x[2], lev[2], levR[2] { 0, 0 };
        for (int c = 0; c < 2; ++c)
        {
            x[c] = in[c] * gIn; if (p.inXfmr) x[c] = xfmr (x[c], xfIn[c], 0.45, 0.006);
            double s = ext ? e[c] : (p.feedback ? fb[c] : x[c]);
            if (hpfOn) s = hpf.run (c, s);
            if (p.scTilt > 0) s = tilt.run (c, s);
            if (p.band != BandOff) s = bandSc.run (c, s);
            tDet[c] = s;
            const double instAbs = p.tpDet ? truePeakAbs (c, s) : std::abs (s);
            if (p.crestRel > 0.0)
            {
                crPk[c] = instAbs > crPk[c] ? instAbs : aCrPk * crPk[c] + (1.0 - aCrPk) * instAbs;
                crRm[c] = aCrRms * crRm[c] + (1.0 - aCrRms) * s * s;
            }
            if (p.scMb)                                             // three detector bands, each weighted; the loudest decides
            {
                const double lo = scLp.run (c, s), hi = scHp.run (c, s), bnd[3] { lo, s - lo - hi, hi };
                const bool needR = p.det == DetRMS || p.det == DetCrest; double pk = 0, rm = 0;
                for (int b = 0; b < 3; ++b)
                {
                    pk = std::max (pk, std::abs (bnd[b]) * scW[b]);
                    if (needR) { msB[b][c] = aM * msB[b][c] + (1 - aM) * bnd[b] * bnd[b]; rm = std::max (rm, std::sqrt (msB[b][c]) * scW[b]); }
                }
                levR[c] = rm; lev[c] = p.det == DetRMS ? rm : pk;
            }
            else
            {
                if (p.det == DetRMS || p.det == DetCrest) { ms[c] = aM * ms[c] + (1 - aM) * s * s; levR[c] = std::sqrt (ms[c]); }
                lev[c] = p.det == DetRMS ? levR[c] : instAbs;
            }
        }
        if (p.stereo == 2) lev[1] = levR[1] = 0; else if (p.stereo == 3) lev[0] = levR[0] = 0;       // mid only / side only: the other channel is neither measured nor changed
        const double mx = std::max (lev[0], lev[1]), mxR = std::max (levR[0], levR[1]);
        tIn = std::max (tIn, std::max (std::abs (x[0]), std::abs (x[1])));
        for (int c = 0; c < 2; ++c)
        {
            const double l = p.link * mx + (1.0 - p.link) * lev[c];
            if (p.law == LawClip)                                   // a clipper is a compressor with no time constants: the gain follows every single sample
            {
                const double ax = std::abs (x[c]), am = std::max (std::abs (x[0]), std::abs (x[1]));
                const double lc = ((p.stereo == 2 && c == 1) || (p.stereo == 3 && c == 0)) ? 0.0 : p.link * am + (1.0 - p.link) * ax;
                gs[c] = dynLawGr (lc > 1e-7 ? 20.0 * std::log10 (lc) : -140.0, p); slowNow[c] = 0;
            }
            else if (p.law == LawRide)                              // gain riding: a slow fader towards the target level, held while the input is idle
            {
                const double ldb = l > 1e-7 ? 20.0 * std::log10 (l) : -140.0;
                if (ldb >= p.upThreshold) { const double t = -dynLawGr (ldb, p), a = t < yRide[c] ? aA : aR; yRide[c] = a * yRide[c] + (1 - a) * t; }
                gs[c] = -yRide[c]; slowNow[c] = 0;
            }
            else if (p.det == DetDiode)                             // rectifier + RC: smooth rectified voltage first, then dB ballistics
            {
                const double aD = l > diodeEnv[c] ? aDiodeA : aDiodeR;
                diodeEnv[c] = aD * diodeEnv[c] + (1.0 - aD) * l;
                gs[c] = ballistics (c, dynLawGr (diodeEnv[c] > 1e-7 ? 20.0 * std::log10 (diodeEnv[c]) : -140.0, p), gs[c]);
            }
            else if (p.det == DetCrest)                             // peak path (threshold raised by the crest setting) and RMS path, whichever asks for more
            {
                const double lr = p.link * mxR + (1.0 - p.link) * levR[c];
                const double gP = ballistics (c, dynLawGr ((l > 1e-7 ? 20.0 * std::log10 (l) : -140.0) - p.crestDb, p), gs[c]);
                const double tR = dynLawGr (lr > 1e-7 ? 20.0 * std::log10 (lr) : -140.0, p), a = tR > yRms[c] ? aA : aCrest;
                yRms[c] = a * yRms[c] + (1 - a) * tR; slowNow[c] = yRms[c];
                gs[c] = std::max (gP, yRms[c]);
            }
            else if (p.dynMode != DynComp)
            {
                // Expander / Gate: smooth input envelope first, then apply expansion / gating law
                const double inDb = l > 1e-7 ? 20.0 * std::log10 (l) : -140.0;
                yEnv[c] = inDb > yEnv[c] ? (aA * yEnv[c] + (1 - aA) * inDb) : (aR * yEnv[c] + (1 - aR) * inDb);
                const double targetGr = dynLawGr (yEnv[c], p);
                gs[c] = follow (gs[c], targetGr, aA, aFastDn); slowNow[c] = 0;
            }
            else gs[c] = ballistics (c, dynLawGr (l > 1e-7 ? 20.0 * std::log10 (l) : -140.0, p), gs[c]);

            dly[c][(size_t) w] = x[c]; int r = w - la; if (r < 0) r += dsize;
            if (p.upOn && p.law != LawRide)                         // upward: fast when the boost has to come down, slow when it rises
            {
                const double tu = lawUp (l > 1e-7 ? 20.0 * std::log10 (l) : -140.0, p), a = tu < yUp[c] ? aA : aR;
                yUp[c] = a * yUp[c] + (1 - a) * tu;
            }
            else yUp[c] = 0;
            gdl[c][(size_t) w] = gs[c] - yUp[c]; int rb = w - lb; if (rb < 0) rb += dsize;          // look-behind: the gain change arrives late
            const double d = dly[c][(size_t) r], g = std::pow (10.0, -gdl[c][(size_t) rb] / 20.0);
            if ((p.stereo == 2 && c == 1) || (p.stereo == 3 && c == 0)) { out[c] = d; fb[c] = d; continue; }
            double wet;
            if (p.band != BandOff && p.bandOnly) { const double b = bandAu.run (c, d); wet = (d - b) + element (c, b * g, std::max (0.0, gs[c])); }
            else wet = element (c, d * g, std::max (0.0, gs[c]));
            double o = outStage (c, wet);
            fb[c] = p.fbPost ? o : wet;
            if (p.law == LawLimit) o = std::max (-thrLin, std::min (thrLin, o));       // safety clip at the ceiling
            out[c] = p.bypass ? d : o * gMk * p.mix + d * (1.0 - p.mix);
            tGr = std::max (tGr, gs[c]); tSlow = std::max (tSlow, slowNow[c]); tUp = std::max (tUp, std::max (yUp[c], -gs[c]));
        }
        if (++w >= dsize) w = 0;
    }

    double fsBase, fs = 48000; int os = 1;
    Params p;
    std::vector<double> dly[2], gdl[2]; int dsize = 0, w = 0, la = 0, laBase = 0, lb = 0;
    SlideMax smax[2]; Boxcar box[2]; Fir up1[2], dn1[2], up2[2], dn2[2];
    Biquad hpf, tilt, bandSc, bandAu; bool hpfOn = false;
    Biquad scLp, scHp, xfTone1, xfTone2, irBq[4]; double scW[3] { 1, 1, 1 }, msB[3][2] {}, yRide[2] {}, yEnv[2] { -140.0, -140.0 };
    double ms[2] {}, y1[2] {}, y2[2] {}, gS[2] {}, mem[2] {}, avg[2] {}, gs[2] {}, fb[2] {}, slowNow[2] {}, xfIn[2] {}, xfOut[2] {}, adaaEl[2] {}, adaaElNeg[2] {}, adaaOut[2] {}, diodeEnv[2] {}, diodeEnv2[2] {}, crPk[2] {}, crRm[2] {}, tpH[2][4] {}, dc[2][2][2] {};
    double aA = 0, aR = 0, aM = 0, aAvg = 0, aSlowUp = 0, aSlowDn = 0, aFastDn = 0, slowAmt = 0.5, oSA = 0, oMU = 0, oMD = 0, aDiodeA = 0, aDiodeR = 0, aDiode2 = 0, aCrPk = 0, aCrRms = 0, dcR = 0, xfK = 0, gIn = 1, gMk = 1, drv = 1;
    double tGr = 0, tSlow = 0, tIn = 0, tUp = 0, tDet[2] {}, yUp[2] {}, thrLin = 1, yRms[2] {}, xfLp[2] {}, attLinStep = 0, linStep = 0, aCrest = 0, kHf = 0, kLo = 0;
};
} // namespace mod
