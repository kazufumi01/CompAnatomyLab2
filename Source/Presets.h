#pragma once
#include "Params.h"

// =============================================================================
//  Factory presets: a model + a few changed values + a note. Each has a use, so the same list
//  is offered two ways: by model, and by use across models.
//  Levels depend on the material: every note says how much gain reduction to aim for.
// =============================================================================
namespace preset
{
enum Use { Drums = 0, Bass, Vocal, GtrKeys, MixBus, Master, Creative, NumUses };
inline const char* useName (int u, bool ja)
{
    static const char* j[] { "ドラム", "ベース", "ボーカル", "ギター・鍵盤", "ミックスバス", "マスター", "音作り・特殊" };
    static const char* e[] { "Drums", "Bass", "Vocal", "Guitar & keys", "Mix bus", "Master", "Sound design" };
    return (ja ? j : e)[juce::jlimit (0, (int) NumUses - 1, u)];
}
struct Def { const char* model; int use; const char* en; const char* ja; const char* noteJa; const char* noteEn; std::vector<std::pair<const char*, float>> set; };

inline const std::vector<Def>& all()
{
    static const std::vector<Def> v = {
        // ---- Digital Precision
        { "digital", Vocal, "Transparent leveling", "透明なレベリング", "GR 2〜4 dB。色を付けずに音量だけを整える。Threshold を素材に合わせる", "Aim for 2–4 dB GR. Level control without colour; set Threshold to the material",
          { { "ratio", 2.5f }, { "knee", 12 }, { "attack", 5 }, { "release", 150 }, { "lookahead", 2 }, { "hold", 5 }, { "threshold", -24 } } },
        { "digital", MixBus, "Clean bus glue", "クリーンなバスのまとまり", "GR 1〜2 dB。遅い Attack で打点を残す", "1–2 dB GR. Slow attack keeps the transients",
          { { "ratio", 2 }, { "knee", 10 }, { "attack", 30 }, { "release", 200 }, { "lookahead", 0 }, { "hold", 0 }, { "threshold", -18 } } },
        { "digital", Drums, "Peak catcher", "ピークだけ止める", "GR はピークで 3〜6 dB。先読みで打点の頭から抑える", "3–6 dB GR on peaks. Look-ahead catches the very start of the hit",
          { { "ratio", 8 }, { "knee", 2 }, { "attack", 0.05f }, { "release", 60 }, { "lookahead", 3 }, { "hold", 3 }, { "threshold", -16 } } },
        // ---- FET 76
        { "fet76", Vocal, "Vocal up-front", "ボーカルを前に", "GR 3〜7 dB。Input で押し込む量を決める", "3–7 dB GR. Input sets how hard you push",
          { { "ratio", 4 }, { "attack", 0.4f }, { "release", 200 }, { "input", 6 } } },
        { "fet76", Drums, "Snare crack", "スネアの打点", "GR 3〜5 dB。Attack は最も遅い 0.8 ms で打点を通す", "3–5 dB GR. Slowest attack (0.8 ms) lets the crack through",
          { { "ratio", 4 }, { "attack", 0.8f }, { "release", 60 }, { "input", 8 } } },
        { "fet76", Drums, "All-buttons room", "全押しルーム", "GR 10 dB 以上。Mix 50% のパラレルで原音に混ぜる", "10 dB+ GR. Blended in parallel at 50%",
          { { "ratio", 100 }, { "attack", 0.3f }, { "release", 50 }, { "input", 10 }, { "mix", 50 } } },
        { "fet76", Bass, "Bass pin", "ベースを固定", "GR 4〜8 dB。8:1 で音量のばらつきを止める", "4–8 dB GR. 8:1 pins the level",
          { { "ratio", 8 }, { "attack", 0.6f }, { "release", 250 }, { "input", 6 } } },
        // ---- Opto 2A
        { "opto2a", Vocal, "Vocal smooth", "なめらかなボーカル", "GR 3〜6 dB。持続音ほど戻りが遅くなる（光学メモリー）", "3–6 dB GR. Sustained notes recover more slowly (opto memory)", { { "input", 6 } } },
        { "opto2a", Bass, "Bass level", "ベースのレベリング", "GR 4〜7 dB。SC HPF で最低域への反応を減らす", "4–7 dB GR. SC HPF reduces reaction to the very low end", { { "input", 8 }, { "schpf", 2 } } },
        { "opto2a", Vocal, "Limit mode", "Limit モード", "比 10:1。ピークを強めに止める", "10:1: firmer peak control", { { "ratio", 10 }, { "input", 4 } } },
        // ---- Vari-Mu 670
        { "mu670", MixBus, "Mix bus glue (TC 2)", "ミックスバスのまとまり（TC 2）", "GR 1〜3 dB。0.2 ms / 0.8 s", "1–3 dB GR. 0.2 ms / 0.8 s",
          { { "attack", 0.2f }, { "release", 800 }, { "threshold", -14 }, { "ratio", 4 } } },
        { "mu670", Drums, "Drum bus weight (TC 1)", "ドラムバスの重さ（TC 1）", "GR 3〜5 dB。0.2 ms / 0.3 s", "3–5 dB GR. 0.2 ms / 0.3 s",
          { { "attack", 0.2f }, { "release", 300 }, { "threshold", -18 }, { "ratio", 6 }, { "input", 3 } } },
        { "mu670", Master, "Master gentle (TC 3)", "マスターにやさしく（TC 3）", "GR 1〜2 dB。0.4 ms / 2 s", "1–2 dB GR. 0.4 ms / 2 s",
          { { "attack", 0.4f }, { "release", 2000 }, { "threshold", -12 }, { "ratio", 3 } } },
        // ---- Bus VCA G
        { "busg", MixBus, "2-bus glue 4:1 auto", "2ミックスのまとまり 4:1 Auto", "GR 2〜4 dB。メーターが曲のリズムで揺れる程度", "2–4 dB GR. The meter should breathe with the song",
          { { "ratio", 4 }, { "attack", 10 }, { "timing", 3 }, { "threshold", -14 } } },
        { "busg", Drums, "Drum bus punch", "ドラムバスのパンチ", "GR 3〜6 dB。Attack 30 ms で打点を残し、Release 100 ms で次の音までに戻す", "3–6 dB GR. 30 ms attack keeps the hit, 100 ms release recovers before the next",
          { { "ratio", 4 }, { "attack", 30 }, { "release", 100 }, { "threshold", -20 } } },
        { "busg", Master, "Master 2:1", "マスター 2:1", "GR 1〜2 dB", "1–2 dB GR", { { "ratio", 2 }, { "attack", 30 }, { "release", 300 }, { "threshold", -12 }, { "knee", 10 } } },
        // ---- VCA 160
        { "vca160", Drums, "Kick punch", "キックのパンチ", "GR 3〜6 dB。RMS 検出が打点を素通りさせる", "3–6 dB GR. The RMS detector lets the click through", { { "ratio", 4 }, { "threshold", -18 } } },
        { "vca160", Drums, "Snare smack", "スネアを叩きつける", "GR 5〜8 dB", "5–8 dB GR", { { "ratio", 6 }, { "threshold", -22 } } },
        { "vca160", Bass, "Bass tight", "ベースを締める", "GR 4〜6 dB", "4–6 dB GR", { { "ratio", 4 }, { "threshold", -24 } } },
        // ---- Diode 254
        { "diode254", Drums, "Drum bus glue", "ドラムバスのまとまり", "GR 2〜4 dB。奇数次の歪みで太くなる", "2–4 dB GR. Odd harmonics thicken it", { { "ratio", 3 }, { "attack", 5 }, { "release", 100 }, { "threshold", -20 } } },
        { "diode254", MixBus, "Mix bus (auto)", "ミックスバス（Auto）", "GR 1〜3 dB", "1–3 dB GR", { { "ratio", 2 }, { "timing", 3 }, { "threshold", -14 } } },
        { "diode254", GtrKeys, "Piano / keys body", "ピアノ・鍵盤の厚み", "GR 3〜5 dB", "3–5 dB GR", { { "ratio", 4 }, { "release", 400 }, { "threshold", -22 } } },
        // ---- VCA 2500
        { "vca2500", Drums, "Drum bus (tilt loud)", "ドラムバス（Tilt Loud）", "GR 3〜5 dB。Tilt Loud でキックに潰されにくくなる", "3–5 dB GR. Tilt Loud stops the kick from pumping the bus",
          { { "sctilt", 2 }, { "ratio", 4 }, { "attack", 3 }, { "release", 100 }, { "knee", 0 }, { "threshold", -20 } } },
        { "vca2500", MixBus, "Mix bus (feedback)", "ミックスバス（Feedback）", "GR 1〜3 dB。Feedback でなめらかに", "1–3 dB GR. Feedback makes it smoother",
          { { "topology", 1 }, { "ratio", 2 }, { "attack", 10 }, { "release", 300 }, { "knee", 12 }, { "sctilt", 1 }, { "threshold", -14 } } },
        { "vca2500", Drums, "Parallel smash", "パラレルで潰す", "GR 10 dB 以上を Mix 40% で混ぜる", "10 dB+ GR, blended at 40%",
          { { "ratio", 10 }, { "attack", 0.3f }, { "release", 50 }, { "knee", 0 }, { "mix", 40 }, { "threshold", -30 } } },
        // ---- EL 8
        { "el8", Vocal, "Vocal 6:1 warm", "ボーカル 6:1（2次の暖かさ）", "GR 4〜8 dB", "4–8 dB GR", { { "ratio", 6 }, { "attack", 3 }, { "release", 150 }, { "outstage", 3 } } },
        { "el8", Drums, "Drum room nuke", "ドラムルームを破壊", "GR 15 dB 以上を Mix 60% で", "15 dB+ GR at 60% mix",
          { { "ratio", 100 }, { "knee", 0 }, { "attack", 0.5f }, { "release", 80 }, { "outstage", 4 }, { "mix", 60 }, { "threshold", -30 } } },
        { "el8", Bass, "Bass 4:1 + 3rd", "ベース 4:1＋3次", "GR 4〜6 dB。3次倍音で小さいスピーカーでも聞こえる", "4–6 dB GR. The 3rd harmonic helps on small speakers",
          { { "ratio", 4 }, { "attack", 8 }, { "release", 200 }, { "outstage", 4 }, { "schpf", 2 } } },
        { "el8", Vocal, "Opto-style 10:1", "光学風 10:1", "GR 3〜6 dB。遅い Attack と長い Release", "3–6 dB GR. Slow attack, long release",
          { { "ratio", 10 }, { "knee", 12 }, { "attack", 30 }, { "release", 600 }, { "timing", 0 } } },
        // ---- Tube Opto 1B
        { "opto1b", Vocal, "Vocal modern", "現代的なボーカル", "GR 3〜6 dB", "3–6 dB GR", { { "ratio", 4 }, { "attack", 2 }, { "release", 150 }, { "threshold", -22 } } },
        { "opto1b", GtrKeys, "Acoustic guitar", "アコースティックギター", "GR 2〜4 dB", "2–4 dB GR", { { "ratio", 3 }, { "attack", 15 }, { "release", 300 }, { "threshold", -20 } } },
        { "opto1b", Bass, "Bass smooth", "なめらかなベース", "GR 4〜6 dB", "4–6 dB GR", { { "ratio", 5 }, { "attack", 10 }, { "release", 400 } } },
        // ---- Digital DS-1
        { "ds1", Vocal, "De-esser 7 kHz", "ディエッサー 7 kHz", "歯擦音で GR 3〜6 dB。Band only なので他の帯域は変わらない。SC LISTEN で帯域を確認", "3–6 dB GR on esses. Band only leaves the rest untouched; check the band with SC LISTEN",
          { { "band", 3 }, { "bandhz", 7000 }, { "bandq", 1.2f }, { "bandonly", 1 }, { "ratio", 4 }, { "attack", 0.5f }, { "release", 40 }, { "slowmult", 3 }, { "threshold", -34 } } },
        { "ds1", Master, "Master gentle 1.5:1", "マスター 1.5:1", "GR 1〜2 dB。持続音は遅く、瞬間の音は速く戻る", "1–2 dB GR. Sustained sound recovers slowly, transients fast",
          { { "ratio", 1.5f }, { "knee", 8 }, { "attack", 10 }, { "release", 200 }, { "slowmult", 10 }, { "threshold", -16 } } },
        { "ds1", MixBus, "Parallel bus", "パラレルバス", "深く潰した音を 30% 混ぜる", "Heavily compressed signal blended at 30%",
          { { "ratio", 6 }, { "threshold", -30 }, { "mix", 30 }, { "attack", 3 }, { "release", 100 } } },
        // ---- Limiter
        { "limiter", Master, "Master limiter -1 dB", "マスターリミッター −1 dB", "Input で音量を上げる。GR は 2〜4 dB までが目安", "Raise Input for loudness. Keep GR within 2–4 dB", { { "threshold", -1 }, { "input", 6 }, { "release", 80 } } },
        { "limiter", Drums, "Track peak stop", "トラックのピーク止め", "ピークだけを 1〜3 dB 止める", "Stops only the peaks by 1–3 dB", { { "threshold", -6 }, { "input", 0 }, { "release", 40 } } },
        { "limiter", Master, "Loud, slow release", "大きく・遅い Release", "歪みにくいが、戻りが遅い分だけ音量感は下がる", "Less distortion, but the slow recovery costs loudness", { { "threshold", -1 }, { "input", 8 }, { "release", 300 } } },
        // ---- Upward
        { "upward", GtrKeys, "Bring up detail", "細部を持ち上げる", "小さい音だけを最大 12 dB 上げる。ピークは変わらない", "Raises only the quiet parts by up to 12 dB; peaks are untouched", { { "upthr", -40 }, { "upratio", 2 }, { "uprange", 12 } } },
        { "upward", Drums, "Room / ambience lift", "ルーム・余韻を持ち上げる", "余韻を最大 18 dB 上げる。ノイズも上がるので注意", "Lifts tails by up to 18 dB — noise comes up too", { { "upthr", -35 }, { "upratio", 3 }, { "uprange", 18 } } },
        { "upward", Vocal, "Vocal body", "ボーカルの語尾", "語尾や息を最大 9 dB 上げる", "Lifts word endings and breaths by up to 9 dB", { { "upthr", -30 }, { "upratio", 1.5f }, { "uprange", 9 } } },
        // ---- OTT
        { "ott", Creative, "OTT full", "OTT 100%", "3 バンドで上下から圧縮。派手で明るくなる", "Three bands, squeezed from above and below: bright and aggressive", { { "mix", 100 } } },
        { "ott", Creative, "OTT 30% (synths)", "OTT 30%（シンセ）", "Mix 30% が定番。高域の細部が出る", "30% mix is the classic setting: brings out high detail", { { "mix", 30 } } },
        { "ott", Drums, "OTT drums 50%", "OTT ドラム 50%", "ドラムの余韻と高域を持ち上げる", "Lifts drum tails and highs", { { "mix", 50 }, { "xlo", 100 } } },
        // ---- Opto 3A
        { "opto3a", GtrKeys, "Guitar mid-forward", "ギターを前に", "GR 3〜6 dB。光学だが立ち上がりが速く、中域が前に出る", "3–6 dB GR. Opto, but quick to grab: mid-forward", { { "input", 6 } } },
        { "opto3a", Vocal, "Vocal fast opto + HF", "ボーカル：速い光学＋高域検出", "GR 3〜5 dB。Tilt で歯擦音に反応しやすくする（HF Emphasis）", "3–5 dB GR. Tilt makes it react to sibilance (HF emphasis)", { { "input", 6 }, { "sctilt", 1 } } },
        // ---- Channel VCA E
        { "chane", Drums, "Channel: drums, fast attack", "チャンネル：ドラム（Fast Attack）", "GR 4〜8 dB。Attack 1 ms＋短い Release でルームが膨らむ", "4–8 dB GR. 1 ms attack and a short release make the room swell", { { "attack", 1 }, { "release", 100 }, { "ratio", 6 } } },
        { "chane", Vocal, "Channel: vocal, slow attack", "チャンネル：ボーカル（Slow Attack）", "GR 3〜5 dB", "3–5 dB GR", { { "attack", 10 }, { "release", 400 }, { "ratio", 3 } } },
        // ---- Peak + RMS
        { "crest", Master, "Master: RMS leads", "マスター：RMS 主導", "GR 1〜2 dB。Crest 6 dB で、打点はほとんど触らず平均だけを抑える", "1–2 dB GR. Crest 6 dB: the average is controlled, transients barely touched", { { "crest", 6 }, { "ratio", 2 }, { "threshold", -18 }, { "release", 80 }, { "slowmult", 6 } } },
        { "crest", MixBus, "Bus: peaks and body", "バス：打点と胴の両方", "GR 2〜4 dB。Crest 3 dB で、ピーク経路も働く", "2–4 dB GR. Crest 3 dB: the peak path works too", { { "crest", 3 }, { "ratio", 3 }, { "threshold", -20 } } },
        // ---- Rider (gain riding) and stereo modes
        { "rider", Vocal, "Vocal rider +/-9 dB", "ボーカルライダー ±9 dB", "圧縮せずに音量だけを目標へ寄せる。TARGET を歌の平均に合わせる。音色は変わらない。後段に軽いコンプを足すと定番の形", "Moves the level to the target without compressing. Set TARGET to the average of the vocal; the tone is unchanged. A light compressor after it is the classic chain",
          { { "threshold", -20 }, { "uprange", 9 }, { "upthr", -45 }, { "attack", 60 }, { "release", 300 } } },
        { "rider", Bass, "Bass rider +/-6 dB, slow", "ベースライダー ±6 dB（ゆっくり）", "音ごとの音量差をゆっくり均す。アタックの形は変わらない", "Evens out note-to-note level slowly; the attack shape is untouched",
          { { "threshold", -18 }, { "uprange", 6 }, { "upthr", -40 }, { "attack", 120 }, { "release", 600 } } },
        { "digital", Master, "Mid only: hold the centre", "Mid だけ：センターを抑える", "GR 1〜2 dB。Mid（中央）だけを圧縮し、Side（広がり）は変えない。広がって聞こえる", "1–2 dB GR. Compresses only the mid; the side is untouched, so the mix sounds wider",
          { { "stereo", 2 }, { "ratio", 2 }, { "knee", 10 }, { "attack", 20 }, { "release", 200 }, { "threshold", -18 }, { "lookahead", 0 }, { "hold", 0 } } },
        { "ds1", Master, "Side only: tame wide highs", "Side だけ：広がった高域を抑える", "Side の 6 kHz 以上だけを抑える。センターのボーカルは変わらない", "Tames only the side above 6 kHz; the centre vocal is untouched",
          { { "stereo", 3 }, { "band", 2 }, { "bandhz", 6000 }, { "bandonly", 1 }, { "ratio", 3 }, { "threshold", -36 }, { "attack", 2 }, { "release", 80 } } },
        { "busg", Drums, "Drum bus: kick-proof (3-band detect)", "ドラムバス：キックに潰されない（3 バンド検出）", "GR 3〜5 dB。検出だけ 3 分割し、低域の感度を −9 dB。キックで全体が沈まない。音は分割しないので位相は変わらない", "3–5 dB GR. Only the detector is split; the low band is 9 dB less sensitive, so the kick does not duck the bus. The audio is not split",
          { { "scmb", 1 }, { "sclo", -9 }, { "ratio", 4 }, { "attack", 10 }, { "release", 100 }, { "threshold", -20 } } },
        // ---- two stages in series, spectral
        { "dual", Master, "Mastering: level, then peaks", "マスタリング：均してからピーク", "前段（光学 2:1）で GR 1〜2 dB、後段（VCA）で 1〜2 dB。浅く 2 回かけると、深く 1 回より自然。TIME の紫の線が前段", "1–2 dB in the pre-stage (opto 2:1) and 1–2 dB in the VCA. Two shallow passes sound more natural than one deep one. The violet line in TIME is the pre-stage",
          { { "prethr", -24 }, { "threshold", -16 }, { "ratio", 2 }, { "attack", 10 }, { "release", 500 } } },
        { "dual", MixBus, "Bus: opto glue + fast VCA", "バス：光学でまとめて VCA で止める", "前段でゆっくりまとめ、後段を速くしてピークだけ止める", "The pre-stage glues slowly; the fast VCA only catches peaks",
          { { "prethr", -22 }, { "threshold", -12 }, { "ratio", 4 }, { "attack", 1 }, { "release", 100 }, { "outstage", 5 } } },
        { "spectral", MixBus, "Spectral: smooth a harsh mix", "スペクトラム：きつい帯域だけ抑える", "SPECTRUM タブで赤い線が出る帯域だけが下がる。全体の音量感はほぼ変わらない。GR は帯域ごとに 2〜4 dB", "Only the bands where the red line shows are reduced; overall loudness hardly changes. 2–4 dB per band",
          { { "threshold", -38 }, { "ratio", 2.5f }, { "knee", 8 }, { "attack", 15 }, { "release", 200 } } },
        { "spectral", Vocal, "Spectral: even out a vocal", "スペクトラム：ボーカルのむらを均す", "音程や母音で飛び出す帯域を均す。Mix 70% で自然さを残す。遅延が 43 ms あるので、録音中のモニターには向かない", "Evens out bands that jump with pitch and vowels. 70% mix keeps it natural. 43 ms of latency: not for monitoring while recording",
          { { "threshold", -44 }, { "ratio", 4 }, { "knee", 6 }, { "attack", 5 }, { "release", 120 }, { "mix", 70 } } },
        // ---- clipper
        { "clipper", Drums, "Clipper: shave drum peaks", "クリッパー：ドラムのピークを削る", "スネアやキックの頭を 2〜4 dB だけ削る。リミッターと違い、ポンピングが出ない。Delta で削った成分を聴ける", "Shaves 2–4 dB off snare and kick fronts. Unlike a limiter it cannot pump. Delta plays what was cut",
          { { "threshold", -8 }, { "knee", 0 }, { "ratio", 100 }, { "os", 2 } } },
        { "clipper", Master, "Clipper: soft, before the limiter", "クリッパー：リミッターの前で柔らかく", "Softness 6 dB、4x。リミッターの仕事を 1〜2 dB 減らす使い方", "6 dB of softness, 4x. Takes 1–2 dB of work off the limiter that follows",
          { { "threshold", -3 }, { "knee", 6 }, { "ratio", 100 }, { "os", 2 } } },
        { "clipper", Creative, "Clipper: parallel grit", "クリッパー：パラレルで歪みを足す", "深くクリップした音を 35% 混ぜる。1x にすると折り返しが増え、ざらつく（SPECTRUM で確認）", "A hard-clipped copy mixed in at 35%. At 1x aliasing adds grit (see SPECTRUM)",
          { { "input", 12 }, { "threshold", -12 }, { "knee", 0 }, { "ratio", 100 }, { "mix", 35 }, { "os", 0 } } },
    };
    return v;
}
inline int modelIndex (const char* id) { const auto& m = mod::models(); for (size_t i = 0; i < m.size(); ++i) if (juce::String (m[i].id) == id) return (int) i; return -1; }
} // namespace preset
