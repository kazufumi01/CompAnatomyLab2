// Engine checks for mod::Core, and a measured table of every model.
#include "../Source/CompCore.h"
#include "../Source/Models.h"
#include "../Source/MultiCore.h"
#include "../Source/Loudness.h"
#include <cstdio>
#include <cstring>
#include <string>

static int bad = 0;
static void check (const char* what, bool ok, double a = 0, double b = 0) { std::printf ("  %-52s %s  (%g, %g)\n", what, ok ? "OK" : "FAIL", a, b); if (! ok) ++bad; }
static const int FS = 48000;

static std::vector<float> drums (int n, float gainR = 0.9f, bool right = false)
{
    std::vector<float> v ((size_t) n); unsigned s = 12345;
    auto rnd = [&s] { s = s * 1664525u + 1013904223u; return (double) s / 4294967296.0 * 2 - 1; };
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / FS, ph = std::fmod (t * 92.0 / 60.0 * 2.0, 1.0), e = std::exp (-ph * 9.0) * (ph < 0.004 ? ph / 0.004 : 1.0);
        v[(size_t) i] = (float) ((right ? gainR : 1.0) * (0.55 * e * (0.6 * std::sin (2 * mod::kPi * 110 * t) + 0.4 * rnd()) + 0.03 * std::sin (2 * mod::kPi * 440 * t)));
    }
    return v;
}
static void runBlocks (mod::Core& c, const std::vector<float>& L, const std::vector<float>& R, std::vector<float>& oL, std::vector<float>& oR, std::vector<float>* gr = nullptr)
{
    const int n = (int) L.size(); oL.assign ((size_t) n, 0); oR.assign ((size_t) n, 0); if (gr) gr->assign ((size_t) n, 0);
    for (int i = 0; i < n; i += 128)
    {
        const int m = std::min (128, n - i); mod::Taps t; if (gr) t.gr = gr->data() + i;
        c.run (L.data() + i, R.data() + i, oL.data() + i, oR.data() + i, m, nullptr, nullptr, t);
    }
}
static double goertzelDb (const std::vector<float>& x, int from, int to, double f)
{
    double re = 0, im = 0; for (int i = from; i < to; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * mod::kPi * (i - from) / (to - from)); re += w * x[(size_t) i] * std::cos (2 * mod::kPi * f * i / FS); im += w * x[(size_t) i] * std::sin (2 * mod::kPi * f * i / FS); }
    return 20.0 * std::log10 (std::max (1e-12, 4.0 * std::sqrt (re * re + im * im) / (to - from)));
}

