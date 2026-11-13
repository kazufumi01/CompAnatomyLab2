#pragma once
#include "Views.h"

// =============================================================================
//  LOUDNESS: BS.1770 loudness of the input and the output, and the two peak-to-loudness figures.
//  PSR = peak − short-term loudness (how much punch right now), PLR = peak − integrated loudness (of the whole passage).
//  Stopped: the last sound, re-measured with the current settings. Click = restart the integrated values.
// =============================================================================
class LoudView : public juce::Component
{
public:
    explicit LoudView (ParamHelp& h) : ph (h) {}
    void mouseDown (const juce::MouseEvent&) override { ph.proc.loudReset = true; }
    void paint (juce::Graphics& g) override
    {
        paintWell (g, getLocalBounds().toFloat());
        const bool frozen = ph.proc.isFrozen(); float v[2][5];
        for (size_t s = 0; s < 2; ++s) for (size_t k = 0; k < 5; ++k) v[s][k] = frozen ? ph.proc.pvLoud[s][k] : ph.proc.loud[s][k].load();
        const juce::Colour hi = theme::well.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff1e1e1e) : juce::Colour (0xffeaeaea);
        g.setColour (theme::onWell()); g.setFont (theme::mono (8.5f, true)); g.drawText ("LOUDNESS", 8, 5, 70, 12, juce::Justification::centredLeft);
        const int xl = 8, xi = 36, xo = 70, cw = 34;
        g.setFont (theme::mono (8.5f)); g.setColour (theme::onWell());
        g.drawText ("IN", xi, 22, cw, 11, juce::Justification::centredRight); g.setColour (theme::side); g.drawText ("OUT", xo, 22, cw, 11, juce::Justification::centredRight);
        auto num = [] (float x) { return x < -100.0f ? juce::String::fromUTF8 ("\xe2\x80\x94") : minus (x); };
        auto row = [&] (int i, const char* name, float a, float b, bool strong)
        {
            const int y = 37 + i * 19;
            g.setFont (theme::mono (9.0f, true)); g.setColour (strong ? theme::accent : theme::onWell()); g.drawText (name, xl, y, 30, 13, juce::Justification::centredLeft);
            g.setFont (theme::mono (9.5f)); g.setColour (theme::onWell()); g.drawText (num (a), xi, y, cw, 13, juce::Justification::centredRight);
            g.setFont (theme::mono (9.5f, true)); g.setColour (hi); g.drawText (num (b), xo, y, cw, 13, juce::Justification::centredRight);
        };
        auto diff = [] (float pk, float l) { return (pk < -100.0f || l < -100.0f) ? -120.0f : pk - l; };
        row (0, "M", v[0][0], v[1][0], false); row (1, "S", v[0][1], v[1][1], false); row (2, "I", v[0][2], v[1][2], false);
        g.setColour (theme::grid); g.drawHorizontalLine (37 + 3 * 19 - 2, 6.0f, (float) getWidth() - 6.0f);
        row (3, "TP", v[0][3], v[1][3], false);
        row (4, "PSR", diff (v[0][3], v[0][1]), diff (v[1][3], v[1][1]), true); row (5, "PLR", diff (v[0][4], v[0][2]), diff (v[1][4], v[1][2]), true);
        g.setColour (theme::faint); g.setFont (theme::ui (8.5f));
        g.drawText ("M / S / I : LUFS", xl, 153, getWidth() - 12, 11, juce::Justification::centredLeft);
        g.drawText ("TP : dBTP", xl, 164, getWidth() - 12, 11, juce::Justification::centredLeft);
        g.drawText (frozen ? ph.tr ("停止中：今の設定で再計算", "stopped: re-measured") : ph.tr ("クリックで I をリセット", "click: restart I"), xl, 176, getWidth() - 12, 11, juce::Justification::centredLeft);
    }
private:
    ParamHelp& ph;
};
