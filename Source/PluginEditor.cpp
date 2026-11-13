#include "PluginEditor.h"

CompView::CompView (CompProcessor& p) : proc (p)
{
    // the look is shared by every instance on this computer
    theme::setSkin ((int) ZoomPrefs::file().getIntValue ("companatomy2.skin", 0)); lnf.apply();
    for (int i = 0; i < theme::numSkins(); ++i) zoom.skinNames.add (theme::skinName (i));
    zoom.getSkin = [] { return theme::current; };
    zoom.setSkin = [this] (int i) { applySkin (i); };
    theme::surface = juce::jlimit (0, theme::numSurfaces() - 1, ZoomPrefs::file().getIntValue ("companatomy2.surface", 0));
    for (int i = 0; i < theme::numSurfaces(); ++i) zoom.surfaceNames.add (theme::surfaceName (i));
    zoom.getSurface = [] { return theme::surface; };
    zoom.setSurface = [this] (int i) { applySurface (i); };
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    auto toggle = [this] (juce::TextButton& b, const char* id)
    {
        b.onClick = [this, id] { ph.toggleGesture (id); };
        markReset (b); addAndMakeVisible (b);
    };
    toggle (listenBtn, pid::scListen); toggle (autoBtn, pid::autoGain); toggle (deltaBtn, pid::delta); toggle (bypassBtn, pid::bypass);
    langBtn.onClick = [this] { proc.uiState.setProperty ("lang", ph.ja() ? "en" : "ja", nullptr); refreshTexts(); repaint(); };
    addAndMakeVisible (langBtn); addAndMakeVisible (zoom);
    for (auto& grp : mod::menuGroups())                                 // grouped by how the unit works; the item id stays the model number
    {
        modelBox.addSectionHeading (juce::String::fromUTF8 (grp.title));
        for (auto* id : grp.ids) if (const int k = mod::modelIndexOf (id); k >= 0) modelBox.addItem (mod::models()[(size_t) k].name, k + 1);
    }
    modelBox.setTextWhenNothingSelected ("MODEL");
    modelBox.onChange = [this] { const int id = modelBox.getSelectedId(); if (id > 0 && id - 1 != proc.currentModel()) proc.applyModel (id - 1); };
    addAndMakeVisible (modelBox);
    presetBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetBtn);
    prevBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80")); nextBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    prevBtn.onClick = [this] { stepPreset (-1); }; nextBtn.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (prevBtn); addAndMakeVisible (nextBtn);
    setHelp (prevBtn, "PRESET \xe2\x97\x80", "前のプリセット。順番はメニューと同じ（機種の分類順に、機種の初期設定→その機種のプリセット、最後にユーザープリセット）", "Previous preset, in menu order (models by group: default, then its presets; user presets last)");
    setHelp (nextBtn, "PRESET \xe2\x96\xb6", "次のプリセット。順番はメニューと同じ（機種の分類順に、機種の初期設定→その機種のプリセット、最後にユーザープリセット）", "Next preset, in menu order (models by group: default, then its presets; user presets last)");
    lockBtn.onClick = [this] { showLockMenu(); };
    addAndMakeVisible (lockBtn);
    for (int i = 0; i < 4; ++i)
    {
        auto& b = slotBtn[i]; b.setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + i)));
        b.onClick = [this, i] { proc.selectSlot (i, juce::ModifierKeys::currentModifiers.isAltDown()); };
        addAndMakeVisible (b);
        setHelp (b, "A / B / C / D", "設定を 4 つまで持ち替えて聴き比べる。初めて押したスロットは、今の設定のコピーから始まる。Alt＋クリックで、今の設定をそのスロットへ上書きコピー。SC LISTEN・Delta・Bypass は切り替わらない。セッションに保存される",
                 "Four sets of settings to switch between. A slot you open for the first time starts as a copy of the current one. Alt-click copies the current settings over that slot. SC LISTEN, Delta and Bypass do not switch. Saved with the session");
    }
    idealBtn.onClick = [this] { proc.idealOn = ! proc.idealOn.load(); };
    addAndMakeVisible (idealBtn);
    setHelp (idealBtn, "IDEAL", "押している間、同じくらいの GR 量になる「理想コンプ」に差し替える：Peak 検出・フィードフォワード・理想カーブ（今のカーブに合わせて Threshold と Ratio を自動で合わせる）・1 段のタイミング・歪みなし。Attack と Release はそのまま。機種のクセ（歪み、リリースの形、ニー）が何を足しているかを聴き比べる。Rider・Spectral・Limiter では使えない",
             "While on, the unit is replaced by a plain compressor giving about the same amount of reduction: peak detector, feed-forward, an ideal curve (threshold and ratio fitted to the current curve), one-stage timing, no distortion. Attack and release stay. Compare to hear what the model's character (distortion, release shape, knee) adds. Not for Rider, Spectral, Limiter");
    setHelp (lockBtn, "LOCK", "ロックしたパラメーターは、MODEL や PRESET を切り替えても値が変わらない。例：Input と Threshold をロックして、同じ押し込み量のまま機種だけを聴き比べる。\n\n| 操作 | 内容 |\n| 数値を右クリック | その項目をロック／解除（右上に鍵の印） |\n| このメニュー | よく使う組をまとめてロック、すべて解除 |\n\nロックの状態はセッションに保存される。手で動かすことは、ロック中でもできる",
             "Locked parameters keep their value when MODEL or PRESET changes — e.g. lock Input and Threshold to compare units at the same drive.\n\n| Action | Result |\n| Right-click a value | lock / unlock it (a padlock appears) |\n| This menu | lock common groups, or unlock all |\n\nLocks are saved with the session. You can still move a locked value by hand");
    for (auto* c : { (juce::Component*) &timeView, (juce::Component*) &transfer, (juce::Component*) &meters, (juce::Component*) &flow,
                     (juce::Component*) &grWave, (juce::Component*) &info })
        addAndMakeVisible (*c);
    addChildComponent (dyn);
    addAndMakeVisible (loud);
    setHelp (loud, "LOUDNESS", "ラウドネス（ITU-R BS.1770 / EBU R 128）。左＝入力、右＝出力。\n\n| 項目 | 意味 |\n| M | 直近 0.4 秒（Momentary） |\n| S | 直近 3 秒（Short-term） |\n| I | リセットからの全体（Integrated、ゲートあり） |\n| TP | 直近 3 秒のトゥルーピーク（4 倍オーバーサンプリング、dBTP） |\n| PSR | TP − S。今の瞬間の打点の余裕。8 以下はかなり詰まっている |\n| PLR | 全体の最大ピーク − I。曲全体のダイナミクスの指標 |\n\n入力と出力の PSR の差が、圧縮で失った打点の量。クリックで I と PLR を測り直す。停止中は最後に鳴った音を今の設定で測り直す",
             "Loudness (ITU-R BS.1770 / EBU R 128). Left = input, right = output.\n\n| Item | Meaning |\n| M | last 0.4 s (momentary) |\n| S | last 3 s (short-term) |\n| I | everything since the reset (integrated, gated) |\n| TP | true peak of the last 3 s (4x oversampled, dBTP) |\n| PSR | TP − S: the punch left right now. 8 or less is dense |\n| PLR | highest peak − I: dynamics of the whole passage |\n\nThe PSR difference between input and output is the punch compression took. Click to restart I and PLR. Stopped: the last sound, re-measured with the current settings");
    addChildComponent (panelView);
    addChildComponent (mini);
    addChildComponent (harm);
    addChildComponent (scView);
    addChildComponent (cover);
    setHelp (harm, "HARMONICS", "サイン波を今の設定（機種・Drive・Oversample など）に通し、出てきた音のスペクトラムを基音＝0 dB として表示する。\n\n| 表示 | 意味 |\n| 橙の点と数字 | 2 次、3 次…の倍音の位置と大きさ |\n| THD | 倍音の合計が基音の何 % か |\n| 倍音以外の成分 | 倍音の間に出る山。多くは折り返し（エイリアシング）。Oversample を上げると減る |\n\n右上で周波数（100 / 1k / 5k Hz）とレベル（−24 / −12 / 0 dBFS）を選ぶ。FET は 2 次、Tube は 3 次、Clipper のハードは高次まで並ぶ。5k・0 dB・Clipper で 1x と 4x を比べると折り返しが見える",
             "A sine goes through the current settings (model, drive, oversampling ...) and the spectrum of the result is drawn with the fundamental at 0 dB.\n\n| Display | Meaning |\n| orange dots and numbers | position and level of the 2nd, 3rd ... harmonic |\n| THD | all harmonics together, as a percentage of the fundamental |\n| between the harmonics | peaks that are not harmonics: mostly aliasing. Oversampling reduces them |\n\nTop right: frequency (100 / 1k / 5k Hz) and level (−24 / −12 / 0 dBFS). FET shows the 2nd, Tube the 3rd, a hard clipper a long row. Clipper at 5k and 0 dB: compare 1x and 4x to see aliasing");
    setHelp (scView, "SC FILTER", "検出器が聴く音の周波数特性（② SIDECHAIN と ③ の 3 帯域検出の合計）。0 dB＝そのまま聴く。下がっている帯域ほど、その音では圧縮がかかりにくい。\n\n| 例 | 効果 |\n| HPF 100 Hz | キックやベースで全体が沈まない |\n| Tilt | 低域に鈍く・高域に敏感に（Thrust） |\n| BP 6 kHz | 歯擦音だけに反応（ディエッサー） |\n\n音そのものの周波数特性は変わらない（Band only を除く）",
             "The frequency response of what the detector hears (② SIDECHAIN plus the three detector bands of ③). 0 dB = heard as it is; the lower the curve, the less that range triggers compression.\n\n| Example | Effect |\n| HPF 100 Hz | kick and bass stop pulling the whole mix down |\n| Tilt | less sensitive to lows, more to highs (Thrust) |\n| BP 6 kHz | reacts to sibilance only (de-esser) |\n\nThe audio's own response does not change (except with Band only)");
    addChildComponent (spectrum);
    setHelp (spectrum, "SPECTRUM", "TIME に表示中の区間（最大 4 秒）の平均スペクトラム。灰＝入力、水色＝出力（音楽が平らに見えるよう +3 dB/oct の傾きを付けて表示）。\n\n| 表示 | 意味 |\n| 下段の帯 | 出力 − 入力。赤＝下がった帯域、緑＝上がった帯域 |\n| 黄の破線 | 3-band やマルチバンド検出の分割周波数 |\n| GR / UP の数字 | 3-band のときの帯域ごとの圧縮量と持ち上げ量 |\n\n全帯域コンプは下段がほぼ平ら、3-band・Band only・アップワードは帯域で差が出る。停止中も今の設定で計算し直す",
             "Average spectrum over the span shown in TIME (up to 4 s). Grey = input, cyan = output (drawn with a +3 dB/oct tilt so music reads flat).\n\n| Display | Meaning |\n| lower strip | output − input: red = reduced, green = raised |\n| dashed lines | crossovers of 3-band / multiband detection |\n| GR / UP numbers | per-band reduction and boost in 3-band mode |\n\nA full-band compressor gives an almost flat strip; 3-band, band-only and upward differ by band. Re-computed while stopped");
    setHelp (dyn, "DYNAMICS", "表示している区間（TIME の 1〜8 秒）の数値。\n\n| 項目 | 意味 |\n| PEAK | 最大の瞬間値 |\n| RMS | 平均の大きさ |\n| CREST | PEAK − RMS。打点の鋭さ。Δ がマイナスなら、圧縮で音が詰まった（密度が上がった） |\n| GR 平均・最大 | 絞った量 |\n| 圧縮している時間 | GR が 0.5 dB を超えていた割合 |\n\n右は GR の分布：どの深さに長くいたか",
             "Numbers for the displayed span (the 1–8 s of TIME).\n\n| Item | Meaning |\n| PEAK | largest instant |\n| RMS | average level |\n| CREST | PEAK − RMS: how spiky. A negative Δ means compression made it denser |\n| GR avg / max | how much was taken off |\n| time compressing | share of the time GR exceeded 0.5 dB |\n\nRight: distribution of GR — where it spent its time");
    outV.setParam (pid::output); outV.format = [] (float v) { return sgnDb (v); };
    addAndMakeVisible (outV);

    setHelp (modelBox, "MODEL", "機種を選ぶと、7 つのブロックの方式と定数がその機種の組み合わせに切り替わる（取り消し可）。その後は自由に変更できる",
             "Choosing a model switches the seven blocks to that unit's methods and constants (undoable). You can then change anything");
    setHelp (presetBtn, "PRESET", "| メニュー | 内容 |\n| 機種別 | その機種の初期設定と、用途別の設定 |\n| 用途別 | ドラム・ベース・ボーカルなど、機種をまたいだ設定 |\n| ユーザー | 自分で保存した設定。保存・削除ができる |\n\n設定を変えると名前に * が付く。数値は出発点で、Threshold や Input は素材に合わせて、説明欄の GR の目安になるよう調整する",
             "| Menu | Contents |\n| By model | that unit's default and task settings |\n| By use | drums, bass, vocal … across units |\n| User | your own saved settings: save and delete |\n\nAn edited preset shows a *. Values are starting points: adjust Threshold or Input until the GR matches the note");
    setHelp (timeView, "TIME", "入力（灰）と出力（水色）のレベルの時間変化。赤い帯が GR で、上から下へ伸びる（レベルと同じ dB 目盛）。黄色の破線が Threshold。右上で 1〜8 秒を切り替え。音が止まると、最後に鳴った音を保持し、設定を変えるたびに今の設定で計算し直す（再生を止めたまま、画面を見ながら調整できる）",
             "Input (grey) and output (cyan) level over time. The red band is GR, hanging from the top on the same dB scale. The dashed line is Threshold. Top right: 1–8 s. When the sound stops, the last sound is kept and re-computed with the current settings whenever you change one — so you can adjust with the transport stopped");
    setHelp (transfer, "TRANSFER", "入力レベル → 出力レベルの静特性。水色の点が今の音の位置。○ をドラッグで Threshold、● をドラッグで Ratio。Ideal 以外では、同じ設定の理想カーブを灰色で重ねる",
             "Static input → output level. The cyan dot is where the signal is now. Drag ○ for Threshold, ● for Ratio. For non-ideal laws the ideal curve is overlaid in grey");
    setHelp (meters, "METERS", "IN（入力ゲイン後）・GR（上から下）・OUT（最終出力）のピーク。停止中は、表示している区間の最大値", "Peaks of IN (after input gain), GR (from the top) and OUT. While stopped: the maximum over the displayed span");
    setHelp (grWave, "GR WAVE",
             "STEP：音が Threshold を超えたとき、GR が目標に向かって動く様子を実際の DSP で測る。Attack は目標の 63% に達するまで（90% は約 2.3 倍、99% は約 4.6 倍）、Release は 63% 戻るまで。黄色の面積が「まだ絞れていない量」。検出方式・Feedback・タイミング方式を含めた実効値が出る。LIVE：再生中（または停止中に保持した）音で同じ測定をする",
             "STEP: measures, on the real DSP, how GR moves toward its target after a level step. Attack = time to 63% (90% ≈ 2.3×, 99% ≈ 4.6×), Release = time to fall by 63%. The yellow area is what is not reduced yet. Detector, feedback and timing method are included. LIVE: the same on the music playing (or held while stopped)");
    setHelp (outV, "OUT", "最終出力のレベル（Auto gain の後）", "Final output level (after Auto gain)");
    setHelp (autoBtn, "AUTO GAIN", "入力と出力の短期的な音量差を自動で埋める。Makeup に惑わされず、音色の違いだけを比べられる。ボタンに今の補正量を表示", "Closes the short-term loudness gap between input and output so you hear tone, not level. The button shows the current correction");
    setHelp (deltaBtn, "DELTA", "圧縮で変わった成分だけを聴く（原音 − 出力）。打点が残っていれば、それは圧縮で削られた成分", "Hear only what the compressor changed (input minus output). Whatever transient you hear here is what was taken away");
    setHelp (bypassBtn, "BYPASS", "圧縮を通さない原音（遅れは揃えたまま）", "The unprocessed signal (delay kept aligned)");
    setHelp (listenBtn, "SC LISTEN", "検出器が聞いている音を聴く（SC HPF・Tilt・帯域フィルター後）。保存されない", "Hear what the detector hears (after SC HPF, tilt and band filter). Not saved");
    setHelp (langBtn, "LANGUAGE", "説明の言語（日本語 / English）", "Language of the help (日本語 / English)");
    setHelp (zoom, "SIZE / LOOK", "画面の大きさ、外観、隙間の質感。大きさは右下の角のドラッグでも変えられる", "Window size, look and surface. Size can also be changed by dragging the bottom-right corner");

    addMouseListener (this, true);
    setWantsKeyboardFocus (true);
    setSize (1160, 760);
    refreshTexts();
    startTimerHz (30);
}

