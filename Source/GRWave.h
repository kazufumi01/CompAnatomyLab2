#pragma once
#include "Views.h"
#include "StepResponse.h"

inline juce::String fmtTime (double ms)
{
    if (ms < 1.0) return juce::String (juce::roundToInt (ms * 1000.0)) + juce::String::fromUTF8 (" \xc2\xb5s");
    if (ms < 10.0) return juce::String (ms, 1) + " ms";
    if (ms < 1000.0) return juce::String (juce::roundToInt (ms)) + " ms";
    return juce::String (ms / 1000.0, 2) + " s";
}

// =============================================================================
//  GR WAVE. STEP: the compressor's real response to a level step, with the time to 63 / 90 / 99 % marked.
//  LIVE: catches the moment GR rises on the music that is playing and measures the same way.
// =============================================================================
class GRWaveView : public juce::Component
{
public:
    explicit GRWaveView (ParamHelp& h) : ph (h) {}
    int mode() const { return (int) ph.proc.uiState.getProperty ("grMode", 0); }
    int overDb() const { return juce::jlimit (6, 18, (int) ph.proc.uiState.getProperty ("stepOver", 12)); }
    juce::Rectangle<int> modeChip (int i) const { return { getWidth() - 10 - 2 * 38 + i * 38, 5, 34, 15 }; }
    juce::Rectangle<int> overChip (int i) const { return { getWidth() - 10 - 2 * 38 - 8 - 3 * 31 + i * 31, 5, 28, 15 }; }

    void refresh()
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (mode() == 0)
        {
            const auto p = ph.proc.currentParams(); const int ov = overDb();
            if (! haveKey || differs (p) || ov != keyOver)
            {
                if (now - lastCalc < 60) return;                      // while dragging, at most ~16 per second
                step = mod::measureStep (p, (double) ov, 48000.0);
                key = mod::stepKey (p); keyOver = ov; haveKey = true; lastCalc = now; repaint();
            }
        }
        else
        {
            const bool fz = ph.proc.isFrozen(); const int ver = ph.proc.previewVersion();      // stopped: measure again whenever the preview was re-computed
            if (fz ? ver != lastVer : now >= holdUntil) { lastVer = ver; if (captureLive()) { holdUntil = now + 2500; repaint(); } }
        }
    }
    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth();
        g.setColour (theme::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        drawLowerTabs (g, 0);
        for (int i = 0; i < 2; ++i) chipDraw (g, modeChip (i), i == 0 ? "STEP" : "LIVE", i == mode());
        if (mode() == 0) for (int i = 0; i < 3; ++i) chipDraw (g, overChip (i), "+" + juce::String (6 + 6 * i), overDb() == 6 + 6 * i);
        else { g.setColour (theme::faint); g.setFont (theme::ui (8.5f)); g.drawText (ph.tr ("実音で測定", "measured on the music"), 150, 5, 110, 15, juce::Justification::centredLeft); }
        paintWell (g, juce::Rectangle<float> (6.0f, 24.0f, W - 12.0f, 132.0f));
        if (mode() == 0) paintStep (g); else paintLive (g);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (const int t = hitLowerTab (e.getPosition()); t >= 0) { ph.proc.uiState.setProperty ("lowerTab", t, nullptr); return; }
        for (int i = 0; i < 2; ++i) if (modeChip (i).contains (e.getPosition())) { ph.proc.uiState.setProperty ("grMode", i, nullptr); holdUntil = 0; repaint(); return; }
        if (mode() == 0) for (int i = 0; i < 3; ++i) if (overChip (i).contains (e.getPosition())) { ph.proc.uiState.setProperty ("stepOver", 6 + 6 * i, nullptr); repaint(); return; }
    }

