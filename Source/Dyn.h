#pragma once
#include "GRWave.h"

// =============================================================================
//  DYNAMICS: numbers for the span shown in TIME — peak, RMS and crest factor of input and output,
//  how much and how often the compressor worked, and where the gain reduction spent its time.
// =============================================================================
class DynView : public juce::Component
{
public:
    explicit DynView (ParamHelp& h) : ph (h) {}
    void refresh()
    {
        const auto now = juce::Time::getMillisecondCounter(); const int ver = ph.proc.previewVersion();
        if (now - last < 160 && ver == lastVer) return;
        last = now; lastVer = ver; compute(); repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override { if (const int t = hitLowerTab (e.getPosition()); t >= 0) ph.proc.uiState.setProperty ("lowerTab", t, nullptr); }

    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        g.setColour (theme::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        drawLowerTabs (g, 1);
        g.setColour (theme::faint); g.setFont (theme::ui (8.5f));
        g.drawText (ph.tr ("表示中の ", "over the last ") + juce::String (spanShown, 0) + ph.tr (" 秒", " s"), 150, 5, (int) W - 160, 15, juce::Justification::centredRight);
        paintWell (g, juce::Rectangle<float> (6.0f, 24.0f, W - 12.0f, 132.0f));
        auto cell = [&] (const juce::String& t, int x, int y, int w, juce::Colour c, bool bold = false, juce::Justification j = juce::Justification::centredRight)
        { g.setColour (c); g.setFont (theme::mono (10.0f, bold)); g.drawText (t, x, y, w, 14, j); };
        const int cx[] { 14, 62, 108, 154 }, cw = 44;
        cell ("dB", cx[0], 30, 40, theme::faint, false, juce::Justification::centredLeft);
        cell ("IN", cx[1], 30, cw, theme::onWell(), true); cell ("OUT", cx[2], 30, cw, theme::side, true); cell (juce::String::fromUTF8 ("\xce\x94"), cx[3], 30, cw - 10, theme::onWell(), true);
        auto row = [&] (const char* name, float a, float b, int y, bool strong)
        {
            cell (name, cx[0], y, 46, strong ? theme::accent : theme::onWell(), strong, juce::Justification::centredLeft);
            cell (minus (a), cx[1], y, cw, theme::text); cell (minus (b), cx[2], y, cw, theme::text);
            const float d = b - a; cell ((d > 0.05f ? "+" : "") + minus (d), cx[3], y, cw - 10, std::abs (d) < 0.05f ? theme::faint : theme::text, strong);
        };
        if (! valid) { g.setColour (theme::muted); g.setFont (theme::ui (10.0f)); g.drawFittedText (ph.tr ("音を流すと表示します", "Play some audio"), 14, 60, 170, 30, juce::Justification::centredLeft, 2); }
        else
        {
            row ("PEAK", inPk, outPk, 48, false); row ("RMS", inRms, outRms, 64, false); row ("CREST", inPk - inRms, outPk - outRms, 80, true); row ("RANGE", inRange, outRange, 96, true);
            g.setColour (theme::grid); g.drawHorizontalLine (113, 14.0f, 190.0f);
            g.setColour (theme::pre); g.setFont (theme::mono (10.0f, true)); g.drawText ("GR", 14, 116, 30, 14, juce::Justification::centredLeft);
            g.setColour (theme::text); g.setFont (theme::mono (10.0f));
            g.drawText (ph.tr ("平均 ", "avg ") + minus (-grAvg) + ph.tr ("  最大 ", "  max ") + minus (-grMax), 40, 116, 150, 14, juce::Justification::centredLeft);
            g.drawText (ph.tr ("圧縮している時間 ", "time compressing ") + juce::String (juce::roundToInt (active * 100.0f)) + " %", 14, 130, 176, 14, juce::Justification::centredLeft);
            if (upMax > 0.05f) { g.setColour (theme::ok); g.drawText (ph.tr ("UP 平均 +", "UP avg +") + juce::String (upAvg, 1) + ph.tr ("  最大 +", "  max +") + juce::String (upMax, 1), 14, 144, 176, 14, juce::Justification::centredLeft); }
            // GR distribution
            const float hx = 204, hy = 44, hw = 120, hh = 88;
            g.setColour (theme::onWell()); g.setFont (theme::ui (8.5f)); g.drawText (ph.tr ("GR の分布（時間の割合）", "GR distribution (share of time)"), (int) hx, 30, (int) hw, 12, juce::Justification::centredLeft);
            float mx = 1.0e-6f; for (int i = 1; i < 24; ++i) mx = std::max (mx, hist[(size_t) i]);
            mx = std::max (mx, hist[0] * 0.25f);
            for (int i = 0; i < 24; ++i)
            {
                const float f = juce::jlimit (0.0f, 1.0f, hist[(size_t) i] / mx);
                g.setColour (i == 0 ? theme::faint : theme::pre); g.fillRect (hx + (float) i * 5.0f, hy + hh - hh * f, 4.0f, hh * f);
            }
            g.setColour (theme::grid); g.drawHorizontalLine ((int) (hy + hh), hx, hx + hw);
            g.setColour (theme::faint); g.setFont (theme::mono (7.5f));
            for (int k : { 0, 6, 12, 18, 24 }) g.drawText (juce::String (k), (int) hx + k * 5 - 10, (int) (hy + hh) + 2, 20, 10, juce::Justification::centred);
            g.drawText ("dB", (int) (hx + hw) - 4, (int) (hy + hh) + 2, 18, 10, juce::Justification::centredLeft);
            // reading
            const float dc = (outPk - outRms) - (inPk - inRms);
            juce::String t = std::abs (dc) < 0.3f ? ph.tr ("クレストはほぼ同じ：打点と平均の比は変わっていない", "Crest is about the same: the ratio of hits to average is unchanged")
                           : dc < 0 ? ph.tr ("クレスト ", "Crest ") + minus (dc) + ph.tr (" dB：打点と平均の差が縮んだ（音が詰まった・密度が上がった）", " dB: hits and average moved closer (denser)")
                                    : ph.tr ("クレスト +", "Crest +") + juce::String (dc, 1) + ph.tr (" dB：打点が相対的に強くなった（遅い Attack や Look-behind で起きる）", " dB: the hits got relatively stronger (slow attack or look-behind does this)");
            g.setColour (theme::text); g.setFont (theme::ui (9.5f));
            g.drawFittedText (t, 10, 158, getWidth() - 20, 30, juce::Justification::centredLeft, 2, 0.85f);
        }
    }

private:
    void compute()
    {
        const auto& H = ph.proc.display(); const double fs = ph.proc.getSampleRateOrDefault();
        spanShown = (float) TimeView::spanSec ((int) ph.proc.uiState.getProperty ("timeSpan", 2));
        const uint64_t end = H.pos.load (std::memory_order_acquire), n = std::min (end, (uint64_t) (spanShown * fs));
        valid = n > (uint64_t) (fs * 0.2);
        if (! valid) return;
        double si = 0, so = 0, sg = 0, su = 0; float pi = 0, po = 0, gm = 0, um = 0; uint64_t act = 0; hist.fill (0.0f);
        for (uint64_t i = end - n; i < end; ++i)
        {
            const size_t k = (size_t) (i & (uint64_t) History::kMask);
            const float a = H.inAbs[k], o = H.outAbs[k], gr = H.gr[k], u = H.up[k];
            si += (double) a * a; so += (double) o * o; pi = std::max (pi, a); po = std::max (po, o);
            sg += gr; gm = std::max (gm, gr); su += u; um = std::max (um, u); if (gr > 0.5f) ++act;
            hist[(size_t) juce::jlimit (0, 23, (int) gr)] += 1.0f;
        }
        const double N = (double) n;
        inPk = dbOf (pi); outPk = dbOf (po); inRms = dbOf ((float) std::sqrt (si / N)); outRms = dbOf ((float) std::sqrt (so / N));
        grAvg = (float) (sg / N); grMax = gm; upAvg = (float) (su / N); upMax = um; active = (float) ((double) act / N);
        for (auto& h : hist) h /= (float) N;
        {                                                               // RANGE: 50 ms RMS blocks, 95th minus 10th percentile (silent blocks left out)
            const uint64_t bl = (uint64_t) std::max (1.0, fs * 0.05); std::vector<float> bi, bo;
            for (uint64_t s0 = end - n; s0 + bl <= end; s0 += bl)
            {
                double a = 0, b = 0; for (uint64_t i = s0; i < s0 + bl; ++i) { const size_t k = (size_t) (i & (uint64_t) History::kMask); a += (double) H.inAbs[k] * H.inAbs[k]; b += (double) H.outAbs[k] * H.outAbs[k]; }
                const float da = dbOf ((float) std::sqrt (a / (double) bl)), db = dbOf ((float) std::sqrt (b / (double) bl));
                if (da > -70.0f) bi.push_back (da); if (db > -70.0f) bo.push_back (db);
            }
            auto spread = [] (std::vector<float>& v) { if (v.size() < 4) return 0.0f; std::sort (v.begin(), v.end()); return v[(size_t) (0.95 * (double) (v.size() - 1))] - v[(size_t) (0.10 * (double) (v.size() - 1))]; };
            inRange = spread (bi); outRange = spread (bo);
        }
        if (pi < 1.0e-5f) valid = false;
    }
    ParamHelp& ph; juce::uint32 last = 0; int lastVer = -1; bool valid = false;
    float inRange = 0, outRange = 0, spanShown = 4, inPk = -120, outPk = -120, inRms = -120, outRms = -120, grAvg = 0, grMax = 0, upAvg = 0, upMax = 0, active = 0;
    std::array<float, 24> hist {};
};