CompView::~CompView()
{
    removeMouseListener (this);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void CompView::mouseDown (const juce::MouseEvent&) { proc.undo.beginNewTransaction(); }

void CompView::applySkin (int i)
{
    theme::setSkin (i);
    ZoomPrefs::file().setValue ("companatomy2.skin", i); ZoomPrefs::file().saveIfNeeded();
    lnf.apply(); sendLookAndFeelChange(); refreshTexts(); repaint();
}
void CompView::applySurface (int i)
{
    theme::surface = juce::jlimit (0, theme::numSurfaces() - 1, i);
    ZoomPrefs::file().setValue ("companatomy2.surface", theme::surface); ZoomPrefs::file().saveIfNeeded();
    repaint();
}

// PRESET menu: by model · by use · user (load / save / delete)
void CompView::showPresetMenu()
{
    const bool ja = ph.ja(); const auto& ps = preset::all(); const auto& ms = mod::models();
    auto U = [] (const char* s) { return juce::String::fromUTF8 (s); };
    juce::PopupMenu m, byModel, byUse, del;
    for (auto& grp : mod::menuGroups())                                 // the same grouping as the MODEL menu
    {
        byModel.addSectionHeader (U (grp.title));
        for (auto* gid : grp.ids)
        {
            const int mi = mod::modelIndexOf (gid); if (mi < 0) continue;
            juce::PopupMenu sub; sub.addItem (5000 + mi, ja ? U ("初期設定") : juce::String ("Default"));
            for (int i = 0; i < (int) ps.size(); ++i) if (preset::modelIndex (ps[(size_t) i].model) == mi) sub.addItem (1000 + i, U (ja ? ps[(size_t) i].ja : ps[(size_t) i].en));
            byModel.addSubMenu (ms[(size_t) mi].name, sub);
        }
    }
    for (int u = 0; u < (int) preset::NumUses; ++u)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) ps.size(); ++i)
            if (ps[(size_t) i].use == u) { const int mi = preset::modelIndex (ps[(size_t) i].model); sub.addItem (1000 + i, U (ja ? ps[(size_t) i].ja : ps[(size_t) i].en) + "   [" + juce::String (mi >= 0 ? ms[(size_t) mi].name : "") + "]"); }
        byUse.addSubMenu (U (preset::useName (u, ja)), sub);
    }
    m.addSubMenu (ja ? U ("機種別") : juce::String ("By model"), byModel);
    m.addSubMenu (ja ? U ("用途別（機種をまたぐ）") : juce::String ("By use (across models)"), byUse);
    m.addSeparator();
    m.addSectionHeader (ja ? U ("ユーザープリセット") : juce::String ("User presets"));
    const auto users = proc.userPresets();
    const auto cur = (int) proc.uiState.getProperty ("presetKind", 0) == 2 ? juce::File::createLegalFileName (proc.uiState.getProperty ("userPreset", "").toString()) : juce::String();
    for (int i = 0; i < users.size(); ++i) { m.addItem (3000 + i, users[i].getFileNameWithoutExtension(), true, users[i].getFileNameWithoutExtension() == cur); del.addItem (4000 + i, users[i].getFileNameWithoutExtension()); }
    if (users.isEmpty()) m.addItem (-1, ja ? U ("（まだありません）") : juce::String ("(none yet)"), false);
    m.addSeparator();
    m.addItem (1, ja ? U ("現在の設定を保存…") : juce::String ("Save current settings…"));
    m.addSubMenu (ja ? U ("削除（ゴミ箱へ）") : juce::String ("Delete (to Trash)"), del, ! users.isEmpty());
    m.addItem (2, ja ? U ("プリセットのフォルダーを開く") : juce::String ("Show the preset folder"));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this, users] (int r)
    {
        if (r >= 5000) proc.applyModel (r - 5000);
        else if (r >= 4000 && r - 4000 < users.size()) proc.deleteUserPreset (users[r - 4000]);
        else if (r >= 3000 && r - 3000 < users.size()) proc.loadUserPreset (users[r - 3000]);
        else if (r >= 1000) proc.applyPreset (r - 1000);
        else if (r == 1) savePresetDialog();
        else if (r == 2) { auto d = CompProcessor::userPresetDir(); d.createDirectory(); d.revealToUser(); }
        refreshTexts();
    });
}

