#pragma once
#include "Spectrum.h"
#include "Dyn.h"
#include <complex>

// =============================================================================
//  HARMONICS: what the current settings add to a pure tone. A sine goes through a copy of the engine
//  (same oversampling), and the spectrum of the result is drawn relative to the fundamental:
//  harmonics at 2x, 3x ... the frequency, and whatever lands between them (aliasing).
// =============================================================================
class HarmView : public juce::Component
{
public:
    explicit HarmView (ParamHelp& h) : ph (h) {}
    void refresh()
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - last >= 200) { last = now; compute(); }
        repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (const int t = hitLowerTab (e.getPosition()); t >= 0) { ph.proc.uiState.setProperty ("lowerTab", t, nullptr); return; }
        for (int i = 0; i < 3; ++i)
        {
            if (freqChip (i).contains (e.getPosition())) ph.proc.uiState.setProperty ("harmF", i, nullptr);
            if (levelChip (i).contains (e.getPosition())) ph.proc.uiState.setProperty ("harmL", i, nullptr);
        }
    }
    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        g.setColour (theme::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        drawLowerTabs (g, 3);
        static const char* fn[] { "100", "1k", "5k" }; static const char* ln[] { "-24", "-12", "0" };
        for (int i = 0; i < 3; ++i) { chip (g, freqChip (i), fn[i], i == fIdx()); chip (g, levelChip (i), ln[i], i == lIdx()); }
        const juce::Rectangle<float> well (6.0f, 24.0f, W - 12.0f, 130.0f);
        paintWell (g, well);
        const float x0 = well.getX() + 30.0f, x1 = well.getRight() - 6.0f, y0 = well.getY() + 14.0f, y1 = well.getBottom() - 14.0f;
        const double lo = 50.0, hi = nyq;
        auto X = [&] (double f) { return x0 + (float) (std::log (std::max (f, lo) / lo) / std::log (hi / lo)) * (x1 - x0); };
        auto Y = [&] (float db) { return y0 + juce::jlimit (0.0f, 1.0f, -db / 120.0f) * (y1 - y0); };
        g.setFont (theme::mono (8.0f));
        for (int d : { 0, -40, -80, -120 })
        {
            g.setColour (theme::grid); g.drawHorizontalLine ((int) Y ((float) d), x0, x1);
            g.setColour (theme::faint); g.drawText (d == 0 ? juce::String ("0") : minus ((double) d, 0), (int) well.getX() + 2, (int) Y ((float) d) - 5, 26, 10, juce::Justification::centredRight);
        }
        for (double f : { 100.0, 1000.0, 10000.0 })
        {
            g.setColour (theme::grid); g.drawVerticalLine ((int) X (f), y0, y1);
            g.setColour (theme::faint); g.drawText (fmtHz (f), (int) X (f) - 16, (int) y1 + 2, 32, 10, juce::Justification::centred);
        }
        if (! ok)
        {
            g.setColour (theme::onWell()); g.setFont (theme::ui (10.0f));
            g.drawText (ph.tr ("この設定では信号が出ません", "No output with these settings"), well.toNearestInt(), juce::Justification::centred);
            return;
        }
        juce::Path line; bool started = false;
        const int cols = (int) (x1 - x0);
        for (int c = 0; c <= cols; ++c)
        {
            const double f0c = lo * std::pow (hi / lo, (double) c / cols), f1c = lo * std::pow (hi / lo, (double) (c + 1) / cols);
            const int b0 = juce::jlimit (1, kN / 2 - 1, (int) (f0c * kN / fs)), b1 = juce::jlimit (b0, kN / 2 - 1, (int) (f1c * kN / fs));
            float m = -200.0f; for (int b = b0; b <= b1; ++b) m = std::max (m, sp[(size_t) b]);
            const float x = x0 + (float) c, y = Y (m);
            if (! started) { line.startNewSubPath (x, y); started = true; } else line.lineTo (x, y);
        }
        g.setColour (theme::side); g.strokePath (line, juce::PathStrokeType (1.2f));
        float lastLabel = -100.0f;
        for (size_t k = 0; k < harm.size(); ++k)                          // harmonic positions: number above, dot on the level
        {
            const float x = X (f0 * (double) (k + 1));
            g.setColour (theme::accent); g.fillEllipse (x - 2.0f, Y (harm[k]) - 2.0f, 4.0f, 4.0f);
            if (x - lastLabel >= 10.0f && k < 12) { g.setFont (theme::mono (8.0f, true)); g.drawText (juce::String ((int) k + 1), (int) x - 8, (int) well.getY() + 2, 16, 10, juce::Justification::centred); lastLabel = x; }
        }
        // readout
        g.setFont (theme::mono (9.5f, true)); g.setColour (theme::accent);
        juce::String t = "THD " + (thd >= 0.1 ? juce::String (thd, 2) : juce::String (thd, 3)) + " %";
        g.drawText (t, 10, 156, 96, 12, juce::Justification::centredLeft);
        g.setFont (theme::mono (9.0f)); g.setColour (theme::text);
        juce::String hs; for (size_t k = 1; k < std::min<size_t> (harm.size(), 6); ++k) hs << (int) (k + 1) << ph.tr ("次 ", ": ") << (harm[k] < -120.0f ? juce::String::fromUTF8 ("\xe2\x80\x94") : minus ((double) juce::roundToInt (harm[k]), 0)) << "   ";
        g.drawText (hs, 104, 156, getWidth() - 112, 12, juce::Justification::centredLeft);
        g.setFont (theme::ui (8.5f)); g.setColour (other > -100.0f ? theme::pre : theme::muted);
        g.drawText (other > -100.0f ? ph.tr ("倍音以外の成分（折り返しなど）：最大 ", "Between the harmonics (aliasing etc.): up to ") + minus ((double) juce::roundToInt (other), 0) + " dB"
                                    : ph.tr ("倍音以外の成分：なし（−100 dB 未満）", "Between the harmonics: nothing (below −100 dB)"), 10, 168, getWidth() - 20, 11, juce::Justification::centredLeft);
        g.setColour (theme::faint);
        g.drawText (ph.tr ("基音＝0 dB。右上：サイン波の周波数（Hz）とレベル（dBFS）", "Fundamental = 0 dB. Top right: the sine's frequency (Hz) and level (dBFS)"),
                    10, 179, getWidth() - 20, 11, juce::Justification::centredLeft);
    }
