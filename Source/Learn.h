#pragma once
#include "Panel.h"

// =============================================================================
//  The card at the bottom: HELP (what the pointer is on / the model) | LESSON (guided steps with a setup to load) |
//  EAR (ear training: one setting is hidden and replaced, the displays are covered until you answer).
// =============================================================================
namespace learn
{
struct Lesson
{
    const char* tj; const char* te; const char* bj; const char* be;      // title, body (one step per line; the last line is the "why")
    const char* model; std::vector<std::pair<const char*, float>> set;   // setup: model, then these parameters
    int view = -1, lower = -1, upper = -1, harmF = -1, harmL = -1;       // which views to open (-1 = leave)
};
inline const std::vector<Lesson>& lessons()
{
    using namespace mod;
    static const std::vector<Lesson> L {
        { "Threshold と Ratio を TRANSFER で読む", "Reading threshold and ratio in TRANSFER",
          "1. ループを再生する。TRANSFER の丸（Threshold）を左右にドラッグ：折れ曲がる位置が動く。\n2. 右端の点（Ratio）を上下にドラッグ：折れた先の傾きが変わる。4:1 は「4 dB 超えたら 1 dB だけ出す」。\n3. TIME の赤（GR）を見る。Threshold を下げても Ratio を上げても GR は増えるが、かかる範囲が違う。\nなぜ：Threshold は「どこから」、Ratio は「どれだけ」。同じ GR 6 dB でも、低い Threshold＋低い Ratio は全体を、高い Threshold＋高い Ratio は山だけを抑える。",
          "1. Play a loop. Drag the ring (threshold) in TRANSFER left and right: the bend moves.\n2. Drag the dot at the right (ratio) up and down: the slope after the bend changes. 4:1 = 4 dB over gives 1 dB out.\n3. Watch the red GR in TIME. Lower threshold and higher ratio both add GR, but over a different range.\nWhy: threshold is where, ratio is how much. The same 6 dB of GR from a low threshold and low ratio shapes everything; from a high threshold and high ratio it only catches peaks.",
          "digital", { { pid::threshold, -24 }, { pid::ratio, 4 }, { pid::knee, 0 }, { pid::attack, 5 }, { pid::release, 120 }, { pid::lookahead, 0 } }, 4, 0, 0 },
        { "Knee：折れ曲がりの幅", "Knee: the width of the bend",
          "1. KNEE を 0 → 12 → 24 dB と上げる。TRANSFER の角が丸くなる。\n2. 灰色の線（ニー 0 の理想カーブ）と比べる。ニーが広いほど、Threshold の手前から少しずつかかり始める。\n3. ボーカルやベースで、0 dB と 12 dB を聴き比べる。\nなぜ：ハードニーは「かかった瞬間」が聴こえる。ソフトニーは境目が曖昧になり、自然に聴こえる。Opto や Vari-Mu が自然と言われる理由の 1 つ。",
          "1. Raise KNEE from 0 to 12 to 24 dB: the corner in TRANSFER rounds off.\n2. Compare with the grey line (the same curve with no knee). A wide knee starts acting a little before the threshold.\n3. Compare 0 dB and 12 dB on a vocal or a bass.\nWhy: a hard knee lets you hear the moment compression starts. A soft knee blurs that boundary. One reason optos and vari-mus are called natural.",
          "digital", { { pid::threshold, -24 }, { pid::ratio, 4 }, { pid::knee, 0 }, { pid::lookahead, 0 } }, 4, 0, 0 },
        { "Attack の定義：63% まで", "What attack means: the time to 63%",
          "1. 左下 GR タブ（STEP）を見る。Attack 10 ms の印が、目標 GR の 63% の位置にある。\n2. ATTACK を 1 ms と 30 ms にする。黄色い部分＝まだ絞れていない量＝通り抜ける打点。\n3. ドラムで聴く。速い＝打点が潰れる、遅い＝打点が残る。\nなぜ：Attack は「かかり始めるまでの待ち時間」ではない。すぐに下げ始め、63% に届くまでの時間。90% には約 2.3 倍、99% には約 4.6 倍かかる。",
          "1. Look at the GR tab (STEP), bottom left. The mark for a 10 ms attack sits at 63% of the target GR.\n2. Set ATTACK to 1 ms and to 30 ms. The yellow area = not reduced yet = the transient that gets through.\n3. Listen on drums. Fast squashes the hit, slow leaves it.\nWhy: attack is not a waiting time before compression starts. It starts at once; attack is the time to reach 63%. 90% takes about 2.3 times as long, 99% about 4.6 times.",
          "digital", { { pid::threshold, -24 }, { pid::ratio, 4 }, { pid::attack, 10 }, { pid::release, 150 }, { pid::lookahead, 0 }, { pid::timing, (float) TimOne } }, 4, 0, 0 },
        { "Release とポンピング", "Release and pumping",
          "1. RELEASE を 30 ms にする。TIME の GR が打点ごとに 0 まで戻る＝音量が波打つ（ポンピング）。\n2. 1 s にする。GR が戻りきらず、常に下がったまま＝ただ小さくなる。\n3. テンポに合わせ、次の打点の直前に戻りきる値を探す（BPM 120 の 4 分＝500 ms）。\nなぜ：Release は「余韻をどれだけ持ち上げるか」を決める。短いと余韻が上がって派手に、長いと静かで安定する。",
          "1. Set RELEASE to 30 ms. GR in TIME returns to 0 after every hit: the level surges (pumping).\n2. Set it to 1 s. GR never recovers: the track is simply quieter.\n3. Find the value where GR is just back before the next hit (a quarter note at 120 BPM = 500 ms).\nWhy: release decides how far the tail is lifted. Short brings up the decay and sounds lively; long is calm and stable.",
          "digital", { { pid::threshold, -26 }, { pid::ratio, 6 }, { pid::attack, 3 }, { pid::release, 150 }, { pid::lookahead, 0 }, { pid::timing, (float) TimOne } }, 4, 0, 0 },
        { "Makeup と Auto gain：音量差にだまされない", "Makeup and auto gain: do not be fooled by level",
          "1. Bypass を押して、圧縮前後を比べる。圧縮後のほうが小さい。\n2. MAKEUP で同じ大きさに戻す。右下 LOUDNESS の S（入力と出力）が同じになるまで上げる。\n3. Auto gain を押す。入力と出力の音量差を自動で埋める。\nなぜ：人は大きいほうを「良い音」と感じる。音量をそろえて初めて、圧縮の良し悪し（打点、余韻、密度）が判断できる。",
          "1. Press Bypass to compare before and after: after is quieter.\n2. Bring it back with MAKEUP until S in LOUDNESS (input and output) reads the same.\n3. Press Auto gain: it closes the level gap by itself.\nWhy: louder always sounds better. Only at matched loudness can you judge what compression did to the hit, the tail and the density.",
          "digital", { { pid::threshold, -26 }, { pid::ratio, 4 }, { pid::attack, 5 }, { pid::release, 150 }, { pid::makeup, 0 } }, 4, 0, 0 },
        { "Delta：削った成分だけを聴く", "Delta: hear only what was removed",
          "1. Delta を押す。聴こえるのは「原音 − 出力」＝コンプが変えた成分だけ。\n2. ATTACK を 0.1 ms と 30 ms で比べる。速いと打点が Delta に入る＝打点を削っている。\n3. RELEASE を短くすると、Delta が打点の直後で途切れる。\nなぜ：Delta に聴こえる音は、ミックスから消えた音。打点が聴こえたらアタックが速すぎ、低音だけが聴こえたらサイドチェーン HPF の出番。",
          "1. Press Delta. You hear input minus output: only what the compressor changed.\n2. Compare ATTACK 0.1 ms and 30 ms. Fast puts the hit into Delta: the hit is being removed.\n3. A short RELEASE makes Delta stop right after the hit.\nWhy: what you hear in Delta is what left the mix. A clear hit there means the attack is too fast; only bass there means the sidechain high-pass can help.",
          "digital", { { pid::threshold, -26 }, { pid::ratio, 4 }, { pid::attack, 0.1f }, { pid::release, 150 }, { pid::lookahead, 0 } }, 4, 0, 0 },
        { "Look-ahead：行き過ぎないピーク制御", "Look-ahead: peak control without overshoot",
          "1. LOOK を 0 ms にして ATTACK を 1 ms にする。GR タブ（STEP）で、最初の山が通り抜けるのが見える。\n2. LOOK を 3 ms にする。ピークが来る前から下げ始めるので、通り抜けがなくなる。\n3. ヘッダーの LAT（遅延）が増えるのを確認する。\nなぜ：未来は見られないので、音のほうを遅らせて「先読み」する。代わりに遅延が出る。リミッターはこの仕組みで天井を守る。",
          "1. Set LOOK to 0 ms and ATTACK to 1 ms. In the GR tab (STEP) the first peak gets through.\n2. Set LOOK to 3 ms: reduction starts before the peak arrives, and nothing gets through.\n3. Check LAT (latency) in the header: it has grown.\nWhy: nobody can see the future, so the audio is delayed instead. The price is latency. This is how limiters hold their ceiling.",
          "digital", { { pid::threshold, -24 }, { pid::ratio, 20 }, { pid::attack, 1 }, { pid::lookahead, 0 } }, 4, 0, 0 },
        { "Peak 検出と RMS 検出", "Peak and RMS detection",
          "1. ③ DETECTOR を Peak にする。ドラムの打点ごとに GR が鋭く動く。\n2. RMS にして RMS WIN を 30 ms にする。打点には反応が鈍く、平均の大きさに沿って動く。\n3. 左下 DYN タブで CREST（PEAK − RMS）の Δ を比べる。Peak のほうが大きく減る。\nなぜ：Peak は「瞬間の山」、RMS は「聴感上の大きさ」を測る。打点を抑えたいなら Peak、音量を均したいなら RMS。",
          "1. Set ③ DETECTOR to Peak. GR jumps sharply with every drum hit.\n2. Switch to RMS with RMS WIN at 30 ms. It reacts less to hits and follows the average level.\n3. In the DYN tab compare the Δ of CREST (peak − RMS): Peak reduces it more.\nWhy: peak measures the instant, RMS measures how loud it feels. Peak to control hits, RMS to even out level.",
          "digital", { { pid::threshold, -26 }, { pid::ratio, 4 }, { pid::attack, 1 }, { pid::release, 100 }, { pid::lookahead, 0 }, { pid::timing, (float) TimOne }, { pid::detector, (float) DetPeak }, { pid::rmsWin, 30 } }, 4, 1, 0 },
        { "Feed-forward と Feedback", "Feed-forward and feedback",
          "1. TOPOLOGY を Feed-fwd にする。検出器は圧縮前の音を測る。GR は式どおり。\n2. Feedback にする。検出器は圧縮後の音を測る。同じ設定でも GR が浅くなる。\n3. Threshold を下げて GR をそろえ、聴き比べる。Feedback のほうが動きが穏やか。\nなぜ：Feedback は「下げた結果を見て、また決める」自己調整。実質の比が下がり、かかり方が緩やかになる。1176、LA-2A、Fairchild など古い機種はすべてこの方式。",
          "1. Set TOPOLOGY to Feed-fwd: the detector measures the signal before compression. GR follows the formula.\n2. Set Feedback: the detector measures the compressed signal. The same settings give less GR.\n3. Lower the threshold to match GR and compare: feedback moves more gently.\nWhy: feedback looks at its own result and corrects itself. The effective ratio drops and the action softens. 1176, LA-2A, Fairchild — the old units all work this way.",
          "digital", { { pid::threshold, -26 }, { pid::ratio, 6 }, { pid::attack, 3 }, { pid::release, 150 }, { pid::lookahead, 0 }, { pid::timing, (float) TimOne }, { pid::topology, 0 } }, 4, 0, 0 },
        { "サイドチェーン HPF：キックで全体が沈まない", "Sidechain high-pass: the kick stops ducking the mix",
          "1. ドラムやミックスで、キックのたびに全体が沈むのを確認する（TIME の GR）。\n2. ② SC HPF を 100 → 150 Hz に上げる。左下 SC タブで、低域が下がった特性が見える。\n3. キックでの GR が減り、スネアや全体の大きさで動くようになる。\nなぜ：低音はエネルギーが大きく、検出器を独占する。検出器の耳から低音を外すと、音そのものの低域はそのままで、ポンピングだけが減る。",
          "1. On drums or a mix, watch everything dip with each kick (GR in TIME).\n2. Raise ② SC HPF to 100, then 150 Hz. The SC tab shows the lows going down.\n3. The kick causes less GR; the snare and the overall level now drive it.\nWhy: bass carries the most energy and monopolises the detector. Taking it out of the detector's ears leaves the audio's low end alone and removes only the pumping.",
          "busg", { { pid::threshold, -22 }, { pid::ratio, 4 }, { pid::scHpf, 0 } }, 4, 4, 0 },
        { "ディエッサー：帯域だけを聴き、帯域だけを下げる", "De-esser: listen to one band, turn down one band",
          "1. ボーカルを再生。② BAND を BP、BAND Hz を 6 kHz にする。SC タブに山が出る＝検出器は歯擦音だけを聴く。\n2. SC LISTEN を押して、検出器が聴いている音を確認する。\n3. Band only にする。下がるのは 6 kHz 付近だけになる（SPECTRUM の下段で確認）。\nなぜ：ディエッサーは「特定の帯域が大きいときだけ、その帯域を下げる」コンプ。Band only を外すと、歯擦音のたびに声全体が下がる。",
          "1. Play a vocal. Set ② BAND to BP and BAND Hz to 6 kHz. The SC tab shows a peak: the detector hears only sibilance.\n2. Press SC LISTEN to hear what the detector hears.\n3. Switch to Band only: now only the range around 6 kHz is turned down (see the lower strip of SPECTRUM).\nWhy: a de-esser is a compressor that turns one band down only when that band is loud. Without Band only, the whole voice ducks on every s.",
          "digital", { { pid::threshold, -34 }, { pid::ratio, 6 }, { pid::attack, 0.5f }, { pid::release, 60 }, { pid::lookahead, 0 }, { pid::band, (float) BandBP }, { pid::bandHz, 6000 }, { pid::bandQ, 1.5f }, { pid::bandOnly, 0 } }, 4, 4, 1 },
        { "ステレオリンクと Mid / Side", "Stereo link and mid / side",
          "1. 左右で音量が違う素材を再生。LINK を 100% にする。左右が同じ量だけ下がる＝定位が動かない。\n2. LINK を 0% にする。大きい側だけが下がり、定位が中央へ寄る。\n3. STEREO を Mid only にする。センター（キック、ベース、歌）だけを圧縮し、広がりは触らない。\nなぜ：リンクは「定位を守るか、左右それぞれを最大限そろえるか」の選択。バスでは 100%、広げたいときは下げる。",
          "1. Play something louder on one side. Set LINK to 100%: both sides drop by the same amount and the image stays put.\n2. Set LINK to 0%: only the louder side drops and the image drifts to the centre.\n3. Set STEREO to Mid only: the centre (kick, bass, vocal) is compressed, the width is left alone.\nWhy: linking is a choice between keeping the image and levelling each side fully. 100% on buses; less when you want width.",
          "busg", { { pid::threshold, -22 }, { pid::ratio, 4 }, { pid::link, 100 }, { pid::stereo, 0 } }, 4, 0, 0 },
        { "1176 はなぜ Threshold ノブが無いのか", "Why the 1176 has no threshold knob",
          "1. PANEL を見る。あるのは INPUT・OUTPUT・ATTACK・RELEASE・RATIO だけ。\n2. INPUT を上げると GR が増える。信号のほうを、固定のしきい値へ押し込んでいる。\n3. 透過 2 にして下の BUILD を見る。橙の枠＝いま動かした INPUT（①）。THRESH（④）は動いていない。\n4. RATIO を 4 → 20 にする。TRANSFER の折れ曲がりが右上へ動く＝比としきい値が連動。\nなぜ：FET は制御できる電圧の範囲が狭く、しきい値を回路で固定して入力で押し込むほうが作りやすかった。INPUT と OUTPUT を逆に回して使うのはそのため。",
          "1. Look at PANEL: only INPUT, OUTPUT, ATTACK, RELEASE and RATIO.\n2. Raising INPUT adds GR: the signal is pushed into a fixed threshold.\n3. Choose See 2 and look at BUILD underneath. The orange frame is the INPUT (①) you just moved; THRESH (④) has not moved.\n4. Change RATIO from 4 to 20: the bend in TRANSFER moves up and right — ratio and threshold are tied.\nWhy: a FET works over a narrow control range, so a fixed threshold with the signal pushed into it was easier to build. That is why INPUT and OUTPUT are turned in opposite directions.",
          "fet76", {}, 2, 0, 0 },
        { "FET 76 の ALL：歪みとリリースの変化", "FET 76, all buttons in: distortion and release",
          "1. 左下 HARM タブを見る。RATIO 4 では 2 次倍音が中心。\n2. RATIO を ALL にする。倍音が増え、THD が上がる。BUILD では ⑥ DRIVE と ⑤ AMOUNT が変わる。\n3. ドラムのルームやオーバーヘッドで聴く。余韻が持ち上がり、荒々しくなる。\nなぜ：全押しは設計外の使い方で、バイアスがずれて歪みが増え、リリースの形も変わる。「圧縮」というより「歪み＋激しい動き」のエフェクト。",
          "1. Open the HARM tab. At RATIO 4 the 2nd harmonic leads.\n2. Set RATIO to ALL: more harmonics, higher THD. In BUILD, ⑥ DRIVE and ⑤ AMOUNT change.\n3. Listen on a drum room or overheads: the decay comes up and gets wild.\nWhy: all buttons in is outside the design. The bias shifts, distortion rises and the release changes shape. Less a compressor than a distortion-and-movement effect.",
          "fet76", { { pid::input, 6 } }, 0, 3, 0, 1, 1 },
        { "Opto 2A：長く鳴るほど戻りが遅い", "Opto 2A: the longer it sounds, the slower it recovers",
          "1. PANEL には PEAK REDUCTION と GAIN しかない。Attack・Release のノブは無い。\n2. 短い音（スネア）と長い音（ロングトーン）で、TIME の GR の戻り方を比べる。長い音のあとは戻りが遅い。\n3. 黄色の点線（遅いリリース成分）が、圧縮が続くほどたまっていくのを見る。\nなぜ：光学セル（光る素子＋光で抵抗が変わる素子）には残光がある。長く光ったあとほど暗くなるのが遅い。これが「歌に自然」と言われる、素材に合わせて変わるリリース。",
          "1. PANEL has only PEAK REDUCTION and GAIN: no attack or release knobs.\n2. Compare how GR recovers in TIME after a short sound (snare) and a long one (held note): slower after the long one.\n3. Watch the yellow dotted line (the slow release part) build up while compression lasts.\nWhy: a photocell (a light and a light-dependent resistor) has afterglow: the longer it was lit, the slower it goes dark. That is the programme-dependent release people call natural on vocals.",
          "opto2a", {}, 1, 0, 0 },
        { "Vari-Mu 670：レベルで比が変わる", "Vari-Mu 670: the ratio changes with level",
          "1. TRANSFER を見る。折れ曲がりがなく、なだらかに寝ていく＝レベルが上がるほど比が上がる。\n2. INPUT GAIN を上げる。小さく入れると 2:1 程度、大きく入れるとリミッターに近づく。\n3. TIME CONSTANT を 1 → 5 → 6 にする。5 と 6 は、圧縮が続くほど戻りが遅くなる 2 段リリース。\nなぜ：真空管の増幅率（μ）そのものを変えて音量を下げる方式。深くかけるほど管が飽和に近づき、比が上がる。マスタリングで「かかっているのに分からない」理由。",
          "1. Look at TRANSFER: no corner, the curve leans over gradually — the ratio rises with level.\n2. Raise INPUT GAIN. Low level gives about 2:1; high level approaches limiting.\n3. Step TIME CONSTANT through 1, 5 and 6. 5 and 6 are two-stage releases that slow down the longer compression lasts.\nWhy: it turns the level down by changing the tube's own gain (mu). The deeper it goes, the closer the tube is to cut-off and the higher the ratio. Why it works on masters without being noticed.",
          "mu670", {}, 1, 0, 0 },
        { "VCA 160：RMS 検出器がタイミングを決める", "VCA 160: the RMS detector sets the timing",
          "1. PANEL には THRESHOLD・COMPRESSION・OUTPUT GAIN だけ。Attack・Release は無い。\n2. BUILD で ③ を見る。RMS、RMS WIN 36 ms。この窓の長さが速さを決めている。\n3. GR タブ（STEP）で、リリースが曲線ではなく直線に近いことを確認する。\n4. ベースやキックで聴く。ハードニーで、打点が前に出る「パンチ」。\nなぜ：RMS 検出器の出力は dB に比例するので、戻りが dB で直線になる（毎秒何 dB）。指数カーブの機種より、戻りが速く感じる。",
          "1. PANEL has only THRESHOLD, COMPRESSION and OUTPUT GAIN: no attack, no release.\n2. In BUILD look at ③: RMS with a 36 ms window. That window sets the speed.\n3. In the GR tab (STEP) the release is close to a straight line, not a curve.\n4. Listen on bass or kick: hard knee, the hit steps forward — punch.\nWhy: the RMS detector's output is proportional to dB, so recovery is linear in dB (so many dB per second). It feels quicker than units with an exponential curve.",
          "vca160", {}, 0, 0, 0 },
        { "Bus VCA G：グルーと Auto リリース", "Bus VCA G: glue and auto release",
          "1. ミックスかドラムバスで、GR を 2〜4 dB にする（THRESHOLD）。\n2. ATTACK を 0.1 → 30 ms にする。30 ms でキックの頭が抜け、前に出る。\n3. RELEASE を Auto にする。短い音のあとは速く、圧縮が続いたあとは遅く戻る。\n4. ② SC HPF を 100 Hz にして、キックでの沈みを減らす。\nなぜ：遅い Attack で打点を通し、全体を数 dB だけ一緒に動かすと、別々の楽器が「1 つの演奏」に聴こえる。これがグルー。",
          "1. On a mix or drum bus set 2–4 dB of GR (THRESHOLD).\n2. Change ATTACK from 0.1 to 30 ms: at 30 ms the kick's front gets through and steps forward.\n3. Set RELEASE to Auto: fast after short sounds, slow after sustained compression.\n4. Set ② SC HPF to 100 Hz to stop the kick dipping the mix.\nWhy: a slow attack passes the hits while a few dB of shared movement makes separate instruments sound like one performance. That is glue.",
          "busg", {}, 0, 0, 0 },
        { "Diode 254：ダイオードブリッジの色", "Diode 254: the colour of a diode bridge",
          "1. 左下 HARM タブを見る。奇数次（3 次、5 次）の倍音が出ている。\n2. THRESHOLD を下げて GR を深くする。倍音が増える＝かけるほど歪む素子。\n3. RECOVERY を a1・a2 にする。自動リリースで、バスでも不自然に波打たない。\nなぜ：ダイオードブリッジは扱える信号が小さく、内部で信号を下げてから増幅し直す。その過程とトランスが、太く前に出る中域を作る。",
          "1. Open the HARM tab: odd harmonics (3rd, 5th).\n2. Lower THRESHOLD for more GR: more harmonics — this element distorts more the harder it works.\n3. Set RECOVERY to a1 or a2: automatic release that does not surge on a bus.\nWhy: a diode bridge only handles a small signal, so the unit pads the signal down and amplifies it again. That, plus the transformers, makes the thick, forward midrange.",
          "diode254", { { pid::input, 6 } }, 0, 3, 0, 1, 1 },
        { "IDEAL 比較：機種の「クセ」を取り出す", "IDEAL: isolating a unit's character",
          "1. FET 76 で GR を 5 dB 前後にする。\n2. 右の IDEAL を押す。同じくらいの GR 量の、クセのないコンプに差し替わる。\n3. 切り替えながら聴く。変わるのは歪み（⑥⑦）、リリースの形（⑤）、ニー（④）。\n4. MODEL を Opto 2A、Vari-Mu 670 に替えて同じことをする。\nなぜ：機種の違いは「GR の量」ではなく「GR の動き方と歪み」。量をそろえて比べると、その機種を選ぶ理由が聴こえる。",
          "1. With the FET 76, set about 5 dB of GR.\n2. Press IDEAL (right): a plain compressor with about the same GR takes over.\n3. Switch back and forth. What changes is distortion (⑥⑦), release shape (⑤) and knee (④).\n4. Do the same with Opto 2A and Vari-Mu 670.\nWhy: units differ not in how much GR but in how it moves and what it adds. Matching the amount lets you hear the reason to choose one.",
          "fet76", { { pid::input, 6 } }, 4, 0, 0 },
        { "パラレルコンプと Upward", "Parallel compression and upward compression",
          "1. Digital で深く潰す（GR 10 dB 以上）。DRY / WET を 100 → 40% にする。打点は原音、余韻は潰した音。\n2. MODEL を Upward に替える。小さい音だけを持ち上げる。TIME の緑（下から上）が持ち上げ量。\n3. 両方とも「小さい音が大きくなる」が、パラレルは潰した音色が混ざり、Upward は音色が変わらない。\nなぜ：大きい音を下げる（下向き）と、小さい音を上げる（上向き）は、ダイナミクスを狭める 2 つの道。打点を守りたいなら上向き側の発想が有利。",
          "1. Crush it with Digital (10 dB of GR or more). Bring DRY / WET from 100 to 40%: hits from the dry signal, tails from the crushed one.\n2. Switch MODEL to Upward: only quiet sound is raised. The green in TIME (rising from the bottom) is the boost.\n3. Both make quiet parts louder; parallel mixes in the crushed tone, upward leaves the tone alone.\nWhy: turning loud down and turning quiet up are two roads to a narrower dynamic range. To protect transients, think upward.",
          "digital", { { pid::threshold, -34 }, { pid::ratio, 10 }, { pid::attack, 1 }, { pid::release, 80 }, { pid::lookahead, 0 }, { pid::mix, 100 } }, 4, 0, 0 },
        { "OTT：3 バンドで上下から挟む", "OTT: squeezed from above and below in three bands",
          "1. シンセやドラムを再生。SPECTRUM タブで、低・中・高の 3 帯域それぞれの GR と UP を見る。\n2. DEPTH（DRY / WET）を 100 → 30% にする。30% 前後が定番。\n3. HIGH を下げる、UPWARD を下げる。明るさと密度がどこから来ているかを確かめる。\nなぜ：帯域ごとに「大きい音は下げ、小さい音は上げる」ので、どの帯域も常に鳴っている状態になる。高域の細部が持ち上がり、派手で明るくなる。",
          "1. Play a synth or drums. In SPECTRUM read GR and UP for each of the three bands.\n2. Bring DEPTH (DRY / WET) from 100 to 30%: around 30% is the usual setting.\n3. Lower HIGH, lower UPWARD: find where the brightness and density come from.\nWhy: every band has its loud parts turned down and its quiet parts turned up, so every band is always on. High detail comes up: bright and aggressive.",
          "ott", {}, 0, 0, 1 },
        { "Limiter と Clipper を PSR で比べる", "Limiter and clipper compared by PSR",
          "1. Limiter で INPUT を上げ、GR を 4 dB にする。LOUDNESS の PSR（出力）を読む。\n2. A/B の B を押し、MODEL を Clipper にして、同じ天井・同じくらいの音量にする。\n3. A と B を切り替える。Limiter は余韻が持ち上がり、Clipper は打点の形が残る代わりに歪む。\n4. Delta で、それぞれが削った成分を聴く。\nなぜ：Limiter は時間をかけてゲインを動かす（歪まないが、動きが聴こえる）。Clipper は瞬間だけ切る（動かないが、歪む）。ドラムは Clipper、歌やミックス全体は Limiter が向く。",
          "1. With Limiter, raise INPUT for 4 dB of GR. Read PSR (output) in LOUDNESS.\n2. Press B of A/B, choose MODEL Clipper, set the same ceiling and about the same loudness.\n3. Switch A and B. The limiter lifts the tails; the clipper keeps the shape of the hit but distorts.\n4. Use Delta to hear what each one removed.\nWhy: a limiter moves gain over time (no distortion, audible movement). A clipper cuts instants only (no movement, distortion). Clipper for drums, limiter for vocals and full mixes.",
          "limiter", { { pid::input, 6 } }, 0, 0, 0 },
        { "Clipper の Oversample：折り返しを見る", "Clipper oversampling: seeing aliasing",
          "1. 左下 HARM タブ。右上を 5k・0 にする。\n2. OVERSAMPLE を 1x にする。橙の点（倍音）の間に山が立つ＝折り返し。\n3. 4x にする。間の山が下がる。下の「倍音以外の成分」の数字で比べる。\n4. SOFTNESS を 0 → 12 dB にする。高次の倍音が減り、折り返しも減る。\nなぜ：クリップは高い倍音を大量に作る。サンプルレートの半分を超えた倍音は、折り返して元の音と無関係な周波数に現れる。オーバーサンプリングは、内部だけレートを上げてこれを減らす。",
          "1. Open the HARM tab. Choose 5k and 0 at the top right.\n2. Set OVERSAMPLE to 1x: peaks stand between the orange dots (harmonics) — aliasing.\n3. Set 4x: those peaks drop. Compare the number after \"between the harmonics\".\n4. Raise SOFTNESS from 0 to 12 dB: fewer high harmonics, less aliasing.\nWhy: clipping makes many high harmonics. Those above half the sample rate fold back to frequencies unrelated to the note. Oversampling raises the rate inside the unit to reduce that.",
          "clipper", { { pid::knee, 0 }, { pid::os, 0 } }, 0, 3, 0, 2, 2 },
        { "Dual：浅く 2 回かける", "Dual: two shallow passes",
          "1. TIME を見る。紫の点線＝前段（光学 2:1）の GR、赤＝後段（VCA）の GR。\n2. OPTICAL THR を下げて前段を 2 dB、DISCRETE THR で後段を 2 dB にする。\n3. A/B の B で、後段だけで 4 dB かけた設定を作り、聴き比べる。\nなぜ：遅い前段が全体の音量を均し、速い後段は残ったピークだけを見る。1 台が両方をやるより、それぞれの動きが小さく、かかりが目立たない。",
          "1. Look at TIME: violet dots = GR of the first stage (opto 2:1), red = GR of the second (VCA).\n2. Lower OPTICAL THR for 2 dB in the first stage; set DISCRETE THR for 2 dB in the second.\n3. In slot B of A/B make 4 dB with the second stage alone, and compare.\nWhy: the slow first stage evens out the level, the fast second one only sees the peaks that remain. Each moves less than one unit doing both, so the compression shows less.",
          "dual", {}, 0, 0, 0 },
        { "Rider と Spectral：コンプ以外の音量制御", "Rider and Spectral: level control beyond compression",
          "1. MODEL を Rider にしてボーカルを再生。TARGET へ向けてゆっくりゲインが動く。音色も打点も変わらない。\n2. 左下 DYN タブで、CREST の Δ がほぼ 0 のままなのを確認する（コンプなら減る）。\n3. MODEL を Spectral にして SPECTRUM タブを開く。山の高い帯域だけが下がる。\nなぜ：Rider は「人がフェーダーを動かす」のと同じで、時間方向だけを均す。Spectral は周波数ごとに別々に均す。コンプは、その間で「音量と一緒に音色と打点も変える」道具。",
          "1. Choose MODEL Rider and play a vocal. The gain moves slowly towards TARGET; tone and transients stay.\n2. In the DYN tab the Δ of CREST stays near 0 (a compressor would reduce it).\n3. Choose MODEL Spectral and open SPECTRUM: only the tallest bands come down.\nWhy: a rider is a hand on the fader: it evens out level over time only. Spectral evens out each frequency separately. A compressor sits between: it changes tone and transients along with level.",
          "rider", {}, 0, 1, 0 } };
    return L;
}
} // namespace learn

