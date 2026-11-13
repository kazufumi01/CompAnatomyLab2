#pragma once
#include "Widgets.h"

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga; ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}
inline float dbOf (float lin) { return lin > 1.0e-6f ? 20.0f * std::log10 (lin) : -120.0f; }
inline void paintWell (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (theme::well); g.fillRoundedRectangle (b, 6.0f);
    g.setColour (theme::line); g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
}
inline juce::String minus (double v, int d = 1) { return (v < -0.0001 ? juce::String::fromUTF8 ("\xe2\x88\x92") : juce::String()) + juce::String (std::abs (v), d); }

// tabs of the lower-left panel (GR WAVE | DYNAMICS): drawn and hit-tested the same way by both views
inline juce::Rectangle<int> lowerTabRect (int i) { static const int x[] { 8, 32, 60, 96, 128 }, w[] { 22, 26, 34, 30, 20 }; return { x[i], 5, w[i], 15 }; }     // ends at 148: the GR WAVE chips start at 153
inline void drawLowerTabs (juce::Graphics& g, int active)
{
    static const char* nm[] { "GR", "DYN", "PANEL", "HARM", "SC" };   // GR wave | dynamics | the model's controls, small | harmonics | sidechain filter
    for (int i = 0; i < 5; ++i)
    {
        const auto r = lowerTabRect (i); const bool on = i == active;
        g.setColour (on ? theme::accent : theme::hilite (0.12f)); g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (on ? theme::onAccent : theme::onWell()); g.setFont (theme::mono (8.5f, true)); g.drawText (nm[i], r, juce::Justification::centred);
    }
}
inline int hitLowerTab (juce::Point<int> p) { for (int i = 0; i < 5; ++i) if (lowerTabRect (i).contains (p)) return i; return -1; }
// top-left view: TIME | SPECTRUM
inline juce::Rectangle<int> upperTabRect (int i) { return i == 0 ? juce::Rectangle<int> (8, 5, 44, 16) : juce::Rectangle<int> (54, 5, 76, 16); }
inline void drawUpperTabs (juce::Graphics& g, int active)
{
    static const char* nm[] { "TIME", "SPECTRUM" };
    for (int i = 0; i < 2; ++i)
    {
        const auto r = upperTabRect (i); const bool on = i == active;
        g.setColour (on ? theme::accent : theme::hilite (0.12f)); g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (on ? theme::onAccent : theme::onWell()); g.setFont (theme::mono (8.5f, true)); g.drawText (nm[i], r, juce::Justification::centred);
    }
}
inline int hitUpperTab (juce::Point<int> p) { for (int i = 0; i < 2; ++i) if (upperTabRect (i).contains (p)) return i; return -1; }
// middle row, top right, always shown: PANEL (opaque) > see-through 1 – 3 > BUILD
inline juce::Rectangle<int> viewBtnRect (int i, int w) { return { w - 10 - (5 - i) * 46, 5, 44, 16 }; }
inline int viewModeOf (const juce::ValueTree& s) { return (int) s.getProperty ("panelMode", 0) == 0 ? 4 : juce::jlimit (0, 3, (int) s.getProperty ("panelSee", 1)); }
inline void setViewMode (juce::ValueTree& s, int m) { if (m >= 4) s.setProperty ("panelMode", 0, nullptr); else { s.setProperty ("panelSee", m, nullptr); s.setProperty ("panelMode", 1, nullptr); } }
inline void drawViewBtns (juce::Graphics& g, int w, int active, bool ja)        // own colours: readable on every faceplate and in every look
{
    for (int i = 0; i < 5; ++i)
    {
        const auto r = viewBtnRect (i, w).toFloat(); const bool on = i == active;
        g.setColour (on ? theme::accent : juce::Colour (0xff2b2f36)); g.fillRoundedRectangle (r, 3.0f);
        if (! on) { g.setColour (juce::Colour (0xff596070)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f); }
        const juce::String t = i == 0 ? juce::String ("PANEL") : i == 4 ? juce::String ("BUILD") : (ja ? juce::String::fromUTF8 ("\xe9\x80\x8f\xe9\x81\x8e ") : juce::String ("SEE ")) + juce::String (i);
        g.setColour (on ? theme::onAccent : juce::Colour (0xffe6e6e6)); g.setFont (theme::ui (9.5f, true)); g.drawText (t, r, juce::Justification::centred);
    }
}
inline int hitViewBtn (juce::Point<int> p, int w) { for (int i = 0; i < 5; ++i) if (viewBtnRect (i, w).contains (p)) return i; return -1; }