private:
    static constexpr int kOrder = 13, kN = 1 << kOrder;
    int fIdx() const { return juce::jlimit (0, 2, (int) ph.proc.uiState.getProperty ("harmF", 1)); }
    int lIdx() const { return juce::jlimit (0, 2, (int) ph.proc.uiState.getProperty ("harmL", 1)); }
    juce::Rectangle<int> freqChip (int i) const { return { getWidth() - 10 - 178 + i * 27, 5, 25, 15 }; }
    juce::Rectangle<int> levelChip (int i) const { return { getWidth() - 10 - 84 + i * 28, 5, 26, 15 }; }
    static void chip (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, bool on)
    {
        g.setColour (on ? theme::accent : theme::hilite (0.12f)); g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (on ? theme::onAccent : theme::onWell()); g.setFont (theme::mono (8.5f, true)); g.drawText (t, r, juce::Justification::centred);
    }
    void compute()
    {
        static const double fr[] { 100.0, 1000.0, 5000.0 }, lv[] { -24.0, -12.0, 0.0 };
        fs = ph.proc.getSampleRateOrDefault(); nyq = fs * 0.5;
        auto p = ph.proc.currentParams(); p.bypass = false;
        const int fi = fIdx(), li = lIdx();
        if (std::memcmp (&p, &lastP, sizeof (p)) == 0 && fi == lastF && li == lastL && fs == lastFs) return;
        std::memcpy (&lastP, &p, sizeof (p)); lastF = fi; lastL = li; lastFs = fs;
        if (core == nullptr || coreFs != fs) { core = std::make_unique<mod::Multi> (fs); coreFs = fs; }
        core->reset(); core->set (p);
        f0 = fr[fi]; const double amp = std::pow (10.0, lv[li] / 20.0), dph = 2.0 * mod::kPi * f0 / fs;
        const int blk = 512, runIn = ((int) (fs * 0.6) / blk + 1) * blk, total = runIn + kN + 4096 + blk;
        std::vector<float> in ((size_t) blk), oL ((size_t) blk), oR ((size_t) blk), dL ((size_t) blk), dR ((size_t) blk);
        std::fill (buf.begin(), buf.end(), 0.0f); double ph0 = 0;
        for (int o = 0; o < total; o += blk)
        {
            for (int i = 0; i < blk; ++i) { in[(size_t) i] = (float) (amp * std::sin (ph0)); ph0 += dph; if (ph0 > 2.0 * mod::kPi) ph0 -= 2.0 * mod::kPi; }
            core->run (in.data(), in.data(), oL.data(), oR.data(), blk, nullptr, nullptr, {}, dL.data(), dR.data());
            for (int i = 0; i < blk; ++i) { const int idx = o + i - (total - kN); if (idx >= 0 && idx < kN) buf[(size_t) idx] = oL[(size_t) i]; }
        }
        window.multiplyWithWindowingTable (buf.data(), (size_t) kN);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        auto peak = [this] (double f) { const int c = (int) std::lround (f * kN / fs); float m = 0; for (int b = std::max (1, c - 3); b <= std::min (kN / 2 - 1, c + 3); ++b) m = std::max (m, buf[(size_t) b]); return m; };
        const float m1 = peak (f0); ok = m1 > 1.0e-6f;
        harm.clear(); if (! ok) return;
        std::vector<char> isH ((size_t) kN / 2, 0); double sum = 0;
        for (int k = 1; k * f0 < nyq - 50.0; ++k)
        {
            const float m = peak (k * f0); harm.push_back (m > 0 ? 20.0f * std::log10 (m / m1) : -200.0f);
            if (k >= 2) sum += (double) m * m;
            const int c = (int) std::lround (k * f0 * kN / fs); for (int b = std::max (0, c - 7); b <= std::min (kN / 2 - 1, c + 7); ++b) isH[(size_t) b] = 1;
        }
        thd = 100.0 * std::sqrt (sum) / m1;
        float om = 0; const int bLo = (int) (30.0 * kN / fs) + 8;
        for (int b = 0; b < kN / 2; ++b) { sp[(size_t) b] = buf[(size_t) b] > 0 ? 20.0f * std::log10 (buf[(size_t) b] / m1) : -200.0f; if (b >= bLo && ! isH[(size_t) b]) om = std::max (om, buf[(size_t) b]); }
        other = om > 0 ? 20.0f * std::log10 (om / m1) : -200.0f;
    }
    ParamHelp& ph; juce::uint32 last = 0;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kN, juce::dsp::WindowingFunction<float>::blackmanHarris, false };
    std::vector<float> buf = std::vector<float> ((size_t) kN * 2, 0.0f), sp = std::vector<float> ((size_t) kN / 2, -200.0f), harm;
    std::unique_ptr<mod::Multi> core; double coreFs = 0, fs = 48000, nyq = 24000, f0 = 1000, thd = 0, lastFs = 0;
    mod::Params lastP; int lastF = -1, lastL = -1; float other = -200.0f; bool ok = false;
};