// ---------------------------------------------------------------------------- covers the displays while an ear-training question is open
class QuizCover : public juce::Component
{
public:
    explicit QuizCover (ParamHelp& h) : ph (h) {}
    std::vector<juce::Rectangle<int>> rects;
    bool hitTest (int x, int y) override { for (auto& r : rects) if (r.contains (x, y)) return true; return false; }
    void paint (juce::Graphics& g) override
    {
        for (size_t i = 0; i < rects.size(); ++i)
        {
            g.setColour (juce::Colour (0xff14161a)); g.fillRoundedRectangle (rects[i].toFloat(), 6.0f);
            g.setColour (juce::Colour (0xff3a3f48)); g.drawRoundedRectangle (rects[i].toFloat().reduced (0.5f), 6.0f, 1.0f);
            if (rects[i].getWidth() > 300 && rects[i].getHeight() > 100)
            {
                g.setColour (juce::Colour (0xffb9bcc2)); g.setFont (theme::ui (13.0f));
                g.drawText (ph.tr ("耳トレ中：表示を隠しています。Bypass で原音と比べ、耳で選んでください", "Ear training: displays are hidden. Compare with Bypass and choose by ear"), rects[i], juce::Justification::centred);
            }
        }
    }
private:
    ParamHelp& ph;
};