// =============================================================================
//  TIME: input / output level and gain reduction over the last 1–8 s.
//  GR hangs from the top on the same dB scale as the levels.
// =============================================================================
class TimeView : public juce::Component
{
public:
    explicit TimeView (ParamHelp& h) : ph (h) {}
    static double spanSec (int i) { static const double s[] { 1, 2, 4, 8 }; return s[juce::jlimit (0, 3, i)]; }
    int spanIdx() const { return (int) ph.proc.uiState.getProperty ("timeSpan", 2); }
    juce::Rectangle<int> chip (int i) const { return { getWidth() - 12 - 4 * 38 + i * 38, 5, 34, 16 }; }

    void paint (juce::Graphics& g) override
    {
        paintWell (g, getLocalBounds().toFloat());
        drawUpperTabs (g, 0);
        if (ph.proc.isFrozen())                                       // the input has stopped: the last sound, re-processed with the current settings
        {
            const juce::String t = ph.proc.frozenIsBuiltin() ? ph.tr ("停止中：内蔵テスト音を今の設定で計算", "Stopped: built-in test loop, computed with the current settings")
                                                             : ph.tr ("停止中：最後に鳴った音を今の設定で再計算", "Stopped: the last sound, re-computed with the current settings");
            const int w = (int) textWidth (theme::ui (9.5f, true), t) + 16;
            g.setColour (theme::accent.withAlpha (0.18f)); g.fillRoundedRectangle (140.0f, 5.0f, (float) w, 16.0f, 3.0f);
            g.setColour (theme::accent); g.setFont (theme::ui (9.5f, true)); g.drawText (t, 140, 5, w, 16, juce::Justification::centred);
        }
        for (int i = 0; i < 4; ++i)
        {
            const bool on = i == spanIdx(); auto r = chip (i);
            g.setColour (on ? theme::accent : theme::hilite (0.12f)); g.fillRoundedRectangle (r.toFloat(), 3.0f);
            g.setColour (on ? theme::onAccent : theme::onWell()); g.setFont (theme::mono (9.0f, on));
            g.drawText (juce::String ((int) spanSec (i)) + " s", r, juce::Justification::centred);
        }
        const float x0 = 46, x1 = (float) getWidth() - 12, y0 = 30, y1 = (float) getHeight() - 28;
        auto yL = [&] (float db) { return y0 + (-juce::jlimit (-60.0f, 0.0f, db)) / 60.0f * (y1 - y0); };
        g.setFont (theme::mono (9.0f));
        for (int k = 0; k <= 60; k += 12)
        {
            g.setColour (theme::grid); g.drawHorizontalLine ((int) yL ((float) -k), x0, x1);
            g.setColour (theme::faint); g.drawText (k == 0 ? "0" : minus (-k, 0), 4, (int) yL ((float) -k) - 6, 38, 12, juce::Justification::centredRight);
        }
        const auto p = ph.proc.currentParams();
        const auto& H = ph.proc.display(); const double fs = ph.proc.getSampleRateOrDefault();
        const uint64_t end = H.pos.load (std::memory_order_acquire);
        const int cols = juce::jmax (1, (int) (x1 - x0)); const double spc = spanSec (spanIdx()) * fs / cols;
        juce::Path pin, pout, pmem; bool sIn = false, sOut = false;
        const bool opto = p.timing == mod::TimProg2 || p.timing == mod::TimAuto || p.timing == mod::TimOptoMem || p.det == mod::DetCrest;
        for (int c = 0; c < cols; ++c)
        {
            const double s0 = (double) end - (double) (cols - c) * spc;
            if (s0 < 0) continue;
            const int64_t a = (int64_t) s0, b = std::max (a + 1, (int64_t) (s0 + spc));
            float mi = 0, mo = 0, mg = 0, mm = 0, mu = 0, mp = 0;
            for (int64_t i = a; i < b; ++i)
            {
                const size_t k = (size_t) ((uint64_t) i & (uint64_t) History::kMask);
                mi = std::max (mi, H.inAbs[k]); mo = std::max (mo, H.outAbs[k]); mg = std::max (mg, H.gr[k]); mm = std::max (mm, H.mem[k]); mu = std::max (mu, H.up[k]); mp = std::max (mp, H.pre[k]);
            }
            const float x = x0 + (float) c;
            if (mg > 0.02f) { g.setColour (theme::pre.withAlpha (0.42f)); g.fillRect (x, y0, 1.0f, yL (-mg) - y0); }
            if (mu > 0.02f) { const float hgt = mu / 60.0f * (y1 - y0); g.setColour (theme::ok.withAlpha (0.6f)); g.fillRect (x, y1 - hgt, 1.0f, hgt); }   // boost rises from the bottom
            if (mp > 0.02f) { g.setColour (juce::Colour (0xffb48cf2)); g.fillRect (x, yL (-mp), 1.0f, 1.6f); }           // pre-stage GR
            const float yi = yL (dbOf (mi)), yo = yL (dbOf (mo));
            if (! sIn) { pin.startNewSubPath (x, yi); sIn = true; } else pin.lineTo (x, yi);
            if (! sOut) { pout.startNewSubPath (x, yo); sOut = true; } else pout.lineTo (x, yo);
            if (opto && c % 2 == 0 && mm > 0.02f) { g.setColour (theme::phase.withAlpha (0.9f)); g.fillRect (x, yL (-mm), 1.5f, 1.5f); }
        }
        g.setColour (theme::faint.withAlpha (0.9f)); g.strokePath (pin, juce::PathStrokeType (1.1f));
        g.setColour (theme::side); g.strokePath (pout, juce::PathStrokeType (1.5f));
        // threshold
        const float ty = yL ((float) p.threshold);
        const float dash[] { 4.0f, 3.0f };
        g.setColour (theme::accent); g.drawDashedLine (juce::Line<float> (x0, ty, x1, ty), dash, 2, 1.0f);
        g.setFont (theme::mono (9.0f)); g.drawText (juce::String (p.law == mod::LawRide ? "Target " : "Threshold ") + minus (p.threshold) + " dB", (int) x1 - 160, (int) ty - 13, 156, 12, juce::Justification::centredRight);
        if (p.upOn)
        {
            const float uy = yL ((float) p.upThreshold);
            g.setColour (theme::ok); g.drawDashedLine (juce::Line<float> (x0, uy, x1, uy), dash, 2, 1.0f);
            g.setFont (theme::mono (9.0f)); g.drawText ("Up thr " + minus (p.upThreshold) + " dB", (int) x0 + 4, (int) uy - 13, 150, 12, juce::Justification::centredLeft);
        }
        // legend
        const float ly = y1 + 8; float lx = x0;
        auto key = [&] (juce::Colour c, const juce::String& t) { g.setColour (c); g.fillRect (lx, ly + 2, 8.0f, 8.0f); g.setColour (theme::onWell()); g.setFont (theme::ui (9.5f));
                                                                  const int w = (int) textWidth (theme::ui (9.5f), t) + 6; g.drawText (t, (int) lx + 11, (int) ly, w, 12, juce::Justification::centredLeft); lx += 11 + w + 8; };
        key (theme::faint, ph.tr ("入力", "Input")); key (theme::side, ph.tr ("出力", "Output")); key (theme::pre, ph.tr ("GR（上から下向き・同じ dB 目盛）", "GR (hangs from the top, same dB scale)"));
        if (p.pre && ! p.mb) key (juce::Colour (0xffb48cf2), ph.tr ("前段（光学 2:1）の GR", "pre-stage (opto 2:1) GR"));
        if (opto) key (theme::phase, p.det == mod::DetCrest ? ph.tr ("RMS 経路", "RMS path") : ph.tr ("遅いリリース成分", "slow release part"));
        if (p.upOn || p.law == mod::LawRide) key (theme::ok, p.law == mod::LawRide ? ph.tr ("ゲインを上げた量（下から上向き・1 目盛 12 dB）", "gain raised (from the bottom, 12 dB per line)")
                                                                                    : ph.tr ("アップワードの持ち上げ量（下から上向き・1 目盛 12 dB）", "upward boost (rises from the bottom, 12 dB per line)"));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (const int t = hitUpperTab (e.getPosition()); t >= 0) { ph.proc.uiState.setProperty ("upperTab", t, nullptr); return; }
        for (int i = 0; i < 4; ++i) if (chip (i).contains (e.getPosition())) { ph.proc.uiState.setProperty ("timeSpan", i, nullptr); repaint(); return; }
        drag = hitLine (e.getPosition());
        if (drag != 0) { ph.proc.undo.beginNewTransaction(); ph.begin (drag == 1 ? pid::threshold : pid::upThr); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == 0) return;
        const float y0 = 30, y1 = (float) getHeight() - 28, db = juce::jlimit (-60.0f, 0.0f, -((float) e.y - y0) / (y1 - y0) * 60.0f);
        ph.set (drag == 1 ? pid::threshold : pid::upThr, db); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag != 0) { ph.end (drag == 1 ? pid::threshold : pid::upThr); drag = 0; } }
    void mouseMove (const juce::MouseEvent& e) override { setMouseCursor (hitLine (e.getPosition()) != 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); }
