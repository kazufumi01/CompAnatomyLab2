#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModCore.h"
#include "Models.h"

// =============================================================================
//  Parameter IDs and layout. New parameters are only ever appended.
// =============================================================================
namespace pid
{
static const char* input = "input";       static const char* inXfmr = "inxfmr";
static const char* scSource = "scsource"; static const char* scHpf = "schpf";     static const char* scTilt = "sctilt";
static const char* link = "link";         static const char* band = "band";       static const char* bandHz = "bandhz";
static const char* bandQ = "bandq";       static const char* bandOnly = "bandonly";
static const char* detector = "detector"; static const char* rmsWin = "rmswin";   static const char* topology = "topology";
static const char* law = "law";           static const char* threshold = "threshold"; static const char* ratio = "ratio"; static const char* knee = "knee";
static const char* timing = "timing";     static const char* attack = "attack";   static const char* release = "release";
static const char* slowMult = "slowmult"; static const char* slowAmt = "slowamt"; static const char* hold = "hold";
static const char* lookahead = "lookahead"; static const char* avg = "avg";
static const char* element = "element";   static const char* drive = "drive";     static const char* os = "os";
static const char* outStage = "outstage"; static const char* makeup = "makeup";   static const char* mix = "mix";
static const char* scListen = "sclisten"; static const char* output = "output";   static const char* autoGain = "autogain";
static const char* delta = "delta";       static const char* bypass = "bypass";
static const char* upOn = "upon";         static const char* upThr = "upthr";     static const char* upRatio = "upratio"; static const char* upRange = "uprange";
static const char* mb = "mb";             static const char* xLo = "xlo";         static const char* xHi = "xhi";
static const char* mbLo = "mblo";         static const char* mbMid = "mbmid";     static const char* mbHi = "mbhi";
static const char* relCurve = "relcurve"; static const char* crest = "crest";     static const char* grLimit = "grlimit";
static const char* stereo = "stereo";     static const char* scMb = "scmb";       static const char* scLo = "sclo"; static const char* scMid = "scmid"; static const char* scHi = "schi";
static const char* pre = "prestage";   static const char* preThr = "prethr";
}