// ◀ ▶: every preset in menu order — per model (by group): its default, then its presets; then the user presets
void CompView::stepPreset (int dir)
{
    struct E { int kind, index; juce::File file; };
    std::vector<E> list; const auto& ps = preset::all();
    for (auto& grp : mod::menuGroups())
        for (auto* gid : grp.ids)
        {
            const int mi = mod::modelIndexOf (gid); if (mi < 0) continue;
            list.push_back ({ 0, mi, {} });
            for (int i = 0; i < (int) ps.size(); ++i) if (preset::modelIndex (ps[(size_t) i].model) == mi) list.push_back ({ 1, i, {} });
        }
    for (auto& f : proc.userPresets()) list.push_back ({ 2, -1, f });
    if (list.empty()) return;
    const int kind = (int) proc.uiState.getProperty ("presetKind", 0);
    const auto user = juce::File::createLegalFileName (proc.uiState.getProperty ("userPreset", "").toString());
    int cur = -1;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& e = list[(size_t) i];
        if (e.kind != kind) continue;
        if ((kind == 0 && e.index == proc.currentModel()) || (kind == 1 && e.index == (int) proc.uiState.getProperty ("preset", -1)) || (kind == 2 && e.file.getFileNameWithoutExtension() == user)) { cur = i; break; }
    }
    const int n = (int) list.size(), nxt = cur < 0 ? (dir > 0 ? 0 : n - 1) : ((cur + dir) % n + n) % n;
    const auto& e = list[(size_t) nxt];
    if (e.kind == 0) proc.applyModel (e.index); else if (e.kind == 1) proc.applyPreset (e.index); else proc.loadUserPreset (e.file);
    refreshTexts();
}