private:
    int hitLine (juce::Point<int> pt) const               // 1 = threshold line, 2 = upward threshold line
    {
        if (pt.x < 46 || pt.y < 24) return 0;
        const auto p = ph.proc.currentParams(); const float y0 = 30, y1 = (float) getHeight() - 28;
        auto yL = [&] (double db) { return y0 + (float) (-juce::jlimit (-60.0, 0.0, db)) / 60.0f * (y1 - y0); };
        if (std::abs ((float) pt.y - yL (p.threshold)) <= 5.0f) return 1;
        if (p.upOn && std::abs ((float) pt.y - yL (p.upThreshold)) <= 5.0f) return 2;
        return 0;
    }
    ParamHelp& ph; int drag = 0;
};

// =============================================================================
//  TRANSFER: static input → output curve. Drag the circle = Threshold, the dot at 0 dB = Ratio.
// =============================================================================
class TransferView : public juce::Component
{
public:
    explicit TransferView (ParamHelp& h) : ph (h) {}
    void paint (juce::Graphics& g) override
    {
        paintWell (g, getLocalBounds().toFloat());
        g.setColour (theme::onWell()); g.setFont (theme::ui (10.0f));
        g.drawText (ph.tr ("TRANSFER　入出力カーブ（静特性）", "TRANSFER  input / output curve"), 10, 7, getWidth() - 20, 14, juce::Justification::centredLeft);
        const auto p = ph.proc.currentParams(); setGeom();
        g.setFont (theme::mono (8.5f));
        for (int k = 0; k <= 60; k += 12)
        {
            g.setColour (theme::grid); g.drawVerticalLine ((int) X (-k), py0, py0 + sz); g.drawHorizontalLine ((int) Y (-k), px0, px0 + sz);
            g.setColour (theme::faint); g.drawText (k == 0 ? "0" : minus (-k, 0), (int) X (-k) - 12, (int) (py0 + sz) + 2, 24, 11, juce::Justification::centred);
            g.drawText (k == 0 ? "0" : minus (-k, 0), (int) px0 - 30, (int) Y (-k) - 6, 26, 12, juce::Justification::centredRight);
        }
        g.setColour (theme::line); g.drawLine (X (-60), Y (-60), X (0), Y (0), 1.0f);
        if (p.law != mod::LawIdeal)                                  // the ideal curve for the same threshold / ratio / knee, for comparison
        {
            juce::Path id;
            for (int i = 0; i <= 240; ++i) { const double x = -60.0 + i * 0.25, y = x - mod::idealGr (x, p.threshold, p.ratio, p.knee); if (i == 0) id.startNewSubPath (X (x), Y (y)); else id.lineTo (X (x), Y (y)); }
            g.setColour (theme::faint); g.strokePath (id, juce::PathStrokeType (1.2f));
            g.setFont (theme::ui (8.5f)); g.drawText (ph.tr ("灰色＝理想（同じ T / R / Knee）", "grey = ideal (same T / R / knee)"), (int) px0 + 4, (int) py0 + 2, 170, 11, juce::Justification::centredLeft);
        }
        juce::Path c;
        for (int i = 0; i <= 240; ++i)
        {
            const double x = -60.0 + i * 0.25, y = mod::lawOut (x, p);
            if (i == 0) c.startNewSubPath (X (x), Y (y)); else c.lineTo (X (x), Y (y));
        }
        g.setColour (theme::curve); g.strokePath (c, juce::PathStrokeType (2.2f));
        const float tx = X (p.threshold), ty = Y (mod::lawOut (p.threshold, p));
        g.setColour (theme::curve); g.drawEllipse (tx - 5, ty - 5, 10, 10, 2.0f);
        const float rx = X (0), ry = Y (mod::lawOut (0, p));
        g.fillEllipse (rx - 3.5f, ry - 3.5f, 7, 7);
        // live operating point
        const auto& H = ph.proc.display(); const uint64_t end = H.pos.load (std::memory_order_acquire);
        if (end > 4096)
        {
            float mi = 0; for (uint64_t i = end - 2400; i < end; ++i) mi = std::max (mi, H.inAbs[(size_t) (i & (uint64_t) History::kMask)]);
            const float gr = H.gr[(size_t) ((end - 1) & (uint64_t) History::kMask)], x = dbOf (mi);
            if (x > -59.0f) { g.setColour (theme::side); g.fillEllipse (X (x) - 4.5f, Y (x - gr) - 4.5f, 9, 9); g.setColour (theme::well); g.drawEllipse (X (x) - 4.5f, Y (x - gr) - 4.5f, 9, 9, 1.2f); }
        }
        g.setColour (theme::faint); g.setFont (theme::ui (9.0f));
        g.drawText (ph.tr ("ドラッグ：○ = Threshold ／ ● = Ratio", "Drag: ○ = Threshold / ● = Ratio"), 10, getHeight() - 16, 200, 12, juce::Justification::centredLeft);
        g.setColour (theme::onWell()); g.setFont (theme::mono (9.0f));
        g.drawText (ph.tr ("入力 dB", "IN dB"), (int) px0 + (int) sz - 70, (int) (py0 + sz) + 12, 70, 11, juce::Justification::centredRight);
        g.drawText (ph.tr ("出力", "OUT"), 6, 22, 30, 11, juce::Justification::centredLeft);
    }
    void mouseMove (const juce::MouseEvent& e) override { setMouseCursor (hit (e.position) != None ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        mode = hit (e.position); if (mode == None) return;
        ph.proc.undo.beginNewTransaction(); ph.begin (mode == Thr ? pid::threshold : pid::ratio);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (mode == None) return; setGeom();
        if (mode == Thr) ph.set (pid::threshold, juce::jlimit (-60.0f, 0.0f, (float) (((double) e.position.x - px0) / sz * 60.0 - 60.0)));
        else
        {
            const double out = juce::jlimit (-60.0, 0.0, -60.0 + (py0 + sz - (double) e.position.y) / sz * 60.0), T = ph.value (pid::threshold);
            ph.set (pid::ratio, out > T + 0.2 ? (float) juce::jlimit (1.0, 20.0, (0.0 - T) / (out - T)) : 20.0f);
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (mode != None) { ph.end (mode == Thr ? pid::threshold : pid::ratio); mode = None; } }
private:
    enum Mode { None, Thr, Ratio };
    void setGeom() { sz = (float) juce::jmin (getWidth() - 46, getHeight() - 58); px0 = 36; py0 = 28; }
    float X (double db) const { return px0 + (float) ((db + 60.0) / 60.0) * sz; }
    float Y (double db) const { return py0 + sz - (float) ((std::min (db, 0.0) + 60.0) / 60.0) * sz; }
    Mode hit (juce::Point<float> pt)
    {
        setGeom(); const auto p = ph.proc.currentParams();
        const juce::Point<float> t (X (p.threshold), Y (mod::lawOut (p.threshold, p)));
        const juce::Point<float> r (X (0), Y (mod::lawOut (0, p)));
        if (pt.getDistanceFrom (t) < 10.0f) return Thr;
        if (pt.getDistanceFrom (r) < 10.0f) return Ratio;
        return None;
    }
    ParamHelp& ph; float sz = 200, px0 = 36, py0 = 28; Mode mode = None;
};

// =============================================================================
//  IN / GR / OUT peak meters
// =============================================================================
class MeterView : public juce::Component
{
public:
    explicit MeterView (ParamHelp& h) : ph (h) {}
    void tick()
    {
        auto& P = ph.proc;
        if (P.isFrozen()) { P.inPeak = 0.0f; P.outPeak = 0.0f; P.grPeak = 0.0f; inDb = dbOf (P.pvInPk); outDb = dbOf (P.pvOutPk); grDb = P.pvGrPk; upDb = P.pvUpPk; repaint(); return; }
        inDb = std::max (dbOf (P.inPeak.exchange (0.0f)), inDb - 1.3f);
        outDb = std::max (dbOf (P.outPeak.exchange (0.0f)), outDb - 1.3f);
        grDb = std::max (P.grPeak.exchange (0.0f), grDb - 0.7f);
        upDb = std::max (P.upPeak.exchange (0.0f), upDb - 0.7f);
        repaint();
    }
    float grNow() const { return grDb; }
    void paint (juce::Graphics& g) override
    {
        paintWell (g, getLocalBounds().toFloat());
        const float top = 22, bot = (float) getHeight() - 34, h = bot - top;
        auto bar = [&] (int k, float frac, juce::Colour c, bool hang, const char* lab)
        {
            const float x = 8.0f + (float) k * 24.0f;
            g.setColour (theme::shade (0.55f)); g.fillRoundedRectangle (x, top, 16, h, 3.0f);
            frac = juce::jlimit (0.0f, 1.0f, frac);
            g.setColour (c);
            if (hang) g.fillRoundedRectangle (x, top, 16, h * frac, 3.0f); else g.fillRoundedRectangle (x, bot - h * frac, 16, h * frac, 3.0f);
            g.setColour (theme::onWell()); g.setFont (theme::mono (9.0f)); g.drawText (lab, (int) x - 4, (int) bot + 4, 24, 12, juce::Justification::centred);
        };
        bar (0, (inDb + 60.0f) / 60.0f, theme::faint, false, "IN");
        bar (1, grDb / 24.0f, theme::pre, true, "GR");
        if (upDb > 0.05f) { const float f = juce::jlimit (0.0f, 1.0f, upDb / 24.0f); g.setColour (theme::ok); g.fillRoundedRectangle (32.0f, bot - h * f, 16, h * f, 3.0f); }   // upward boost, from the bottom
        bar (2, (outDb + 60.0f) / 60.0f, theme::side, false, "OUT");
        g.setColour (theme::pre); g.setFont (theme::mono (9.5f, true));
        if (upDb > grDb + 0.05f) { g.setColour (theme::ok); g.drawText ("+" + juce::String (upDb, 1), 0, getHeight() - 17, getWidth(), 12, juce::Justification::centred); }
        else g.drawText (minus (-grDb), 0, getHeight() - 17, getWidth(), 12, juce::Justification::centred);
    }
private:
    ParamHelp& ph; float inDb = -120, outDb = -120, grDb = 0, upDb = 0;
};

// =============================================================================
//  Gain element picker (VCA / FET / Opto / Vari-Mu)
// =============================================================================
class ElementPicker : public juce::Component
{
public:
    explicit ElementPicker (ParamHelp& h) : ph (h) { markReset (*this); }
    void paint (juce::Graphics& g) override
    {
        static const char* nm[] { "VCA", "FET", "Opto", "Vari-Mu" };
        static const char* dj[] { "素直・基準", "速い・歪む", "2段・記憶", "真空管" };
        static const char* de[] { "clean", "fast, grit", "2-stage, memory", "tube" };
        const int sel = (int) ph.value (pid::element), rh = getHeight() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto r = juce::Rectangle<float> (0, (float) (i * rh), (float) getWidth(), (float) rh).reduced (0.0f, 1.5f);
            g.setColour (i == sel ? theme::accent : theme::hilite (0.08f)); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (i == sel ? theme::onAccent : theme::text); g.setFont (theme::mono (11.0f, true));
            g.drawText (nm[i], r.reduced (7, 0), juce::Justification::centredLeft);
            g.setColour (i == sel ? theme::onAccent.withAlpha (0.85f) : theme::muted); g.setFont (theme::ui (8.5f));
            g.drawText (juce::String::fromUTF8 (ph.ja() ? dj[i] : de[i]), r.reduced (7, 0), juce::Justification::centredRight);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        if (e.mods.isAltDown() && ph.resetDefault (pid::element)) { repaint(); return; }
        ph.proc.undo.beginNewTransaction();
        ph.setGesture (pid::element, (float) juce::jlimit (0, 3, e.y * 4 / std::max (1, getHeight()))); repaint();
    }
private:
    ParamHelp& ph;
};
