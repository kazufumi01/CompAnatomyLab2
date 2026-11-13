#pragma once
#include "Flow.h"

// =============================================================================
//  PANEL: the controls the hardware has, on its own scales, each with a note saying which block (① – ⑦)
//  of the signal flow it moves. Nothing here adds DSP: every control writes ordinary parameters,
//  so BUILD shows the same state from the inside.
// =============================================================================
namespace panel
{
using Set = std::vector<std::pair<const char*, float>>;
struct Opt { const char* label; Set set; };
enum Fmt { FDb, FMinusDb, FTime, FRatio, FPct, FX, FUpRatio, FMs };
struct Ctl
{
    bool knob; const char* cap; const char* id; float lo, hi; bool log; int fmt; float nLo, nHi;    // knob: value fully left / fully right; nLo..nHi = the scale printed on the unit
    std::vector<Opt> opts;                                                                          // switch: each position sets one or more parameters
    const char* ja; const char* en;
};
using Slot = std::vector<Ctl>;
struct Def { const char* model; std::vector<Slot> slots; };

inline Ctl K (const char* cap, const char* id, float lo, float hi, bool log, int fmt, const char* ja, const char* en, float nLo = 0, float nHi = 0)
{ return { true, cap, id, lo, hi, log, fmt, nLo, nHi, {}, ja, en }; }
inline Ctl S (const char* cap, std::vector<Opt> opts, const char* ja, const char* en) { return { false, cap, "", 0, 1, false, FDb, 0, 0, std::move (opts), ja, en }; }
inline std::vector<Opt> vals (const char* id, std::initializer_list<std::pair<const char*, float>> l) { std::vector<Opt> o; for (auto& p : l) o.push_back ({ p.first, { { id, p.second } } }); return o; }

inline const std::vector<Def>& defs()
{
    using namespace mod;
    static const std::vector<Def> d = [] {
        std::vector<Def> v;
        const float one = (float) TimOne, prog = (float) TimProg2, aut = (float) TimAuto;
        auto thr = [] (float lo, float hi, const char* ja, const char* en, float nLo = 0, float nHi = 0) { return K ("THRESHOLD", pid::threshold, lo, hi, false, FMinusDb, ja, en, nLo, nHi); };
        auto mk = [] (const char* cap, const char* ja = "⑦ Makeup", const char* en = "⑦ Makeup") { return K (cap, pid::makeup, -12, 24, false, FDb, ja, en); };

        v.push_back ({ "digital", {
            { thr (0, -60, "④ Threshold", "④ Threshold") },
            { K ("RATIO", pid::ratio, 1, 100, true, FRatio, "④ Ratio", "④ Ratio") },
            { K ("KNEE", pid::knee, 0, 24, false, FDb, "④ 折れ曲がりの幅", "④ width of the bend") },
            { K ("ATTACK", pid::attack, 0.02f, 300, true, FTime, "⑤ 先読みの時間で直線的に下げる", "⑤ ramps down linearly over the look-ahead") },
            { K ("RELEASE", pid::release, 5, 5000, true, FTime, "⑤ Release", "⑤ Release") },
            { K ("LOOKAHEAD", pid::lookahead, 0, 10, false, FMs, "⑤ 先読み。その分だけ遅延が出る", "⑤ look-ahead: adds that much latency") },
            { mk ("MAKEUP") } } });

        v.push_back ({ "fet76", {
            { K ("INPUT", pid::input, -12, 24, false, FDb, "① 入力ゲイン。Threshold ノブは無く、固定のしきい値へ信号を押し込む", "① input gain. No Threshold knob: it pushes the signal into a fixed threshold") },
            { mk ("OUTPUT") },
            { K ("ATTACK", pid::attack, 0.8f, 0.02f, true, FTime, "⑤ 800 µs〜20 µs。右へ回すほど速い（数字が大きい＝速い）", "⑤ 800 µs – 20 µs. Clockwise = faster (higher number = faster)", 1, 7) },
            { K ("RELEASE", pid::release, 1100, 50, true, FTime, "⑤ 1.1 s〜50 ms＋遅い成分。右へ回すほど速い", "⑤ 1.1 s – 50 ms plus a slow part. Clockwise = faster", 1, 7) },
            { S ("RATIO", { { "4", { { pid::ratio, 4 }, { pid::drive, 0 }, { pid::slowAmt, 40 } } }, { "8", { { pid::ratio, 8 }, { pid::drive, 0 }, { pid::slowAmt, 40 } } },
                            { "12", { { pid::ratio, 12 }, { pid::drive, 0 }, { pid::slowAmt, 40 } } }, { "20", { { pid::ratio, 20 }, { pid::drive, 0 }, { pid::slowAmt, 40 } } },
                            { "ALL", { { pid::ratio, 20 }, { pid::drive, 8 }, { pid::slowAmt, 70 } } } },
                 "④ 比を上げると Threshold も上がり、ニーが硬くなる。ALL＝全押し：⑥ の歪みと ⑤ の遅い成分が増える", "④ a higher ratio raises the threshold and hardens the knee. ALL = all buttons in: more ⑥ distortion and more of the ⑤ slow part") } } });

        const std::vector<Opt> compLimit { { "Comp", { { pid::ratio, 3 } } }, { "Limit", { { pid::ratio, 10 } } } };
        v.push_back ({ "opto2a", {
            { K ("PEAK REDUCTION", pid::threshold, 0, -50, false, FMinusDb, "④ Threshold を下げる。Attack・Release のノブは無く、⑤ 光学セルが決める", "④ lowers the threshold. No attack / release knobs: the ⑤ photocell decides", 0, 100) },
            { K ("GAIN", pid::makeup, 0, 24, false, FDb, "⑦ Makeup（真空管の出力段）", "⑦ makeup (tube output stage)", 0, 100) },
            { S ("MODE", compLimit, "④ 比の切替。光学式は深くかかるほど比が上がる", "④ ratio switch. An opto's ratio grows with depth") } } });

        v.push_back ({ "mu670", {
            { K ("INPUT GAIN", pid::input, -12, 12, false, FDb, "① 入力ゲイン。大きく入れるほど深くかかる", "① input gain: more level in = more reduction", 0, 20) },
            { thr (0, -40, "④ Threshold。比は固定ではなく、レベルが上がるほど上がる", "④ Threshold. The ratio is not fixed: it rises with level", 0, 10) },
            { S ("TIME CONSTANT", { { "1", { { pid::attack, 0.2f }, { pid::release, 300 }, { pid::timing, one } } }, { "2", { { pid::attack, 0.2f }, { pid::release, 800 }, { pid::timing, one } } },
                                    { "3", { { pid::attack, 0.4f }, { pid::release, 2000 }, { pid::timing, one } } }, { "4", { { pid::attack, 0.8f }, { pid::release, 5000 }, { pid::timing, one } } },
                                    { "5", { { pid::attack, 0.2f }, { pid::release, 2000 }, { pid::timing, prog }, { pid::slowMult, 5 }, { pid::slowAmt, 50 } } },
                                    { "6", { { pid::attack, 0.4f }, { pid::release, 300 }, { pid::timing, prog }, { pid::slowMult, 20 }, { pid::slowAmt, 60 } } } },
                 "⑤ Attack と Release の組。1〜4 は固定（0.3 / 0.8 / 2 / 5 s）、5・6 は 2 段：圧縮が続くほど戻りが遅くなる", "⑤ attack / release pairs. 1–4 fixed (0.3 / 0.8 / 2 / 5 s); 5 and 6 are two-stage: the longer it compresses, the slower it recovers"),
              S ("AGC", { { "L / R", { { pid::stereo, 0 } } }, { "LAT / VERT", { { pid::stereo, 1 } } } }, "① LAT / VERT＝Mid / Side で別々に圧縮", "① LAT / VERT = compress mid and side separately") },
            { mk ("OUTPUT") } } });

        v.push_back ({ "busg", {
            { thr (0, -40, "④ Threshold", "④ Threshold") },
            { S ("ATTACK ms", vals (pid::attack, { { "0.1", 0.1f }, { "0.3", 0.3f }, { "1", 1 }, { "3", 3 }, { "10", 10 }, { "30", 30 } }), "⑤ 遅いほどキックの頭が抜ける", "⑤ slower lets the kick's front through"),
              S ("RELEASE s", { { "0.1", { { pid::release, 100 }, { pid::timing, one } } }, { "0.3", { { pid::release, 300 }, { pid::timing, one } } }, { "0.6", { { pid::release, 600 }, { pid::timing, one } } },
                                { "1.2", { { pid::release, 1200 }, { pid::timing, one } } }, { "Auto", { { pid::release, 300 }, { pid::timing, aut } } } },
                 "⑤ Auto＝短い音は速く、長い音は遅く戻る", "⑤ Auto = short sounds recover fast, long ones slowly") },
            { S ("RATIO", vals (pid::ratio, { { "2", 2 }, { "4", 4 }, { "10", 10 } }), "④ 2 はニーが広く、10 は硬い", "④ 2 is gentle, 10 is firm") },
            { mk ("MAKEUP") } } });

        v.push_back ({ "vca160", {
            { thr (0, -40, "④ Threshold（ハードニー）", "④ Threshold (hard knee)") },
            { K ("COMPRESSION", pid::ratio, 1, 100, true, FRatio, "④ Ratio。Attack・Release のノブは無い：③ RMS 検出器の窓（36 ms）が速さを決め、戻りは dB で直線になる", "④ Ratio. No attack / release knobs: the ③ RMS detector's window (36 ms) sets the speed, and the release is linear in dB") },
            { mk ("OUTPUT GAIN") } } });

        v.push_back ({ "diode254", {
            { thr (0, -40, "④ Threshold", "④ Threshold") },
            { S ("RATIO", vals (pid::ratio, { { "1.5", 1.5f }, { "2", 2 }, { "3", 3 }, { "4", 4 }, { "6", 6 } }), "④ ニーは固定で広い（ダイオードブリッジ）", "④ the knee is fixed and wide (diode bridge)"),
              S ("ATTACK", vals (pid::attack, { { "Fast", 2 }, { "Slow", 5 } }), "⑤ 2 段切替のみ", "⑤ two positions only") },
            { S ("RECOVERY ms", { { "100", { { pid::release, 100 }, { pid::timing, one } } }, { "400", { { pid::release, 400 }, { pid::timing, one } } }, { "800", { { pid::release, 800 }, { pid::timing, one } } },
                                  { "1500", { { pid::release, 1500 }, { pid::timing, one } } }, { "a1", { { pid::release, 100 }, { pid::timing, aut } } }, { "a2", { { pid::release, 400 }, { pid::timing, prog }, { pid::slowMult, 12 }, { pid::slowAmt, 50 } } } },
                 "⑤ a1・a2 は自動：短い音は速く、圧縮が続くと遅く戻る", "⑤ a1 / a2 are automatic: fast after short sounds, slow after sustained reduction") },
            { mk ("GAIN") } } });

        v.push_back ({ "vca2500", {
            { thr (0, -40, "④ Threshold", "④ Threshold") },
            { S ("ATTACK ms", vals (pid::attack, { { ".03", 0.03f }, { ".1", 0.1f }, { ".3", 0.3f }, { "1", 1 }, { "3", 3 }, { "10", 10 }, { "30", 30 } }), "⑤ Attack", "⑤ Attack"),
              S ("RELEASE s", vals (pid::release, { { ".05", 50 }, { ".1", 100 }, { ".2", 200 }, { ".5", 500 }, { "1", 1000 }, { "2", 2000 } }), "⑤ Release", "⑤ Release") },
            { S ("RATIO", vals (pid::ratio, { { "1.5", 1.5f }, { "2", 2 }, { "3", 3 }, { "4", 4 }, { "6", 6 }, { "10", 10 }, { "\xe2\x88\x9e", 100 } }), "④ Ratio", "④ Ratio"),
              S ("KNEE", vals (pid::knee, { { "Hard", 0 }, { "Med", 6 }, { "Soft", 12 } }), "④ 折れ曲がりの幅", "④ width of the bend") },
            { S ("THRUST", vals (pid::scTilt, { { "Norm", 0 }, { "Med", 1 }, { "Loud", 2 } }), "② 検出器を低域に鈍く・高域に敏感にする。キックで全体が沈まない", "② makes the detector less sensitive to lows, more to highs: the kick stops pumping the mix"),
              S ("TYPE", vals (pid::topology, { { "New", 0 }, { "Old", 1 } }), "③ New＝フィードフォワード、Old＝フィードバック", "③ New = feed-forward, Old = feedback") },
            { mk ("MAKEUP") } } });

        v.push_back ({ "el8", {
            { K ("INPUT", pid::input, -12, 24, false, FDb, "① 入力ゲイン。Threshold ノブは無く、ここで圧縮量を決める", "① input gain. No Threshold knob: this sets the amount", 0, 10) },
            { K ("ATTACK", pid::attack, 0.05f, 30, true, FTime, "⑤ 0.05〜30 ms", "⑤ 0.05 – 30 ms", 0, 10) },
            { K ("RELEASE", pid::release, 50, 3000, true, FTime, "⑤ 50 ms〜3 s＋遅い成分", "⑤ 50 ms – 3 s plus a slow part", 0, 10) },
            { S ("RATIO", { { "2", { { pid::ratio, 2 }, { pid::knee, 12 } } }, { "3", { { pid::ratio, 3 }, { pid::knee, 10 } } }, { "4", { { pid::ratio, 4 }, { pid::knee, 8 } } }, { "6", { { pid::ratio, 6 }, { pid::knee, 8 } } },
                            { "10", { { pid::ratio, 10 }, { pid::knee, 4 } } }, { "20", { { pid::ratio, 20 }, { pid::knee, 2 } } }, { "NUKE", { { pid::ratio, 100 }, { pid::knee, 0 } } } },
                 "④ 比ごとにニーの幅も変わる。NUKE＝ブリックウォール", "④ each ratio has its own knee. NUKE = brick wall"),
              S ("DIST", vals (pid::outStage, { { "Off", (float) OutClean }, { "2", (float) OutH2 }, { "3", (float) OutH3 } }), "⑦ 2 次／3 次の倍音を足す", "⑦ adds 2nd / 3rd harmonics") },
            { S ("DET HP", vals (pid::scHpf, { { "Off", 0 }, { "On", 3 } }), "② 検出器から 100 Hz 以下を外す。キックやベースで全体が沈まない", "② keeps everything below 100 Hz out of the detector: kick and bass stop ducking the rest"),
              S ("LINK", vals (pid::link, { { "Off", 0 }, { "On", 100 } }), "② 左右のゲインを連動させる。Off＝左右別々（デュアルモノ）", "② ties the left and right gain together. Off = dual mono") },
            { mk ("OUTPUT") } } });

        v.push_back ({ "opto1b", {
            { mk ("GAIN", "⑦ Makeup（真空管の出力段）", "⑦ makeup (tube output stage)") },
            { K ("RATIO", pid::ratio, 2, 10, true, FRatio, "④ 2:1〜10:1。ニーは広い", "④ 2:1 – 10:1, soft knee") },
            { thr (0, -40, "④ Threshold", "④ Threshold") },
            { K ("ATTACK", pid::attack, 0.5f, 300, true, FTime, "⑤ 0.5〜300 ms", "⑤ 0.5 – 300 ms") },
            { K ("RELEASE", pid::release, 50, 5000, true, FTime, "⑤ 50 ms〜。光学セルなので、長く鳴った後は戻りが遅い", "⑤ from 50 ms. A photocell: slower to recover after long sounds") },
            { S ("TIMES", { { "Fixed", { { pid::timing, (float) TimOptoMem } } }, { "Manual", { { pid::timing, (float) TimOne } } } },
                 "⑤ Fixed＝光学セルのまま（長く鳴った後ほど戻りが遅い）。Manual＝ATTACK・RELEASE のとおりの一定の時定数", "⑤ Fixed = the photocell's own behaviour (slower recovery after long sounds). Manual = a constant time set by ATTACK and RELEASE") } } });

        v.push_back ({ "ds1", {
            { thr (0, -60, "④ Threshold", "④ Threshold") },
            { K ("RATIO", pid::ratio, 1, 100, true, FRatio, "④ Ratio", "④ Ratio") },
            { K ("ATTACK", pid::attack, 0.02f, 300, true, FTime, "⑤ Attack", "⑤ Attack") },
            { K ("RELEASE FAST", pid::release, 5, 5000, true, FTime, "⑤ 短い音のあとの戻り", "⑤ recovery after short sounds") },
            { K ("RELEASE SLOW", pid::slowMult, 1, 20, true, FX, "⑤ 持続音のあとの戻り（Fast の何倍か）", "⑤ recovery after sustained sound (multiple of Fast)") },
            { K ("AVERAGE", pid::avg, 10, 1000, true, FTime, "⑤ この時間より長く鳴った音を「持続音」とみなす", "⑤ sound lasting longer than this counts as sustained") },
            { mk ("GAIN") } } });

        v.push_back ({ "limiter", {
            { K ("INPUT", pid::input, 0, 24, false, FDb, "① 上げるほど天井に強く当たる＝音圧が上がる", "① more input hits the ceiling harder = louder") },
            { K ("CEILING", pid::threshold, -12, 0, false, FMinusDb, "④ これを超える音は出ない（∞:1）", "④ nothing passes this level (∞:1)") },
            { K ("RELEASE", pid::release, 5, 1000, true, FTime, "⑤ 速いほど音圧が上がるが、低域が歪みやすい", "⑤ faster is louder but distorts lows sooner") },
            { K ("LOOKAHEAD", pid::lookahead, 0, 10, false, FMs, "⑤ 先読み。ピークの前から下げ始めるので行き過ぎない", "⑤ look-ahead: starts turning down before the peak, so no overshoot") } } });

        v.push_back ({ "upward", {
            { K ("UP THRESHOLD", pid::upThr, -70, 0, false, FMinusDb, "④ これより小さい音を持ち上げる", "④ sounds below this are raised") },
            { K ("UP RATIO", pid::upRatio, 1, 10, true, FUpRatio, "④ 持ち上げる強さ", "④ how strongly") },
            { K ("RANGE", pid::upRange, 0, 36, false, FDb, "④ 持ち上げ量の上限。無音やノイズまで上げないため", "④ the most it may raise: keeps silence and noise down") },
            { K ("ATTACK", pid::attack, 0.02f, 300, true, FTime, "⑤ 大きい音が来て、持ち上げをやめる速さ", "⑤ how fast the boost lets go when a loud sound arrives") },
            { K ("RELEASE", pid::release, 5, 5000, true, FTime, "⑤ 持ち上がる速さ", "⑤ how fast the boost rises") },
            { K ("MIX", pid::mix, 0, 100, false, FPct, "⑦ 原音との割合", "⑦ balance with the dry signal") } } });

        v.push_back ({ "ott", {
            { K ("DEPTH", pid::mix, 0, 100, false, FPct, "⑦ 原音との割合。100% はかなり強い", "⑦ balance with the dry signal. 100% is extreme") },
            { K ("IN GAIN", pid::input, -12, 24, false, FDb, "① 入力ゲイン", "① input gain") },
            { K ("UPWARD", pid::upRange, 0, 36, false, FDb, "④ 小さい音を持ち上げる上限", "④ the most quiet sound may be raised") },
            { K ("LOW", pid::mbLo, -12, 12, false, FDb, "⑦ 低域（〜88 Hz）の出力", "⑦ low band output (below 88 Hz)") },
            { K ("MID", pid::mbMid, -12, 12, false, FDb, "⑦ 中域の出力", "⑦ mid band output") },
            { K ("HIGH", pid::mbHi, -12, 12, false, FDb, "⑦ 高域（2.5 kHz〜）の出力", "⑦ high band output (above 2.5 kHz)") },
            { mk ("OUT GAIN") } } });

        v.push_back ({ "opto3a", {
            { K ("PEAK REDUCTION", pid::threshold, 0, -50, false, FMinusDb, "④ Threshold を下げる。2A より ⑤ のアタックが速い（半導体の回路）", "④ lowers the threshold. Faster ⑤ attack than the 2A (solid-state circuit)", 0, 100) },
            { K ("GAIN", pid::makeup, 0, 24, false, FDb, "⑦ Makeup（トランス出力）", "⑦ makeup (transformer output)", 0, 100) },
            { S ("MODE", compLimit, "④ 比の切替", "④ ratio switch") } } });

        v.push_back ({ "chane", {
            { K ("RATIO", pid::ratio, 1, 20, true, FRatio, "④ 1:1〜∞:1。ニーは広い", "④ 1:1 up to limiting, soft knee") },
            { thr (0, -40, "④ Threshold。実機は下げるほど自動で Makeup がかかる", "④ Threshold. The hardware adds makeup automatically as it goes down") },
            { K ("RELEASE", pid::release, 100, 4000, true, FTime, "⑤ 0.1〜4 s", "⑤ 0.1 – 4 s") },
            { S ("ATTACK", vals (pid::attack, { { "Auto", 3 }, { "Fast", 1 } }), "⑤ 通常は約 3 ms、Fast で約 1 ms", "⑤ about 3 ms normally, about 1 ms with Fast") },
            { mk ("MAKEUP") } } });

        v.push_back ({ "crest", {
            { thr (0, -60, "④ Threshold", "④ Threshold") },
            { K ("RATIO", pid::ratio, 1, 100, true, FRatio, "④ Ratio", "④ Ratio") },
            { K ("PEAK CREST", pid::crest, 0, 12, false, FDb, "③ ピークの経路をどれだけ鈍くするか。上げるほど平均（RMS）で動く", "③ how far the peak path is held back. Higher = follows the average (RMS) more") },
            { K ("ATTACK", pid::attack, 0.02f, 300, true, FTime, "⑤ Attack", "⑤ Attack") },
            { K ("RELEASE PEAK", pid::release, 5, 5000, true, FTime, "⑤ ピークで下がった分の戻り（速く）", "⑤ recovery of what peaks took (fast)") },
            { K ("RELEASE RMS", pid::slowMult, 1, 20, true, FX, "⑤ 平均で下がった分の戻り（Peak の何倍か）", "⑤ recovery of what the average took (multiple of Peak)") },
            { mk ("MAKEUP") } } });

        v.push_back ({ "rider", {
            { K ("TARGET", pid::threshold, -40, 0, false, FMinusDb, "④ そろえたい音量。これより小さければ上げ、大きければ下げる", "④ the level to aim for: below it the gain goes up, above it down") },
            { K ("RANGE", pid::upRange, 0, 18, false, FDb, "④ 上げ下げの上限（±）", "④ the most it may move (±)") },
            { K ("IDLE BELOW", pid::upThr, -70, -20, false, FMinusDb, "④ これより小さいときはフェーダーを止める（ブレスやノイズを上げない）", "④ below this the fader holds still (no riding up breaths and noise)") },
            { K ("FADE DOWN", pid::attack, 5, 300, true, FTime, "⑤ 下げる速さ", "⑤ speed going down") },
            { K ("FADE UP", pid::release, 50, 2000, true, FTime, "⑤ 上げる速さ", "⑤ speed going up") } } });

        v.push_back ({ "dual", {
            { K ("OPTICAL THR", pid::preThr, 0, -40, false, FMinusDb, "⑥ 前段（光学 2:1）の Threshold。ゆっくり全体を均す", "⑥ threshold of the first stage (opto 2:1): slow levelling") },
            { K ("DISCRETE THR", pid::threshold, 0, -40, false, FMinusDb, "④ 後段（VCA）の Threshold。ピークを止める", "④ threshold of the second stage (VCA): catches peaks") },
            { S ("RATIO", vals (pid::ratio, { { "1.2", 1.2f }, { "2", 2 }, { "3", 3 }, { "4", 4 }, { "6", 6 }, { "Flood", 20 } }), "④ 後段の比", "④ ratio of the VCA stage"),
              S ("TRANSFORMER", vals (pid::outStage, { { "Nickel", (float) OutNickel }, { "Iron", (float) OutIron }, { "Steel", (float) OutSteel } }), "⑦ Nickel＝澄んだ高域、Iron＝低域が太い、Steel＝歪みが多い", "⑦ Nickel = clear top, Iron = thick lows, Steel = most colour") },
            { S ("ATTACK ms", vals (pid::attack, { { ".1", 0.1f }, { ".5", 0.5f }, { "1", 1 }, { "5", 5 }, { "10", 10 }, { "30", 30 } }), "⑤ 後段の Attack", "⑤ attack of the VCA stage"),
              S ("RECOVER s", { { ".1", { { pid::release, 100 }, { pid::timing, one } } }, { ".25", { { pid::release, 250 }, { pid::timing, one } } }, { ".5", { { pid::release, 500 }, { pid::timing, one } } },
                                { ".8", { { pid::release, 800 }, { pid::timing, one } } }, { "1.2", { { pid::release, 1200 }, { pid::timing, one } } }, { "Dual", { { pid::release, 150 }, { pid::timing, prog }, { pid::slowMult, 8 }, { pid::slowAmt, 50 } } } },
                 "⑤ Dual＝速い戻り＋遅い戻りの 2 段", "⑤ Dual = a fast and a slow recovery together") },
            { S ("SC FILTER", vals (pid::scHpf, { { "Off", 0 }, { "On", 3 } }), "② 検出器から 100 Hz 以下を外す", "② keeps everything below 100 Hz out of the detector") },
            { mk ("GAIN") } } });

        v.push_back ({ "spectral", {
            { thr (0, -60, "④ 周波数ごとの Threshold。山の高い帯域から先にかかる", "④ threshold for every frequency: the tallest peaks are reduced first") },
            { K ("RATIO", pid::ratio, 1, 20, true, FRatio, "④ Ratio", "④ Ratio") },
            { K ("KNEE", pid::knee, 0, 24, false, FDb, "④ 折れ曲がりの幅", "④ width of the bend") },
            { K ("ATTACK", pid::attack, 1, 300, true, FTime, "⑤ 帯域ごとの Attack（11 ms 刻みで更新）", "⑤ attack per band (updated every 11 ms)") },
            { K ("RELEASE", pid::release, 20, 5000, true, FTime, "⑤ 帯域ごとの Release", "⑤ release per band") },
            { K ("GR LIMIT", pid::grLimit, 1, 40, false, FDb, "④ 1 帯域あたりの GR の上限", "④ the most any one band may be reduced") },
            { K ("MIX", pid::mix, 0, 100, false, FPct, "⑦ 原音との割合", "⑦ balance with the dry signal") } } });
        v.push_back ({ "clipper", {
            { K ("DRIVE", pid::input, 0, 24, false, FDb, "① 入力ゲイン。上げるほど天井に深く当たる", "① input gain: more drive pushes further into the ceiling") },
            { K ("CEILING", pid::threshold, 0, -24, false, FMinusDb, "④ これを超えた波形の頭を切る。Attack も Release も無く、1 サンプルごとにゲインが決まる（＝時定数ゼロのコンプ）。TIME の GR が波形の山だけに出る", "④ everything above this is cut off. No attack, no release: the gain is set for every sample (a compressor with zero time constants). In TIME the GR appears only on the wave's crests") },
            { K ("SOFTNESS", pid::knee, 0, 24, false, FDb, "④ 0 dB＝ハードクリップ（角が立ち、高次倍音が多い）。上げると角が丸まり、倍音が低次に寄る（ソフトクリップ）", "④ 0 dB = hard clip (sharp corner, many high harmonics). Higher rounds the corner and keeps the harmonics low (soft clip)") },
            { S ("SHAPE", vals (pid::ratio, { { "Clip", 100 }, { "8:1", 8 }, { "4:1", 4 }, { "2:1", 2 } }), "④ Clip＝天井で完全に止める。比を下げると、超えた分を少し通すサチュレーターになる", "④ Clip = stops dead at the ceiling. Lower ratios let some of the excess through: a saturator"),
              S ("OVERSAMPLE", vals (pid::os, { { "1x", 0 }, { "2x", 1 }, { "4x", 2 } }), "⑥ クリップで生まれる高い倍音の折り返し（エイリアシング）を減らす。1x と 4x を SPECTRUM で比べる", "⑥ reduces the aliasing of the high harmonics clipping creates. Compare 1x and 4x in SPECTRUM") },
            { S ("STEREO", vals (pid::link, { { "Unlinked", 0 }, { "Linked", 100 } }), "② Linked＝左右を同じ量だけ下げる（定位が動かない）。Unlinked＝左右別々に切る（より大きくできる）", "② Linked = both sides reduced alike (the image stays put). Unlinked = each side clipped on its own (louder)"),
              S ("BANDS", vals (pid::mb, { { "Single", 0 }, { "3-band", 1 } }), "① 3-band＝帯域ごとに別々にクリップ。低域のクリップが高域を濁らせない", "① 3-band = each band clipped separately: clipping the lows no longer smears the highs") },
            { K ("MIX", pid::mix, 0, 100, false, FPct, "⑦ 原音との割合（パラレル）", "⑦ balance with the dry signal (parallel)") },
            { mk ("OUTPUT") } } });
        return v;
    }();
    return d;
}
inline const Def& forModel (const char* id) { for (auto& x : defs()) if (std::strcmp (x.model, id) == 0) return x; return defs().front(); }

// faceplate colours, after the hardware each model is patterned on: face, lettering, knob body, knob pointer, lit switch
struct Face { juce::uint32 face, ink, knob, ptr, on; };
inline Face faceFor (const char* id)
{
    static const std::pair<const char*, Face> f[] = {
        { "digital",  { 0xff20242b, 0xffe4e1da, 0xff2c3138, 0xff6fc3d6, 0xff6fc3d6 } },
        { "fet76",    { 0xff161718, 0xffdcdcd8, 0xff0b0b0c, 0xffffffff, 0xffdcdcd8 } },      // black face, silver lettering
        { "opto2a",   { 0xff9b9d99, 0xff1a1a1a, 0xff121212, 0xffffffff, 0xff1a1a1a } },      // grey face, black knobs
        { "mu670",    { 0xff383a3e, 0xffe8e2d0, 0xff17181a, 0xffe8e2d0, 0xffc8a24a } },      // dark grey hammertone, cream lettering
        { "busg",     { 0xff2a2d32, 0xffe2e2e2, 0xff3c4046, 0xffffffff, 0xff4f8fd0 } },      // console grey, blue cap
        { "vca160",   { 0xff131313, 0xffe6e6e6, 0xff2a2a2a, 0xffffffff, 0xffc0392b } },      // black, red LEDs
        { "diode254", { 0xff5d6d79, 0xfff2f2f2, 0xff2a3036, 0xffffffff, 0xffc63a2f } },      // blue-grey, red cap
        { "vca2500",  { 0xff15171b, 0xffe6e6e6, 0xff2b5fa8, 0xffffffff, 0xff2b5fa8 } },      // black, blue knobs
        { "el8",      { 0xff0f0f10, 0xfff0f0f0, 0xffeeeeee, 0xff101010, 0xfff2c230 } },      // black, white knobs, yellow LEDs
        { "opto1b",   { 0xff2d5ca0, 0xfff4f4f4, 0xff111111, 0xffffffff, 0xfff4f4f4 } },      // blue face
        { "ds1",      { 0xffc8cbcf, 0xff1a1a1a, 0xff2a2a2a, 0xffe8a020, 0xff1a1a1a } },      // brushed silver
        { "limiter",  { 0xff24272e, 0xffe4e1da, 0xff2c3138, 0xffe05a4a, 0xffe05a4a } },
        { "upward",   { 0xff1f2a26, 0xffe4e1da, 0xff2a3531, 0xff58c08a, 0xff58c08a } },
        { "ott",      { 0xff272c37, 0xffe6e9ee, 0xff343b48, 0xff4fb3e8, 0xff4fb3e8 } },
        { "opto3a",   { 0xff1a1a1b, 0xffd2d2d0, 0xff0c0c0d, 0xffffffff, 0xffd2d2d0 } },      // black, silver lettering
        { "chane",    { 0xffb6babe, 0xff1a1a1a, 0xff4a4e55, 0xffffffff, 0xff2f8a55 } },      // light console grey, green cap
        { "crest",    { 0xff22262c, 0xffe4e1da, 0xff2e333a, 0xffe8a23a, 0xffe8a23a } },
        { "rider",    { 0xff20282b, 0xffe4e1da, 0xff2a3438, 0xff5bc0a0, 0xff5bc0a0 } },
        { "dual",     { 0xff1b2921, 0xffe6dfc8, 0xff101211, 0xffe6dfc8, 0xffc8a24a } },      // dark green-black, cream lettering
        { "spectral", { 0xff211f2c, 0xffe6e2f0, 0xff2e2b3c, 0xffb48cf2, 0xffb48cf2 } },
        { "clipper",  { 0xff2b1f1b, 0xfff2e6de, 0xff3b2a25, 0xffff7a45, 0xffff7a45 } } };
    for (auto& x : f) if (std::strcmp (x.first, id) == 0) return x.second;
    return f[0].second;
}
inline juce::Colour onInk (juce::Colour on) { return on.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff141414) : juce::Colours::white; }
inline juce::StringArray ids (const char* model)                  // every parameter the model's panel can move
{
    juce::StringArray a;
    for (auto& s : forModel (model).slots) for (auto& k : s)
    {
        if (k.knob) a.addIfNotAlreadyThere (k.id);
        else for (auto& o : k.opts) for (auto& kv : o.set) a.addIfNotAlreadyThere (kv.first);
    }
    return a;
}