void CompView::savePresetDialog()
{
    const bool ja = ph.ja(); auto U = [] (const char* s) { return juce::String::fromUTF8 (s); };
    const bool user = (int) proc.uiState.getProperty ("presetKind", 0) == 2;
    auto* w = new juce::AlertWindow (ja ? U ("ユーザープリセットを保存") : juce::String ("Save user preset"),
                                     ja ? U ("名前と、用途のメモ（説明欄に出る）。同じ名前は上書き") : juce::String ("A name, and a note on what it is for (shown in the text panel). The same name overwrites"),
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", user ? proc.uiState.getProperty ("userPreset", "").toString() : juce::String(), ja ? U ("名前") : juce::String ("Name"));
    w->addTextEditor ("note", user ? proc.uiState.getProperty ("presetNote", "").toString() : juce::String(), ja ? U ("メモ") : juce::String ("Note"));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ja ? U ("キャンセル") : juce::String ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int b)
    {
        if (b != 1) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isNotEmpty()) proc.saveUserPreset (name, w->getTextEditorContents ("note"));
        refreshTexts();
    }), true);
}

// LOCK menu: lock common groups of parameters against model / preset changes
void CompView::showLockMenu()
{
    const bool ja = ph.ja(); auto U = [] (const char* s) { return juce::String::fromUTF8 (s); };
    struct Group { const char* ja; const char* en; std::vector<const char*> ids; };
    static const std::vector<Group> groups = {
        { "レベル（Input・Threshold・Makeup）", "Levels (Input, Threshold, Makeup)", { pid::input, pid::threshold, pid::makeup } },
        { "Threshold だけ", "Threshold only", { pid::threshold } },
        { "Dry / Wet", "Dry / Wet", { pid::mix } },
        { "サイドチェーン（② のフィルターとリンク）", "Sidechain (filters and link of ②)", { pid::scHpf, pid::scTilt, pid::link, pid::band, pid::bandHz, pid::bandQ, pid::bandOnly } },
        { "Attack・Release", "Attack, Release", { pid::attack, pid::release } },
        { "オーバーサンプリングと先読み", "Oversampling and look-ahead", { pid::os, pid::lookahead } },
        { "アップワード", "Upward", { pid::upOn, pid::upThr, pid::upRatio, pid::upRange } },
        { "3 バンド（分割と帯域ゲイン）", "3-band (crossovers and band gains)", { pid::mb, pid::xLo, pid::xHi, pid::mbLo, pid::mbMid, pid::mbHi } } };
    juce::PopupMenu m;
    m.addSectionHeader (ja ? U ("MODEL・PRESET を切り替えても変えない項目") : juce::String ("Keep these when MODEL / PRESET changes"));
    for (int i = 0; i < (int) groups.size(); ++i)
    {
        bool all = true; for (auto* id : groups[(size_t) i].ids) all = all && proc.isLocked (id);
        m.addItem (10 + i, U (ja ? groups[(size_t) i].ja : groups[(size_t) i].en), true, all);
    }
    m.addSeparator();
    m.addItem (1, ja ? U ("すべて解除") : juce::String ("Unlock all"), proc.numLocked() > 0);
    m.addItem (-1, ja ? U ("個別のロック：数値を右クリック") : juce::String ("Single values: right-click the value"), false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&lockBtn), [this] (int r)
    {
        if (r == 1) proc.clearLocks();
        else if (r >= 10 && r - 10 < (int) groups.size())
        {
            bool all = true; for (auto* id : groups[(size_t) (r - 10)].ids) all = all && proc.isLocked (id);
            for (auto* id : groups[(size_t) (r - 10)].ids) proc.setLocked (id, ! all);
        }
        refreshTexts(); repaint();
    });
}

