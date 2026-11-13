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
    int element = ElClean; double driveDb = 0; int os = 1;
    // ⑦ output
    int out = OutClean; double makeup = 0, mix = 1; bool bypass = false;
    // serial pre-stage (used by mod::Multi): an opto leveller, fixed 2:1, in front of the main compressor
    bool pre = false; double preThr = -20;
    // 3-band mode (used by mod::Multi, ignored by Core)
    bool mb = false; double xLo = 120, xHi = 2500, bandGain[3] { 0, 0, 0 };
};

// ---- ④ static gain law: gain reduction (dB, ≥ 0) for a detector level (dB) ----
inline double idealGr (double x, double T, double R, double W)
{
    const double d = x - T;
    if (2 * d < -W) return 0.0;
    if (W > 0 && 2 * std::abs (d) <= W) return (1.0 - 1.0 / R) * (d + W / 2) * (d + W / 2) / (2 * W);
    return d - d / R;
}
// FET: the ratio buttons also move the threshold and harden the knee
struct FetStep { double ratio, dT, knee; };
inline FetStep fetStep (double ratio)
{
    if (ratio >= 30) return { 20.0, 3.0, 1.0 };      // all buttons
    if (ratio >= 16) return { 20.0, 5.0, 2.0 };
    if (ratio >= 10) return { 12.0, 3.5, 4.0 };
    if (ratio >= 6)  return { 8.0, 2.0, 6.0 };
    return { 4.0, 0.0, 8.0 };
}
inline double lawGrRaw (double x, const Params& p)
{
    switch (p.law)
    {
        // gain riding: move the level to the target (threshold), by at most ±upRange; input below upThreshold is idle (no change). Negative = gain up.
        case LawRide:   return x < p.upThreshold ? 0.0 : -std::max (-p.upRange, std::min (p.upRange, p.threshold - x));
        case LawFET:    { const auto s = fetStep (p.ratio); return idealGr (x, p.threshold + s.dT, s.ratio, s.knee); }
        case LawOpto:   { const double o = x - p.threshold + 4.0;  return o <= 0 ? 0.0 : (1.0 - 1.0 / p.ratio) * o * o / (o + 8.0); }
        case LawVariMu: { const double o = x - p.threshold + 6.0;  return o <= 0 ? 0.0 : (1.0 - 1.0 / p.ratio) * o * o / (o + 12.0); }
        case LawDiode:  return idealGr (x, p.threshold, p.ratio, 8.0);
        case LawLimit:  return std::max (0.0, x - p.threshold);            // brick wall: nothing passes the threshold
        case LawClip:   return (p.ratio >= 99.5 && p.knee <= 0.0) ? std::max (0.0, x - p.threshold) : idealGr (x, p.threshold, p.ratio >= 99.5 ? 1.0e6 : p.ratio, p.knee);
        default:        return idealGr (x, p.threshold, p.ratio, p.knee);
    }
}
inline double lawGr (double x, const Params& p) { return std::min (lawGrRaw (x, p), p.grLimit >= 39.95 ? 1.0e9 : p.grLimit); }
// upward compression: boost (dB, ≥ 0) for a detector level below upThreshold. Very quiet input (< −70 dB) is not lifted.
inline double lawUp (double x, const Params& p)
{
    if (! p.upOn || p.law == LawRide || p.upRatio <= 1.0 || x >= p.upThreshold) return 0.0;
    double b = std::min (p.upRange, (p.upThreshold - x) * (1.0 - 1.0 / p.upRatio));
    if (x < -70.0) b *= std::max (0.0, (x + 90.0) / 20.0);
    return b;
}
inline double lawOut (double x, const Params& p) { return x - lawGr (x, p) + lawUp (x, p); }

