# Harmonic Compressor 1.5.1 — QA and release run (2026-11-09)

Source: master after `bceb0c6` plus the state-restore race fix (uncommitted at QA time;
committed immediately after this run). Artifact folder: `dist/release-1.5.1/`.
Installed copies under `/Library` are byte-identical to the ZIP (SHA-256 per format,
`dist/release-1.5.1/plugin-verification.json`).

## Scope and result

| Item | Evidence | Status |
|---|---|---|
| Slider click no longer jumps to maximum (13 linear sliders, 4 themes, 2 editor lifecycles) | CTest `SliderClickTest`, `EditorSliderTest`: 2/2, 1,144 checks | PASS |
| 25 APVTS parameters bound to the GUI | `gui-coverage.log`: main: 25/25 parameters bound | PASS |
| Every parameter changes the sound and is visible, per format | REAPER suite: VST3, AU, CLAP, LV2 | PASS (LV2 see below) |
| AAX page table loads and exposes control names | AAX Validator 2024.6 on installed AAX: 6/6 tests PASS | PASS |
| Plugin state save/reopen, per format | REAPER suite VST3/AU/CLAP PASS; LV2 host limitation below | PASS with host exception |
| AAX state-restore race (mode switch queued before a restore) | pluginval strict 10: failed 1 of 2 runs before the fix, 3/3 installed runs after | FIXED |

## Builds

| Build | Evidence | Status |
|---|---|---|
| Release universal arm64+x86_64, min macOS 11.0, VST3/AU/CLAP/LV2/AAX | `lipo -archs` per format; manifest `build_class` modern, `min_os` 11.0 | PASS |
| Installed copies = ZIP bytes | `plugin-verification.json` SHA-256 comparison for all five formats | PASS |
| Apple Developer ID signatures, Hardened Runtime | `codesign --verify --strict` on every format in the ZIP | PASS |
| PACE Eden AAX signature (Signing Only) | `aax-verify.log`; Product GUID `56C67100-D558-11F1-804C-005056928F3B` | PASS |
| Apple notarization of the Manager ZIP | `notary.log`, submission `0929bda7-3c25-475f-be2f-fd165afcbcd5`, Accepted | PASS |

## Validators (installed build)

| Check | Evidence | Status |
|---|---|---|
| CTest (Release) | `ctest.log`: 2/2 | PASS |
| auval (aufx Hcmp OpGr) | `auval.log`: AU VALIDATION SUCCEEDED | PASS |
| clap-validator 0.4.1 | `clap-validator.log`: 44 tests, 32 passed, 0 failed, 1 warning, 11 skipped | PASS |
| pluginval 1.0.4 strictness 10, VST3 | `pluginval-vst3-installed-{1,2,3}.log`: SUCCESS | PASS |
| pluginval 1.0.4 strictness 10, AU | `pluginval-au-installed.log`: SUCCESS | PASS |
| AAX Validator 2024.6 (installed AAX) | `aax-installed/aaxval_summary.txt`: 6/6 PASS | PASS |

The one clap-validator warning is the JUCE wrapper's plugin_id suffix
(`com.opengrid.harmoniccompressorx1` vs `com.opengrid.harmoniccompressor`), present since 1.5.0.
The AAX warnings (algorithm context gaps; automatable parameters not in the default
one-control-per-page table) also appear on the sibling plugins.

## Host matrix (REAPER 7.80/macOS-arm64, installed plugins)

| Format | Report | Result |
|---|---|---|
| VST3 | `reaper-vst3/report.md` | 9/9 PASS |
| AU | `reaper-au/report.md` | 9/9 PASS |
| CLAP | `reaper-clap/report.md` | 9/9 PASS |
| LV2 | `reaper-lv2/report.md` | 7/9; see host limitation |

Checks per format: loads, finite output, not silent, deterministic render, editor opens,
every parameter changes the sound, every parameter is shown in the GUI, state survives
save and reopen, editor reopens 5 cycles.

## State-restore race fix

`setStateInformation` could be followed by a mode switch queued earlier on the message
thread; its `loadSnapshot` then overwrote restored parameters. pluginval's strictness-10
state test caught it: 1–3 parameters (different each run) were not restored, on both the
1.5.0 candidate and the 1.5.1 build. Fix: `stateRestoreGeneration` in `PluginProcessor`;
a queued switch is dropped if a restore happened after it was queued. Evidence:
`pluginval-vst3.log` / `pluginval-vst3-rerun.log` (before) and
`pluginval-vst3-installed-{1,2,3}.log` (after).

## LV2 state: REAPER host limitation

REAPER 7.80 restores no LV2 plugin state for any plugin on this machine. Same host, same
project flow, same plugin code:

| Test | Audio round-trip difference | Result |
|---|---|---|
| VST3: set state, render, save, reopen, render | -240.0 dB | restored exactly |
| LV2: same flow | -5.6 dB | not restored |
| LV2: Harmonic Compressor, ArcVu, Blue Channel, DualSone, third-party AGridder | all reopen at defaults | not restored |
| LV2: install-mode parameter (scHPFreq with sidechain on) | -13.2 dB | parameter path works |

The LV2 state is written into the project (`<STATE>` `urn:opengrid:harmoniccompressor:StateString`),
so the plugin saves it; REAPER does not pass it back. Recorded as a host limitation, not a
plugin defect. No second LV2 host is installed here to prove the plugin's state interface.
The suite's state check compares host display text when no state chunk is readable, so its
LV2 FAIL is expected on this host.

The LV2 suite also flagged `Free Wheeling` (REAPER's own LV2 freeWheeling sync port, now
excluded in `qa/exceptions.txt` with a reason) and `Sidechain HP Freq` when the sweep was
not in a compressing state. Isolated run `reaper-lv2-sc/report.md` passes that parameter;
the context in `qa/exceptions.txt` now drives Threshold/Ratio so the detector filter reaches
the output.

## Not run

- Pro Tools interactive test (needs foreground host interaction and a running session);
  the AAX Validator suite is the automated AAX evidence for this build.
- Logic (not installed), Studio One (not part of this release request).
- Controlled performance comparison (no paint/timer/analyser change in 1.5.1; the slider
  fix changes hit-testing only).
- Windows and Linux builds (user-owned native machines required).

## Artifacts

- `Harmonic-Compressor-1.5.1-macos.zip` — 24,012,245 bytes, SHA-256
  `3216d42c1f0f41ae35402ca1afe1ed5cb7eee76f6d0899f23d8582bd9ebfa2bf`
- `opengrid-manifest.json` (schema 2), `plugin-verification.json`, `ctest.log`,
  `aax-verify.log`, `notary.log`
- Notarization submission `0929bda7-3c25-475f-be2f-fd165afcbcd5` — Accepted