inline juce::String fmt (int f, float v)
{
    switch (f)
    {
        case FMinusDb: return minus (v) + " dB";
        case FTime:    return fmtTime (v);
        case FRatio:   return v >= 99.5f ? juce::String::fromUTF8 ("\xe2\x88\x9e : 1") : (v < 10 ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " : 1";
        case FPct:     return juce::String (juce::roundToInt (v)) + " %";
        case FX:       return juce::String (v, 1) + " x";
        case FUpRatio: return "1 : " + juce::String (v, 1);
        case FMs:      return juce::String (v, 1) + " ms";
        default:       return sgnDb (v);
    }
}
inline float legal (ParamHelp& ph, const char* id, float v) { if (auto* p = ph.proc.apvts.getParameter (id)) return p->getNormalisableRange().snapToLegalValue (v); return v; }
inline float modelDefault (ParamHelp& ph, const char* id, float fallback)
{
    const int m = ph.proc.currentModel(); if (m < 0 || m >= (int) mod::models().size()) return fallback;
    for (auto& kv : modelValues (mod::models()[(size_t) m].p)) if (std::strcmp (kv.first, id) == 0) return kv.second;
    return fallback;
}
} // namespace panel

// ---------------------------------------------------------------------------- a rotary control on the unit's own scale
inline void panelLabel (juce::Graphics& g, const juce::String& t, juce::Font f, juce::Colour face, juce::Colour ink, int cx, int y, int h)      // lettering on a pill of the faceplate colour: readable over what shows through
{
    const int w = (int) textWidth (f, t) + 12;
    g.setColour (face.withAlpha (0.86f)); g.fillRoundedRectangle ((float) (cx - w / 2), (float) y, (float) w, (float) h, 4.0f);
    g.setColour (ink); g.setFont (f); g.drawText (t, cx - w / 2, y, w, h, juce::Justification::centred);
}
class PanelKnob : public juce::Component
{
public:
    static constexpr int kHeight = 118;
    PanelKnob (ParamHelp& h, panel::Ctl ctl, panel::Face face, bool small = false) : ph (h), c (std::move (ctl)), F (face), mini (small) { getProperties().set ("pid", juce::String (c.id)); }
    float fracOf (float v) const { const float f = c.log ? std::log (std::max (v, 1.0e-6f) / c.lo) / std::log (c.hi / c.lo) : (v - c.lo) / (c.hi - c.lo); return juce::jlimit (0.0f, 1.0f, f); }
    float valueOf (float f) const { return c.log ? c.lo * std::pow (c.hi / c.lo, f) : c.lo + (c.hi - c.lo) * f; }
    void paint (juce::Graphics& g) override
    {
        const juce::Colour ink (F.ink), on (F.on), face (F.face);
        const float w = (float) getWidth(), cx = w * 0.5f, cy = mini ? 27.0f : 58.0f, R = mini ? 12.0f : 31.0f, ring = mini ? 3.5f : 5.0f;
        const float v = ph.value (c.id), f = fracOf (v); const bool locked = ph.proc.isLocked (c.id);
        if (mini) { g.setColour (ink); g.setFont (theme::mono (7.5f, true)); g.drawFittedText (c.cap, 0, 0, getWidth(), 11, juce::Justification::centred, 1, 0.6f); }
        else panelLabel (g, c.cap, theme::mono (9.5f, true), face, ink, (int) cx, 0, 15);
        const float a0 = -2.36f, a1 = 2.36f, a = a0 + (a1 - a0) * f;
        if (! mini) { g.setColour (face.withAlpha (0.80f)); g.fillEllipse (cx - R - 9.0f, cy - R - 9.0f, (R + 9.0f) * 2, (R + 9.0f) * 2); }
        juce::Path track; track.addCentredArc (cx, cy, R + ring, R + ring, 0.0f, a0, a1, true);
        g.setColour (ink.withAlpha (0.25f)); g.strokePath (track, juce::PathStrokeType (2.5f));
        juce::Path val; val.addCentredArc (cx, cy, R + ring, R + ring, 0.0f, a0, a, true);
        g.setColour (on.withAlpha (locked ? 0.45f : 1.0f)); g.strokePath (val, juce::PathStrokeType (2.5f));
        g.setColour (juce::Colour (F.knob)); g.fillEllipse (cx - R, cy - R, R * 2, R * 2);
        g.setColour (over || dragging ? on : ink.withAlpha (0.5f)); g.drawEllipse (cx - R, cy - R, R * 2, R * 2, 1.3f);
        g.setColour (juce::Colour (F.ptr)); g.drawLine (cx + std::sin (a) * R * 0.45f, cy - std::cos (a) * R * 0.45f, cx + std::sin (a) * (R - 3.0f), cy - std::cos (a) * (R - 3.0f), mini ? 1.8f : 2.6f);
        juce::String t = panel::fmt (c.fmt, v);
        if (! mini && c.nLo != c.nHi) t = (c.nHi > 20 ? juce::String (juce::roundToInt (c.nLo + (c.nHi - c.nLo) * f)) : juce::String (c.nLo + (c.nHi - c.nLo) * f, 1)) + juce::String::fromUTF8 ("  (\xe2\x89\x88 ") + t + ")";
        if (mini) { g.setColour (ink); g.setFont (theme::mono (8.5f, true)); g.drawFittedText (t, 0, 44, getWidth(), 12, juce::Justification::centred, 1, 0.7f); }
        else panelLabel (g, t, theme::mono (10.5f, true), face, ink, (int) cx, 101, 16);
        if (locked) drawLock (g, cx + R + (mini ? 2.0f : 8.0f), cy - R - 2.0f);
    }
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { ph.proc.setLocked (c.id, ! ph.proc.isLocked (c.id)); repaint(); return; }
        if (ph.proc.isLocked (c.id)) return;
        ph.proc.undo.beginNewTransaction();
        if (e.mods.isAltDown()) { reset(); return; }
        dragging = true; start = fracOf (ph.value (c.id)); lastY = e.getScreenPosition().toFloat().y; ph.begin (c.id);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const float y = e.getScreenPosition().toFloat().y;
        start = juce::jlimit (0.0f, 1.0f, start + (lastY - y) * (e.mods.isShiftDown() ? 0.0008f : 0.005f)); lastY = y;
        ph.set (c.id, panel::legal (ph, c.id, valueOf (start))); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragging) { dragging = false; ph.end (c.id); } }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (! ph.proc.isLocked (c.id)) { ph.proc.undo.beginNewTransaction(); reset(); } }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (ph.proc.isLocked (c.id) || std::abs (w.deltaY) < 1.0e-4f) return;
        const float f = juce::jlimit (0.0f, 1.0f, fracOf (ph.value (c.id)) + (e.mods.isShiftDown() ? 0.002f : 0.01f) * (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f));
        ph.setGesture (c.id, panel::legal (ph, c.id, valueOf (f))); repaint();
    }