void CompView::refreshTexts()
{
    langBtn.setButtonText (ph.ja() ? juce::String::fromUTF8 ("日本語") : juce::String ("English"));
    if (! modelBox.isPopupActive())
    {
        const int m = proc.currentModel();
        if (m >= 0 && m < (int) mod::models().size() && modelBox.getSelectedId() != m + 1) modelBox.setSelectedId (m + 1, juce::dontSendNotification);
    }
    const auto label = proc.presetLabel (ph.ja());
    presetBtn.getProperties().set ("fontPx", 11.0);
    presetBtn.setButtonText (label.isEmpty() ? juce::String::fromUTF8 ("PRESET  \xe2\x96\xbe") : label + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
    lockBtn.setButtonText (proc.numLocked() > 0 ? "LOCK " + juce::String (proc.numLocked()) : juce::String ("LOCK"));
    lockBtn.setToggleState (proc.numLocked() > 0, juce::dontSendNotification);
    listenBtn.setButtonText ("SC LISTEN"); deltaBtn.setButtonText ("Delta"); bypassBtn.setButtonText ("Bypass");
    for (int i = 0; i < 4; ++i) slotBtn[i].setToggleState (i == proc.currentSlot(), juce::dontSendNotification);
    if (! proc.idealAvailable()) proc.idealOn = false;
    idealBtn.setButtonText ("IDEAL"); idealBtn.setEnabled (proc.idealAvailable()); idealBtn.setToggleState (proc.idealOn.load(), juce::dontSendNotification);
    const bool ag = ph.value (pid::autoGain) > 0.5f;
    autoBtn.getProperties().set ("fontPx", 10.0);
    autoBtn.setButtonText (ag ? "Auto " + sgnDb (proc.autoGainDbNow()) : juce::String ("Auto gain"));
    listenBtn.setToggleState (ph.value (pid::scListen) > 0.5f, juce::dontSendNotification);
    autoBtn.setToggleState (ag, juce::dontSendNotification);
    deltaBtn.setToggleState (ph.value (pid::delta) > 0.5f, juce::dontSendNotification);
    bypassBtn.setToggleState (ph.value (pid::bypass) > 0.5f, juce::dontSendNotification);
}

void CompView::paint (juce::Graphics& g)
{
    theme::paintSurface (g, getLocalBounds());
    g.setColour (theme::header); g.fillRect (0, 0, getWidth(), 44);
    g.setColour (theme::line); g.drawHorizontalLine (44, 0, (float) getWidth());
    g.setColour (theme::panel); g.fillRoundedRectangle (outV.getBounds().toFloat(), 5.0f);          // OUT sits on the surface: give it a panel so its text reads in every look
    g.setColour (theme::line); g.drawRoundedRectangle (outV.getBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
    g.setColour (theme::text); g.setFont (theme::mono (11.5f, true));
    g.drawText ("COMP ANATOMY LAB 2", 12, 6, 150, 20, juce::Justification::centredLeft);
    g.setColour (theme::faint); g.setFont (theme::ui (9.0f));
    g.drawText ("by Johan Plus Kei", 12, 24, 150, 14, juce::Justification::centredLeft);
    const int lat = proc.getLatencySamples(); const double fs = proc.getSampleRateOrDefault();
    g.setFont (theme::mono (10.0f)); g.setColour (lat > 0 ? theme::pre : theme::muted);
    g.drawText ("LAT " + (lat > 0 ? juce::String (lat * 1000.0 / fs, 1) + " ms" : juce::String ("0 ms")), 612, 6, 90, 16, juce::Justification::centredLeft);
    g.drawText ("OS " + juce::String (proc.currentParams().os) + "x", 612, 24, 70, 14, juce::Justification::centredLeft);
    g.setColour (theme::muted);
    g.drawText ("GR " + minus (-meters.grNow()) + " dB", 702, 6, 100, 16, juce::Justification::centredLeft);
    g.setColour (proc.sidechainConnected() ? theme::ok : theme::faint);
    g.drawText (proc.sidechainConnected() ? "SC IN on" : "SC IN off", 702, 24, 100, 14, juce::Justification::centredLeft);
}

void CompView::resized()
{
    zoom.setBounds (162, 12, 46, 20);                    // right after the title, as in the Delay labs
    modelBox.setBounds (218, 10, 150, 24);
    prevBtn.setBounds (374, 10, 24, 24); presetBtn.setBounds (400, 10, 174, 24); nextBtn.setBounds (576, 10, 24, 24);
    listenBtn.setBounds (816, 10, 84, 24);
    lockBtn.setBounds (904, 10, 70, 24);
    for (int i = 0; i < 4; ++i) slotBtn[i].setBounds (980 + i * 26, 10, 24, 24);
    langBtn.setBounds (1090, 10, 58, 24);
    timeView.setBounds (12, 54, 740, 250); spectrum.setBounds (12, 54, 740, 250);
    transfer.setBounds (760, 54, 300, 250);
    meters.setBounds (1068, 54, 80, 250);
    flow.setBounds (12, 312, 1136, 236); panelView.setBounds (12, 312, 1136, 236);
    const int y = 556;
    grWave.setBounds (12, y, 340, 192); dyn.setBounds (12, y, 340, 192); mini.setBounds (12, y, 340, 192); harm.setBounds (12, y, 340, 192); scView.setBounds (12, y, 340, 192);
    const int bw = 76, bx = 1148 - bw;                                // output and listening buttons: a narrow column right of the info card
    info.setBounds (362, y, bx - 10 - 110 - 10 - 362, 192); loud.setBounds (bx - 10 - 110, y, 110, 192);
    outV.setBounds (bx, y, bw, 36);
    autoBtn.setBounds (bx, y + 39, bw, 36);
    deltaBtn.setBounds (bx, y + 78, bw, 36); idealBtn.setBounds (bx, y + 117, bw, 36);
    bypassBtn.setBounds (bx, y + 156, bw, 36);
    cover.setBounds (getLocalBounds());                               // ear training: everything that would give the answer away
    cover.rects = { { 12, 54, 1136, 250 }, { 12, 312, 1136, 236 }, { 12, y, 340, 192 }, loud.getBounds(), { 606, 4, 204, 37 } };
}

bool CompView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.undo.redo(); return true; }
    const int kc = k.getKeyCode();
    if ((kc == juce::KeyPress::leftKey || kc == juce::KeyPress::rightKey) && k.getModifiers().isCommandDown()) { stepPreset (kc == juce::KeyPress::rightKey ? 1 : -1); return true; }
    if (kc == juce::KeyPress::upKey || kc == juce::KeyPress::downKey)      // the value under the pointer (or the last one pointed at); Shift = finer
    {
        auto* p = ph.get (keyPid); if (p == nullptr || proc.isLocked (keyPid)) return false;
        const float dir = kc == juce::KeyPress::upKey ? 1.0f : -1.0f; const bool fine = k.getModifiers().isShiftDown();
        const auto lab = p->getLabel(); const float v = ph.value (keyPid); const auto rg = p->getNormalisableRange();
        float nv;
        if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr) nv = dir > 0 ? 1.0f : 0.0f;
        else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p)) nv = (float) juce::jlimit (0, c->choices.size() - 1, c->getIndex() + (int) dir);
        else if (lab == "Hz" && rg.start > 0) nv = v * std::pow (2.0f, dir * (fine ? 1.0f / 48.0f : 1.0f / 12.0f));
        else if (lab == "dB") nv = v + dir * (fine ? 0.1f : 0.5f);
        else if ((lab == "ms" || lab == ":1" || lab == "x") && rg.start > 0) nv = v * std::pow (fine ? 1.02f : 1.1f, dir);
        else nv = v + dir * (fine ? 0.2f : 1.0f) * std::max (0.01f, (rg.end - rg.start) / 100.0f);
        proc.undo.beginNewTransaction();
        ph.setGesture (keyPid, rg.snapToLegalValue (juce::jlimit (rg.start, rg.end, nv)));
        return true;
    }
    return false;
}

