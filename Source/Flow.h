#pragma once
#include "Views.h"
#include "GRWave.h"

inline juce::String sgnDb (double v) { return (v > 0.04 ? "+" : v < -0.04 ? juce::String::fromUTF8 ("\xe2\x88\x92") : juce::String::fromUTF8 ("\xc2\xb1")) + juce::String (std::abs (v), 1) + " dB"; }

// A menu bound to a choice parameter (the "method" of a block)
struct ChoiceBox : public ResetCombo
{
    ChoiceBox (ParamHelp& h, const char* id, const juce::StringArray& items) : ph (h), paramID (id)
    {
        addItemList (items, 1); markReset (*this);
        onAlt = [this] { ph.resetDefault (paramID); };
        onChange = [this] { if ((int) ph.value (paramID) != getSelectedId() - 1) { ph.proc.undo.beginNewTransaction(); ph.setGesture (paramID, (float) (getSelectedId() - 1)); } };
    }
    void sync() { const int v = (int) ph.value (paramID); if (getSelectedId() != v + 1) setSelectedId (v + 1, juce::dontSendNotification); }
    ParamHelp& ph; juce::String paramID;
};

// =============================================================================
//  SIGNAL FLOW (BUILD): ① input → ② sidechain → ③ detector → ④ gain law → ⑤ timing → ⑥ element → ⑦ output.
//  Each block has a method menu; values the chosen method fixes are greyed with the reason.
// =============================================================================
class FlowPanel : public juce::Component
{
public:
    explicit FlowPanel (ParamHelp& h)
        : ph (h), inputV (h, "INPUT"), hpfV (h, "SC HPF"), tiltV (h, "TILT"), linkV (h, "LINK"), bandV (h, "BAND"), bandHzV (h, "BAND Hz"), bandQV (h, "BAND Q"),
          rmsV (h, "RMS WIN"), thrV (h, "THRESH"), ratioV (h, "RATIO"), kneeV (h, "KNEE"),
          attV (h, "ATTACK"), relV (h, "RELEASE"), lookV (h, "LOOK"), holdV (h, "HOLD"), slowV (h, "SLOW x"), amtV (h, "AMOUNT"),
          driveV (h, "DRIVE"), makeV (h, "MAKEUP"), mixV (h, "DRY / WET"),
          upThrV (h, "UP THR"), upRatioV (h, "UP RATIO"), xLoV (h, "XOVER LO"), xHiV (h, "XOVER HI"), gLoV (h, "LO"), gMidV (h, "MID"), gHiV (h, "HI"),
          relCurveV (h, "REL CURVE"), crestV (h, "CREST"), grLimV (h, "GR LIMIT"), upRangeV (h, "UP RANGE"),
          stereoV (h, "STEREO"), sLoV (h, "LO"), sMidV (h, "MID"), sHiV (h, "HI"), preThrV (h, "PRE THR"),
          xfSeg (h, { "Off", "On" }), srcSeg (h, { "Internal", "External" }), onlySeg (h, { "Full", "Band only" }), topoSeg (h, { "Feed-fwd", "Feedback" }), osSeg (h, { "1x", "2x", "4x" }), upSeg (h, { "Off", "On" }), mbSeg (h, { "Single", "3-band" }), scMbSeg (h, { "1 band", "3 bands" }), preSeg (h, { "Off", "Opto 2:1" }),
          detBox (h, pid::detector, detNames()), lawBox (h, pid::law, lawNames()), timBox (h, pid::timing, timingNames()),
          elBox (h, pid::element, elementNames()), outBox (h, pid::outStage, outNames())
    {
        auto dv = [this] (DragValue& d, const char* id, std::function<juce::String (float)> f)
        { d.compact = true; d.setParam (id); d.format = std::move (f); addAndMakeVisible (d); };
        auto names = [] (const juce::StringArray& n) { return [n] (float v) { return n[juce::jlimit (0, n.size() - 1, (int) std::lround (v))]; }; };
        dv (inputV, pid::input, [] (float v) { return sgnDb (v); });
        dv (hpfV, pid::scHpf, names (hpfNames()));   dv (tiltV, pid::scTilt, names ({ "Off", "Med", "Loud" }));
        dv (linkV, pid::link, [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; });
        dv (bandV, pid::band, names ({ "Off", "LP", "HP", "BP" }));
        dv (bandHzV, pid::bandHz, [] (float v) { return fmtHz (v); });  dv (bandQV, pid::bandQ, [] (float v) { return juce::String (v, 2); });
        dv (rmsV, pid::rmsWin, [] (float v) { return fmtTime (v); });
        dv (thrV, pid::threshold, [] (float v) { return minus (v) + " dB"; });
        dv (ratioV, pid::ratio, [] (float v) { return (v < 10 ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " : 1"; });
        dv (kneeV, pid::knee, [] (float v) { return juce::String (v, 1) + " dB"; });
        dv (attV, pid::attack, [] (float v) { return fmtTime (v); });   dv (relV, pid::release, [] (float v) { return fmtTime (v); });
        dv (lookV, pid::lookahead, [] (float v) { return minus (v) + " ms"; });
        dv (holdV, pid::hold, [] (float v) { return juce::String (v, 1) + " ms"; });
        dv (slowV, pid::slowMult, [] (float v) { return juce::String (v, 1) + " x"; });
        dv (amtV, pid::slowAmt, [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; });
        dv (driveV, pid::drive, [] (float v) { return sgnDb (v); });
        dv (makeV, pid::makeup, [] (float v) { return sgnDb (v); });
        dv (mixV, pid::mix, [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; });
        dv (stereoV, pid::stereo, names ({ "L / R", "Mid / Side", "Mid only", "Side only" }));
        for (auto* sv : { &sLoV, &sMidV, &sHiV }) { sv->format = [] (float v) { return fmtDb (v); }; sv->compact = true; addAndMakeVisible (*sv); }
        sLoV.setParam (pid::scLo); sMidV.setParam (pid::scMid); sHiV.setParam (pid::scHi);
        dv (upThrV, pid::upThr, [] (float v) { return minus (v) + " dB"; });
        dv (upRatioV, pid::upRatio, [] (float v) { return "1 : " + juce::String (v, 1); });
        dv (relCurveV, pid::relCurve, [] (float v) { return v < 0.5f ? juce::String ("Exp (log)") : v > 99.5f ? juce::String ("Linear") : juce::String (juce::roundToInt (v)) + " % lin"; });
        dv (crestV, pid::crest, [] (float v) { return juce::String (v, 1) + " dB"; });
        dv (grLimV, pid::grLimit, [] (float v) { return v >= 39.95f ? juce::String ("Off") : juce::String (v, 1) + " dB"; });
        dv (upRangeV, pid::upRange, [] (float v) { return juce::String (v, 1) + " dB"; });
        dv (preThrV, pid::preThr, [] (float v) { return minus (v) + " dB"; });
        dv (xLoV, pid::xLo, [] (float v) { return fmtHz (v); });   dv (xHiV, pid::xHi, [] (float v) { return fmtHz (v); });
        for (auto* gv : { &gLoV, &gMidV, &gHiV }) gv->format = [] (float v) { return fmtDb (v); };
        gLoV.compact = gMidV.compact = gHiV.compact = true; gLoV.setParam (pid::mbLo); gMidV.setParam (pid::mbMid); gHiV.setParam (pid::mbHi);
        for (auto* gv : { &gLoV, &gMidV, &gHiV }) addAndMakeVisible (*gv);
        xfSeg.setParam (pid::inXfmr); srcSeg.setParam (pid::scSource); onlySeg.setParam (pid::bandOnly); topoSeg.setParam (pid::topology); osSeg.setParam (pid::os); upSeg.setParam (pid::upOn); mbSeg.setParam (pid::mb); scMbSeg.setParam (pid::scMb); preSeg.setParam (pid::pre);
        for (auto* c : { (juce::Component*) &xfSeg, (juce::Component*) &srcSeg, (juce::Component*) &onlySeg, (juce::Component*) &topoSeg, (juce::Component*) &osSeg, (juce::Component*) &upSeg, (juce::Component*) &mbSeg, (juce::Component*) &scMbSeg, (juce::Component*) &preSeg,
                         (juce::Component*) &detBox, (juce::Component*) &lawBox, (juce::Component*) &timBox, (juce::Component*) &elBox, (juce::Component*) &outBox })
            addAndMakeVisible (*c);

        // values a method fixes: greyed, with the reason
        auto T = [this] (const char* j, const char* e) { return ph.tr (j, e); };
        rmsV.lockNote = [this, T] { const int d = (int) ph.value (pid::detector); return (d == mod::DetRMS || d == mod::DetCrest) ? juce::String() : T ("RMS のときだけ", "RMS only"); };
        kneeV.lockNote = [this, T] { const int l = (int) ph.value (pid::law);
                                     return (l == mod::LawIdeal || l == mod::LawSpectral || l == mod::LawClip) ? juce::String() : l == mod::LawFET ? T ("比が決める", "set by the ratio") : l == mod::LawLimit ? T ("硬い（0 dB）", "hard (0 dB)") : T ("素子が決める", "set by the element"); };
        ratioV.lockNote = [this] { return (int) ph.value (pid::law) == mod::LawLimit ? juce::String::fromUTF8 ("\xe2\x88\x9e : 1") : juce::String(); };
        upThrV.lockNote = [this, T] { return ph.value (pid::upOn) > 0.5f ? juce::String() : T ("Upward Off", "upward off"); };
        upRatioV.lockNote = upThrV.lockNote; upRangeV.lockNote = upThrV.lockNote;
        crestV.lockNote = [this, T] { return (int) ph.value (pid::detector) == mod::DetCrest ? juce::String() : T ("Peak + RMS のみ", "Peak + RMS only"); };
        relCurveV.lockNote = [this, T] { const int t = (int) ph.value (pid::timing);
                                         return (int) ph.value (pid::detector) == mod::DetDiode ? T ("電圧領域では固定", "fixed in the voltage domain")
                                              : (t == mod::TimOptoMem || t == mod::TimDualAvg) ? T ("この方式では固定", "fixed for this method") : juce::String(); };
        xLoV.lockNote = [this, T] { return (ph.value (pid::mb) > 0.5f || ph.value (pid::scMb) > 0.5f) ? juce::String() : T ("3-band のみ", "3-band only"); };
        xHiV.lockNote = xLoV.lockNote;
        for (auto* gv : { &gLoV, &gMidV, &gHiV }) gv->lockNote = [this] { return ph.value (pid::mb) > 0.5f ? juce::String() : juce::String::fromUTF8 ("\xe2\x80\x94"); };
        holdV.lockNote = [this, T] { return (int) ph.value (pid::timing) == mod::TimPrecise ? juce::String() : T ("Precise のみ", "Precise only"); };
        slowV.lockNote = [this, T] { const int t = (int) ph.value (pid::timing); return (t == mod::TimProg2 || t == mod::TimDualAvg || (int) ph.value (pid::detector) == mod::DetCrest) ? juce::String() : T ("2段 / Dual", "2-stage / Dual"); };
        amtV.lockNote = [this, T] { const int t = (int) ph.value (pid::timing); return (t == mod::TimProg2 || t == mod::TimDualAvg) ? juce::String() : T ("2段 / Dual", "2-stage / Dual"); };
        driveV.lockNote = [this, T] { const int e = (int) ph.value (pid::element); return (e == mod::ElClean || e == mod::ElOpto) ? T ("この素子は歪まない", "this element is clean") : juce::String(); };
        bandHzV.lockNote = [this, T] { return (int) ph.value (pid::band) == mod::BandOff ? T ("Band Off", "band off") : juce::String(); };
        bandQV.lockNote = bandHzV.lockNote;
        for (auto* d : { &hpfV, &tiltV, &linkV, &bandV, &bandHzV, &bandQV, &rmsV, &crestV, &lookV, &holdV, &slowV, &amtV, &relCurveV, &driveV, &upThrV, &upRatioV, &upRangeV, &xLoV, &xHiV })
        {                                                               // spectral compression uses only threshold, ratio, knee, attack, release, GR limit, makeup, mix
            auto old = d->lockNote;
            d->lockNote = [this, old, T] { if ((int) ph.value (pid::law) == mod::LawSpectral) return T ("Spectral では無効", "n/a in Spectral"); return old ? old() : juce::String(); };
        }
        for (auto* d : { &hpfV, &tiltV, &bandV, &bandHzV, &bandQV, &rmsV, &crestV, &attV, &relV, &lookV, &holdV, &slowV, &amtV, &relCurveV })
        {                                                               // a clipper has no detector filters and no time constants
            auto old = d->lockNote;
            d->lockNote = [this, old, T] { if ((int) ph.value (pid::law) == mod::LawClip) return T ("Clipper では無効", "n/a in Clipper"); return old ? old() : juce::String(); };
        }
        preThrV.lockNote = [this, T] { return ph.value (pid::mb) > 0.5f && (int) ph.value (pid::law) != mod::LawSpectral ? T ("3-band では使えない", "not in 3-band") : ph.value (pid::pre) > 0.5f ? juce::String() : T ("Pre-stage Off", "pre-stage off"); };

        ctlIds = { { &inputV, pid::input }, { &hpfV, pid::scHpf }, { &tiltV, pid::scTilt }, { &linkV, pid::link }, { &bandV, pid::band }, { &bandHzV, pid::bandHz }, { &bandQV, pid::bandQ },
                   { &rmsV, pid::rmsWin }, { &thrV, pid::threshold }, { &ratioV, pid::ratio }, { &kneeV, pid::knee }, { &attV, pid::attack }, { &relV, pid::release }, { &lookV, pid::lookahead },
                   { &holdV, pid::hold }, { &slowV, pid::slowMult }, { &amtV, pid::slowAmt }, { &driveV, pid::drive }, { &makeV, pid::makeup }, { &mixV, pid::mix }, { &stereoV, pid::stereo },
                   { &upThrV, pid::upThr }, { &upRatioV, pid::upRatio }, { &relCurveV, pid::relCurve }, { &crestV, pid::crest }, { &grLimV, pid::grLimit }, { &upRangeV, pid::upRange },
                   { &xLoV, pid::xLo }, { &xHiV, pid::xHi }, { &gLoV, pid::mbLo }, { &gMidV, pid::mbMid }, { &gHiV, pid::mbHi }, { &preThrV, pid::preThr },
                   { &topoSeg, pid::topology }, { &osSeg, pid::os }, { &timBox, pid::timing }, { &outBox, pid::outStage } };
        blocks = {
            { "\xe2\x91\xa0", "INPUT", "入力", "input", "INPUT BLOCK", "入力ゲインと入力トランス。ゲインは検出器と素子の両方の前にあり、Threshold との相対関係を変える。Threshold ノブのない機種（FET 76・Opto 2A）は、ここで信号を押し込んで圧縮量を決める。トランスは低域だけを飽和させる",
              "Input gain and input transformer. The gain sits before both the detector and the element, so it moves the signal against Threshold. Units without a Threshold knob (FET 76, Opto 2A) set the amount of compression here. The transformer saturates only the lows" },
            { "\xe2\x91\xa1", "SIDECHAIN", "何を見るか", "what it listens to", "SIDECHAIN BLOCK", "検出器に送る音。外部入力、ハイパス、Tilt（高域に敏感・低域に鈍感にする。API の Thrust 相当）、帯域フィルター、ステレオリンク。Band only にすると、その帯域だけを絞る（ディエッサー型）",
              "What the detector hears: external input, high-pass, tilt (more sensitive to highs, less to lows — like API's Thrust), a band filter and stereo link. Band only turns down just that band (de-esser style)" },
            { "\xe2\x91\xa2", "DETECTOR", "どう測るか", "how it measures", "DETECTOR BLOCK", "| 方式 | 動作 |\n| Peak | 瞬間の山を測る。タイミングは dB の領域で動く |\n| RMS | 窓の平均。窓の長さ分だけ反応が遅れる。長い窓ではリリースが dB で直線になる（VCA 160） |\n| Diode + RC | 整流した電圧を平滑する。同じ Release でも dB では速く落ちる |\n| Peak + RMS | ピークと平均を別々に測り、大きい方に従う。Crest で配分 |\n\nFeed-forward は入力を、Feedback は素子の出力を測る",
              "| Method | Behaviour |\n| Peak | follows instants; timing works in dB |\n| RMS | window average; lags by the window. A long window gives a release that is linear in dB (VCA 160) |\n| Diode + RC | smooths the rectified voltage: the same Release falls faster in dB |\n| Peak + RMS | measures peak and average separately and follows the larger; Crest sets the balance |\n\nFeed-forward measures the input, feedback the element's output" },
            { "\xe2\x91\xa3", "GAIN LAW", "どれだけ絞るか", "how much", "GAIN LAW BLOCK", "| 方式 | カーブ |\n| Ideal | Threshold・Ratio・Knee を自由に決める |\n| FET | 比を上げると Threshold が上がり、ニーが硬くなる |\n| Opto | 浅いうちは緩く、深くなるほど比が上がる |\n| Vari-Mu | レベルが上がるほど比が上がる。ニーが広い |\n| Diode | 固定の広いニー |\n| Limiter | Threshold を絶対に超えない（∞:1） |\n| Spectral | 周波数ごとに別々のコンプ（2048 点 FFT）。使うのは Threshold・Ratio・Knee・Attack・Release・GR Limit・Makeup・Mix。遅延 2048 サンプル |\n| Clipper | Attack も Release も無いコンプ＝1 サンプルごとにゲインが決まる。Threshold＝天井、Knee＝角の丸め、Ratio を下げるとサチュレーター |\n\nTRANSFER に、同じ T / R の理想カーブを灰色で重ねて表示",
              "| Method | Curve |\n| Ideal | free Threshold, Ratio, Knee |\n| FET | higher ratio raises the threshold and hardens the knee |\n| Opto | gentle at first, ratio grows with depth |\n| Vari-Mu | ratio rises with level, wide knee |\n| Diode | fixed wide knee |\n| Limiter | nothing passes the threshold (∞:1) |\n| Spectral | a compressor per frequency (2048-point FFT). Uses Threshold, Ratio, Knee, Attack, Release, GR Limit, Makeup, Mix. 2048 samples of latency |\n| Clipper | a compressor with no attack and no release: the gain is set for every sample. Threshold = ceiling, Knee = how round the corner is, lower Ratio = saturator |\n\nTRANSFER overlays the ideal curve for the same T / R in grey" },
            { "\xe2\x91\xa4", "TIMING", "どれだけ速く", "how fast", "TIMING BLOCK", "| 方式 | 動作 |\n| 1-stage | Attack と Release の1次平滑 |\n| 2-stage | 速い成分＋遅い成分。圧縮が続くほど遅い成分がたまり、戻りが遅くなる |\n| Opto memory | 光学セル。長く光るほど戻りが遅い |\n| Auto | 短い音は速く、長い音は遅く戻る |\n| Precise | ピークを保持し、先読みの間に直線で絞る。オーバーシュートなし |\n| Dual average | 持続音は遅い Release、瞬間の音は速い Release |",
              "| Method | Behaviour |\n| 1-stage | one-pole attack and release |\n| 2-stage | fast + slow part; sustained reduction charges the slow part |\n| Opto memory | photocell: the longer it was lit, the slower it recovers |\n| Auto | short sounds recover fast, long ones slowly |\n| Precise | holds the peak and ramps linearly over the look-ahead: no overshoot |\n| Dual average | sustained sound → slow release, transients → fast |" },
            { "\xe2\x91\xa5", "ELEMENT", "何で絞るか", "what does it", "ELEMENT BLOCK", "| 素子 | 歪み |\n| Clean VCA | なし |\n| FET | 2次が中心。GR が深いほど増える |\n| Tube push-pull | 偶数次が打ち消され、3次が残る |\n| Opto cell | ほぼなし |\n| Diode bridge | 奇数次。GR が深いほど増える |\n\nDrive で素子に入るレベルを変える。Oversample は歪みの折り返しを減らす（2x は 31、4x は 35 サンプル遅れる）",
              "| Element | Distortion |\n| Clean VCA | none |\n| FET | mostly 2nd, grows with GR |\n| Tube push-pull | even harmonics cancel, 3rd remains |\n| Opto cell | almost none |\n| Diode bridge | odd, grows with GR |\n\nDrive sets the level into the element. Oversampling reduces aliasing (31 samples at 2x, 35 at 4x)" },
            { "\xe2\x91\xa6", "OUTPUT", "出力", "output", "OUTPUT BLOCK", "出力段の色付け（Transformer は低域だけ飽和、Tube と 2nd は偶数次、3rd は奇数次）、Makeup、原音との混ぜ具合（パラレル圧縮）",
              "Output stage colour (transformer saturates the lows, tube and 2nd add even harmonics, 3rd odd), makeup, and the blend with the dry signal (parallel compression)" } };

        // gain riding re-uses three controls under other names (see refresh): they must stay live there
        upThrV.lockNote = [this, T] { return (ph.value (pid::upOn) > 0.5f || (int) ph.value (pid::law) == mod::LawRide) ? juce::String() : T ("Upward Off", "upward off"); };
        upRangeV.lockNote = upThrV.lockNote;
        upRatioV.lockNote = [this, T] { return (int) ph.value (pid::law) == mod::LawRide ? juce::String::fromUTF8 ("\xe2\x80\x94") : ph.value (pid::upOn) > 0.5f ? juce::String() : T ("Upward Off", "upward off"); };
        ratioV.lockNote = [this] { const int l = (int) ph.value (pid::law); return l == mod::LawLimit ? juce::String::fromUTF8 ("\xe2\x88\x9e : 1") : l == mod::LawRide ? juce::String::fromUTF8 ("\xe2\x80\x94") : juce::String(); };
        for (auto* sv : { &sLoV, &sMidV, &sHiV }) sv->lockNote = [this] { return ph.value (pid::scMb) > 0.5f ? juce::String() : juce::String::fromUTF8 ("\xe2\x80\x94"); };
        setHelp (stereoV, "STEREO MODE", "| モード | 動作 |\n| L / R | 左右を処理（LINK で連動の度合い） |\n| Mid / Side | 中央成分と広がり成分に分けて処理（LINK 0% で別々に動く） |\n| Mid only | 中央だけを圧縮。広がりは変えない → 広く聞こえる |\n| Side only | 広がりだけを圧縮。中央のボーカルやキックは変えない |",
                 "| Mode | Behaviour |\n| L / R | left and right (LINK sets how much they move together) |\n| Mid / Side | centre and width processed separately (LINK 0% = independent) |\n| Mid only | compresses the centre only: the mix sounds wider |\n| Side only | compresses the width only: centre vocal and kick are untouched |");
        setHelp (scMbSeg, "DETECT BANDS", "マルチバンド検出。検出器だけを低・中・高の 3 帯域に分け、帯域ごとに感度を変える。いちばん強く反応した帯域が全体のゲインを決める。音そのものは分割しないので、位相も音色も変わらない（3-band とは別物）。例：低域の感度を下げると、キックで全体が沈まなくなる。分割周波数は ① の XOVER",
                 "Multiband detection: only the detector is split into low / mid / high, each with its own sensitivity; the band that reacts most sets the one gain for everything. The audio is not split, so phase and tone stay intact (unlike 3-band). E.g. lower the low sensitivity so the kick stops ducking the mix. Split frequencies are the XOVER values in block 1");
        setHelp (sLoV, "DETECT LO", "検出器の低域の感度。マイナスで低域に反応しにくくなる", "Detector sensitivity for the low band; negative = reacts less to lows");
        setHelp (sMidV, "DETECT MID", "検出器の中域の感度", "Detector sensitivity for the mid band");
        setHelp (sHiV, "DETECT HI", "検出器の高域の感度。プラスで高域（歯擦音・シンバル）に反応しやすくなる", "Detector sensitivity for the high band; positive = reacts more to highs (sibilance, cymbals)");
        setHelp (inputV, "INPUT", "入力ゲイン。上げるほど同じ Threshold に対して深く圧縮される", "Input gain. Raising it compresses deeper at the same Threshold");
        setHelp (xfSeg, "IN TRANSFORMER", "入力トランス。低域だけが飽和し、太さが出る", "Input transformer: only the lows saturate");
        setHelp (srcSeg, "SOURCE", "検出器に送る音。External は DAW のサイドチェイン入力（未接続のときは Internal のまま）", "What feeds the detector. External is the DAW sidechain input (falls back to Internal when nothing is routed)");
        setHelp (hpfV, "SC HPF", "検出器だけにかかるハイパス。音は変わらず、低域に反応しにくくなる", "High-pass on the detector only. The audio is untouched; the compressor reacts less to lows");
        setHelp (tiltV, "TILT", "検出器の周波数特性を傾ける。Med は ±4 dB、Loud は ±10 dB（1 kHz を中心に低域を下げ、高域を上げる）。低域で潰れにくく、高域で反応しやすくなる", "Tilts what the detector hears: Med ±4 dB, Loud ±10 dB around 1 kHz. Less pumping from lows, more reaction to highs");
        setHelp (linkV, "STEREO LINK", "100% は左右を同じ量だけ絞る（定位が動かない）。0% は左右別々（デュアルモノ）", "100% reduces both channels equally (the image stays put). 0% is dual mono");
        setHelp (bandV, "BAND", "検出器の帯域フィルター（LP / HP / BP）。特定の帯域にだけ反応させる", "Band filter for the detector (LP / HP / BP): react to one band only");
        setHelp (bandHzV, "BAND Hz", "帯域フィルターの周波数", "Band filter frequency");
        setHelp (bandQV, "BAND Q", "帯域フィルターの鋭さ", "Band filter Q");
        setHelp (onlySeg, "APPLY TO", "Full は全帯域を絞る。Band only は、その帯域だけを絞る（ディエッサー。GR が 0 なら原音と完全に一致）", "Full reduces everything. Band only reduces just that band (de-esser; identical to the input when GR is 0)");
        setHelp (detBox, "DETECTOR", "音量の測り方。ブロックの説明を参照", "How the level is measured; see the block help");
        setHelp (topoSeg, "TOPOLOGY", "Feed-forward は入力を測る（正確）。Feedback は素子の出力を測る（古い機材の挙動。実効の比が下がり、なめらか）", "Feed-forward measures the input (exact). Feedback measures the element output (vintage: lower effective ratio, smoother)");
        setHelp (rmsV, "RMS WINDOW", "RMS の平均を取る時間。長いほどゆったり反応する", "Averaging time of the RMS");
        setHelp (lawBox, "GAIN LAW", "静特性のカーブの種類。ブロックの説明を参照", "The kind of static curve; see the block help");
        setHelp (thrV, "THRESHOLD", "圧縮が始まるレベル。FET では基準値（比によって上がる）", "Level where compression begins. For FET it is the base value (it rises with the ratio)");
        setHelp (ratioV, "RATIO", "Threshold を超えた分を何分の1にするか。FET は 4 / 8 / 12 / 20、30 以上は全押し", "How much the part over Threshold is divided. FET uses 4 / 8 / 12 / 20; 30 and above is all-buttons");
        setHelp (kneeV, "KNEE", "圧縮の始まり方。Ideal のときだけ自由に決められる", "How compression begins. Free only with the Ideal law");
        setHelp (timBox, "TIMING", "絞る量の動き方。ブロックの説明を参照", "How the reduction moves; see the block help");
        setHelp (attV, "ATTACK", "GR が目標の 63% に達するまでの時間（90% は約 2.3 倍、99% は約 4.6 倍）。実効値は GR WAVE で確認", "Time for GR to reach 63% of its target (90% ≈ 2.3×, 99% ≈ 4.6×). GR WAVE shows the effective value");
        setHelp (relV, "RELEASE", "GR が 63% 戻るまでの時間。方式と検出器によって実効値が変わる（GR WAVE で確認）", "Time for GR to fall by 63%. The effective value depends on the method and detector (see GR WAVE)");
        setHelp (lookV, "LOOKAHEAD / BEHIND", "プラス：音を遅らせて、先読みで絞り始める（レイテンシーが出る）。Precise では、この時間をかけて直線で絞る。マイナス（Look-behind）：絞る動きの方を遅らせて、打点を素通りさせる。レイテンシーは出ない",
                 "Positive: delays the audio so the gain can start early (adds latency); Precise ramps linearly over this time. Negative (look-behind): delays the gain change instead, so transients pass untouched; no latency");
        setHelp (holdV, "HOLD", "ピークを保持する時間。低音での歪み（ゲインの波打ち）を防ぐ", "How long the peak is held: prevents gain ripple on low notes");
        setHelp (slowV, "SLOW x", "遅い Release は Release の何倍か", "The slow release as a multiple of Release");
        setHelp (amtV, "AMOUNT", "2-stage：遅い成分の割合／Dual average：平均を取る時間は 100 ms 固定", "2-stage: share of the slow part / Dual average: averaging time is fixed at 100 ms");
        setHelp (elBox, "ELEMENT", "音量を下げる素子のモデル。ブロックの説明を参照", "Model of the part that turns the level down; see the block help");
        setHelp (preSeg, "PRE-STAGE", "直列 2 段。メインのコンプの前に、光学式のレベラー（2:1 固定、RMS、フィードバック、Attack 15 ms、Release は速い 80 ms ＋遅い成分）を 1 段入れる。前段で全体をゆっくり均し、後段でピークを止める。浅く 2 回かけると、深く 1 回より自然（Shadow Hills の構成）。TIME の紫の点線が前段の GR。3-band では使えない",
                 "Two stages in series: an opto leveller (fixed 2:1, RMS, feedback, 15 ms attack, 80 ms release plus a slow part) in front of the main compressor. The first evens out the level slowly, the second catches peaks. Two shallow passes sound more natural than one deep one (the Shadow Hills layout). The violet dots in TIME are the pre-stage GR. Not available in 3-band");
        setHelp (preThrV, "PRE THRESHOLD", "前段（光学 2:1）の Threshold。前段の GR が 1〜3 dB になるあたりから始める", "Threshold of the pre-stage (opto 2:1). Start where it shows 1–3 dB of GR");
        setHelp (driveV, "DRIVE", "素子に入るレベル（出口で同じだけ戻す）。上げるほど歪みが増える", "Level into the element (compensated at its output). More drive = more distortion");
        setHelp (osSeg, "OVERSAMPLE", "内部の処理を 2 倍・4 倍のサンプルレートで行う。歪みの折り返しと、速いゲイン変化の折り返しが減る", "Runs the engine at 2x / 4x: less aliasing from distortion and fast gain changes");
        setHelp (outBox, "OUTPUT STAGE", "出力段の色付け", "Output stage colour");
        setHelp (makeV, "MAKEUP", "圧縮で下がった音量を戻す", "Brings back the level lost to compression");
        setHelp (mixV, "DRY / WET", "原音と圧縮音の混ぜ具合。50% 前後がパラレル圧縮。OTT では「Amount」にあたる", "Blend of dry and compressed. Around 50% is parallel compression. This is the 'Amount' of OTT");
        setHelp (upSeg, "UPWARD", "アップワード・コンプレッション。UP THR より小さい音を持ち上げる（下向きの圧縮と同時に使える）。大きい音は変えずに、小さい音・余韻・細部を前に出す。−70 dB より小さい音（無音・ノイズ）は持ち上げない。TIME では緑の帯で表示",
                 "Upward compression: raises what is below UP THR (can run together with downward compression). Brings up quiet parts, tails and detail without touching the loud parts. Nothing below −70 dB is lifted. Shown as a green band in TIME");
        setHelp (relCurveV, "RELEASE CURVE", "Release の戻り方の形。Exp（対数）は最初が速く、だんだん遅くなる（多くのコンプ）。Linear は dB で一定の速さ（毎秒何 dB）で戻る。Linear は深い圧縮からの戻りが遅く、浅い圧縮からは速い。GR WAVE の Release 区間で形を確認できる",
                 "Shape of the release. Exp (log) starts fast and slows down (most compressors). Linear recovers at a constant dB per second: slower from deep reduction, quicker from shallow. See the shape in GR WAVE");
        setHelp (crestV, "PEAK CREST", "Peak + RMS 検出のとき、ピーク経路の Threshold を RMS 経路より何 dB 上に置くか。0 dB：打点にも平均にも同じように反応。3〜8 dB：平均（RMS）が主に働き、大きな打点だけピーク経路が止める。持続音がほぼ歪まない。黄色の線が RMS 経路の GR",
                 "With the Peak + RMS detector: how far the peak path's threshold sits above the RMS path's. 0 dB: reacts to hits and to the average alike. 3–8 dB: the RMS path does most of the work and the peak path only stops big hits; sustained sound stays clean. The yellow line is the RMS path's GR");
        setHelp (grLimV, "GR LIMIT", "ゲインリダクションの上限。これ以上は絞らない。深くかかりすぎるのを防ぎ、大きい音のダイナミクスを残す。真空管コンプで管が飽和して GR が頭打ちになるのと同じ形（TRANSFER で、カーブが途中から斜め 45° に戻る）",
                 "The most gain reduction allowed. Prevents over-compression and keeps the dynamics of loud passages — like a tube compressor running out of range (in TRANSFER the curve returns to 45°)");
        setHelp (upRangeV, "UP RANGE", "アップワードで持ち上げる量の上限", "The most the upward stage may raise");
        setHelp (upThrV, "UP THRESHOLD", "これより小さい音を持ち上げる", "Sound below this is raised");
        setHelp (upRatioV, "UP RATIO", "持ち上げる強さ。1:2 なら、Threshold より 10 dB 小さい音が 5 dB 持ち上がる（最大 Range まで。初期値 18 dB）", "Strength: at 1:2 a sound 10 dB under the threshold is raised by 5 dB (up to the range, 18 dB by default)");
        setHelp (mbSeg, "BANDS", "3-band にすると、低・中・高の 3 帯域に分け、同じ設定のコンプを帯域ごとに動かす（低域は 2 倍遅く、高域は 2 倍速く）。分割は 24 dB/oct で、足すと原音に戻る（位相だけ回る）",
                 "3-band splits into low / mid / high and runs the same compressor on each (low twice as slow, high twice as fast). 24 dB/oct crossovers that sum back to the input (phase only)");
        setHelp (xLoV, "CROSSOVER LOW", "低域と中域の境目", "Boundary between low and mid");
        setHelp (xHiV, "CROSSOVER HIGH", "中域と高域の境目", "Boundary between mid and high");
        setHelp (gLoV, "BAND GAIN LO", "低域の出力ゲイン（3-band のとき）", "Low band output gain (3-band)");
        setHelp (gMidV, "BAND GAIN MID", "中域の出力ゲイン（3-band のとき）", "Mid band output gain (3-band)");
        setHelp (gHiV, "BAND GAIN HI", "高域の出力ゲイン（3-band のとき）", "High band output gain (3-band)");
    }

    void resized() override
    {
        const int bw = 148, gap = 13;
        for (size_t i = 0; i < blocks.size(); ++i) blocks[i].r = { 12 + (int) i * (bw + gap), 26, bw, 186 };
        auto X = [this] (int b) { return blocks[(size_t) b].r.getX() + 10; };
        const int y0 = 26 + 36, w = 128, hw = 62;
        // ① ② have no method menu: rows of 32 px. ③–⑦: the menu, then four rows of 31 px.
        auto rowY = [&] (int b, int row) { return b < 2 ? y0 + row * 30 : y0 + 26 + (row - 1) * 31; };
        auto full = [&] (int b, int row) { return juce::Rectangle<int> (X (b), rowY (b, row), w, 29); };
        auto half = [&] (int b, int row, int k) { return juce::Rectangle<int> (X (b) + k * (hw + 4), rowY (b, row), hw, 29); };
        auto seg = [&] (int b, int row) { return juce::Rectangle<int> (X (b), rowY (b, row) + 11, w, 18); };
        inputV.setBounds (full (0, 0)); xfSeg.setBounds (seg (0, 1)); mbSeg.setBounds (seg (0, 2)); xLoV.setBounds (half (0, 3, 0)); xHiV.setBounds (half (0, 3, 1));
        srcSeg.setBounds (seg (1, 0)); hpfV.setBounds (half (1, 1, 0)); tiltV.setBounds (half (1, 1, 1)); linkV.setBounds (half (1, 2, 0)); bandV.setBounds (half (1, 2, 1));
        bandHzV.setBounds (half (1, 3, 0)); bandQV.setBounds (half (1, 3, 1)); onlySeg.setBounds (X (1), y0 + 4 * 30 + 8, w, 16); stereoV.setBounds (full (0, 4));
        detBox.setBounds (X (2), y0, w, 22); topoSeg.setBounds (seg (2, 1)); rmsV.setBounds (half (2, 2, 0)); crestV.setBounds (half (2, 2, 1));
        scMbSeg.setBounds (seg (2, 3)); sLoV.setBounds (X (2), rowY (2, 4), 40, 29); sMidV.setBounds (X (2) + 44, rowY (2, 4), 40, 29); sHiV.setBounds (X (2) + 88, rowY (2, 4), 40, 29);
        lawBox.setBounds (X (3), y0, w, 22); thrV.setBounds (half (3, 1, 0)); ratioV.setBounds (half (3, 1, 1)); kneeV.setBounds (half (3, 2, 0));
        upSeg.setBounds (X (3) + hw + 4, rowY (3, 2) + 11, hw, 18); upThrV.setBounds (half (3, 3, 0)); upRatioV.setBounds (half (3, 3, 1));
        grLimV.setBounds (half (3, 4, 0)); upRangeV.setBounds (half (3, 4, 1));
        timBox.setBounds (X (4), y0, w, 22); attV.setBounds (half (4, 1, 0)); relV.setBounds (half (4, 1, 1)); lookV.setBounds (half (4, 2, 0)); holdV.setBounds (half (4, 2, 1));
        slowV.setBounds (half (4, 3, 0)); amtV.setBounds (half (4, 3, 1)); relCurveV.setBounds (full (4, 4));
        elBox.setBounds (X (5), y0, w, 22); driveV.setBounds (full (5, 1)); osSeg.setBounds (seg (5, 2)); preSeg.setBounds (seg (5, 3)); preThrV.setBounds (full (5, 4));
        outBox.setBounds (X (6), y0, w, 22); makeV.setBounds (full (6, 1)); mixV.setBounds (full (6, 2));
        gLoV.setBounds (X (6), rowY (6, 3), 40, 29); gMidV.setBounds (X (6) + 44, rowY (6, 3), 40, 29); gHiV.setBounds (X (6) + 88, rowY (6, 3), 40, 29);
    }

    void refresh()
    {
        for (auto* b : { &detBox, &lawBox, &timBox, &elBox, &outBox }) b->sync();
        {                                                               // Spectral skips ③ ⑤ ⑥ ⑦ and the topology; Clipper skips ③ and ⑤
            const int law = (int) ph.value (pid::law); const bool sp = law == mod::LawSpectral, cl = law == mod::LawClip;
            auto en = [] (juce::Component& c, bool on) { if (c.isEnabled() != on) { c.setEnabled (on); c.setAlpha (on ? 1.0f : 0.4f); } };
            en (detBox, ! sp && ! cl); en (timBox, ! sp && ! cl); en (elBox, ! sp); en (outBox, ! sp); en (topoSeg, ! sp && ! cl);
        }
        const bool ride = (int) ph.value (pid::law) == mod::LawRide;           // gain riding: the same parameters under their riding names
        thrV.setCaption (ride ? "TARGET" : "THRESH"); upThrV.setCaption (ride ? "IDLE <" : "UP THR");
        upRangeV.setCaption (ride ? juce::String::fromUTF8 ("RANGE \xc2\xb1") : juce::String ("UP RANGE"));
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override { if (const int t = hitViewBtn (e.getPosition(), getWidth()); t >= 0) setViewMode (ph.proc.uiState, t); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int hover = -1;
        for (size_t i = 0; i < blocks.size(); ++i) if (blocks[i].r.contains (e.getPosition())) hover = (int) i;
        if (hover >= 0) setHelp (*this, blocks[(size_t) hover].ht, blocks[(size_t) hover].hj, blocks[(size_t) hover].he);
        else getProperties().remove ("helpT");
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panel); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (theme::onWell()); g.setFont (theme::mono (9.5f, true)); g.drawText ("SIGNAL FLOW", 12, 7, 90, 14, juce::Justification::centredLeft);
        drawViewBtns (g, getWidth(), 4, ph.ja());
        g.setFont (theme::ui (9.5f)); g.setColour (theme::faint);
        g.drawText (ph.tr ("各ブロックの方式（メニュー）を差し替えて、コンプを組み立てる。灰色の値は、その方式では回路が決める項目。数値を右クリックでロック。ブロックにポインターを置くと説明",
                           "Swap the method (menu) of each block to build a compressor. Grey values are fixed by the chosen method. Right-click a value to lock it. Hover a block for help"),
                    100, 7, getWidth() - 112 - 236, 14, juce::Justification::centredLeft);
        const auto mouse = getMouseXYRelative();
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            const auto& b = blocks[i]; const bool on = b.r.contains (mouse) && isMouseOver (true);
            g.setColour (theme::shade (0.30f)); g.fillRoundedRectangle (b.r.toFloat(), 6.0f);
            g.setColour (on ? theme::accent : theme::line); g.drawRoundedRectangle (b.r.toFloat().reduced (0.5f), 6.0f, on ? 1.6f : 1.0f);
            g.setColour (on ? theme::accent : theme::text); g.setFont (theme::mono (10.0f, true));
            g.drawText (juce::String::fromUTF8 (b.num) + " " + b.en, b.r.getX() + 10, b.r.getY() + 5, b.r.getWidth() - 14, 14, juce::Justification::centredLeft);
            g.setColour (theme::onWell()); g.setFont (theme::ui (9.5f));
            g.drawText (juce::String::fromUTF8 (ph.ja() ? b.subJa : b.subEn), b.r.getX() + 10, b.r.getY() + 19, b.r.getWidth() - 14, 13, juce::Justification::centredLeft);
            if (i + 1 < blocks.size())
            {
                const float ax = (float) b.r.getRight(), ay = (float) b.r.getCentreY();
                g.setColour (theme::muted); g.drawLine (ax + 1, ay, ax + 10, ay, 1.4f);
                juce::Path t; t.addTriangle (ax + 10, ay - 3.5f, ax + 14, ay, ax + 10, ay + 3.5f); g.fillPath (t);
            }
        }
        g.setColour (theme::onWell()); g.setFont (theme::ui (9.0f));
        auto cap = [&] (juce::Component& c, const char* t) { g.drawText (t, c.getX() + 2, c.getY() - 11, 126, 11, juce::Justification::centredLeft); };
        cap (xfSeg, "IN TRANSFORMER"); cap (srcSeg, "SOURCE"); cap (topoSeg, "TOPOLOGY"); cap (osSeg, "OVERSAMPLE"); cap (mbSeg, "BANDS"); cap (scMbSeg, "DETECT BANDS"); cap (upSeg, "UPWARD"); cap (preSeg, "PRE-STAGE");
        // feedback return path: element → detector
        const bool fb = (int) ph.value (pid::topology) == 1;
        const float cx5 = (float) blocks[5].r.getCentreX(), cx2 = (float) blocks[2].r.getCentreX(), yb = (float) blocks[2].r.getBottom(), yl = yb + 10.0f;
        juce::Path pth; pth.startNewSubPath (cx5, yb); pth.lineTo (cx5, yl); pth.lineTo (cx2, yl); pth.lineTo (cx2, yb + 1);
        g.setColour (fb ? theme::phase : theme::line); g.strokePath (pth, juce::PathStrokeType (fb ? 1.5f : 1.0f));
        // what the model's PANEL can move: a bright frame (it also shows through the panel)
        if (panelIds.size() > 0)
        {
            for (auto& ci : ctlIds) if (panelIds.contains (ci.second))
            { const auto b = ci.first->getBounds().toFloat().expanded (2.0f); g.setColour (theme::accent.withAlpha (0.16f)); g.fillRoundedRectangle (b, 5.0f); g.setColour (theme::accent); g.drawRoundedRectangle (b, 5.0f, 1.8f); }
            g.setColour (theme::accent); g.drawRoundedRectangle (14.0f, (float) getHeight() - 15.0f, 12.0f, 9.0f, 2.5f, 1.6f);
            g.setColour (theme::onWell()); g.setFont (theme::ui (9.0f)); g.drawText (ph.tr ("＝PANEL で操作できる項目", "= what PANEL can move"), 30, getHeight() - 17, 220, 12, juce::Justification::centredLeft);
        }
        juce::Path ah; ah.addTriangle (cx2 - 3.5f, yb + 5, cx2 + 3.5f, yb + 5, cx2, yb); g.fillPath (ah);
        const juce::String lab = ph.tr (fb ? "フィードバック：素子の出力 → 検出器へ戻る" : "（Feedback にすると、素子の出力が検出器へ戻る）", fb ? "Feedback: element output returns to the detector" : "(Feedback returns the element output to the detector)");
        const int lw = (int) textWidth (theme::ui (9.0f), lab) + 14;
        g.setColour (theme::panel); g.fillRect ((int) ((cx2 + cx5) / 2) - lw / 2, (int) yl - 6, lw, 12);
        g.setColour (fb ? theme::phase : theme::faint); g.setFont (theme::ui (9.0f)); g.drawText (lab, (int) ((cx2 + cx5) / 2) - lw / 2, (int) yl - 6, lw, 12, juce::Justification::centred);
    }

private:
    struct Block { const char* num; const char* en; const char* subJa; const char* subEn; const char* ht; const char* hj; const char* he; juce::Rectangle<int> r; };
    ParamHelp& ph;
    DragValue inputV, hpfV, tiltV, linkV, bandV, bandHzV, bandQV, rmsV, thrV, ratioV, kneeV, attV, relV, lookV, holdV, slowV, amtV, driveV, makeV, mixV, upThrV, upRatioV, xLoV, xHiV, gLoV, gMidV, gHiV, relCurveV, crestV, grLimV, upRangeV, stereoV, sLoV, sMidV, sHiV, preThrV;
    Seg xfSeg, srcSeg, onlySeg, topoSeg, osSeg, upSeg, mbSeg, scMbSeg, preSeg;
    std::vector<std::pair<juce::Component*, const char*>> ctlIds;
public:
    juce::StringArray panelIds;                 // parameters the current model's PANEL moves (set by the editor)
private:
    ChoiceBox detBox, lawBox, timBox, elBox, outBox;
    std::vector<Block> blocks;
};