private:
    void chipDraw (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, bool on)
    {
        g.setColour (on ? theme::accent : theme::hilite (0.12f)); g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (on ? theme::onAccent : theme::onWell()); g.setFont (theme::mono (8.5f, true)); g.drawText (t, r, juce::Justification::centred);
    }
    bool differs (const mod::Params& p) const { return mod::stepKey (p) != key; }
    void marker (juce::Graphics& g, float x, float y, float top, float bottom, const juce::String& label, juce::Colour c, bool big)
    {
        const float dash[] { 2.5f, 2.5f };
        g.setColour (c); g.drawDashedLine (juce::Line<float> (x, top, x, bottom), dash, 2, 0.8f);
        g.fillEllipse (x - 2.6f, y - 2.6f, 5.2f, 5.2f);
        g.setFont (theme::mono (big ? 8.5f : 7.5f, big)); g.drawText (label, (int) x - 30, (int) bottom + 1, 60, 11, juce::Justification::centred);
    }
    void paintStep (juce::Graphics& g)
    {
        const float W = (float) getWidth(), gx0 = 16.0f, gx1 = W - 12.0f, split = gx0 + (gx1 - gx0) * 0.575f;
        const float yIn0 = 33, yIn1 = 47, gy0 = 60, gy1 = 142;
        const auto p = ph.proc.currentParams();
        const bool opto = false;
        // the input strip and the axis break are drawn even when there is nothing to measure
        const float dash[] { 2.0f, 2.0f };
        g.setColour (theme::muted); g.setFont (theme::mono (7.0f)); g.drawText ("IN", 8, (int) yIn0, 14, 14, juce::Justification::centredLeft);
        const float yT = (yIn0 + yIn1) * 0.5f;
        g.setColour (theme::accent); g.drawDashedLine (juce::Line<float> (gx0 + 10, yT, gx1, yT), dash, 2, 0.8f);
        for (float dx : { -2.0f, 2.0f }) { g.setColour (theme::faint); g.drawLine (split + dx - 2, yIn0 - 6, split + dx + 2, gy1 + 6, 0.8f); }
        auto area = juce::Rectangle<int> (10, 158, getWidth() - 20, 32);
        if (! step.valid)
        {
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
            g.drawFittedText (ph.tr ("GR が出ません。Ratio が 1:1 か、ステップがニーの下にあります。Ratio を上げるか、右上の +12 / +18 を選んでください。",
                                     "No gain reduction: Ratio is 1:1 or the step is below the knee. Raise Ratio, or pick +12 / +18."),
                              16, 56, (int) W - 32, 60, juce::Justification::centred, 4, 0.9f);
            return;
        }
        const double aMax = std::max (0.2, step.a99 > 0 ? step.a99 * 1.08 : 100.0), rMax = std::max (20.0, step.r99 > 0 ? step.r99 * 1.08 : 1000.0);
        const double preMs = std::min (step.preMs, aMax * 0.12);      // short lead-in so a fast attack still fills the plot
        const float aL = gx0 + 12.0f, aR = split - 7.0f, rL = split + 9.0f, rR = gx1;
        auto tA = [&] (double ms) { return aL + (float) ((ms + preMs) / (aMax + preMs)) * (aR - aL); };
        auto tR = [&] (double ms) { return rL + (float) (ms / rMax) * (rR - rL); };
        const double fin = step.finalGr; const float dbpx = (gy1 - gy0 - 8.0f) / (float) fin;
        auto yG = [&] (double db) { return gy0 + (float) db * dbpx; };
        // input strip
        g.setColour (theme::faint); juce::Path in;
        in.startNewSubPath (tA (-preMs), yIn1); in.lineTo (tA (0), yIn1); in.lineTo (tA (0), yIn0 + 1); in.lineTo (aR, yIn0 + 1);
        in.startNewSubPath (rL, yIn0 + 1); in.lineTo (tR (0), yIn0 + 1); in.lineTo (tR (0), yIn1); in.lineTo (rR, yIn1);
        g.setColour (theme::muted); g.strokePath (in, juce::PathStrokeType (1.3f));
        // axes
        g.setColour (theme::grid); g.drawHorizontalLine ((int) yG (0), aL, rR);
        g.setColour (theme::faint); g.setFont (theme::mono (7.0f));
        g.drawText ("0", 2, (int) yG (0) - 6, 12, 12, juce::Justification::centredRight); g.drawText (minus (-fin, 1), 0, (int) yG (fin) - 6, 15, 12, juce::Justification::centredRight);
        // target
        g.setColour (theme::accent); g.drawDashedLine (juce::Line<float> (tA (0), yG (fin), aR, yG (fin)), dash, 2, 1.0f);
        g.drawDashedLine (juce::Line<float> (rL, yG (fin), tR (0), yG (fin)), dash, 2, 1.0f);
        // attack curve and the part that is not yet reduced
        juce::Path ac, shade; const int na = std::max (2, (int) (aR - aL));
        for (int i = 0; i <= na; ++i)
        {
            const double ms = -preMs + (aMax + preMs) * i / na; const size_t k = (size_t) std::max (0.0, (ms + step.preMs) * 0.001 * step.fs);
            const float gr = k < step.attack.size() ? step.attack[k] : (float) fin, x = aL + (aR - aL) * i / na;
            if (i == 0) { ac.startNewSubPath (x, yG (gr)); shade.startNewSubPath (x, yG (fin)); } else ac.lineTo (x, yG (gr));
            shade.lineTo (x, yG (gr));
        }
        shade.lineTo (aR, yG (fin)); shade.closeSubPath();
        g.setColour (theme::phase.withAlpha (0.20f)); g.fillPath (shade);
        g.setColour (theme::pre); g.strokePath (ac, juce::PathStrokeType (1.8f));
        // release curve
        juce::Path rc; const int nr = std::max (2, (int) (rR - rL));
        for (int i = 0; i <= nr; ++i)
        {
            const double ms = rMax * i / nr; const size_t k = (size_t) (ms * 0.001 * step.fs);
            const float gr = k < step.release.size() ? step.release[k] : 0.0f, x = rL + (rR - rL) * i / nr;
            if (i == 0) { rc.startNewSubPath (rL, yG (fin)); rc.lineTo (x, yG (gr)); } else rc.lineTo (x, yG (gr));
        }
        g.strokePath (rc, juce::PathStrokeType (1.8f));
        if (! step.slowRel.empty())                                  // the slow part of a 2-stage / opto / auto release
        {
            juce::Path sp;
            for (int i = 0; i <= nr; ++i)
            {
                const size_t k = (size_t) (rMax * i / nr * 0.001 * step.fs); const float s = k < step.slowRel.size() ? std::min ((float) fin, step.slowRel[k]) : 0.0f, x = rL + (rR - rL) * i / nr;
                if (i == 0) sp.startNewSubPath (x, yG (s)); else sp.lineTo (x, yG (s));
            }
            g.setColour (theme::phase); g.strokePath (sp, juce::PathStrokeType (1.1f));
            g.setFont (theme::ui (7.5f)); g.drawText (ph.tr ("黄＝遅い成分", "yellow = slow part"), (int) rR - 90, (int) gy1 - 22, 88, 10, juce::Justification::centredRight);
        }
        g.setColour (theme::muted); g.setFont (theme::ui (7.5f));
        g.drawText (ph.tr ("Attack 区間（黄＝まだ絞れていない量）", "Attack (yellow = not reduced yet)"), (int) aL, (int) gy0 - 12, (int) (aR - aL) + 8, 10, juce::Justification::centredLeft);
        g.drawText (ph.tr ("Release 区間（軸を縮小）", "Release (time axis compressed)"), (int) rL, (int) gy0 - 12, (int) (rR - rL), 10, juce::Justification::centredLeft);
        // markers
        const float bot = gy1 + 2;
        if (step.a63 > 0) marker (g, tA (step.a63), yG (fin * (1 - std::exp (-1.0))), yIn1, bot, fmtTime (step.a63), theme::text, true);
        if (step.a90 > 0) marker (g, tA (step.a90), yG (fin * 0.9), yIn1, bot, fmtTime (step.a90), theme::muted, false);
        if (step.a99 > 0) marker (g, tA (step.a99), yG (fin * 0.99), yIn1, bot, fmtTime (step.a99), theme::muted, false);
        if (step.r63 > 0) marker (g, tR (step.r63), yG (fin * std::exp (-1.0)), yIn1, bot, fmtTime (step.r63), theme::text, true);
        if (step.r99 > 0) marker (g, tR (step.r99), yG (fin * 0.01), yIn1, bot, fmtTime (step.r99), theme::muted, false);
        g.setColour (theme::text); g.setFont (theme::ui (8.0f, true)); g.drawText ("63%", (int) tA (step.a63) + 4, (int) yG (fin * (1 - std::exp (-1.0))) - 12, 30, 10, juce::Justification::centredLeft);
        g.drawText (ph.tr ("63% 戻る", "63% back"), (int) tR (step.r63) + 4, (int) yG (fin * std::exp (-1.0)) - 12, 50, 10, juce::Justification::centredLeft);
        // readouts
        const bool ja = ph.ja();
        const juce::String atSet = opto ? ph.tr ("（光学は固定）", "(Opto: fixed)") : ph.tr ("設定", "set") + " " + fmtTime (p.attack);
        const juce::String reSet = opto ? ph.tr ("（光学は固定）", "(Opto: fixed)") : ph.tr ("設定", "set") + " " + fmtTime (p.release);
        const bool eff = ! opto && std::abs (step.a63 - p.attack) > 0.06 * p.attack + 0.02;
        g.setFont (theme::ui (8.5f, true)); g.setColour (eff ? theme::accent : theme::pre);
        const juce::String eq = ja ? juce::String::fromUTF8 ("\xef\xbc\x9d") : juce::String (" = "), sep = juce::String::fromUTF8 ("\xef\xbc\x8f"), arrow = juce::String::fromUTF8 (" \xe2\x86\x92 ");
        g.drawFittedText ("Attack " + atSet + arrow + (eff ? ph.tr ("実効 ", "effective ") : juce::String()) + "63%" + eq + fmtTime (step.a63) + sep + "90%" + eq + fmtTime (step.a90) + sep + "99%" + eq + fmtTime (step.a99),
                          area.removeFromTop (16), juce::Justification::centredLeft, 1, 0.7f);
        g.setColour (theme::pre);
        g.drawFittedText ("Release " + reSet + juce::String::fromUTF8 (" \xe2\x86\x92 ") + ph.tr ("63% 戻るまで ", "63% back in ") + fmtTime (step.r63) + juce::String::fromUTF8 ("\xef\xbc\x8f") + "99% " + fmtTime (step.r99),
                          area, juce::Justification::centredLeft, 1, 0.7f);
    }

    // ---- LIVE ----
    bool captureLive()
    {
        const auto& H = ph.proc.display(); const double fs = ph.proc.getSampleRateOrDefault();
        const uint64_t end = H.pos.load (std::memory_order_acquire);
        const int back = (int) (fs * 2.4), post = (int) (fs * 0.4), pre = (int) (fs * 0.005);
        if (end < (uint64_t) (back + post)) return false;
        auto gr = [&] (uint64_t i) { return H.gr[(size_t) (i & (uint64_t) History::kMask)]; };
        for (uint64_t i = end - (uint64_t) post - 1; i > end - (uint64_t) back; i -= 4)
        {
            if (! (gr (i) >= 0.5f && gr (i - 4) < 0.5f)) continue;
            float pk = 0; for (int k = 0; k < (int) (fs * 0.12); ++k) pk = std::max (pk, gr (i + (uint64_t) k));
            if (pk < 3.0f) continue;
            uint64_t on = i; while (on > i - 64 && gr (on) > 0.05f) --on;
            live.clear(); for (int k = -pre; k < post; ++k) live.push_back (gr ((uint64_t) ((int64_t) on + k)));
            livePre = pre; liveFs = fs; livePeak = pk;
            auto firstAbove = [&] (double f) { for (size_t k = (size_t) pre; k < live.size(); ++k) if (live[k] >= f * pk) return (double) ((int) k - pre) / fs * 1000.0; return -1.0; };
            lt63 = firstAbove (1.0 - std::exp (-1.0)); lt90 = firstAbove (0.9); lt99 = firstAbove (0.99);
            return true;
        }
        return false;
    }
    void paintLive (juce::Graphics& g)
    {
        const float W = (float) getWidth(), x0 = 24, x1 = W - 14, y0 = 40, y1 = 140;
        if (live.empty())
        {
            g.setColour (theme::muted); g.setFont (theme::ui (10.0f));
            g.drawFittedText (ph.tr ("再生すると、GR が 0.5 dB から 3 dB 以上へ立ち上がった瞬間を自動で捕まえて、同じ印で測ります（2.5 秒表示を止めます）。",
                                     "While playing, this catches the moment GR rises from 0.5 dB past 3 dB and measures it the same way (holds for 2.5 s)."),
                              16, 50, (int) W - 32, 70, juce::Justification::centred, 4, 0.9f);
            return;
        }
        const double tMax = juce::jlimit (20.0, 400.0, (lt99 > 0 ? lt99 : 100.0) * 1.3), preMs = (double) livePre / liveFs * 1000.0;
        auto X = [&] (double ms) { return x0 + (float) ((ms + preMs) / (tMax + preMs)) * (x1 - x0); };
        const float dbpx = (y1 - y0 - 6.0f) / livePeak; auto Y = [&] (double db) { return y0 + (float) db * dbpx; };
        g.setColour (theme::grid); g.drawHorizontalLine ((int) Y (0), x0, x1);
        const float dash[] { 2.5f, 2.5f };
        g.setColour (theme::accent); g.drawDashedLine (juce::Line<float> (X (0), Y (livePeak), x1, Y (livePeak)), dash, 2, 1.0f);
        g.setFont (theme::mono (7.5f)); g.drawText (ph.tr ("ピーク GR ", "peak GR ") + minus (livePeak) + " dB", (int) x1 - 130, (int) Y (livePeak) + 2, 130, 10, juce::Justification::centredRight);
        juce::Path c; const int n = std::max (2, (int) (x1 - x0));
        for (int i = 0; i <= n; ++i)
        {
            const double ms = -preMs + (tMax + preMs) * i / n; const size_t k = (size_t) juce::jlimit (0.0, (double) live.size() - 1, (ms + preMs) * 0.001 * liveFs);
            if (i == 0) c.startNewSubPath (x0, Y (live[k])); else c.lineTo (x0 + (x1 - x0) * i / n, Y (live[k]));
        }
        g.setColour (theme::pre); g.strokePath (c, juce::PathStrokeType (1.8f));
        auto mk = [&] (double ms, double f, bool big) { if (ms > 0) marker (g, X (ms), Y (f * livePeak), y0 - 6, y1 + 2, fmtTime (ms), big ? theme::text : theme::muted, big); };
        mk (lt63, 1.0 - std::exp (-1.0), true); mk (lt90, 0.9, false); mk (lt99, 0.99, false);
        auto area = juce::Rectangle<int> (10, 158, getWidth() - 20, 32);
        g.setFont (theme::ui (8.5f, true)); g.setColour (theme::pre);
        g.drawFittedText (ph.tr ("実測 Attack　63%＝", "Measured attack  63% = ") + fmtTime (lt63) + juce::String::fromUTF8 ("\xef\xbc\x8f") + "90% " + fmtTime (lt90) + juce::String::fromUTF8 ("\xef\xbc\x8f") + "99% " + fmtTime (lt99),
                          area.removeFromTop (16), juce::Justification::centredLeft, 1, 0.7f);
        g.setColour (theme::muted); g.setFont (theme::ui (8.0f));
        g.drawFittedText (ph.tr ("実音は最終 GR がまだ動くので、ピーク GR（120 ms 以内）を 100% とした目安です。", "On music GR keeps moving, so 100% is the peak GR within 120 ms — a guide, not exact."),
                          area, juce::Justification::centredLeft, 1, 0.7f);
    }

    ParamHelp& ph;
    mod::StepResult step; std::array<double, 19> key {}; int keyOver = 0; bool haveKey = false; juce::uint32 lastCalc = 0, holdUntil = 0;
    int lastVer = -1;
    std::vector<float> live; int livePre = 0; double liveFs = 48000, lt63 = -1, lt90 = -1, lt99 = -1; float livePeak = 1;
};