inline const juce::StringArray& hpfNames() { static const juce::StringArray n { "Off", "30 Hz", "60 Hz", "100 Hz", "150 Hz", "250 Hz", "400 Hz" }; return n; }
inline double hpfHz (int idx) { static const double f[] { 0, 30, 60, 100, 150, 250, 400 }; return f[juce::jlimit (0, 6, idx)]; }
inline const juce::StringArray& detNames()     { static const juce::StringArray n { "Peak", "RMS", "Diode + RC", "Peak + RMS" }; return n; }
inline const juce::StringArray& lawNames()     { static const juce::StringArray n { "Ideal", "FET", "Opto", "Vari-Mu", "Diode", "Limiter", "Rider", "Spectral", "Clipper" }; return n; }
inline const juce::StringArray& timingNames()  { static const juce::StringArray n { "1-stage", "2-stage (program)", "Opto memory", "Auto", "Precise", "Dual average" }; return n; }
inline const juce::StringArray& elementNames() { static const juce::StringArray n { "Clean VCA", "FET", "Tube push-pull", "Opto cell", "Diode bridge" }; return n; }
inline const juce::StringArray& outNames()     { static const juce::StringArray n { "Clean", "Transformer", "Tube", "2nd harmonic", "3rd harmonic", "Nickel", "Iron", "Steel" }; return n; }
inline const juce::StringArray& tiltNames()    { static const juce::StringArray n { "Off", "Medium", "Loud" }; return n; }
inline const juce::StringArray& bandNames()    { static const juce::StringArray n { "Off", "Low-pass", "High-pass", "Band-pass" }; return n; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto fl = [&l] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (unit))); };
    auto skew = [] (float lo, float hi, float step, float centre) { NormalisableRange<float> r (lo, hi, step); r.setSkewForCentre (centre); return r; };
    auto ch = [&l] (const char* id, const char* name, StringArray items, int def) { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto bl = [&l] (const char* id, const char* name, bool def) { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    fl (pid::input, "Input", { -24.0f, 24.0f, 0.1f }, 0.0f, "dB");                 bl (pid::inXfmr, "Input transformer", false);
    ch (pid::scSource, "Sidechain source", { "Internal", "External" }, 0);         ch (pid::scHpf, "Sidechain HPF", hpfNames(), 0);
    ch (pid::scTilt, "Sidechain tilt", tiltNames(), 0);                            fl (pid::link, "Stereo link", { 0.0f, 100.0f, 1.0f }, 100.0f, "%");
    ch (pid::band, "Detector band", bandNames(), 0);                               fl (pid::bandHz, "Band frequency", skew (200.0f, 16000.0f, 1.0f, 3000.0f), 6000.0f, "Hz");
    fl (pid::bandQ, "Band Q", skew (0.3f, 8.0f, 0.01f, 1.5f), 1.0f, "");           bl (pid::bandOnly, "Band only", false);
    ch (pid::detector, "Detector", detNames(), 0);                                 fl (pid::rmsWin, "RMS window", skew (1.0f, 300.0f, 0.1f, 17.0f), 10.0f, "ms");
    ch (pid::topology, "Topology", { "Feed-forward", "Feedback" }, 0);
    ch (pid::law, "Gain law", lawNames(), 0);                                      fl (pid::threshold, "Threshold", { -60.0f, 0.0f, 0.1f }, -18.0f, "dB");
    fl (pid::ratio, "Ratio", skew (1.0f, 100.0f, 0.1f, 6.0f), 4.0f, ":1");         fl (pid::knee, "Knee", { 0.0f, 24.0f, 0.1f }, 6.0f, "dB");
    ch (pid::timing, "Timing", timingNames(), 0);                                  fl (pid::attack, "Attack", skew (0.02f, 300.0f, 0.01f, 3.0f), 10.0f, "ms");
    fl (pid::release, "Release", skew (5.0f, 5000.0f, 0.1f, 200.0f), 120.0f, "ms"); fl (pid::slowMult, "Slow release x", skew (1.0f, 20.0f, 0.1f, 6.0f), 8.0f, "x");
    fl (pid::slowAmt, "Slow amount", { 0.0f, 100.0f, 1.0f }, 50.0f, "%");          fl (pid::hold, "Hold", skew (0.0f, 50.0f, 0.1f, 8.0f), 0.0f, "ms");
    fl (pid::lookahead, "Lookahead", { -10.0f, 10.0f, 0.1f }, 0.0f, "ms");           fl (pid::avg, "Average time", skew (10.0f, 1000.0f, 1.0f, 120.0f), 100.0f, "ms");
    ch (pid::element, "Gain element", elementNames(), 0);                          fl (pid::drive, "Element drive", { -12.0f, 24.0f, 0.1f }, 0.0f, "dB");
    ch (pid::os, "Oversampling", { "1x", "2x", "4x" }, 0);
    ch (pid::outStage, "Output stage", outNames(), 0);                             fl (pid::makeup, "Makeup", { -12.0f, 24.0f, 0.1f }, 0.0f, "dB");
    fl (pid::mix, "Mix", { 0.0f, 100.0f, 1.0f }, 100.0f, "%");
    bl (pid::scListen, "Sidechain listen", false);                                 fl (pid::output, "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, "dB");
    bl (pid::autoGain, "Auto gain", false); bl (pid::delta, "Delta", false);       bl (pid::bypass, "Bypass", false);
    bl (pid::upOn, "Upward", false);                                               fl (pid::upThr, "Upward threshold", { -70.0f, 0.0f, 0.1f }, -40.0f, "dB");
    fl (pid::upRatio, "Upward ratio", skew (1.0f, 10.0f, 0.1f, 2.5f), 2.0f, ":1");  fl (pid::upRange, "Upward range", { 0.0f, 36.0f, 0.1f }, 18.0f, "dB");
    bl (pid::mb, "3-band", false);                                                 fl (pid::xLo, "Crossover low", skew (40.0f, 500.0f, 1.0f, 120.0f), 120.0f, "Hz");
    fl (pid::xHi, "Crossover high", skew (1000.0f, 10000.0f, 1.0f, 3000.0f), 2500.0f, "Hz");
    fl (pid::mbLo, "Low band gain", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB"); fl (pid::mbMid, "Mid band gain", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB"); fl (pid::mbHi, "High band gain", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");
    fl (pid::relCurve, "Release curve", { 0.0f, 100.0f, 1.0f }, 0.0f, "%");         fl (pid::crest, "Peak crest", { 0.0f, 12.0f, 0.1f }, 4.0f, "dB");
    fl (pid::grLimit, "GR limit", { 1.0f, 40.0f, 0.1f }, 40.0f, "dB");
    ch (pid::stereo, "Stereo mode", { "L / R", "Mid / Side", "Mid only", "Side only" }, 0);   bl (pid::scMb, "Multiband detection", false);
    fl (pid::scLo, "Detector low sensitivity", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB"); fl (pid::scMid, "Detector mid sensitivity", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");
    fl (pid::scHi, "Detector high sensitivity", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");
    bl (pid::pre, "Pre-stage", false);                                             fl (pid::preThr, "Pre-stage threshold", { -40.0f, 0.0f, 0.1f }, -20.0f, "dB");
    return l;
}

// model → parameter values (for applying a model) : id, plain value
inline std::vector<std::pair<const char*, float>> modelValues (const mod::Params& p)
{
    int hp = 0; for (int i = 0; i < 7; ++i) if (std::abs (hpfHz (i) - p.scHpfHz) < std::abs (hpfHz (hp) - p.scHpfHz)) hp = i;
    return { { pid::input, (float) p.input }, { pid::inXfmr, p.inXfmr ? 1.0f : 0.0f }, { pid::scHpf, (float) hp }, { pid::scTilt, (float) p.scTilt },
             { pid::link, (float) (p.link * 100.0) }, { pid::band, (float) p.band }, { pid::bandHz, (float) p.bandHz }, { pid::bandQ, (float) p.bandQ }, { pid::bandOnly, p.bandOnly ? 1.0f : 0.0f },
             { pid::detector, (float) p.det }, { pid::rmsWin, (float) p.rmsMs }, { pid::topology, p.feedback ? 1.0f : 0.0f },
             { pid::law, (float) p.law }, { pid::threshold, (float) p.threshold }, { pid::ratio, (float) p.ratio }, { pid::knee, (float) p.knee },
             { pid::timing, (float) p.timing }, { pid::attack, (float) p.attack }, { pid::release, (float) p.release }, { pid::slowMult, (float) p.slowMult },
             { pid::slowAmt, (float) (p.slowAmt * 100.0) }, { pid::hold, (float) p.holdMs }, { pid::lookahead, (float) p.lookaheadMs }, { pid::avg, (float) p.avgMs },
             { pid::element, (float) p.element }, { pid::drive, (float) p.driveDb }, { pid::os, p.os >= 4 ? 2.0f : p.os >= 2 ? 1.0f : 0.0f },
             { pid::outStage, (float) p.out }, { pid::makeup, (float) p.makeup }, { pid::mix, (float) (p.mix * 100.0) },
             { pid::upOn, p.upOn ? 1.0f : 0.0f }, { pid::upThr, (float) p.upThreshold }, { pid::upRatio, (float) p.upRatio }, { pid::upRange, (float) p.upRange },
             { pid::mb, p.mb ? 1.0f : 0.0f }, { pid::xLo, (float) p.xLo }, { pid::xHi, (float) p.xHi },
             { pid::mbLo, (float) p.bandGain[0] }, { pid::mbMid, (float) p.bandGain[1] }, { pid::mbHi, (float) p.bandGain[2] },
             { pid::relCurve, (float) (p.relCurve * 100.0) }, { pid::crest, (float) p.crestDb }, { pid::grLimit, (float) p.grLimit },
             { pid::stereo, (float) p.stereo }, { pid::scMb, p.scMb ? 1.0f : 0.0f }, { pid::scLo, (float) p.scSens[0] }, { pid::scMid, (float) p.scSens[1] }, { pid::scHi, (float) p.scSens[2] },
             { pid::pre, p.pre ? 1.0f : 0.0f }, { pid::preThr, (float) p.preThr } };
}