// =============================================================================
//  SC FILTER: what the detector hears. The frequency response of everything between the signal and the detector:
//  high-pass, tilt, band filter and the three weighted detector bands. 0 dB = heard as it is; lower = the detector reacts less there.
// =============================================================================
class ScView : public juce::Component
{
public:
    explicit ScView (ParamHelp& h) : ph (h) {}
    void refresh() { repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { if (const int t = hitLowerTab (e.getPosition()); t >= 0) ph.proc.uiState.setProperty ("lowerTab", t, nullptr); }
    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        g.setColour (theme::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        drawLowerTabs (g, 4);
        g.setColour (theme::faint); g.setFont (theme::ui (8.5f));
        g.drawText (ph.tr ("検出器が聴く音の周波数特性", "what the detector hears"), 150, 5, (int) W - 160, 15, juce::Justification::centredRight);
        const juce::Rectangle<float> well (6.0f, 24.0f, W - 12.0f, 130.0f);
        paintWell (g, well);
        const float x0 = well.getX() + 30.0f, x1 = well.getRight() - 6.0f, y0 = well.getY() + 8.0f, y1 = well.getBottom() - 14.0f;
        const float top = 12.0f, bot = -36.0f;
        auto X = [&] (double f) { return x0 + (float) (std::log (f / 20.0) / std::log (1000.0)) * (x1 - x0); };
        auto Y = [&] (double db) { return y0 + (float) juce::jlimit (0.0, 1.0, (top - db) / (top - bot)) * (y1 - y0); };
        g.setFont (theme::mono (8.0f));
        for (int d : { 12, 0, -12, -24, -36 })
        {
            g.setColour (d == 0 ? theme::line : theme::grid); g.drawHorizontalLine ((int) Y (d), x0, x1);
            g.setColour (theme::faint); g.drawText (d > 0 ? "+" + juce::String (d) : d == 0 ? juce::String ("0") : minus ((double) d, 0), (int) well.getX() + 2, (int) Y (d) - 5, 26, 10, juce::Justification::centredRight);
        }
        for (double f : { 100.0, 1000.0, 10000.0 })
        {
            g.setColour (theme::grid); g.drawVerticalLine ((int) X (f), y0, y1);
            g.setColour (theme::faint); g.drawText (fmtHz (f), (int) X (f) - 16, (int) y1 + 2, 32, 10, juce::Justification::centred);
        }
        const auto p = ph.proc.currentParams(); const double fs = ph.proc.getSampleRateOrDefault();
        const bool unused = p.law == mod::LawClip || p.law == mod::LawSpectral;
        mod::Biquad hpf, tilt, band, lp, hp;
        const bool hOn = p.scHpfHz >= 20.0, tOn = p.scTilt > 0, bOn = p.band != mod::BandOff, mOn = p.scMb;
        if (hOn) hpf.set (mod::BandHP, p.scHpfHz, 0.7071, fs);
        if (tOn) tilt.tilt (p.scTilt == 2 ? -10.0 : -4.0, p.scTilt == 2 ? 10.0 : 4.0, fs);
        if (bOn) band.set (p.band, p.bandHz, p.bandQ, fs);
        const double f1 = std::min (std::max (p.xLo, 30.0), 1000.0), f2 = std::min (std::max (p.xHi, f1 * 2.0), fs * 0.4);
        if (mOn) { lp.set (mod::BandLP, f1, 0.70710678, fs); hp.set (mod::BandHP, f2, 0.70710678, fs); }
        auto H = [fs] (const mod::Biquad& q, double f)
        {
            const std::complex<double> z1 = std::polar (1.0, -2.0 * mod::kPi * f / fs), z2 = z1 * z1;
            return (q.b0 + q.b1 * z1 + q.b2 * z2) / (1.0 + q.a1 * z1 + q.a2 * z2);
        };
        auto dB = [] (double m) { return 20.0 * std::log10 (std::max (m, 1.0e-6)); };
        const int cols = (int) (x1 - x0);
        juce::Path tot, ph1, ph2, ph3, ph4;
        for (int c = 0; c <= cols; ++c)
        {
            const double f = 20.0 * std::pow (1000.0, (double) c / cols); const float x = x0 + (float) c;
            const double a = hOn ? std::abs (H (hpf, f)) : 1.0, b = tOn ? std::abs (H (tilt, f)) : 1.0, d = bOn ? std::abs (H (band, f)) : 1.0;
            double m = 1.0;
            if (mOn) { const auto l = H (lp, f), h = H (hp, f); m = std::max ({ std::abs (l) * std::pow (10.0, p.scSens[0] / 20.0), std::abs (1.0 - l - h) * std::pow (10.0, p.scSens[1] / 20.0), std::abs (h) * std::pow (10.0, p.scSens[2] / 20.0) }); }
            auto add = [&] (juce::Path& pth, double v) { if (c == 0) pth.startNewSubPath (x, Y (dB (v))); else pth.lineTo (x, Y (dB (v))); };
            add (tot, a * b * d * m); if (hOn) add (ph1, a); if (tOn) add (ph2, b); if (bOn) add (ph3, d); if (mOn) add (ph4, m);
        }
        { juce::Graphics::ScopedSaveState s (g); g.reduceClipRegion (juce::Rectangle<float> (x0, y0, x1 - x0, y1 - y0).toNearestInt());
          g.setColour (theme::phase.withAlpha (0.55f)); for (auto* q : { &ph1, &ph2, &ph3, &ph4 }) if (! q->isEmpty()) g.strokePath (*q, juce::PathStrokeType (1.0f));
          g.setColour (unused ? theme::faint : theme::accent); g.strokePath (tot, juce::PathStrokeType (2.0f)); }
        // what is in the path
        juce::StringArray on;
        if (hOn) on.add ("HPF " + fmtHz (p.scHpfHz) + " Hz");
        if (tOn) on.add (juce::String ("Tilt ") + (p.scTilt == 2 ? "Loud" : "Med"));
        if (bOn) on.add (juce::String (p.band == mod::BandLP ? "LP " : p.band == mod::BandHP ? "HP " : "BP ") + fmtHz (p.bandHz) + (p.bandOnly ? ph.tr ("（Band only）", " (band only)") : juce::String()));
        if (mOn) on.add (ph.tr ("3 帯域検出 ", "3 bands ") + fmtDb (p.scSens[0]) + " / " + fmtDb (p.scSens[1]) + " / " + fmtDb (p.scSens[2]));
        g.setFont (theme::mono (9.5f, true)); g.setColour (theme::accent);
        g.drawText (on.isEmpty() ? ph.tr ("フィルターなし：全帯域を同じ重みで聴く", "No filters: every frequency weighs the same") : on.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  ")), 10, 156, getWidth() - 20, 12, juce::Justification::centredLeft);
        g.setFont (theme::ui (8.5f)); g.setColour (theme::text);
        const bool ext = ph.value (pid::scSource) > 0.5f;
        g.drawText (unused ? ph.tr ("この方式（Clipper / Spectral）は、検出器のフィルターを使わない", "This method (Clipper / Spectral) does not use the detector filters")
                           : (ext ? ph.tr ("入力元：外部サイドチェーン", "Source: external sidechain") : p.feedback ? ph.tr ("入力元：圧縮後の音（フィードバック）", "Source: the compressed signal (feedback)") : ph.tr ("入力元：圧縮前の音（フィードフォワード）", "Source: the signal before compression (feed-forward)")),
                    10, 168, getWidth() - 20, 11, juce::Justification::centredLeft);
        g.setColour (theme::faint);
        g.drawText (ph.tr ("太線＝合計、細線＝各フィルター。低い帯域ほど圧縮がかかりにくい", "Bold = total, thin = each filter. Lower = triggers less compression"), 10, 179, getWidth() - 20, 11, juce::Justification::centredLeft);
    }
private:
    ParamHelp& ph;
};