// =============================================================================
//  Info card: help for whatever is under the mouse ("| a | b |" lines draw as a table); the model summary otherwise.
// =============================================================================
class InfoCard : public juce::Component
{
public:
    explicit InfoCard (ParamHelp& h) : ph (h) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::lessonBg); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::lessonLn); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        auto a = getLocalBounds().reduced (14, 10);
        juce::String title = ph.hoverTitle, text = ph.hoverText;
        if (title.isEmpty())
        {
            const int mi = ph.proc.currentModel(); const auto& ms = mod::models();
            if (mi >= 0 && mi < (int) ms.size())
            {
                const auto& p = ms[(size_t) mi].p;
                const auto label = ph.proc.presetLabel (ph.ja()), note = ph.proc.presetNote (ph.ja());
                title = "MODEL  " + juce::String (ms[(size_t) mi].name) + juce::String::fromUTF8 ("　—　") + juce::String (ms[(size_t) mi].ref);
                const auto now = ph.proc.currentParams();
                text = (label.isNotEmpty() ? "PRESET  " + label + (note.isNotEmpty() ? juce::String::fromUTF8 ("：") + note : juce::String()) + "\n\n" : juce::String())
                     + "| " + ph.tr ("ブロック", "Block") + " | " + ph.tr ("いまの方式", "Method now") + " |\n"
                     + "| " + juce::String::fromUTF8 ("\xe2\x91\xa2 Detector") + " | " + detNames()[now.det] + (now.feedback ? ", Feedback" : ", Feed-forward") + " |\n"
                     + "| " + juce::String::fromUTF8 ("\xe2\x91\xa3 Gain law") + " | " + lawNames()[now.law] + (now.upOn ? " + Upward" : "") + " |\n"
                     + "| " + juce::String::fromUTF8 ("\xe2\x91\xa4 Timing") + " | " + timingNames()[now.timing] + " |\n"
                     + "| " + juce::String::fromUTF8 ("\xe2\x91\xa5 Element") + " | " + elementNames()[now.element] + (now.pre && ! now.mb ? ph.tr ("＋前段 Opto 2:1", " + pre-stage opto 2:1") : juce::String()) + (now.mb ? ph.tr ("（3 バンド）", " (3-band)") : juce::String()) + " |\n"
                     + "| " + juce::String::fromUTF8 ("\xe2\x91\xa6 Output") + " | " + outNames()[now.out] + (now.inXfmr ? ph.tr ("（入力トランスあり）", " (+ input transformer)") : juce::String()) + " |\n\n"
                     + (label.isEmpty() ? ph.tr ("定数は文献をもとにした初期値で、実機の測定値ではありません。各ブロックにポインターを置くと説明が出ます。",
                                                 "Constants are starting values from published descriptions, not measurements of a unit. Hover any block for help.") : juce::String());
            }
            else
            {
                title = ph.tr ("はじめに", "START HERE");
                text = ph.tr ("左上の MODEL から機種を選ぶと、7 つのブロックの方式と定数がその機種の組み合わせに切り替わります。その後は各ブロックの方式を自由に差し替えられます。\n\nAttack は「GR が目標の 63% に達するまでの時間」です（90% は約 2.3 倍、99% は約 4.6 倍）。左下の GR WAVE で、方式ごとの実効値を確認できます。",
                              "Pick a MODEL (top left): the seven blocks switch to that unit's methods and constants. After that every block's method can be swapped freely.\n\nAttack is the time for GR to reach 63% of its target (90% ≈ 2.3×, 99% ≈ 4.6×). GR WAVE (bottom left) shows the effective value for each method.");
            }
        }
        g.setColour (theme::accent); g.setFont (theme::mono (11.0f, true)); g.drawFittedText (title, a.getX(), a.getY(), a.getWidth() - 160, 16, juce::Justification::centredLeft, 1, 0.75f);
        int y = a.getY() + 22; juce::String para;
        auto flush = [&] { if (para.isEmpty()) return; g.setColour (theme::text); g.setFont (theme::ui (12.0f));
                           const int lines = juce::jmax (1, (int) std::ceil (textWidth (theme::ui (12.0f), para) / (float) a.getWidth() + 0.25f)), h = lines * 16;
                           g.drawFittedText (para, a.getX(), y, a.getWidth(), h, juce::Justification::topLeft, lines + 1, 0.9f); y += h + 4; para.clear(); };
        for (auto& line : juce::StringArray::fromLines (text))
        {
            if (line.trimStart().startsWithChar ('|'))
            {
                flush();
                auto cells = juce::StringArray::fromTokens (line.trim().trimCharactersAtStart ("|").trimCharactersAtEnd ("|"), "|", "");
                const int c0 = 130; const bool head = ! inTable; inTable = true;
                g.setFont (theme::ui (11.0f, head)); g.setColour (head ? theme::muted : theme::accent);
                g.drawFittedText (cells[0].trim(), a.getX(), y, c0 - 6, 15, juce::Justification::centredLeft, 1, 0.8f);
                g.setColour (head ? theme::muted : theme::text);
                g.drawFittedText (cells.size() > 1 ? cells[1].trim() : juce::String(), a.getX() + c0, y, a.getWidth() - c0, 15, juce::Justification::centredLeft, 1, 0.75f);
                g.setColour (theme::lessonLn.withAlpha (0.5f)); g.drawHorizontalLine (y + 15, (float) a.getX(), (float) a.getRight());
                y += 16;
            }
            else { if (inTable) { y += 4; inTable = false; } if (line.trim().isEmpty()) flush(); else para += line; }
        }
        flush(); inTable = false;
    }
private:
    ParamHelp& ph; bool inTable = false;
};