// ---------------------------------------------------------------------------- the card
class LearnCard : public juce::Component
{
public:
    explicit LearnCard (ParamHelp& h) : ph (h), help (h) { addAndMakeVisible (help); help.setInterceptsMouseClicks (false, false); }
    int mode() const { return juce::jlimit (0, 2, (int) ph.proc.uiState.getProperty ("cardMode", 0)); }
    bool covering() const { return mode() == 2 && asked && ! answered; }
    void resized() override { help.setBounds (getLocalBounds()); }
    void tick()
    {
        help.setVisible (mode() == 0);
        if (mode() != 2 && asked) stopQuiz();
        repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        for (int i = 0; i < 3; ++i) if (modeChip (i).contains (p)) { ph.proc.uiState.setProperty ("cardMode", i, nullptr); return; }
        if (mode() == 1)
        {
            const int n = (int) learn::lessons().size(), cur = lessonIdx();
            if (titleRect().contains (p))
            {
                juce::PopupMenu m;
                for (int i = 0; i < n; ++i) m.addItem (i + 1, juce::String (i + 1).paddedLeft ('0', 2) + "  " + ph.tr (learn::lessons()[(size_t) i].tj, learn::lessons()[(size_t) i].te), true, i == cur);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) ph.proc.uiState.setProperty ("lesson", r - 1, nullptr); });
            }
            else if (loadRect().contains (p)) load (learn::lessons()[(size_t) cur]);
            else if (prevRect().contains (p)) ph.proc.uiState.setProperty ("lesson", (cur + n - 1) % n, nullptr);
            else if (nextRect().contains (p)) ph.proc.uiState.setProperty ("lesson", (cur + 1) % n, nullptr);
        }
        else if (mode() == 2)
        {
            for (int i = 0; i < 4; ++i) if (kindChip (i).contains (p)) { stopQuiz(); kind = i; score = total = 0; return; }
            if (! asked || answered) { if (goRect().contains (p)) ask(); }
            else for (int i = 0; i < (int) labels.size(); ++i) if (choiceRect (i).contains (p)) { chosen = i; answered = true; ++total; if (i == correct) ++score; }
        }
    }
    void paint (juce::Graphics& g) override
    {
        if (mode() == 0) return;
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::lessonBg); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::lessonLn); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        const auto a = getLocalBounds().reduced (14, 10);
        if (mode() == 1)
        {
            const auto& L = learn::lessons()[(size_t) lessonIdx()];
            g.setColour (theme::accent); g.setFont (theme::mono (11.0f, true));
            g.drawText ("LESSON " + juce::String (lessonIdx() + 1).paddedLeft ('0', 2) + " / " + juce::String ((int) learn::lessons().size()), a.getX(), a.getY(), 104, 16, juce::Justification::centredLeft);
            g.setColour (theme::text); g.setFont (theme::ui (12.5f, true));
            g.drawFittedText (ph.tr (L.tj, L.te) + juce::String::fromUTF8 ("  \xe2\x96\xbe"), a.getX() + 108, a.getY(), a.getWidth() - 108 - 160, 16, juce::Justification::centredLeft, 1, 0.8f);
            int y = a.getY() + 22; const auto lines = juce::StringArray::fromLines (ph.tr (L.bj, L.be)); const int bottom = getHeight() - 36;
            const float fpx = lines.size() > 4 ? 10.5f : 11.5f; const int lh = lines.size() > 4 ? 13 : 15;
            for (int i = 0; i < lines.size(); ++i)
            {
                const bool why = i == lines.size() - 1; const auto f = theme::ui (fpx);
                const int nl = juce::jmax (1, (int) std::ceil (textWidth (f, lines[i]) / (float) a.getWidth() + 0.12f)), h = juce::jmin (nl * lh, bottom - y);
                if (h < lh) break;
                g.setFont (f); g.setColour (why ? theme::muted : theme::text);
                g.drawFittedText (lines[i], a.getX(), y, a.getWidth(), h, juce::Justification::topLeft, juce::jmax (1, h / lh), 0.85f); y += h + 2;
            }
            button (g, loadRect(), ph.tr ("この設定を読み込む", "Load this setup"), true);
            button (g, prevRect(), ph.tr ("前へ", "Prev"), false); button (g, nextRect(), ph.tr ("次へ", "Next"), false);
        }
        else
        {
            static const char* kn[] { "Attack", "Release", "Ratio", "Model" };
            g.setColour (theme::accent); g.setFont (theme::mono (11.0f, true)); g.drawText ("EAR TRAINING", a.getX(), a.getY(), 100, 16, juce::Justification::centredLeft);
            for (int i = 0; i < 4; ++i) chip (g, kindChip (i), kn[i], i == kind);
            g.setFont (theme::ui (11.5f)); g.setColour (theme::text);
            if (! asked)
                g.drawFittedText (ph.tr ("1. 先に、GR が 4〜8 dB かかる設定にして、ループを再生する。\n2. 上で種類を選び「開始」を押す。表示が隠れ、その項目だけが秘密の値に変わる（画面の数値はそのまま）。\n3. Bypass で原音と比べながら、耳で選ぶ。答えると表示が戻り、正解の値のまま音と波形を確認できる。\n4. EAR を離れる（HELP か LESSON）と、元の設定に戻る。",
                                         "1. First set 4–8 dB of GR and play a loop.\n2. Pick a kind above and press Start. The displays are covered and that one setting takes a secret value (the numbers on screen do not change).\n3. Compare with Bypass and choose by ear. After you answer, the displays return with the correct value still active.\n4. Leaving EAR (HELP or LESSON) restores your own setting."),
                                  a.getX(), a.getY() + 24, a.getWidth(), 100, juce::Justification::topLeft, 7, 0.85f);
            else
            {
                g.drawText (ph.tr ("いま鳴っているのはどれ？", "Which one is playing?") + "   (" + kn[kind] + ")", a.getX(), a.getY() + 26, a.getWidth(), 16, juce::Justification::centredLeft);
                for (int i = 0; i < (int) labels.size(); ++i)
                {
                    const auto b = choiceRect (i).toFloat(); const bool right = answered && i == correct, wrong = answered && i == chosen && i != correct;
                    g.setColour (right ? theme::ok.withAlpha (0.85f) : wrong ? theme::pre.withAlpha (0.85f) : theme::hilite (0.12f)); g.fillRoundedRectangle (b, 5.0f);
                    g.setColour (theme::lessonLn); g.drawRoundedRectangle (b, 5.0f, 1.0f);
                    g.setColour ((right || wrong) ? juce::Colours::white : theme::text); g.setFont (theme::ui (12.0f, true)); g.drawFittedText (labels[i], b.toNearestInt().reduced (3, 0), juce::Justification::centred, 1, 0.75f);
                }
                if (answered)
                {
                    g.setFont (theme::ui (12.0f, true)); g.setColour (chosen == correct ? theme::ok : theme::pre);
                    g.drawText ((chosen == correct ? ph.tr ("正解。", "Correct. ") : ph.tr ("不正解。正解は ", "Not this time. It was ") + labels[correct] + ph.tr ("。", ". "))
                                + ph.tr ("表示を戻しました。この値のまま、音と波形を確認できます", "Displays are back: check the sound and the curves at this value"), a.getX(), a.getY() + 96, a.getWidth(), 16, juce::Justification::centredLeft);
                }
            }
            if (! asked || answered) button (g, goRect(), asked ? ph.tr ("次の問題", "Next question") : ph.tr ("開始", "Start"), true);
            g.setColour (theme::muted); g.setFont (theme::mono (10.5f, true));
            g.drawText (ph.tr ("正解 ", "Score ") + juce::String (score) + " / " + juce::String (total), a.getX(), getHeight() - 30, 140, 18, juce::Justification::centredLeft);
        }
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        static const char* mn[] { "HELP", "LESSON", "EAR" };
        g.setColour (theme::lessonBg); g.fillRect (modeChip (0).getX() - 8, 5, getWidth() - modeChip (0).getX() + 2, 21);
        for (int i = 0; i < 3; ++i) chip (g, modeChip (i), mn[i], i == mode());
    }
