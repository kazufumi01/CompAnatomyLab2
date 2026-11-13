# Compressor Anatomy Lab 2 β

**English** | [日本語](README.ja.md)

by Johan Plus Kei

A free compressor-learning plug-in that lets you **build, visualize, and listen to** compressor designs. Available in **VST3, AU, and Standalone** formats.

Build 21 compressor models by combining seven processing blocks.

![Compressor Anatomy Lab 2 — PANEL](docs/screenshot-panel.png)

## Download the beta

**[Download the latest release](https://github.com/kazufumi01/CompAnatomyLab2/releases/latest)** — Free

| Platform | Manual installation (recommended) | Installer |
|---|---|---|
| **macOS** (11+, Apple Silicon / Intel) | `CompAnatomyLab2-0.2.0-mac.zip` | `CompAnatomyLab2-0.2.0-mac.pkg` |
| **Windows** (10 / 11, 64-bit) ⚠️ Untested | `CompAnatomyLab2-0.2.0-win64.zip` | `CompAnatomyLab2-0.2.0-win64.exe` |

- **Installation guide:** [INSTALL.md](INSTALL.md) (English / 日本語)
- **Japanese introduction:** [Build and learn 21 compressor designs in one plug-in](https://jpk.seesaa.net/article/522006512.html)
- ⚠️ **The Windows build has not been tested on physical Windows hardware.** It is an automated VST3 and Standalone build.
- The macOS build is not notarized and the Windows build is not code-signed. Your operating system may display a security warning on first launch.
- Report bugs or request features via [GitHub Issues](https://github.com/kazufumi01/CompAnatomyLab2/issues).

---

## Features

### Build a compressor from seven processing blocks

Swap the design or behavior of each block:

1. **INPUT**
2. **SIDECHAIN**
3. **DETECTOR**
4. **GAIN LAW**
5. **TIMING**
6. **ELEMENT**
7. **OUTPUT**

Available options include:

- **Detection:** Peak, RMS, Diode + RC, Peak + RMS; feed-forward and feedback topologies
- **Gain curves:** Ideal, FET, Opto, Vari-Mu, Diode, Limiter, Rider, Spectral, Clipper, Upward, and GR Limit
- **Timing:** Single-stage, two-stage (program-dependent), optical cell, Auto, Precise (look-ahead), Dual Average, release curves, and Look-behind
- **Nonlinear elements and output stages:** Clean, FET, Tube, Opto, Diode; Nickel / Iron / Steel transformers; harmonic generation; 1x / 2x / 4x oversampling; two compressors in series

### 21 compressor models

Explore models such as FET 76, Opto 2A / 3A, Vari-Mu 670, Bus VCA G, VCA 160 / 2500, Channel VCA E, Diode 254, EL 8, Tube Opto 1B, Digital DS-1, Dual Opto + VCA, Limiter, Clipper, Spectral, Upward, OTT 3-Band, and Rider.

Model names refer to the design families used as references. Initial parameter values are based on published technical information; they are not claimed to be exact hardware component values.

### PANEL

The PANEL displays only the controls associated with the selected model, using model-inspired labels, scales, and colors. Its transparent layout lets you compare the front panel with the underlying BUILD configuration. Controls that affect the PANEL are highlighted in BUILD.

### Visual analysis

- **TIME:** Input/output levels and gain reduction
- **SPECTRUM:** Frequency-dependent level changes
- **TRANSFER:** Static transfer curve, with direct manipulation
- **GR WAVE:** Attack and release behavior
- **DYNAMICS:** Peak, RMS, crest factor, and range
- **HARMONICS:** Harmonic generation and aliasing
- **SC FILTER:** The signal heard by the detector
- **LOUDNESS:** Momentary / Short-term / Integrated loudness, true peak, PSR, and PLR

### Learn by listening

- **IDEAL comparison:** Switch to a neutral compressor with a similar amount of gain reduction to hear the character added by the selected design.
- **26 lessons:** Load guided settings and explore compressor behavior step by step.
- **EAR training:** Identify Attack, Release, Ratio, and compressor models by listening.
- **62 presets:** Explore model-based and application-based presets, plus user presets.
- **A / B / C / D slots** and parameter locking for comparisons.
- Recalculate the last-played audio using the current settings, even while playback is stopped.
- Auto Gain and Delta (difference) monitoring.
- Fine parameter adjustment with the arrow keys; 14 color themes; Japanese / English interface.

![BUILD](docs/screenshot-build.png)

![Spectral compression](docs/screenshot-spectral.png)

![Lesson](docs/screenshot-lesson.png)

---

## Build from source

```bash
./build.sh
```

The script uses a local JUCE installation at `~/Documents/jpkplugins/JUCE` if available. Otherwise, CMake downloads JUCE 9.0.4.

- **Windows:** Run the GitHub Actions workflow `.github/workflows/build-win.yml` manually to build VST3 / Standalone and package the installer with Inno Setup.
- **macOS installers:** `packaging/mac/build_pkg.sh [version]` and `packaging/mac/build_zip.sh [version]` (output is written to `dist/`).

## License

JUCE is used under the **JUCE Starter license**. Distributed builds are subject to the freeware license in `packaging/mac/resources/license.txt`.

The source code is published for learning purposes. Redistribution or resale without permission is not allowed.

---

**Johan Plus Kei** — [Spotify](https://open.spotify.com/artist/30PV2gqUbDDwvuWtKaok0x) · [Apple Music](https://music.apple.com/us/artist/johan-plus-kei/519360150) · [X](https://x.com/johanpluskei) · [Instagram](https://www.instagram.com/johanpluskei) · [Learning labs](https://kazufumi01.github.io/)

## Support

Compressor Anatomy Lab 2 is free. A one-time tip via [PayPal](https://paypal.me/johanpluskei) is welcome. Even more helpful: share the project and listen to my music on [Spotify](https://open.spotify.com/artist/30PV2gqUbDDwvuWtKaok0x).

More ways to support: <https://kazufumi01.github.io/support/>

