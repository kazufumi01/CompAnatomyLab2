#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Views.h"

// =============================================================================
//  SPECTRUM: input (grey) and output (line) spectrum over the span shown in TIME, and below them
//  the change per frequency (output − input): what the compressor did to each part of the spectrum.
//  Crossovers (3-band, multiband detection), the detector band and the sidechain high-pass are marked.
//  Works on the live history, and on the held sound while stopped.
// =============================================================================
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (ParamHelp& h) : ph (h) {}
    void refresh()
    {
        const auto now = juce::Time::getMillisecondCounter(); const int ver = ph.proc.previewVersion();
        for (size_t b = 0; b < 3; ++b) { bandShow[b] = std::max (ph.proc.bandGr[b].exchange (0.0f), bandShow[b] - 0.4f); bandUpShow[b] = std::max (ph.proc.bandUp[b].exchange (0.0f), bandUpShow[b] - 0.4f); }
        for (size_t j = 0; j < specShow.size(); ++j) { const float live = ph.proc.specGr[j].exchange (0.0f); specShow[j] = ph.proc.isFrozen() ? ph.proc.pvSpec[j] : std::max (live, specShow[j] - 0.3f); }
        if (now - last < 100 && ver == lastVer) { repaint(); return; }
        last = now; lastVer = ver; compute(); repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (const int t = hitUpperTab (e.getPosition()); t >= 0) ph.proc.uiState.setProperty ("upperTab", t, nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        paintWell (g, getLocalBounds().toFloat());
        drawUpperTabs (g, 1);
        if (ph.proc.isFrozen())
        {
            const juce::String t = ph.proc.frozenIsBuiltin() ? ph.tr ("停止中：内蔵テスト音を今の設定で計算", "Stopped: built-in test loop, computed with the current settings")
                                                             : ph.tr ("停止中：最後に鳴った音を今の設定で再計算", "Stopped: the last sound, re-computed with the current settings");
            const int w = (int) textWidth (theme::ui (9.5f, true), t) + 16;
            g.setColour (theme::accent.withAlpha (0.18f)); g.fillRoundedRectangle (140.0f, 5.0f, (float) w, 16.0f, 3.0f);
            g.setColour (theme::accent); g.setFont (theme::ui (9.5f, true)); g.drawText (t, 140, 5, w, 16, juce::Justification::centred);
        }
        const float x0 = 46, x1 = (float) getWidth() - 12, y0 = 30, yS = (float) getHeight() - 78, yD0 = yS + 8, yD1 = (float) getHeight() - 28, yDm = (yD0 + yD1) * 0.5f;
        auto X = [&] (double f) { return x0 + (float) (std::log (f / 20.0) / std::log (1000.0)) * (x1 - x0); };
        auto Y = [&] (float db) { return y0 + (kTop - juce::jlimit (kBot, kTop, db)) / (kTop - kBot) * (yS - y0); };
        g.setFont (theme::mono (8.5f));
        for (int db = -24; db >= -84; db -= 12)
        {
            g.setColour (theme::grid); g.drawHorizontalLine ((int) Y ((float) db), x0, x1);
            g.setColour (theme::faint); g.drawText (minus (db, 0), 4, (int) Y ((float) db) - 6, 38, 12, juce::Justification::centredRight);
        }
        for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
        {
            g.setColour (theme::grid); g.drawVerticalLine ((int) X (f), y0, yD1);
            g.setColour (theme::faint); g.drawText (fmtHz (f), (int) X (f) - 20, (int) yD1 + 2, 40, 10, juce::Justification::centred);
        }
        const auto p = ph.proc.currentParams();
        const float dash[] { 3.0f, 3.0f };
        auto mark = [&] (double f, juce::Colour c, const juce::String& t, int row)
        {
            if (f < 20.0 || f > 20000.0) return;
            g.setColour (c); g.drawDashedLine (juce::Line<float> (X (f), y0, X (f), yD1), dash, 2, 1.0f);
            g.setFont (theme::mono (8.5f)); g.drawText (t, (int) X (f) + 3, (int) y0 + 2 + row * 11, 120, 10, juce::Justification::centredLeft);
        };
        if (p.mb || p.scMb)
        {
            const juce::String what = p.mb ? ph.tr ("分割 ", "split ") : ph.tr ("検出の分割 ", "detector split ");
            mark (p.xLo, theme::accent, what + fmtHz (p.xLo), 0); mark (p.xHi, theme::accent, what + fmtHz (p.xHi), 0);
        }
        if (p.band != mod::BandOff) mark (p.bandHz, theme::phase, ph.tr ("検出帯域 ", "detector band ") + fmtHz (p.bandHz), 1);
        if (p.scHpfHz >= 20.0) mark (p.scHpfHz, theme::phase, "SC HPF " + fmtHz (p.scHpfHz), 2);

        if (! ok)
        {
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
            g.drawText (ph.tr ("音を流すと表示されます", "Play audio to see the spectrum"), (int) x0, (int) y0, (int) (x1 - x0), (int) (yS - y0), juce::Justification::centred);
            return;
        }
        const int n = (int) colIn.size();
        juce::Path fill, line; fill.startNewSubPath (x0, yS);
        for (int i = 0; i < n; ++i)
        {
            const float x = x0 + (x1 - x0) * (float) i / (float) (n - 1);
            fill.lineTo (x, Y (colIn[(size_t) i]));
            if (i == 0) line.startNewSubPath (x, Y (colOut[(size_t) i])); else line.lineTo (x, Y (colOut[(size_t) i]));
        }
        fill.lineTo (x1, yS); fill.closeSubPath();
        g.setColour (theme::faint.withAlpha (0.45f)); g.fillPath (fill);
        g.setColour (theme::side); g.strokePath (line, juce::PathStrokeType (1.5f));
        // change per frequency: output − input, ±12 dB
        g.setColour (theme::shade (0.25f)); g.fillRect (x0, yD0, x1 - x0, yD1 - yD0);
        g.setColour (theme::line); g.drawHorizontalLine ((int) yDm, x0, x1);
        g.setColour (theme::faint); g.setFont (theme::mono (8.0f));
        g.drawText ("+12", 4, (int) yD0 - 4, 38, 10, juce::Justification::centredRight); g.drawText ("0", 4, (int) yDm - 5, 38, 10, juce::Justification::centredRight);
        g.drawText (minus (-12.0, 0), 4, (int) yD1 - 6, 38, 10, juce::Justification::centredRight);
        for (int i = 0; i < n; ++i)
        {
            const float x = x0 + (x1 - x0) * (float) i / (float) (n - 1), dlt = juce::jlimit (-12.0f, 12.0f, colOut[(size_t) i] - colIn[(size_t) i]);
            if (colIn[(size_t) i] < kBot + 6.0f) continue;                      // nothing there to compare
            const float hgt = std::abs (dlt) / 12.0f * (yD1 - yDm);
            g.setColour ((dlt < 0 ? theme::pre : theme::ok).withAlpha (0.8f));
            g.fillRect (x - 0.5f, dlt < 0 ? yDm : yDm - hgt, (x1 - x0) / (float) (n - 1) + 0.5f, hgt);
        }
        if (p.law == mod::LawSpectral)                                          // the reduction each frequency's compressor asks for
        {
            juce::Path gp;
            for (int j = 0; j < mod::Spectral::kBands; ++j)
            {
                const float x = X (20.0 * std::pow (1000.0, (j + 0.5) / mod::Spectral::kBands)), y = yDm + juce::jmin (12.0f, specShow[(size_t) j]) / 12.0f * (yD1 - yDm);
                if (j == 0) gp.startNewSubPath (x, y); else gp.lineTo (x, y);
            }
            g.setColour (theme::accent); g.strokePath (gp, juce::PathStrokeType (1.4f));
        }
        if (p.mb)                                                               // per-band gain reduction / boost while 3-band is on
        {
            const double edges[4] { 20.0, juce::jlimit (30.0, 1000.0, p.xLo), juce::jmax (p.xHi, p.xLo * 2.0), 20000.0 };
            for (int b = 0; b < 3; ++b)
            {
                const int cx = (int) ((X (edges[b]) + X (edges[b + 1])) * 0.5f);
                g.setFont (theme::mono (9.5f, true));
                g.setColour (theme::pre); g.drawText ("GR " + minus (-bandShow[(size_t) b]), cx - 50, (int) yS - 26, 100, 11, juce::Justification::centred);
                if (p.upOn) { g.setColour (theme::ok); g.drawText ("UP +" + juce::String (bandUpShow[(size_t) b], 1), cx - 50, (int) yS - 14, 100, 11, juce::Justification::centred); }
            }
        }
        // legend
        const float ly = (float) getHeight() - 15; float lx = x0;
        auto key = [&] (juce::Colour c, const juce::String& t) { g.setColour (c); g.fillRect (lx, ly + 2, 8.0f, 8.0f); g.setColour (theme::onWell()); g.setFont (theme::ui (9.5f));
                                                                  const int w = (int) textWidth (theme::ui (9.5f), t) + 6; g.drawText (t, (int) lx + 11, (int) ly, w, 12, juce::Justification::centredLeft); lx += 11 + (float) w + 8; };
        key (theme::faint, ph.tr ("入力", "Input")); key (theme::side, ph.tr ("出力", "Output"));
        key (theme::pre, ph.tr ("下段：出力 − 入力（赤＝下がった帯域）", "Lower strip: output − input (red = reduced)")); key (theme::ok, ph.tr ("緑＝上がった帯域", "green = raised"));
        if (p.law == mod::LawSpectral) key (theme::accent, ph.tr ("線＝周波数ごとの GR", "line = GR per frequency"));
    }

private:
    static constexpr float kTop = -12.0f, kBot = -96.0f;
    static constexpr int kOrder = 12, kN = 1 << kOrder;
    void compute()
    {
        const auto& H = ph.proc.display(); const double fs = ph.proc.getSampleRateOrDefault();
        const uint64_t end = H.pos.load (std::memory_order_acquire);
        ok = false;
        const uint64_t lat = (uint64_t) juce::jmax (0, ph.proc.getLatencySamples());       // the output is late by this much: read the input that far back, so both show the same moment
        if (end < (uint64_t) kN + 16 + lat || H.inL.empty() || H.outM.empty()) return;
        const double span = std::min (4.0, TimeView::spanSec ((int) ph.proc.uiState.getProperty ("timeSpan", 2)));
        const uint64_t avail = std::min (end, (uint64_t) (fs * span));
        const int frames = (int) juce::jlimit ((uint64_t) 1, (uint64_t) 8, avail / (uint64_t) kN);
        std::fill (pIn.begin(), pIn.end(), 0.0); std::fill (pOut.begin(), pOut.end(), 0.0);
        for (int k = 0; k < frames; ++k)
        {
            const uint64_t stop = end - (frames > 1 ? (uint64_t) k * (avail - (uint64_t) kN) / (uint64_t) (frames - 1) : 0);
            for (int pass = 0; pass < 2; ++pass)
            {
                for (int i = 0; i < kN; ++i)
                {
                    const size_t idx = (size_t) ((stop - (uint64_t) kN + (uint64_t) i - (pass == 0 ? lat : 0)) & (uint64_t) History::kMask);
                    buf[(size_t) i] = pass == 0 ? 0.5f * (H.inL[idx] + H.inR[idx]) : H.outM[idx];
                }
                std::fill (buf.begin() + kN, buf.end(), 0.0f);
                window.multiplyWithWindowingTable (buf.data(), (size_t) kN);
                fft.performFrequencyOnlyForwardTransform (buf.data());
                auto& acc = pass == 0 ? pIn : pOut;
                for (int i = 0; i < kN / 2; ++i) acc[(size_t) i] += (double) buf[(size_t) i] * buf[(size_t) i];
            }
        }
        const int cols = 240; colIn.assign ((size_t) cols, kBot); colOut.assign ((size_t) cols, kBot);
        const double norm = 1.0 / ((double) frames * (kN / 4.0) * (kN / 4.0));
        auto column = [&] (const std::vector<double>& acc, std::vector<float>& out)
        {
            for (int c = 0; c < cols; ++c)
            {
                const double f0 = 20.0 * std::pow (1000.0, ((double) c - 0.5) / (cols - 1)), f1 = 20.0 * std::pow (1000.0, ((double) c + 0.5) / (cols - 1));
                const double b0 = f0 * kN / fs, b1 = f1 * kN / fs; double pw;
                if (b1 - b0 < 1.0) { const double bc = 0.5 * (b0 + b1); const int i = juce::jlimit (1, kN / 2 - 2, (int) bc); const double fr = juce::jlimit (0.0, 1.0, bc - i); pw = acc[(size_t) i] * (1 - fr) + acc[(size_t) i + 1] * fr; }
                else { double s = 0; int cnt = 0; for (int i = juce::jmax (1, (int) b0); i <= juce::jmin (kN / 2 - 1, (int) b1); ++i) { s += acc[(size_t) i]; ++cnt; } pw = cnt > 0 ? s / cnt : 0.0; }
                const double fc = std::sqrt (f0 * f1);
                out[(size_t) c] = (float) (10.0 * std::log10 (pw * norm + 1e-14) + 3.0 * std::log2 (fc / 1000.0));     // +3 dB/oct tilt so music reads flat-ish
            }
            std::vector<float> sm (out.size());                                   // light smoothing across frequency
            for (int c = 0; c < cols; ++c) { float s = 0; int n = 0; for (int k = -3; k <= 3; ++k) if (c + k >= 0 && c + k < cols) { s += out[(size_t) (c + k)]; ++n; } sm[(size_t) c] = s / (float) n; }
            out = sm;
        };
        column (pIn, colIn); column (pOut, colOut);
        ok = true;
    }

    ParamHelp& ph;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kN, juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> buf = std::vector<float> ((size_t) kN * 2, 0.0f), colIn, colOut;
    std::vector<double> pIn = std::vector<double> ((size_t) kN / 2, 0.0), pOut = std::vector<double> ((size_t) kN / 2, 0.0);
    std::array<float, 3> bandShow {}, bandUpShow {}; std::array<float, mod::Spectral::kBands> specShow {};
    bool ok = false; juce::uint32 last = 0; int lastVer = -1;
};