void CompView::updateHover()
{
    juce::String t, x;
    if (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
        if (c == this || isParentOf (c))
        {
            for (auto* k = c; k != nullptr && k != this; k = k->getParentComponent())
                if (k->getProperties().contains ("pid")) { keyPid = k->getProperties()["pid"].toString(); break; }
            for (auto* k = c; k != nullptr && k != this; k = k->getParentComponent())
                if (k->getProperties().contains ("helpT"))
                {
                    t = k->getProperties()["helpT"].toString();
                    x = k->getProperties()[ph.ja() ? "helpJ" : "helpE"].toString();
                    for (auto* r = c; r != nullptr && r != this; r = r->getParentComponent())
                        if (r->getProperties().contains ("resettable")) { x += ph.tr ("　／Option＋クリック：初期値", "  /  Option+click: default value"); break; }
                    if (k->getProperties().contains ("pid")) x += ph.tr ("　／↑↓：値を 1 段ずつ（Shift で細かく）", "  /  ↑↓: step the value (Shift: finer)");
                    break;
                }
        }
    ph.hoverTitle = t; ph.hoverText = x;
}

void CompView::timerCallback()
{
    proc.updatePreview (TimeView::spanSec (timeView.spanIdx()));     // stopped → the last sound, re-computed with the current settings
    updateHover();
    refreshTexts();
    meters.tick();
    if (const int vm = viewModeOf (proc.uiState); vm != lastView)      // going to BUILD: the model's controls move to the lower-left view; leaving BUILD: back to what was there
    {
        const int lt = (int) proc.uiState.getProperty ("lowerTab", 0);
        if (lastView >= 0 && vm == 4 && lt != 2) { prevLower = lt; proc.uiState.setProperty ("lowerTab", 2, nullptr); }
        else if (lastView == 4 && vm != 4 && lt == 2) proc.uiState.setProperty ("lowerTab", prevLower, nullptr);
        lastView = vm;
    }
    const int lowerTab = juce::jlimit (0, 4, (int) proc.uiState.getProperty ("lowerTab", 0));
    grWave.setVisible (lowerTab == 0); dyn.setVisible (lowerTab == 1); mini.setVisible (lowerTab == 2); harm.setVisible (lowerTab == 3); scView.setVisible (lowerTab == 4);
    if (lowerTab == 1) dyn.refresh(); else if (lowerTab == 2) mini.refresh(); else if (lowerTab == 3) harm.refresh(); else if (lowerTab == 4) scView.refresh(); else grWave.refresh();
    const bool specTab = (int) proc.uiState.getProperty ("upperTab", 0) == 1;
    timeView.setVisible (! specTab); spectrum.setVisible (specTab);
    if (specTab) spectrum.refresh(); else timeView.repaint();
    transfer.repaint();
    const bool panelMode = viewModeOf (proc.uiState) != 4;
    if (const int m = proc.currentModel(); m != idsFor) { idsFor = m; flow.panelIds = m >= 0 && m < (int) mod::models().size() ? panel::ids (mod::models()[(size_t) m].id) : juce::StringArray(); }
    panelView.setVisible (panelMode);                                 // the panel is see-through: the signal flow stays underneath
    flow.refresh(); if (panelMode) panelView.refresh();
    outV.repaint(); info.tick(); loud.repaint();
    if (const bool cv = info.covering(); cv != cover.isVisible()) { cover.setVisible (cv); if (cv) cover.toFront (false); }
    repaint (608, 0, 200, 44);
}
