# Harmonic Compressor

[![Version](https://img.shields.io/badge/version-1.5.2-blue.svg)](VERSION)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Format](https://img.shields.io/badge/formats-VST3%20%7C%20AU%20%7C%20AAX%20%7C%20CLAP%20%7C%20LV2-brightgreen.svg)]()
[![Platform](https://img.shields.io/badge/platform-macOS%20(Universal%20arm64%2Fx86__64)-lightgrey.svg)]()

Harmonic Compressor is a mastering-grade analogue dynamics processor and harmonic saturation plugin built with JUCE. Combining seven meticulously modeled analogue compression circuits, continuous first-order Antiderivative Anti-Aliasing (ADAA) harmonic coloration, an integrated transient designer, auto-scaling meters, and an interactive real-time compression transfer curve display.

![Harmonic Compressor Soft White Interface](Docs/harmonic-compressor-soft-white.png)

| Themes | Movable modules | Auto Gain on, live metering |
|---|---|---|
| ![All four themes](Docs/harmonic-compressor-themes-grid.png) | ![Floating modules](Docs/harmonic-compressor-modules.png) | ![Auto Gain and metering](Docs/harmonic-compressor-autoscale.png) |

## Version 1.5.2

- **Auto Gain:** matches the output to the input's loudness (K-weighted, 3 s window, ±24 dB), so you hear the processing without a level change. The key shows the applied gain.
- **Delta:** plays only what the plugin adds or removes.
- **Fixed parallel Mix with oversampling:** the dry signal is no longer dulled or phase-shifted.
- **Fixed** a possible crash when a host removed the plugin right after a model or oversampling change.
- Existing sessions load unchanged; the two new switches start off.

## Download

- **macOS (11 or later, Apple silicon and Intel):** [Harmonic-Compressor-1.5.2-macos.pkg](https://github.com/MistaMin/HarmonicCompressor/releases/download/v1.5.2/Harmonic-Compressor-1.5.2-macos.pkg) ([SHA-256](https://github.com/MistaMin/HarmonicCompressor/releases/download/v1.5.2/Harmonic-Compressor-1.5.2-macos.pkg.sha256)). The installer puts VST3, AU, CLAP, LV2 and AAX in the standard plug-in folders and the manual and licences in `/Library/Application Support/Harmonic Compressor`. It is signed with a Developer ID and notarized by Apple; the AAX is PACE-signed for Pro Tools.
- **User manual:** [HarmonicCompressor-1.5.2-User-Manual.pdf](https://github.com/MistaMin/HarmonicCompressor/releases/download/v1.5.2/HarmonicCompressor-1.5.2-User-Manual.pdf)
- All releases: [GitHub Releases](https://github.com/MistaMin/HarmonicCompressor/releases). Also available through the OpenGrid Plugin Manager.
- Not available yet: Windows and Linux builds, and a Standalone app. Tested hosts for 1.5.2: REAPER (VST3) and OpenGrid9; Pro Tools, Logic and Studio One were not tested with this version.

## Highlights & Features

- **7 Analogue Modeled Topologies:**
  - **Red:** Discrete Class-A VCA console channel dynamics with reactive punch relief.
  - **Hybrid:** Dual feed-forward/feedback topology for mix bus and master stems.
  - **Bridge:** Classic German diode-bridge feedback circuit with rich harmonic density.
  - **Bus VCA:** British console stereo master bus compressor with logarithmic release decay.
  - **76:** Ultra-fast FET peak limiter with lightning attack and dual-time-constant release.
  - **Opto Leveler:** Dual electro-luminescent photocell T4B optical leveling attenuator.
  - **VCA 160:** Aggressive RMS-detecting feed-forward VCA with signature punchy snap.
- **Continuous 1st-Order ADAA Saturation:**
  - High-order polynomial waveshaping linearised through exact antiderivatives.
  - Even harmonics (Tube warmth: $H_2, H_4$) with sub-audio 3.5 Hz DC blocker.
  - Odd harmonics (Tape saturation: $H_3, H_5$) and secondary smooth rail ceiling limiter.
- **Built-in Transient Designer:**
  - Independent Attack and Sustain controls (-100% to +100%) with 0.5-50 ms detector speed, after the dry/wet mix and before output trim.
  - Sidechain High-Pass Filter (20 to 500 Hz, with bypass and audition modes).
- **Auto Gain & Delta:**
  - Auto Gain matches the output to the input's K-weighted short-term loudness (BS.1770 / EBU R128), so processing is judged without a level change.
  - Delta plays only what the plugin changes (output minus the delay-compensated dry signal).
- **Visual Ergonomics & 4 Themes:**
  - **Soft White:** Minimalist high-contrast studio chassis.
  - **Matrix Phosphor:** Retro CRT green glow.
  - **Cryo Plasma:** Deep cobalt & cyan neon accents.
  - **Hyperdrive:** Ultraviolet, magenta and cyan synthwave palette.
  - Movable, collapsible modules inside the plugin window.
- **Zero Latency:** Native 0 samples latency at 1x oversampling, ideal for tracking.
- **Multi-Format:** Ships as **VST3, AU, AAX Native (PACE Eden Signed), CLAP and LV2** (Universal Apple Silicon arm64 & Intel x86_64). No Standalone app is shipped.

## Building from Source

```bash
# Clone the repository
git clone https://github.com/MistaMin/HarmonicCompressor.git
cd HarmonicCompressor
git submodule update --init --recursive

# Configure with CMake & Ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAAX_SDK_PATH=/path/to/aax-sdk-2-8-1

# Compile all formats
ninja -C build HarmonicCompressor_VST3 HarmonicCompressor_AU HarmonicCompressor_CLAP HarmonicCompressor_LV2 HarmonicCompressor_AAX
```

JUCE and GoodLookinUI must be available locally before configuring. The dependency
revisions used for the 1.5.2 release build are recorded in
[`qa/2026-11-14-release-1.5.2/build-env.txt`](qa/2026-11-14-release-1.5.2/build-env.txt).
Pass `-DGOODLOOKINUI_ROOT=/path/to/GoodLookinUI` if it is not in the default sibling location.

See the [release notes](RELEASE_NOTES.md) for every version.

To compile the versioned PDF user manual:
```bash
python3 tools/build_user_manual.py
```

## Documentation

A color user manual with model descriptions, signal flow and all 27 host parameters is available in [`Docs/UserManual.md`](Docs/UserManual.md) and compiles to `output/pdf/HarmonicCompressor-User-Manual.pdf`.

## Licensing & Compliance

- **Plugin Code:** Open-source under the [MIT License](LICENSE), copyright Marcos Quirino Deida.
- **Official binaries:** freeware under [LICENSE-BINARIES.txt](LICENSE-BINARIES.txt); third-party notices in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
- **Framework:** Depends on [JUCE](https://juce.com) (dual-licensed under AGPLv3 / Commercial Starter/Pro).
- **Trademarks:** AAX and Pro Tools are trademarks of Avid Technology, Inc.
