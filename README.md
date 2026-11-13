# Compressor Anatomy Lab 2 β

**by Johan Plus Kei** · VST3 / AU / Standalone · 無料ベータ / Free beta

🇯🇵 コンプの中身を「組み立てて・見て・聴いて」学べるプラグイン。1 台で 21 機種のコンプを、7 つのブロックの組み合わせとして再現します。
🇬🇧 A plug-in for learning compressors by building, seeing and hearing them. It recreates 21 compressor types as combinations of seven building blocks.

**[日本語](#日本語) · [English](#english) · [ダウンロード / Download](#ダウンロード--download) · [開発者向け / For developers](#開発者向け--for-developers)**

![Compressor Anatomy Lab 2 — PANEL](docs/screenshot-panel.png)

## ダウンロード / Download

**[最新版をダウンロード / Download latest (0.2.0 beta)](https://github.com/kazufumi01/CompAnatomyLab2/releases/latest)**

| | 手動インストール（おすすめ）<br>Manual (recommended) | インストーラー<br>Installer |
|---|---|---|
| **Mac**<br>macOS 11+, Apple Silicon / Intel | `CompAnatomyLab2-0.2.0-mac.zip` | `CompAnatomyLab2-0.2.0-mac.pkg` |
| **Windows** ⚠️ 未検証 / untested<br>10 / 11, 64-bit | `CompAnatomyLab2-0.2.0-win64.zip` | `CompAnatomyLab2-0.2.0-win64.exe` |

| | 日本語 | English |
|---|---|---|
| インストール方法 / How to install | [INSTALL.md（日本語）](INSTALL.md#日本語) | [INSTALL.md (English)](INSTALL.md#english) |
| 紹介記事 / Article | [1台で21機種のコンプを組み立てて学ぶ](https://jpk.seesaa.net/article/522006512.html) | — |
| 不具合・要望 / Bugs & requests | [Issues](https://github.com/kazufumi01/CompAnatomyLab2/issues) | [Issues](https://github.com/kazufumi01/CompAnatomyLab2/issues) |

> **ご注意 / Notes**
> - ベータ版です。Mac の Ableton Live で動作確認済み。<br>This is a beta, tested with Ableton Live on Mac.
> - ⚠️ Windows 版は自動ビルドのみで、実機での動作確認はできていません（VST3 / Standalone）。<br>The Windows build (VST3 / Standalone) is built automatically and has not been tested on real hardware.
> - Apple の公証・Windows のコード署名をしていないため、初回のみ警告が出ます（回避手順は INSTALL.md）。<br>The builds are not notarized or code-signed, so you will see a one-time warning (see INSTALL.md).

---

## 日本語

### 主な機能

- **7 つのブロックでコンプを組み立てる（BUILD）**：① INPUT ② SIDECHAIN ③ DETECTOR ④ GAIN LAW ⑤ TIMING ⑥ ELEMENT ⑦ OUTPUT の「方式」を差し替える
  - 検出：Peak / RMS / Diode+RC / Peak+RMS、フィードフォワード / フィードバック
  - ゲインカーブ：Ideal / FET / Opto / Vari-Mu / Diode / Limiter / Rider / Spectral / Clipper、Upward、GR Limit
  - タイミング：1 段 / 2 段（プログラム依存）/ 光学セル / Auto / Precise（先読み）/ Dual average、リリースカーブ、Look-behind
  - 素子と出力段：Clean / FET / Tube / Opto / Diode、トランス（Nickel / Iron / Steel）、倍音、オーバーサンプリング 1x / 2x / 4x、直列 2 段
- **21 の MODEL**（実機名は参考にした機種。定数は文献ベースの初期値で、回路の再現ではありません。製品名は各社の商標）

  | MODEL | 参考にした実機 |
  |---|---|
  | FET 76 | Universal Audio 1176 |
  | Opto 2A / Opto 3A | Teletronix LA-2A / UREI LA-3A |
  | Tube Opto 1B | Tube-Tech CL 1B |
  | Vari-Mu 670 | Fairchild 670 |
  | Bus VCA G / Channel VCA E | SSL G シリーズ バスコンプ / SSL 4000 E チャンネル |
  | VCA 160 / VCA 2500 | dbx 160 / API 2500 |
  | EL 8 | Empirical Labs Distressor |
  | Diode 254 | Neve 2254 / 33609 |
  | Dual Opto + VCA | Shadow Hills Mastering Compressor |
  | Digital DS-1 / Peak + RMS | Weiss DS1-MK3 / TDR Kotelnikov |
  | OTT 3-Band / Rider | Xfer OTT / Waves Vocal Rider |
  | Digital Precision・Limiter・Clipper・Spectral・Upward | 方式の比較用（特定の機種なし） |
- **PANEL**：その機種の実機にある操作子だけを、実機の目盛りと配色で表示。透過して下の BUILD と見比べられ、BUILD 側では PANEL が動かす項目を強調
- **表示**：TIME（入出力レベルと GR）、SPECTRUM（周波数ごとの増減）、TRANSFER（静特性、ドラッグで操作）、GR WAVE（Attack / Release の定義）、DYNAMICS（PEAK / RMS / CREST / RANGE）、HARMONICS（倍音と折り返し）、SC FILTER（検出器が聴く音）、LOUDNESS（M / S / I、トゥルーピーク、PSR / PLR）
- **IDEAL 比較**：同じくらいの GR 量のクセのないコンプに切り替えて、機種の「クセ」だけを聴く
- **LESSON 26 本**（設定の読み込み付き）と **EAR（耳トレ）**：Attack / Release / Ratio / 機種当て
- **プリセット 62 本**（機種別・用途別）とユーザープリセット、◀ ▶ で順送り。A / B / C / D スロット、パラメーターロック
- 停止中も最後に鳴った音を今の設定で再計算して表示、Auto gain、Delta（差分）試聴
- ↑↓ キーで値を 1 段ずつ（Shift で細かく）、配色 14 種、日本語 / English

![BUILD](docs/screenshot-build.png)

![Spectral compression](docs/screenshot-spectral.png)

![Lesson](docs/screenshot-lesson.png)

---

## English

### Features

- **Build a compressor from seven blocks (BUILD)**: swap the method used in ① INPUT ② SIDECHAIN ③ DETECTOR ④ GAIN LAW ⑤ TIMING ⑥ ELEMENT ⑦ OUTPUT.
  - Detection: Peak / RMS / Diode+RC / Peak+RMS, feed-forward / feedback
  - Gain law: Ideal / FET / Opto / Vari-Mu / Diode / Limiter / Rider / Spectral / Clipper, Upward, GR Limit
  - Timing: single / two-stage (program-dependent) / optical cell / Auto / Precise (look-ahead) / Dual average, release curves, look-behind
  - Element and output stage: Clean / FET / Tube / Opto / Diode, transformer (Nickel / Iron / Steel), harmonics, 1x / 2x / 4x oversampling, two units in series
- **21 MODELS** (hardware names show what each model is based on; constants are literature-based starting values, not circuit recreations; product names are trademarks of their owners)

  | MODEL | Based on |
  |---|---|
  | FET 76 | Universal Audio 1176 |
  | Opto 2A / Opto 3A | Teletronix LA-2A / UREI LA-3A |
  | Tube Opto 1B | Tube-Tech CL 1B |
  | Vari-Mu 670 | Fairchild 670 |
  | Bus VCA G / Channel VCA E | SSL G-Series bus compressor / SSL 4000 E channel |
  | VCA 160 / VCA 2500 | dbx 160 / API 2500 |
  | EL 8 | Empirical Labs Distressor |
  | Diode 254 | Neve 2254 / 33609 |
  | Dual Opto + VCA | Shadow Hills Mastering Compressor |
  | Digital DS-1 / Peak + RMS | Weiss DS1-MK3 / TDR Kotelnikov |
  | OTT 3-Band / Rider | Xfer OTT / Waves Vocal Rider |
  | Digital Precision, Limiter, Clipper, Spectral, Upward | Method references (no specific unit) |
- **PANEL**: shows only the controls the real unit has, with its scales and colours. It is see-through, so you can compare it with BUILD underneath; BUILD highlights what the panel is moving.
- **Displays**: TIME (in/out level and GR), SPECTRUM (gain change per frequency), TRANSFER (static curve, draggable), GR WAVE (how Attack / Release are defined), DYNAMICS (PEAK / RMS / CREST / RANGE), HARMONICS (harmonics and aliasing), SC FILTER (what the detector hears), LOUDNESS (M / S / I, true peak, PSR / PLR).
- **IDEAL compare**: switch to a neutral compressor with about the same gain reduction, so you hear only the model's character.
- **26 LESSONS** (each loads its settings) and **EAR training**: Attack / Release / Ratio / guess the model.
- **62 presets** (by model and by use) plus user presets, ◀ ▶ stepping, A / B / C / D slots and parameter lock.
- Even when stopped, the last audio is re-processed with the current settings and shown. Auto gain and Delta listening.
- ↑↓ keys step values (Shift for fine steps), 14 skins, Japanese / English UI.

![BUILD](docs/screenshot-build.png)

![Spectral compression](docs/screenshot-spectral.png)

![Lesson](docs/screenshot-lesson.png)

---

## 開発者向け / For developers

```bash
./build.sh   # 手元の JUCE（~/Documents/jpkplugins/JUCE）を使う。無ければ CMake が JUCE 9.0.4 を取得
             # uses local JUCE (~/Documents/jpkplugins/JUCE); otherwise CMake fetches JUCE 9.0.4
```

- Windows：GitHub Actions（`.github/workflows/build-win.yml`、手動実行）で VST3 / Standalone をビルドし、Inno Setup でインストーラーを作成。<br>Windows: built by GitHub Actions (`.github/workflows/build-win.yml`, run manually); the installer is made with Inno Setup.
- Mac：`packaging/mac/build_pkg.sh [version]` ／ `packaging/mac/build_zip.sh [version]`（出力は `dist/` / output in `dist/`）

### ライセンス / License

- JUCE は **Starter ライセンス** で使用。配布物はフリーウェア使用許諾（`packaging/mac/resources/license.txt`）に従います。ソースは学習目的で公開していますが、無断での再配布・販売はできません。
- JUCE is used under the **Starter licence**. The binaries are freeware under `packaging/mac/resources/license.txt`. The source is public for learning; redistribution or sale without permission is not allowed.

---

## 気に入ったら / If you like it

**🇯🇵 気に入ったら、曲の再生とページの拡散をお願いします！** Compressor Anatomy Lab 2 は無料です。友人や授業へのシェア、[ブログ記事](https://jpk.seesaa.net/article/521891797.html)の拡散、曲の再生が何よりの応援になります。

**🇬🇧 If you like it, please play my music and share this page!** Compressor Anatomy Lab 2 is free, and streams and shares help more than anything else.

[![Listen on Spotify](https://img.shields.io/badge/Spotify-Listen-1DB954?logo=spotify&logoColor=white)](https://open.spotify.com/artist/30PV2gqUbDDwvuWtKaok0x)
[![Apple Music](https://img.shields.io/badge/Apple_Music-Listen-FA243C?logo=applemusic&logoColor=white)](https://music.apple.com/us/artist/johan-plus-kei/519360150)
[![Linktree](https://img.shields.io/badge/Linktree-All_links-43E55E?logo=linktree&logoColor=white)](https://linktr.ee/jpk01)
[![X](https://img.shields.io/badge/X-Follow-000000?logo=x&logoColor=white)](https://x.com/johanpluskei)
[![Instagram](https://img.shields.io/badge/Instagram-Follow-E4405F?logo=instagram&logoColor=white)](https://www.instagram.com/johanpluskei)

**Johan Plus Kei** — dream pop / indie pop / post-rock · [Learning labs](https://kazufumi01.github.io/)

PayPal からの投げ銭も歓迎です / Tips via [PayPal](https://paypal.me/johanpluskei) are welcome too. More: <https://kazufumi01.github.io/support/>