// ---- small parts ----
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z[2][2] {};
    void reset() { z[0][0] = z[0][1] = z[1][0] = z[1][1] = 0; }
    double run (int c, double x) { const double y = b0 * x + z[c][0]; z[c][0] = b1 * x - a1 * y + z[c][1]; z[c][1] = b2 * x - a2 * y; return y; }
    void set (int type, double f, double q, double fs)                    // BandLP / BandHP / BandBP (RBJ)
    {
        const double w = 2.0 * kPi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * std::max (0.1, q)), a0 = 1.0 + al;
        if (type == BandLP)      { b0 = (1 - c) * 0.5 / a0; b1 = (1 - c) / a0; b2 = b0; }
        else if (type == BandHP) { b0 = (1 + c) * 0.5 / a0; b1 = -(1 + c) / a0; b2 = b0; }
        else                     { b0 = al / a0; b1 = 0; b2 = -al / a0; }
        a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
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
        const bool autoT = p.timing == TimAuto;
        aSlowUp = tc (autoT ? 400.0 : 200.0); aSlowDn = tc (autoT ? 1500.0 : p.release * p.slowMult); aFastDn = autoT ? tc (100.0) : aR;
        slowAmt = autoT ? 0.7 : p.slowAmt;
        oSA = tc (30); oMU = tc (1500); oMD = tc (4000);
        dcR = std::exp (-2.0 * kPi * 8.0 / fs); xfK = 1.0 - std::exp (-2.0 * kPi * 70.0 / fs);
        laBase = (int) std::lround (std::max (0.0, p.lookaheadMs) * 0.001 * fsBase); la = std::min (dsize - 2, laBase * os); laBase = la / os;
        lb = std::min (dsize - 2, (int) std::lround (std::max (0.0, -p.lookaheadMs) * 0.001 * fs));
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
            ms[c] = y1[c] = y2[c] = gS[c] = mem[c] = avg[c] = gs[c] = fb[c] = yUp[c] = yRms[c] = 0; xfIn[c] = xfOut[c] = xfLp[c] = 0;
            for (auto& d : dc[c]) d[0] = d[1] = 0;
        }
        scLp.reset(); scHp.reset(); for (auto& b : msB) b[0] = b[1] = 0; yRide[0] = yRide[1] = 0;
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
    // transformer: only the lows saturate (flux grows as frequency falls)
    double xfmr (double x, double& st, double d) { st += xfK * (x - st); return (x - st) + std::tanh (d * st) / d; }

    // attack / release step. The release blends from exponential to linear-in-dB with relCurve (dB-domain detectors only).
    double follow (double y, double u, double aAtt, double aRel) const
    {
        if (u > y) return aAtt * y + (1 - aAtt) * u;
        const double e = aRel * y + (1 - aRel) * u;
        if (p.relCurve <= 0.0 || p.det == DetDiode) return e;
        return e + p.relCurve * (std::max (u, y - linStep) - e);
    }
    // ⑤ timing: follows the target u (GR in dB, or detector voltage for the diode detector)
    double ballistics (int c, double u, double grNow)
    {
        switch (p.timing)
        {
            case TimProg2: case TimAuto:
            {
                y1[c] = follow (y1[c], u, aA, aFastDn);
                const double s = u > y2[c] ? aSlowUp : aSlowDn; y2[c] = s * y2[c] + (1 - s) * u;
                slowNow[c] = slowAmt * y2[c];
                return std::max (y1[c], slowNow[c]);
            }
            case TimOptoMem:                                        // EL panel + CdS cell: fast half + slow half with memory
            {
                const double aF = u > y1[c] ? aA : aR; y1[c] = aF * y1[c] + (1 - aF) * u;
                const double aS = u > gS[c] ? oSA : std::exp (-1.0 / ((0.5 + 4.5 * mem[c]) * fs)); gS[c] = aS * gS[c] + (1 - aS) * u;
                const double tgt = std::min (1.0, (p.det == DetDiode ? grNow : u) / 12.0), aMm = tgt > mem[c] ? oMU : oMD;
                mem[c] = aMm * mem[c] + (1 - aMm) * tgt; slowNow[c] = 0.5 * gS[c];
                return 0.5 * y1[c] + 0.5 * gS[c];
            }
            case TimPrecise:                                        // hold the peak, ramp linearly over the look-ahead, then release
            {
                const double b = box[c].run (smax[c].run (u));
                y1[c] = follow (y1[c], b, aA, aR); slowNow[c] = 0;
                return y1[c];
            }
            case TimDualAvg:                                        // sustained reduction → slow release, transients → fast release
            {
                const double on = grNow > 0.1 ? 1.0 : 0.0; avg[c] = aAvg * avg[c] + (1 - aAvg) * on;
                const double rel = p.release * (1.0 + (p.slowMult - 1.0) * avg[c]);
                const double a = u > y1[c] ? aA : std::exp (-1.0 / (std::max (rel, 0.001) * 0.001 * fs));
                y1[c] = a * y1[c] + (1 - a) * u; slowNow[c] = avg[c];
                return y1[c];
            }
            default: { y1[c] = follow (y1[c], u, aA, aR); slowNow[c] = 0; return y1[c]; }
        }
    }

    // ⑥ gain element: where the level is actually turned down
    double element (int c, double wv, double g)
    {
        const double gn = std::min (g, 20.0) / 20.0;
        switch (p.element)
        {
            case ElFET:   { const double dv = (0.4 + 1.2 * gn) * drv, a2 = 0.03 + 0.22 * gn; return dcBlock (c, 0, std::tanh (dv * (wv + a2 * wv * wv)) / dv); }
            case ElTube:  { const double dv = (0.7 + 0.3 * gn) * drv, a2 = 0.25, mis = 0.04;      // push-pull: even harmonics cancel except for the mismatch
                            const double pos = std::tanh (dv * (wv + a2 * wv * wv)), neg = std::tanh (dv * (-wv + a2 * wv * wv));
                            return dcBlock (c, 0, 0.5 * ((1 + mis) * pos - (1 - mis) * neg) / dv); }
            case ElDiode: { const double k = (0.08 + 0.45 * gn) * drv; return wv / (1.0 + k * std::abs (wv)); }
            default: return wv;
        }
    }
    double outStage (int c, double v)
    {
        switch (p.out)
        {
            case OutXfmr: return xfmr (v, xfOut[c], 1.5);
            case OutTube: return dcBlock (c, 1, std::tanh (0.6 * (v + 0.08 * v * v)) / 0.6);
            case OutH2:   return dcBlock (c, 1, std::tanh (0.8 * (v + 0.30 * v * v)) / 0.8);
            case OutH3:   return std::tanh (1.6 * v) / 1.6;
            case OutNickel: { const double u = xfmr (v, xfOut[c], 0.6); xfLp[c] += kHf * (u - xfLp[c]); return u + 0.07 * (u - xfLp[c]); }                 // cleanest, a little air
            case OutIron:   { double u = xfmr (v, xfOut[c], 1.5); u = dcBlock (c, 1, std::tanh (0.7 * (u + 0.12 * u * u)) / 0.7);                         // even harmonics, fuller upper lows
                              xfLp[c] += kLo * (u - xfLp[c]); return u + 0.10 * xfLp[c]; }
            case OutSteel:  { double u = xfmr (v, xfOut[c], 3.0); u = std::tanh (1.2 * u) / 1.2; xfLp[c] += kLo * 0.6 * (u - xfLp[c]); return u + 0.08 * xfLp[c]; }   // most distortion, tight lows
            default: return v;
        }
    }

    void tick (const double* in, const double* e, bool ext, double* out)
    {
        double x[2], lev[2], levR[2] { 0, 0 };
        for (int c = 0; c < 2; ++c)
        {
            x[c] = in[c] * gIn; if (p.inXfmr) x[c] = xfmr (x[c], xfIn[c], 1.5);
            double s = ext ? e[c] : (p.feedback ? fb[c] : x[c]);
            if (hpfOn) s = hpf.run (c, s);
            if (p.scTilt > 0) s = tilt.run (c, s);
            if (p.band != BandOff) s = bandSc.run (c, s);
            tDet[c] = s;
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
                lev[c] = p.det == DetRMS ? levR[c] : std::abs (s);
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
                gs[c] = lawGr (lc > 1e-7 ? 20.0 * std::log10 (lc) : -140.0, p); slowNow[c] = 0;
            }
            else if (p.law == LawRide)                              // gain riding: a slow fader towards the target level, held while the input is idle
            {
                const double ldb = l > 1e-7 ? 20.0 * std::log10 (l) : -140.0;
                if (ldb >= p.upThreshold) { const double t = -lawGr (ldb, p), a = t < yRide[c] ? aA : aR; yRide[c] = a * yRide[c] + (1 - a) * t; }
                gs[c] = -yRide[c]; slowNow[c] = 0;
            }
            else if (p.det == DetDiode)                             // rectifier + RC: timing acts on the voltage, before the gain law
            {
                const double v = ballistics (c, l, gs[c]);
                gs[c] = lawGr (v > 1e-7 ? 20.0 * std::log10 (v) : -140.0, p);
            }
            else if (p.det == DetCrest)                             // peak path (threshold raised by the crest setting) and RMS path, whichever asks for more
            {
                const double lr = p.link * mxR + (1.0 - p.link) * levR[c];
                const double gP = ballistics (c, lawGr ((l > 1e-7 ? 20.0 * std::log10 (l) : -140.0) - p.crestDb, p), gs[c]);
                const double tR = lawGr (lr > 1e-7 ? 20.0 * std::log10 (lr) : -140.0, p), a = tR > yRms[c] ? aA : aCrest;
                yRms[c] = a * yRms[c] + (1 - a) * tR; slowNow[c] = yRms[c];
                gs[c] = std::max (gP, yRms[c]);
            }
            else gs[c] = ballistics (c, lawGr (l > 1e-7 ? 20.0 * std::log10 (l) : -140.0, p), gs[c]);

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
            fb[c] = wet;
            double o = outStage (c, wet);
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
    Biquad scLp, scHp; double scW[3] { 1, 1, 1 }, msB[3][2] {}, yRide[2] {};
    double ms[2] {}, y1[2] {}, y2[2] {}, gS[2] {}, mem[2] {}, avg[2] {}, gs[2] {}, fb[2] {}, slowNow[2] {}, xfIn[2] {}, xfOut[2] {}, dc[2][2][2] {};
    double aA = 0, aR = 0, aM = 0, aAvg = 0, aSlowUp = 0, aSlowDn = 0, aFastDn = 0, slowAmt = 0.5, oSA = 0, oMU = 0, oMD = 0, dcR = 0, xfK = 0, gIn = 1, gMk = 1, drv = 1;
    double tGr = 0, tSlow = 0, tIn = 0, tUp = 0, tDet[2] {}, yUp[2] {}, thrLin = 1, yRms[2] {}, xfLp[2] {}, linStep = 0, aCrest = 0, kHf = 0, kLo = 0;
};
} // namespace mod