private:
    void reset() { ph.setGesture (c.id, panel::legal (ph, c.id, panel::modelDefault (ph, c.id, ph.value (c.id)))); repaint(); }
    ParamHelp& ph; panel::Ctl c; panel::Face F; float start = 0, lastY = 0; bool over = false, dragging = false, mini = false;
};

// ---------------------------------------------------------------------------- a stepped switch: every position sets one or more parameters
class PanelSteps : public juce::Component
{
public:
    static constexpr int kHeight = 44;
    PanelSteps (ParamHelp& h, panel::Ctl ctl, panel::Face face, bool small = false) : ph (h), c (std::move (ctl)), F (face), mini (small) {}
    static int naturalWidth (const panel::Ctl& k)
    {
        int chars = 1; for (auto& o : k.opts) chars = std::max (chars, juce::String::fromUTF8 (o.label).length());
        return (int) k.opts.size() * std::max (32, chars * 6 + 14) + 8;
    }
    int active() const
    {
        for (size_t i = 0; i < c.opts.size(); ++i)
        {
            bool all = true;
            for (auto& kv : c.opts[i].set) if (std::abs (ph.value (kv.first) - kv.second) > 0.006f * std::abs (kv.second) + 0.011f) { all = false; break; }
            if (all) return (int) i;
        }
        return -1;
    }
    static float optW (const panel::Ctl& k, size_t i) { return std::max (20.0f, (float) juce::String::fromUTF8 (k.opts[i].label).length() * 5.6f + 9.0f); }
    static int miniWidth (const panel::Ctl& k) { float w = 0; for (size_t i = 0; i < k.opts.size(); ++i) w += optW (k, i); return (int) w + 2; }
    juce::Rectangle<float> row() const
    {
        if (mini) { const float w = std::min ((float) miniWidth (c), (float) getWidth()); return { ((float) getWidth() - w) * 0.5f, 13.0f, w, 20.0f }; }
        const float w = std::min ((float) naturalWidth (c), (float) getWidth()); return { ((float) getWidth() - w) * 0.5f, 18.0f, w, 24.0f };
    }
    juce::Rectangle<float> optRect (int i) const                       // uniform on the panel; sized to its label in the small version
    {
        const auto r = row(); const int n = (int) c.opts.size();
        if (! mini) { const float bw = r.getWidth() / (float) n; return { r.getX() + (float) i * bw, r.getY(), bw, r.getHeight() }; }
        const float k = r.getWidth() / (float) miniWidth (c); float x = r.getX(); for (int j = 0; j < i; ++j) x += optW (c, (size_t) j) * k;
        return { x, r.getY(), optW (c, (size_t) i) * k, r.getHeight() };
    }
    void paint (juce::Graphics& g) override
    {
        const juce::Colour ink (F.ink), on (F.on), face (F.face);
        if (mini) { g.setColour (ink); g.setFont (theme::mono (7.5f, true)); g.drawFittedText (c.cap, 0, 0, getWidth(), 11, juce::Justification::centred, 1, 0.6f); }
        else panelLabel (g, c.cap, theme::mono (9.5f, true), face, ink, getWidth() / 2, 0, 15);
        const auto r = row(); const int n = (int) c.opts.size(), sel = active();
        g.setColour (face.darker (0.55f)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (ink.withAlpha (0.35f)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        const juce::Colour offInk = face.darker (0.55f).getPerceivedBrightness() > 0.5f ? juce::Colour (0xff141414) : juce::Colour (0xffe8e8e8);
        for (int i = 0; i < n; ++i)
        {
            const auto b = optRect (i).reduced (mini ? 1.5f : 2.0f);
            if (i == sel) { g.setColour (on); g.fillRoundedRectangle (b, 3.0f); }
            g.setColour (i == sel ? panel::onInk (on) : offInk); g.setFont (theme::ui (mini ? 9.0f : 10.5f, i == sel));
            g.drawText (juce::String::fromUTF8 (c.opts[(size_t) i].label), b, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto r = row(); if (! r.contains (e.position) || e.mods.isPopupMenu()) return;
        int i = 0; for (int j = 0; j < (int) c.opts.size(); ++j) if (optRect (j).contains (e.position)) i = j;
        ph.proc.undo.beginNewTransaction();
        for (auto& kv : c.opts[(size_t) i].set) if (! ph.proc.isLocked (kv.first)) ph.setGesture (kv.first, panel::legal (ph, kv.first, kv.second));
        repaint();
    }
private:
    ParamHelp& ph; panel::Ctl c; panel::Face F; bool mini = false;
};

// ---------------------------------------------------------------------------- the panel: a faceplate over the signal flow, opaque or see-through
class PanelView : public juce::Component
{
public:
    explicit PanelView (ParamHelp& h) : ph (h) {}
    void refresh()
    {
        int m = ph.proc.currentModel(); if (m < 0 || m >= (int) mod::models().size()) m = 0;
        if (m != builtFor || getWidth() != builtW) build (m);
        const float target = ph.proc.isFrozen() ? ph.proc.pvGrPk : ph.proc.grLast.load();
        vu += (target - vu) * (target > vu ? 0.5f : 0.18f);
        repaint();
    }
    void resized() override { builtFor = -2; }
    void mouseDown (const juce::MouseEvent& e) override { if (const int t = hitViewBtn (e.getPosition(), getWidth()); t >= 0) setViewMode (ph.proc.uiState, t); }
    void paint (juce::Graphics& g) override
    {
        const auto& M = mod::models()[(size_t) juce::jlimit (0, (int) mod::models().size() - 1, builtFor)];
        const auto F = panel::faceFor (M.id); const juce::Colour face (F.face), ink (F.ink);
        const int mode = viewModeOf (ph.proc.uiState); static const float al[] { 1.0f, 0.86f, 0.70f, 0.52f, 1.0f };
        auto r = getLocalBounds().toFloat();
        g.setColour (face.withAlpha (al[mode])); g.fillRoundedRectangle (r, 6.0f);                    // the signal flow (BUILD) shows through from underneath
        const juce::Colour head (0xff15171b), headInk (0xffe6e3dc);                                   // the title strip is the same on every faceplate
        g.setColour (head); g.fillRoundedRectangle (r.withHeight (26.0f), 6.0f); g.fillRect (r.getX(), 14.0f, r.getWidth(), 12.0f);
        g.setColour (ink.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        for (float x : { 16.0f, r.getRight() - 16.0f }) for (float y : { 38.0f, r.getBottom() - 12.0f })
        { g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f); g.setColour (ink.withAlpha (0.35f)); g.drawEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f, 1.0f); }
        g.setColour (headInk); g.setFont (theme::mono (12.0f, true)); const int nw = (int) textWidth (theme::mono (12.0f, true), M.name) + 4;
        g.drawText (M.name, 12, 4, nw, 18, juce::Justification::centredLeft);
        g.setColour (headInk.withAlpha (0.65f)); g.setFont (theme::ui (9.5f));
        g.drawText (juce::String (M.ref) + ph.tr ("　｜　実機にある操作子を実機の目盛りで表示。ポインターを置くと、内部で動くブロック（①〜⑦）を説明欄に表示。右クリックでロック、ダブルクリックで初期値",
                                                  "   |   The hardware's controls on its own scales. Hover one to read which block (① – ⑦) it moves. Right-click locks, double-click resets"),
                    12 + nw + 10, 6, getWidth() - 12 - nw - 10 - 244, 14, juce::Justification::centredLeft);
        drawViewBtns (g, getWidth(), mode, ph.ja());

        // GR meter
        const auto v = vuRect().toFloat();
        g.setColour (juce::Colour (0xffe9dfc4)); g.fillRoundedRectangle (v, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (v, 4.0f, 1.0f);
        const float px = v.getCentreX(), py = v.getBottom() + 26.0f, rad = 118.0f, span = 0.50f;
        auto ang = [span] (float gr) { return span - juce::jlimit (0.0f, 20.0f, gr) / 20.0f * 2.0f * span; };          // 0 dB at the right, 20 dB at the left
        const juce::Colour dark (0xff3a342a);
        juce::Path arc; arc.addCentredArc (px, py, rad, rad, 0.0f, -span, span, true); g.setColour (dark); g.strokePath (arc, juce::PathStrokeType (1.4f));
        g.setFont (theme::mono (8.5f));
        for (int t : { 0, 3, 6, 10, 15, 20 })
        {
            const float a = ang ((float) t), sx = std::sin (a), cxn = -std::cos (a);
            g.drawLine (px + sx * rad, py + cxn * rad, px + sx * (rad + 5.0f), py + cxn * (rad + 5.0f), 1.2f);
            g.drawText (juce::String (t), (int) (px + sx * (rad + 13.0f)) - 10, (int) (py + cxn * (rad + 13.0f)) - 5, 20, 10, juce::Justification::centred);
        }
        g.setFont (theme::mono (9.5f, true)); g.drawText ("GR " + minus (-vu) + " dB", (int) v.getX(), (int) v.getBottom() - 18, (int) v.getWidth(), 12, juce::Justification::centred);
        { juce::Graphics::ScopedSaveState s (g); g.reduceClipRegion (v.toNearestInt().reduced (2));
          const float a = ang (vu); g.setColour (juce::Colour (0xffb83f36)); g.drawLine (px, py, px + std::sin (a) * (rad + 2.0f), py - std::cos (a) * (rad + 2.0f), 1.8f); }
    }
private:
    juce::Rectangle<int> vuRect() const { return { getWidth() - 162, 62, 144, 124 }; }
    void build (int m)
    {
        ctls.clear(); builtFor = m; builtW = getWidth();
        const auto& def = panel::forModel (mod::models()[(size_t) m].id); const auto F = panel::faceFor (mod::models()[(size_t) m].id);
        std::vector<int> nat; int total = 0;
        for (auto& s : def.slots)
        {
            int w = 132; for (auto& k : s) if (! k.knob) w = std::max (w, PanelSteps::naturalWidth (k) + 8);
            nat.push_back (w); total += w;
        }
        const int x0 = 30, avail = getWidth() - 162 - 14 - x0, n = (int) def.slots.size(), top = 30, area = getHeight() - top - 8;
        const float scale = total > avail ? (float) avail / (float) total : 1.0f;
        const float gap = total > avail ? 0.0f : std::min (40.0f, (float) (avail - total) / (float) std::max (1, n));
        float x = (float) x0 + ((float) avail - (float) total * scale - gap * (float) (n - 1)) * 0.5f;
        for (size_t i = 0; i < def.slots.size(); ++i)
        {
            const int w = (int) ((float) nat[i] * scale); const auto& s = def.slots[i];
            for (size_t k = 0; k < s.size(); ++k)
            {
                juce::Component* comp;
                if (s[k].knob) { comp = ctls.add (new PanelKnob (ph, s[k], F)); comp->setBounds ((int) x, top + (area - PanelKnob::kHeight) / 2, w, PanelKnob::kHeight); }
                else
                {
                    const int hh = PanelSteps::kHeight, pitch = 76, y0 = top + (area - (hh + pitch * ((int) s.size() - 1))) / 2;
                    comp = ctls.add (new PanelSteps (ph, s[k], F)); comp->setBounds ((int) x, y0 + (int) k * pitch, w, hh);
                }
                setHelp (*comp, s[k].cap, s[k].ja, s[k].en);
                addAndMakeVisible (comp);
            }
            x += (float) w + gap;
        }
    }
    ParamHelp& ph; juce::OwnedArray<juce::Component> ctls; int builtFor = -2, builtW = 0; float vu = 0;
};

// ---------------------------------------------------------------------------- the same controls, small: shown in the lower-left view while the middle row is on BUILD
class PanelMini : public juce::Component
{
public:
    explicit PanelMini (ParamHelp& h) : ph (h) {}
    void refresh()
    {
        int m = ph.proc.currentModel(); if (m < 0 || m >= (int) mod::models().size()) m = 0;
        if (m != builtFor || getWidth() != builtW) build (m);
        repaint();
    }
    void resized() override { builtFor = -2; }
    void mouseDown (const juce::MouseEvent& e) override { if (const int t = hitLowerTab (e.getPosition()); t >= 0) ph.proc.uiState.setProperty ("lowerTab", t, nullptr); }
    void paint (juce::Graphics& g) override
    {
        paintWell (g, getLocalBounds().toFloat());
        drawLowerTabs (g, 2);
        const auto& M = mod::models()[(size_t) juce::jlimit (0, (int) mod::models().size() - 1, builtFor)];
        const auto F = panel::faceFor (M.id);
        g.setColour (theme::onWell()); g.setFont (theme::mono (9.0f, true)); g.drawText (M.name, 166, 5, getWidth() - 174, 15, juce::Justification::centredRight);
        const auto r = juce::Rectangle<float> (6.0f, 25.0f, (float) getWidth() - 12.0f, (float) getHeight() - 31.0f);
        g.setColour (juce::Colour (F.face)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (F.ink).withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }
private:
    void build (int m)
    {
        ctls.clear(); builtFor = m; builtW = getWidth();
        const auto& def = panel::forModel (mod::models()[(size_t) m].id); const auto F = panel::faceFor (mod::models()[(size_t) m].id);
        struct Item { juce::Component* c; int w, h; };
        std::vector<Item> items;
        for (int pass = 0; pass < 2; ++pass)                               // knobs first, then the switches: fewer, fuller rows
            for (auto& s : def.slots) for (auto& k : s)
            {
                if (k.knob != (pass == 0)) continue;
                juce::Component* comp = k.knob ? (juce::Component*) ctls.add (new PanelKnob (ph, k, F, true)) : (juce::Component*) ctls.add (new PanelSteps (ph, k, F, true));
                setHelp (*comp, k.cap, k.ja, k.en); addAndMakeVisible (comp);
                items.push_back ({ comp, k.knob ? 48 : std::max (40, PanelSteps::miniWidth (k) + 4), k.knob ? 58 : 34 });
            }
        const int maxW = getWidth() - 24, gap = 5, vgap = 5;
        std::vector<std::vector<Item>> rows; int rw = 0;
        for (auto& it : items)
        {
            if (rows.empty() || rw + gap + it.w > maxW) { rows.push_back ({}); rw = -gap; }
            rows.back().push_back (it); rw += gap + it.w;
        }
        int total = -vgap; std::vector<int> rh;
        for (auto& row : rows) { int h = 0; for (auto& it : row) h = std::max (h, it.h); rh.push_back (h); total += h + vgap; }
        int y = 27 + std::max (0, (getHeight() - 35 - total) / 2);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            int w = -gap; for (auto& it : rows[i]) w += it.w + gap;
            int x = 12 + std::max (0, (maxW - w) / 2);
            for (auto& it : rows[i]) { it.c->setBounds (x, y + (rh[i] - it.h) / 2, it.w, it.h); x += it.w + gap; }
            y += rh[i] + vgap;
        }
    }
    ParamHelp& ph; juce::OwnedArray<juce::Component> ctls; int builtFor = -2, builtW = 0;
};