int main()
{
    const int N = FS * 3; auto L = drums (N), R = drums (N, 0.9f, true);
    std::vector<float> aL, aR, bL (N), bR (N);

    std::printf ("1. ideal methods at 1x == legacy comp::Core\n");
    struct Cfg { const char* name; void (*f) (comp::Params&, mod::Params&); };
    const Cfg cfgs[] = {
        { "default", [] (comp::Params& a, mod::Params& b) { a.makeup = 0; (void) b; } },
        { "fast ff", [] (comp::Params& a, mod::Params& b) { a.threshold = b.threshold = -24; a.ratio = b.ratio = 8; a.knee = b.knee = 0; a.attack = b.attack = 0.2; a.release = b.release = 60; a.makeup = b.makeup = 7; } },
        { "rms fb", [] (comp::Params& a, mod::Params& b) { a.rms = true; b.det = mod::DetRMS; a.feedback = b.feedback = true; a.threshold = b.threshold = -26; a.ratio = b.ratio = 3; a.knee = b.knee = 12; a.makeup = 0; } },
        { "lookahead + mix + hpf", [] (comp::Params& a, mod::Params& b) { a.threshold = b.threshold = -20; a.ratio = b.ratio = 20; a.lookaheadMs = b.lookaheadMs = 5; a.mix = b.mix = 0.35; a.scHpfHz = b.scHpfHz = 150; a.makeup = b.makeup = 6; } },
        { "opto memory", [] (comp::Params& a, mod::Params& b) { a.element = comp::Opto; a.threshold = b.threshold = -26; a.ratio = b.ratio = 3; a.knee = b.knee = 12; a.makeup = 0; b.timing = mod::TimOptoMem; b.attack = 10; b.release = 60; } },
        { "vari-mu law", [] (comp::Params& a, mod::Params& b) { a.element = comp::VariMu; b.law = mod::LawVariMu; a.ratio = b.ratio = 6; a.makeup = 0; a.bypass = false; } } };
    for (auto& cf : cfgs)
    {
        comp::Params a; mod::Params b; cf.f (a, b);
        comp::Core ca ((double) FS); ca.set (a); mod::Core cb ((double) FS); cb.set (b);
        for (int i = 0; i < N; i += 128) ca.run (L.data() + i, R.data() + i, bL.data() + i, bR.data() + i, std::min (128, N - i));
        runBlocks (cb, L, R, aL, aR);
        double d = 0; for (int i = 0; i < N; ++i) d = std::max (d, (double) std::abs (aL[(size_t) i] - bL[(size_t) i]));
        const bool colour = a.element == comp::VariMu;      // legacy adds its own tanh colour there: compare the law only loosely
        check (cf.name, colour ? d < 0.2 : d < 2e-6, d);
    }

    std::printf ("2. oversampling is transparent and reports its latency\n");
    for (int os : { 2, 4 })
    {
        mod::Params p; p.threshold = 0; p.ratio = 1; p.os = os; mod::Core c ((double) FS); c.set (p);
        std::vector<float> imp ((size_t) 4096, 0.0f), o1, o2; imp[100] = 1.0f; runBlocks (c, imp, imp, o1, o2);
        int pk = 0; for (int i = 0; i < 4096; ++i) if (std::abs (o1[(size_t) i]) > std::abs (o1[(size_t) pk])) pk = i;
        check (("impulse lands on the reported latency, " + std::to_string (os) + "x").c_str(), pk - 100 == c.latencySamples(), pk - 100, c.latencySamples());
        for (double f : { 1000.0, 15000.0, 19000.0 })
        {
            std::vector<float> s ((size_t) FS); for (int i = 0; i < FS; ++i) s[(size_t) i] = (float) (0.5 * std::sin (2 * mod::kPi * f * i / FS));
            c.reset(); runBlocks (c, s, s, o1, o2);
            const double g = goertzelDb (o1, FS / 2, FS, f) - goertzelDb (s, FS / 2, FS, f);
            check (("gain at " + std::to_string ((int) f) + " Hz, " + std::to_string (os) + "x (dB)").c_str(), std::abs (g) < (f > 18000 ? 0.5 : 0.05), g);
        }
    }

    std::printf ("3. precise timing: look-ahead removes the overshoot\n");
    {
        std::vector<float> s ((size_t) FS, 0.0f); for (int i = FS / 4; i < FS; ++i) s[(size_t) i] = (float) (0.9 * std::sin (2 * mod::kPi * 1000.0 * (i - FS / 4) / FS + 1.2));
        auto peakAfter = [&] (mod::Params p) { mod::Core c ((double) FS); c.set (p); std::vector<float> o1, o2; runBlocks (c, s, s, o1, o2); double pk = 0; for (float v : o1) pk = std::max (pk, (double) std::abs (v)); return 20 * std::log10 (pk); };
        mod::Params a; a.threshold = -20; a.ratio = 20; a.knee = 0; a.attack = 1.0; a.release = 100;
        mod::Params b = a; b.timing = mod::TimPrecise; b.attack = 0.01; b.lookaheadMs = 2; b.holdMs = 2;
        const double target = -20.0 + (20 * std::log10 (0.9) + 20.0) / 20.0, pa = peakAfter (a), pb = peakAfter (b);
        check ("one-pole 1 ms attack overshoots (dB over target)", pa - target > 3.0, pa - target);
        check ("precise + 2 ms look-ahead stays within 0.3 dB", pb - target < 0.3, pb - target);
    }

    std::printf ("4. band-only with no reduction returns the input exactly; stereo link\n");
    {
        mod::Params p; p.threshold = 0; p.ratio = 4; p.knee = 0; p.band = mod::BandBP; p.bandHz = 6000; p.bandOnly = true; mod::Core c ((double) FS); c.set (p);
        runBlocks (c, L, R, aL, aR); double d = 0; for (int i = 0; i < N; ++i) d = std::max (d, (double) std::abs (aL[(size_t) i] - L[(size_t) i]));
        check ("band-only, GR 0: output == input", d < 1e-6, d);
        std::vector<float> z ((size_t) N, 0.0f), g1, g2;
        mod::Params q; q.threshold = -30; q.link = 1.0; mod::Core c1 ((double) FS); c1.set (q); runBlocks (c1, L, z, aL, aR, &g1);
        mod::Params u = q; u.link = 0.0; mod::Core c2 ((double) FS); c2.set (u); std::vector<float> s ((size_t) N); for (int i = 0; i < N; ++i) s[(size_t) i] = (float) (0.01 * std::sin (2 * mod::kPi * 300.0 * i / FS));   // -40 dBFS: below the threshold on its own
        runBlocks (c2, L, s, aL, aR); double pkR = 0, pkIn = 0; for (int i = FS; i < N; ++i) { pkR = std::max (pkR, (double) std::abs (aR[(size_t) i])); pkIn = std::max (pkIn, (double) std::abs (s[(size_t) i])); }
        check ("link 0: quiet right channel is left alone", std::abs (pkR - pkIn) < 1e-3, pkR, pkIn);
        runBlocks (c1, L, s, aL, aR); pkR = 0; for (int i = FS; i < N; ++i) pkR = std::max (pkR, (double) std::abs (aR[(size_t) i]));
        check ("link 1: right channel is pulled down with the left", pkR < pkIn * 0.99, pkR, pkIn);
    }

    std::printf ("4b. limiter, upward, 3-band\n");
    {
        mod::Params p = mod::models()[11].p; p.input = 12; mod::Core c ((double) FS); c.set (p); runBlocks (c, L, R, aL, aR);
        double pk = 0; for (float v : aL) pk = std::max (pk, (double) std::abs (v));
        check ("limiter: +12 dB in, output never above -1 dBFS", 20 * std::log10 (pk) <= -1.0 + 0.01, 20 * std::log10 (pk));
        std::vector<float> s ((size_t) N), g1, o1, o2; for (int i = 0; i < N; ++i) s[(size_t) i] = (float) (0.003 * std::sin (2 * mod::kPi * 500.0 * i / FS));   // about -50 dBFS
        mod::Core u ((double) FS); u.set (mod::models()[12].p); runBlocks (u, s, s, o1, o2);
        double po = 0; for (int i = N - FS; i < N; ++i) po = std::max (po, (double) std::abs (o1[(size_t) i]));
        const double lift = 20 * std::log10 (po / 0.003), want = mod::lawUp (20 * std::log10 (0.003 / std::sqrt (2.0)), mod::models()[12].p);
        check ("upward: -50 dB tone is lifted by the law's amount (dB)", std::abs (lift - want) < 0.3, lift, want);
        mod::Params q; q.threshold = 0; q.ratio = 1; q.mb = true; mod::Multi mc ((double) FS); mc.set (q);
        std::vector<float> dL ((size_t) N), dR ((size_t) N); o1.assign ((size_t) N, 0); o2.assign ((size_t) N, 0);
        for (int i = 0; i < N; i += 512) mc.run (L.data() + i, R.data() + i, o1.data() + i, o2.data() + i, std::min (512, N - i), nullptr, nullptr, {}, dL.data() + i, dR.data() + i);
        double d = 0; for (int i = 0; i < N; ++i) d = std::max (d, (double) std::abs (o1[(size_t) i] - dL[(size_t) i]));
        check ("3-band, no reduction: output == dry reference", d < 1e-5, d);
        double e1 = 0, e2 = 0; for (int i = FS; i < N; ++i) { e1 += (double) L[(size_t) i] * L[(size_t) i]; e2 += (double) o1[(size_t) i] * o1[(size_t) i]; }
        check ("3-band sums flat (energy ratio dB)", std::abs (10 * std::log10 (e2 / e1)) < 0.05, 10 * std::log10 (e2 / e1));
        mod::Multi mo ((double) FS); mo.set (mod::models()[13].p); bool fin = true;
        for (int i = 0; i < N; i += 512) mo.run (L.data() + i, R.data() + i, o1.data() + i, o2.data() + i, std::min (512, N - i), nullptr, nullptr, {}, dL.data() + i, dR.data() + i);
        for (float v : o1) if (! std::isfinite (v)) fin = false;
        check ("OTT model: finite", fin);
    }

    std::printf ("4c. release curve, GR limit, look-behind, peak + RMS, transformers\n");
    {
        auto grAt = [&] (mod::Params p, const std::vector<float>& s, std::vector<float>& g) { mod::Core c ((double) FS); c.set (p); std::vector<float> o1, o2; runBlocks (c, s, s, o1, o2, &g); };
        std::vector<float> st ((size_t) FS * 2, 1.0e-6f), g; for (int i = FS / 10; i < FS; ++i) st[(size_t) i] = 0.5f;      // DC step, -6 dBFS, then silence
        mod::Params p; p.threshold = -26; p.ratio = 2; p.knee = 0; p.attack = 1; p.release = 100;                            // 10 dB GR
        p.relCurve = 1; grAt (p, st, g);
        const double g0 = g[(size_t) FS - 1], g1 = g[(size_t) (FS + FS / 20)], g2 = g[(size_t) (FS + FS / 10)];
        check ("linear release: 6.3 dB per release time, constant slope", std::abs ((g0 - g1) - 3.15) < 0.1 && std::abs ((g1 - g2) - 3.15) < 0.1, g0 - g1, g1 - g2);
        p.relCurve = 0; grAt (p, st, g);
        check ("exponential release: 63 % back after the release time", std::abs (g[(size_t) (FS + FS / 10)] / g[(size_t) FS - 1] - 0.368) < 0.01, g[(size_t) (FS + FS / 10)] / g[(size_t) FS - 1]);
        p.grLimit = 4; grAt (p, st, g); double mxg = 0; for (float v : g) mxg = std::max (mxg, (double) v);
        check ("GR limit 4 dB: never more", std::abs (mxg - 4.0) < 0.01, mxg);
        p.grLimit = 40; p.attack = 0.05; p.lookaheadMs = -5; grAt (p, st, g); std::vector<float> o1, o2; { mod::Core c ((double) FS); c.set (p); runBlocks (c, st, st, o1, o2); }
        int on = 0; for (int i = FS / 10; i < FS; ++i) if (o1[(size_t) i] < 0.45f) { on = i - FS / 10; break; }
        check ("look-behind 5 ms: the gain change arrives 240 samples late", std::abs (on - 240) <= 8, on, 240);
        // peak + RMS: with a large crest setting a sustained tone is handled by the RMS path
        std::vector<float> tone ((size_t) FS * 2); for (int i = 0; i < FS * 2; ++i) tone[(size_t) i] = (float) (0.5 * std::sin (2 * mod::kPi * 1000.0 * i / FS));
        mod::Params q = mod::models()[16].p; q.os = 1; q.crestDb = 12; grAt (q, tone, g);
        double rip = 0; for (int i = FS; i < FS * 2; ++i) rip = std::max (rip, std::abs ((double) g[(size_t) i] - g[(size_t) FS]));
        check ("peak + RMS, crest 12 dB: steady tone gives a ripple-free GR (dB)", rip < 0.05 && g[(size_t) FS] > 1.0, rip, g[(size_t) FS]);
        for (int os : { mod::OutNickel, mod::OutIron, mod::OutSteel })
        {
            mod::Params t; t.threshold = 0; t.ratio = 1; t.out = os; mod::Core c ((double) FS); c.set (t); runBlocks (c, tone, tone, o1, o2);
            const double h1 = goertzelDb (o1, FS, FS * 2 - 480, 1000), h2 = goertzelDb (o1, FS, FS * 2 - 480, 2000) - h1, h3 = goertzelDb (o1, FS, FS * 2 - 480, 3000) - h1;
            std::printf ("  %-52s gain %+.2f dB  H2 %.1f  H3 %.1f\n", os == mod::OutNickel ? "Nickel @ 1 kHz, -6 dBFS" : os == mod::OutIron ? "Iron" : "Steel", h1 + 6.02, h2, h3);
        }
    }

    std::printf ("4d. rider, multiband detection, M/S\n");
    {
        std::vector<float> s ((size_t) (FS * 4)), o1, o2, gr; for (size_t i = 0; i < s.size(); ++i) s[i] = (float) (0.0316 * std::sin (2 * mod::kPi * 300.0 * (double) i / FS));   // -30 dBFS peak
        mod::Core c ((double) FS); c.set (mod::models()[17].p); runBlocks (c, s, s, o1, o2);
        double pk = 0; for (size_t i = s.size() - (size_t) FS; i < s.size(); ++i) pk = std::max (pk, (double) std::abs (o1[i]));
        check ("rider: quiet tone is raised by the full range (+9 dB)", std::abs (20 * std::log10 (pk / 0.0316) - 9.0) < 0.2, 20 * std::log10 (pk / 0.0316));
        std::vector<float> q ((size_t) (FS * 2)); for (size_t i = 0; i < q.size(); ++i) q[i] = (float) (0.002 * std::sin (2 * mod::kPi * 300.0 * (double) i / FS));       // -54 dBFS: idle
        c.reset(); runBlocks (c, q, q, o1, o2); pk = 0; for (size_t i = q.size() / 2; i < q.size(); ++i) pk = std::max (pk, (double) std::abs (o1[i]));
        check ("rider: idle input is left alone", std::abs (20 * std::log10 (pk / 0.002)) < 0.05, 20 * std::log10 (pk / 0.002));

        std::vector<float> hi ((size_t) FS * 2); for (size_t i = 0; i < hi.size(); ++i) hi[i] = (float) (0.1 * std::sin (2 * mod::kPi * 8000.0 * (double) i / FS));
        mod::Params a; a.threshold = -30; a.ratio = 4; a.knee = 0; mod::Params b = a; b.scMb = true; b.scSens[2] = 12; mod::Params d = a; d.scMb = true; d.scSens[2] = -12;
        auto grOf = [&] (const mod::Params& pp) { mod::Core cc ((double) FS); cc.set (pp); std::vector<float> g; runBlocks (cc, hi, hi, o1, o2, &g); return (double) g[g.size() - 10]; };
        const double g0 = grOf (a), gUp = grOf (b), gDn = grOf (d);
        check ("3-band detection: +12 dB high sensitivity = 9 dB more GR on an 8 kHz tone", std::abs ((gUp - g0) - 9.0) < 0.5, gUp - g0);
        check ("3-band detection: -12 dB high sensitivity = less GR", gDn < g0 - 3.0, gDn, g0);

        std::vector<float> dL ((size_t) N), dR ((size_t) N); o1.assign ((size_t) N, 0); o2.assign ((size_t) N, 0);
        mod::Params m; m.threshold = 0; m.ratio = 1; m.stereo = 1; mod::Multi mc ((double) FS); mc.set (m);
        for (int i = 0; i < N; i += 512) mc.run (L.data() + i, R.data() + i, o1.data() + i, o2.data() + i, std::min (512, N - i), nullptr, nullptr, {}, dL.data() + i, dR.data() + i);
        double e = 0; for (int i = 0; i < N; ++i) e = std::max (e, std::max ((double) std::abs (o1[(size_t) i] - L[(size_t) i]), (double) std::abs (o2[(size_t) i] - R[(size_t) i])));
        check ("M/S, no reduction: output == input", e < 1e-6, e);
        mod::Params so; so.threshold = -50; so.ratio = 20; so.knee = 0; so.stereo = 3; mod::Multi ms2 ((double) FS); ms2.set (so);
        for (int i = 0; i < N; i += 512) ms2.run (L.data() + i, L.data() + i, o1.data() + i, o2.data() + i, std::min (512, N - i), nullptr, nullptr, {}, dL.data() + i, dR.data() + i);
        e = 0; for (int i = 0; i < N; ++i) e = std::max (e, (double) std::abs (o1[(size_t) i] - L[(size_t) i]));
        check ("side only on a mono signal: nothing changes", e < 1e-6, e);
        mod::Params mo = so; mo.stereo = 2; mod::Multi ms3 ((double) FS); ms3.set (mo);
        for (int i = 0; i < N; i += 512) ms3.run (L.data() + i, L.data() + i, o1.data() + i, o2.data() + i, std::min (512, N - i), nullptr, nullptr, {}, dL.data() + i, dR.data() + i);
        double p1 = 0, p0 = 0; for (int i = FS; i < N; ++i) { p1 = std::max (p1, (double) std::abs (o1[(size_t) i])); p0 = std::max (p0, (double) std::abs (L[(size_t) i])); }
        check ("mid only on a mono signal: it is compressed", p1 < p0 * 0.5, p1, p0);
    }

    std::printf ("4e. serial pre-stage, spectral compression\n");
    {
        std::vector<float> dL ((size_t) N), dR ((size_t) N), o1 ((size_t) N), o2 ((size_t) N), pg ((size_t) N), s ((size_t) N);
        for (int i = 0; i < N; ++i) s[(size_t) i] = (float) (0.316 * std::sin (2 * mod::kPi * 500.0 * i / FS));                               // -10 dBFS
        mod::Params a; a.threshold = 0; a.ratio = 1; a.pre = true; a.preThr = -30; mod::Multi mc ((double) FS); mc.set (a);
        for (int i = 0; i < N; i += 512) { mod::Taps t; t.pre = pg.data() + i; mc.run (s.data() + i, s.data() + i, o1.data() + i, o2.data() + i, std::min (512, N - i), nullptr, nullptr, t, dL.data() + i, dR.data() + i); }
        double pk = 0; for (int i = N - FS / 2; i < N; ++i) pk = std::max (pk, (double) std::abs (o1[(size_t) i]));
        const double red = -20 * std::log10 (pk / 0.316);
        check ("pre-stage alone: an opto 2:1 leveller reduces the tone (dB), tap agrees", red > 2.0 && red < 9.0 && std::abs (pg[(size_t) (N - 10)] - red) < 0.5, red, pg[(size_t) (N - 10)]);
        double e = 0; for (int i = 0; i < N; ++i) e = std::max (e, (double) std::abs (dL[(size_t) i] - s[(size_t) i]));
        check ("pre-stage: the dry reference is still the raw input", e == 0.0, e);

        mod::Params sp; sp.law = mod::LawSpectral; sp.threshold = 0; sp.ratio = 4; mod::Multi m2 ((double) FS); m2.set (sp);
        for (int i = 0; i < N; i += 512) m2.run (L.data() + i, R.data() + i, o1.data() + i, o2.data() + i, std::min (512, N - i), nullptr, nullptr, {}, dL.data() + i, dR.data() + i);
        const int lat = m2.latencySamples(); e = 0; for (int i = lat; i < N; ++i) e = std::max (e, (double) std::abs (o1[(size_t) i] - L[(size_t) (i - lat)]));
        check ("spectral, no reduction: output == input, 2048 samples late", lat == 2048 && e < 2e-4, e, lat);
        std::vector<float> two ((size_t) N); for (int i = 0; i < N; ++i) two[(size_t) i] = (float) (0.5 * std::sin (2 * mod::kPi * 1000.0 * i / FS) + 0.01 * std::sin (2 * mod::kPi * 6000.0 * i / FS));
        mod::Params sq; sq.law = mod::LawSpectral; sq.threshold = -20; sq.ratio = 4; sq.knee = 0; sq.attack = 5; sq.release = 100; mod::Multi m3 ((double) FS); m3.set (sq);
        for (int i = 0; i < N; i += 512) m3.run (two.data() + i, two.data() + i, o1.data() + i, o2.data() + i, std::min (512, N - i), nullptr, nullptr, {}, dL.data() + i, dR.data() + i);
        const double d1 = goertzelDb (o1, N - FS, N, 1000) - goertzelDb (two, N - FS, N, 1000), d6 = goertzelDb (o1, N - FS, N, 6000) - goertzelDb (two, N - FS, N, 6000);
        check ("spectral: the loud 1 kHz tone is reduced (dB), by about 0.75 x its excess", d1 < -9.0 && d1 > -14.0, d1);
        check ("spectral: the quiet 6 kHz tone is left alone (dB)", std::abs (d6) < 0.3, d6);
        const float* sb = m3.specBands(); float mxb = 0; int at = -1; for (int j = 0; j < mod::Spectral::kBands; ++j) if (sb[j] > mxb) { mxb = sb[j]; at = j; }
        const double fAt = 20.0 * std::pow (1000.0, (at + 0.5) / mod::Spectral::kBands);
        check ("spectral: the display band with the most GR is around 1 kHz (Hz)", fAt > 700 && fAt < 1500, fAt, mxb);
    }

    std::printf ("4f. clipper, loudness meter\n");
    {
        std::vector<float> s ((size_t) FS), o1, o2;
        for (size_t i = 0; i < s.size(); ++i) s[i] = (float) std::sin (2 * mod::kPi * 220.0 * (double) i / FS);
        mod::Params c; c.law = mod::LawClip; c.link = 0; c.threshold = -6; c.ratio = 100; c.knee = 0; mod::Core k ((double) FS); k.set (c); runBlocks (k, s, s, o1, o2);
        double pk = 0; for (size_t i = 1000; i < s.size(); ++i) pk = std::max (pk, (double) std::abs (o1[i]));
        check ("clipper, hard: a full-scale sine comes out flat at the ceiling (-6 dB)", std::abs (20 * std::log10 (pk) + 6.0) < 0.02, 20 * std::log10 (pk));
        double e = 0; for (size_t i = 1000; i < s.size(); ++i) if (std::abs (s[i]) < 0.45f) e = std::max (e, (double) std::abs (o1[i] - s[i]));
        check ("clipper, hard: everything under the ceiling is untouched", e < 1e-5, e);
        c.knee = 12; k.set (c); k.reset(); runBlocks (k, s, s, o1, o2); pk = 0; for (size_t i = 1000; i < s.size(); ++i) pk = std::max (pk, (double) std::abs (o1[i]));
        check ("clipper, soft (12 dB): the top is rounded and still ends at the ceiling", std::abs (20 * std::log10 (pk) + 6.0) < 0.3, 20 * std::log10 (pk));
        std::vector<float> t ((size_t) (FS * 6)); for (size_t i = 0; i < t.size(); ++i) t[i] = (float) (0.1 * std::sin (2 * mod::kPi * 1000.0 * (double) i / FS));
        mod::LoudMeter lm; lm.prepare ((double) FS); lm.process (t.data(), t.data(), (int) t.size()); const auto v = lm.vals();
        check ("loudness: a 1 kHz sine at -20 dBFS in both channels reads -20 LUFS (M, S, I)", std::abs (v.m + 20.0) < 0.1 && std::abs (v.s + 20.0) < 0.1 && std::abs (v.i + 20.0) < 0.15, v.s, v.i);
        { std::vector<float> q ((size_t) FS); for (size_t i = 0; i < q.size(); ++i) q[i] = (float) (0.5 * std::sin (mod::kPi * 0.5 * (double) i + mod::kPi * 0.25));   // fs/4 at 45 deg: samples miss the crest by 3 dB
          mod::LoudMeter tq; tq.prepare ((double) FS); tq.process (q.data(), q.data(), (int) q.size()); const auto w = tq.vals();
          check ("true peak: an fs/4 sine sampled 3 dB below its crest reads its real peak (-6.02 dBTP)", std::abs (w.pk + 6.02) < 0.25, w.pk); }
        check ("loudness: peak -20 dBFS, so PSR and PLR are 0", std::abs (v.pk + 20.0) < 0.05 && std::abs (v.pkMax - v.i) < 0.2, v.pk, v.pkMax - v.i);
    }

    std::printf ("5. every model: finite, silent in = silent out, measured behaviour\n");
    std::printf ("   1 kHz sine at -10 dBFS for 3 s, then silence. H2 / H3 are relative to the fundamental.\n");
    std::printf ("   %-18s %4s %6s %8s %8s %9s %9s %7s %7s\n", "model", "OS", "lat", "GR dB", "att63", "rel63 ms", "rel90 ms", "H2 dB", "H3 dB");
    for (auto& m : mod::models())
    {
        mod::Core c ((double) FS); c.set (m.p);
        std::vector<float> o1, o2, gr; runBlocks (c, L, R, o1, o2);
        bool fin = true; for (float v : o1) if (! std::isfinite (v)) fin = false;
        std::vector<float> z ((size_t) FS, 0.0f); c.reset(); runBlocks (c, z, z, o1, o2); double pk = 0; for (float v : o1) pk = std::max (pk, (double) std::abs (v));
        const int on = FS / 5, len = FS * 3, tot = on + len + FS * 8;
        std::vector<float> s ((size_t) tot, 0.0f); const double amp = std::pow (10.0, -10.0 / 20.0);
        for (int i = on; i < on + len; ++i) s[(size_t) i] = (float) (amp * std::sin (2 * mod::kPi * 1000.0 * (i - on) / FS));
        c.reset(); runBlocks (c, s, s, o1, o2, &gr);
        double fin_gr = 0; for (int i = on + len - FS / 10; i < on + len; ++i) fin_gr += gr[(size_t) i]; fin_gr /= FS / 10;
        double a63 = -1, r63 = -1, r90 = -1;
        for (int i = on; i < on + len; ++i) if (gr[(size_t) i] >= 0.632 * fin_gr) { a63 = (i - on) * 1000.0 / FS; break; }
        for (int i = on + len; i < tot; ++i) { if (r63 < 0 && gr[(size_t) i] <= 0.368 * fin_gr) r63 = (i - on - len) * 1000.0 / FS; if (gr[(size_t) i] <= 0.1 * fin_gr) { r90 = (i - on - len) * 1000.0 / FS; break; } }
        const int a = on + len - FS, b = on + len - FS / 100;
        const double h1 = goertzelDb (o1, a, b, 1000), h2 = goertzelDb (o1, a, b, 2000) - h1, h3 = goertzelDb (o1, a, b, 3000) - h1;
        std::printf ("   %-18s %3dx %6d %8.2f %8.2f %9.1f %9.1f %7.1f %7.1f %s\n", m.name, m.p.os, c.latencySamples(), fin_gr, a63, r63, r90, h2, h3, (fin && pk < 1e-4) ? "" : "FAIL");
        if (! fin || pk >= 1e-4) ++bad;
    }
    std::printf (bad == 0 ? "ALL OK\n" : "FAILED %d\n", bad);
    return bad;
}
