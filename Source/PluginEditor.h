#pragma once
#include "PluginProcessor.h"
#include "ScalingEditor.h"
#include "Widgets.h"
#include "Views.h"
#include "GRWave.h"
#include "Flow.h"
#include "Panel.h"
#include "Loud.h"
#include "Lower2.h"
#include "Learn.h"
#include "Dyn.h"
#include "Spectrum.h"

// =============================================================================
//  Layout (1160 × 760): header 44 (title · size · MODEL · PRESET · readouts) · TIME + TRANSFER + meters ·
//  SIGNAL FLOW (BUILD) or the hardware panel (PANEL) · bottom row: GR WAVE | buttons | info card
// =============================================================================
class CompView : public juce::Component,
                 private juce::Timer
{
public:
    ZoomButton zoom;
    explicit CompView (CompProcessor&);
    ~CompView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void refreshTexts();
    void updateHover();
    void applySkin (int i);
    void applySurface (int i);
    void showPresetMenu();
    void savePresetDialog();
    void showLockMenu();
    void stepPreset (int dir);
    juce::String keyPid;           // the parameter under the pointer (or the last one pointed at): ↑ / ↓ step it

    CompProcessor& proc;
    theme::LookAndFeel lnf;
    ParamHelp ph { proc };

    juce::TextButton listenBtn, langBtn, autoBtn, deltaBtn, bypassBtn, presetBtn, lockBtn, idealBtn, slotBtn[4], prevBtn, nextBtn;
    int idsFor = -2;
    juce::ComboBox modelBox;
    TimeView timeView { ph };
    TransferView transfer { ph };
    MeterView meters { ph };
    FlowPanel flow { ph };
    PanelView panelView { ph };
    PanelMini mini { ph };
    HarmView harm { ph };
    ScView scView { ph };
    int lastView = -1, prevLower = 0;
    GRWaveView grWave { ph };
    DynView dyn { ph };
    SpectrumView spectrum { ph };
    DragValue outV { ph, "OUT" };
    LearnCard info { ph };
    QuizCover cover { ph };
    LoudView loud { ph };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompView)
};

class CompEditor : public ScalingEditor<CompView, CompProcessor>
{
public:
    explicit CompEditor (CompProcessor& p) : ScalingEditor (p, 1160, 760, "companatomy2.zoom") {}
};