private:
    int lessonIdx() const { return juce::jlimit (0, (int) learn::lessons().size() - 1, (int) ph.proc.uiState.getProperty ("lesson", 0)); }
    juce::Rectangle<int> modeChip (int i) const { return { getWidth() - 12 - (3 - i) * 50, 8, 47, 15 }; }
    juce::Rectangle<int> kindChip (int i) const { return { 120 + i * 58, 9, 55, 16 }; }
    juce::Rectangle<int> titleRect() const { return { 14, 8, getWidth() - 190, 20 }; }
    juce::Rectangle<int> loadRect() const { return { getWidth() - 12 - 252, getHeight() - 31, 136, 22 }; }
    juce::Rectangle<int> prevRect() const { return { getWidth() - 12 - 110, getHeight() - 31, 52, 22 }; }
    juce::Rectangle<int> nextRect() const { return { getWidth() - 12 - 52, getHeight() - 31, 52, 22 }; }
    juce::Rectangle<int> goRect() const { return { getWidth() - 12 - 120, getHeight() - 31, 120, 22 }; }
    juce::Rectangle<int> choiceRect (int i) const { const int n = juce::jmax (1, (int) labels.size()), w = juce::jmin (120, (getWidth() - 28 - (n - 1) * 8) / n); return { 14 + i * (w + 8), 58, w, 30 }; }
    static void chip (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, bool on)
    {
        g.setColour (on ? theme::accent : theme::hilite (0.14f)); g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (on ? theme::onAccent : theme::text); g.setFont (theme::mono (8.5f, true)); g.drawText (t, r, juce::Justification::centred);
    }
    static void button (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, bool strong)
    {
        g.setColour (strong ? theme::accent : theme::hilite (0.14f)); g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (theme::lessonLn); g.drawRoundedRectangle (r.toFloat(), 4.0f, 1.0f);
        g.setColour (strong ? theme::onAccent : theme::text); g.setFont (theme::ui (11.0f, strong)); g.drawText (t, r, juce::Justification::centred);
    }
    void load (const learn::Lesson& L)
    {
        auto& P = ph.proc;
        if (const int m = mod::modelIndexOf (L.model); m >= 0) P.applyModel (m);                          // one undo step
        for (auto& kv : L.set) if (! P.isLocked (kv.first)) ph.setGesture (kv.first, panel::legal (ph, kv.first, kv.second));
        if (L.view >= 0) setViewMode (P.uiState, L.view);
        if (L.lower >= 0) P.uiState.setProperty ("lowerTab", L.lower, nullptr);
        if (L.upper >= 0) P.uiState.setProperty ("upperTab", L.upper, nullptr);
        if (L.harmF >= 0) P.uiState.setProperty ("harmF", L.harmF, nullptr);
        if (L.harmL >= 0) P.uiState.setProperty ("harmL", L.harmL, nullptr);
    }
    void stopQuiz() { ph.proc.quizKind = 0; asked = answered = false; labels.clear(); }
    void ask()
    {
        auto& rnd = juce::Random::getSystemRandom(); labels.clear(); std::vector<float> vals;
        if (kind == 0) vals = { 0.1f, 1.0f, 10.0f, 30.0f, 100.0f };
        else if (kind == 1) vals = { 30.0f, 100.0f, 300.0f, 1000.0f };
        else if (kind == 2) vals = { 1.5f, 3.0f, 6.0f, 20.0f };
        else
        {
            std::vector<const char*> pool { "fet76", "opto2a", "mu670", "vca160", "busg", "diode254", "el8" };
            for (int i = (int) pool.size() - 1; i > 0; --i) std::swap (pool[(size_t) i], pool[(size_t) rnd.nextInt (i + 1)]);
            for (int i = 0; i < 4; ++i) vals.push_back ((float) mod::modelIndexOf (pool[(size_t) i]));
        }
        for (auto v : vals) labels.add (kind == 0 || kind == 1 ? fmtTime (v) : kind == 2 ? juce::String (v, 1) + " : 1" : juce::String (mod::models()[(size_t) (int) v].name));
        int c = rnd.nextInt ((int) vals.size()); if (asked && vals.size() > 1 && c == correct && kind != 3) c = (c + 1 + rnd.nextInt ((int) vals.size() - 1)) % (int) vals.size();   // not the same answer twice in a row
        correct = c; chosen = -1; asked = true; answered = false;
        ph.proc.quizVal = vals[(size_t) c]; ph.proc.quizModel = (int) vals[(size_t) c]; ph.proc.quizKind = kind + 1;
    }
    ParamHelp& ph; InfoCard help;
    int kind = 0, correct = 0, chosen = -1, score = 0, total = 0; bool asked = false, answered = false; juce::StringArray labels;
};
