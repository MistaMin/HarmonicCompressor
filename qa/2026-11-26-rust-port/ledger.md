# Scope ledger: port to OpenPluginSDK (2026-11-26/27, commit f4319f2 + every_param test + editor)

Scope: the 1.5.3 processor on OpenPluginSDK, same identity, parameters, sound at 1x and saved
states (request "port to latest version of openpluginsdk"; 1.5.3 manual and release notes).
The editor is part of the port (owner, 2026-11-27: "the exact same editor"). Then, the same day
(owner): "fix the plugin to match display rules", and meters as the SDK's standards: PPM in dBFS by
default, switchable to RMS and dB VU, 0 VU = -18 dBFS.

## Parameters (27): host list / changes the sound / GUI control

| Parameter | Host list | Sound | GUI |
|---|---|---|---|
| all 27 | PASS: CLAP, VST3, AU scans match 1.5.3 (`port-scan/`, `params::tests`) | PASS: `plugin/tests/every_param.rs`, each moved to its far end, -48.7 to +23.4 dB difference; 1x null vs 1.5.3 in `null/` | PASS: 1.5.3's controls, ported (`editor/mod.rs` KNOBS, KEYS, DROPS): 17 knobs and sliders, 7 keys (Auto Attack, Auto Release, Transient twice, Auto Gain, Delta, Curve), selectors for Algorithm, Oversample, Filter, Harmonics (h2-h5 presets), Sidechain (scEnabled + scListen); renders equal 1.5.3's (`gui/report.txt`) |

Dependencies the test sets, from 1.5.3's code: Sidechain Listen and HP Freq need Sidechain
Enable; Transient Attack, Sustain and Speed need Transient Enable; Transient Enable alone is
silent by design (Attack and Sustain default to 0 %, `TransientDesigner.h:79-80`).

## Features

| Item | Evidence | Status |
|---|---|---|
| Seven models, auto attack/release, harmonics, knee, mix, gains | `null/report.txt` (41 cases at 1x) | PASS |
| Sidechain high-pass and listen, transient designer, Auto Gain, Delta | `null/report.txt` | PASS |
| Oversampling 2x to 8x, Lin/Min phase | `every_param.rs`; latency 3 at 2x as 1.5.3 | PASS, does not null with 1.5.3 (new filters, README) |
| 1.5.3 sessions load (CLAP, VST3, AU states) | `state::tests`, `port-capture/` | PASS |
| External sidechain | CLAP, VST3 layouts; conformance `layouts` | PASS; AU: SDK shell has one input element (README) |
| Input and output section, transfer curve, model memory recall | `gui/report.txt` (equal to 1.5.3 except the text the display rules changed), `editor::tests` (recall on a model change) | PASS |
| Meters: PPM (dBFS, default), RMS (dBFS), VU (0 VU = -18 dBFS), chosen on the input meter, saved with the session | `tests/meters.rs`, `editor::tests::the_meter_scale_is_chosen_on_the_input_meter_and_saved` | PASS (NEW, below) |

## Checks

| Check | Result |
|---|---|
| openplugin-dev full tier (lint, clippy, release tests, soak 24 seeds, strict fuzz, bench), SDK `1f608fe` + `editor-pointer-left`, after the display rules and meters | PASS 6/6 (2026-11-27, 1526 s); bench 0.96 to 1.12 of the old baseline, no case slower; lint, clippy and tests (37 unit, 7 meter, conformance, host behaviour, every parameter) rerun after the 8 pt text: PASS |
| Null against 1.5.3, 41 cases, same SDK base | PASS, 0 failed (values on both grids since the display rules; `null/run.sh`) |
| SDK's own editor tests (`openplugin-dri`) with the view change | PASS 72 + 12 |
| pluginval 1.0.4, VST3, strictness 10, in process, GUI tests skipped (no editor) | PASS (`validators/pluginval-vst3.txt`) |
| pluginval 1.0.4, VST3, strictness 10, in process, with its editor tests, same bundles | PASS (`validators/pluginval-vst3-editor.txt`) |
| clap-validator 0.4.1, bundles rebuilt after the display rules and meters | PASS: 34 passed, 0 failed, 0 warnings, 10 skipped (no preset discovery, f64 audio, note input, port activation, configurable ports: not offered). Its `param-conversions` test found two text bugs of the first attempt (fixed, below). One run under a load average of 36 warned on scan time and denormal timing; five repeats of the denormal test passed |
| auval | SKIPPED: needs the AU registered; the port has 1.5.3's codes, so installing it shadows the shipped copy |
| REAPER, OpenGrid9 host matrix | SKIPPED: same reason |
| AAX validator, Pro Tools | SKIPPED: AAX not built or PACE-signed |
| LV2 | SKIPPED: bundle needs a ttl bin |
| GUI coverage | PASS: every parameter has 1.5.3's control (`every_parameter_has_a_control`, from the control tables) |
| Meter ballistics | PASS, `tests/meters.rs` (table below); the GR bar and readout are the compressor's envelope (not a meter), the history field 1.5.3's block peaks |
| Screenshots against 1.5.3 (10 states, 2 scales) | PASS: 0.5 to 0.7 % of pixels differ, all in text the display rules changed (values, readouts, the labels raised to 8 pt) and in the new meter selector (diff images in `gui/diff/` looked at); everything else within 1/255 as before |
| GUI performance | whole repaint as 1.5.3 (58.4 vs 57.7 ms CPU at 2x, loaded machine), idle 0; after the meter change 62.4 ms with the soak tests running beside it (load average 30): not a clean comparison, quiet-machine numbers owed |
| Audio cost of the meters | the SDK's PPM (8x rectifier) cost 20 to 27 % of the plugin when always on (bench FAIL, 11 cases over the 25 % limit): the engine now runs only the scale an open editor shows (`the_meters_run_only_for_an_editor`); bench against the old baseline in the full-tier row |
| Text audit (sizes, clipping) | PASS: no text under 8 pt (`editor::tests::no_text_is_smaller_than_8_pt`); 1.5.3's meter scale labels, axis captions and curve labels (7.2 to 7.9 pt) are 8 pt now (NEW, display rule 5); the enlarged labels looked at for clipping at 2x and 1x: none |
| Editor in a real window (SDK Cocoa view, audio running) | PASS, looked at: PPM meters move, whole-step values, selector drawn (2026-11-27). Rendered, not clicked: mouse paths are unit tests only |
| Universal binary | NOT DONE: bundles are arm64 only |

## Display rules (each control over its whole travel)

`editor::tests::every_control_meets_the_display_rules` checks 401 points of every control's travel,
in the editor's text and in the host's: at most three digits, no 0.x value, no decimal point on the
dynamics controls. Fixed 2026-11-27 (owner), all NEW against 1.5.3:

| Control | min | mid | max | Change |
|---|---|---|---|---|
| Input / Output Gain | -24 dB | 0 dB | +24 dB | whole dB steps (1.5.3: tenths) |
| Threshold | -60 dB | -30 dB | 0 dB | whole dB |
| Ratio | -2:1 | 24:1 | ∞:1 | whole ratios (1.5.3: tenths, "23.5:1") |
| Makeup | -12 dB | +6 dB | +24 dB | whole dB |
| Knee | 0 dB | 12 dB | 24 dB | whole dB |
| 2nd..5th | 0% | 50% | 100% | whole percent (1.5.3: "100.0%", "0.5%") |
| Transient Attack / Sustain | -100% | 0% | +100% | whole percent; the snapped default is now exactly 0 |
| Attack | 100 µs | 29.9 ms | 300 ms | under 1 ms in µs (1.5.3: "0.1 ms"); same 0.1 ms steps |
| Transient Speed | 500 µs | 9.3 ms | 50.0 ms | as Attack |
| Release | 5.0 ms | 302 ms | 3.00 s | host text in seconds from 1 s (1.5.3 sent hosts "3000") |
| Mix | 0% | 50% | 100% | host text in percent (1.5.3 sent "0.50") |
| HP Freq | 20 Hz | 68 Hz | 500 Hz | unchanged |

Readouts that change every frame are whole numbers (rule 5): the meter readouts, "x dB GR", the
Auto Gain key; -0 reads 0. No readout was named exempt. Attack, Release and Speed now send hosts
their text with its unit ("500 us", "1.01 s") and no separate unit, so a host shows no "us ms".

**Found on the way (fixed):** an off-step value read "1.0" where the step read "1", and 99.96 ms
read "100.0 ms": text to value to text was not stable (clap-validator `param-conversions`). Both
texts now choose their form on the rounded number; `params::tests::text_survives_a_round_trip`
checks 1001 points of every parameter.

**Sessions:** a 1.5.3 session (or automation written by it) can hold tenths, such as a -23.7 dB
threshold. A loaded session keeps them, so it sounds as before (`params::saved`), and the text
shows the tenth ("-23.7 dB"); the next move of the control, or a host value, snaps to a whole step.

## Meter ballistics (`tests/meters.rs`, headless, editor closed, input meter at default settings)

Standards as the SDK's `Ppm` and `VuIec` quote them; the readout behaviour is the owner's: PPM in
dBFS by default, RMS and VU on a switch, 0 VU = -18 dBFS sine.

| Check | Spec | Measured |
|---|---|---|
| Calibration, -18 dBFS sine at 100 Hz, 1 kHz, 10 kHz; 44.1, 48, 96 kHz | PPM reads the peak, VU 0, RMS -21.03 | PPM -18.00 (±0.003), RMS -21.01, VU -0.04 to +0.001 |
| PPM 10 ms burst of 5 kHz (IEC 60268-10 Type I) | -1 dB ±0.5 | -0.78 |
| PPM 5 ms burst | -2 dB ±0.5 | -2.11 |
| PPM return | 20 dB in 1.7 s ±0.3 | 1.70 s |
| VU rise to 99 % (IEC 60268-17) | 300 ms (the SDK's test allows 10 ms) | 302.3 ms (48 kHz), 300.5 ms (96 kHz) |
| VU overshoot | 1 % to 1.5 % | 1.15 %, 1.26 % |
| RMS window | 300 ms one-pole on the power: -2 dB at 300 ms after a step | -2.0 dB |
| Block size 64 / 480 / 960, rate 48 / 96 kHz, at 300 ms | same reading | within 0.01 dB |
| Silence, +6 dBFS, NaN and infinity bursts | floor (no NaN), +6 reads +6 dBFS / +24 VU, finite after | PASS |
| Left loud / right silent, and swapped | each lane its own channel | PASS |

## Release preparation (openplugin-dev `prepare-release`, 2026-11-27): not released

Test build: all five formats of 1.5.3 (CLAP, LV2, VST3, AU, AAX), universal, macOS 11.0 minimum,
ad-hoc signed, nothing installed (`plugin/target/dist-universal`, command in README), on the
working tree before its commit and SDK `1f608fe` + `editor-pointer-left`. Owner's decisions
(2026-11-27): version 1.6.0; commit and push; build on the SDK's current `main` plus the view fix,
committed; Developer ID signing and a test-identity install for host tests.

SDK base since then: branch `editor-pointer-left` `f2e8d15` (`main` `07f97e1` plus the view fix as one
commit, local). On it: the SDK's editor tests PASS (72 + 12); the plugin's lint, clippy and release
tests PASS; the null against 1.5.3 PASS (41 cases, 0 failed).

| Step | Result |
|---|---|
| 1. Release commit, full tier on it (RL-1) | NOT DONE: the work is uncommitted; the full tier passed on the working tree (Checks) |
| 2. Soak at 200 seeds on that commit (RL-1) | NOT DONE: needs the commit |
| 3. Version (RL-2) | 1.6.0 (owner), in `Cargo.toml` only (`PluginInfo` reads it); AU component version 67072, above 1.5.3's 66819 |
| 3. Identities (PR-2) | PASS as built: CLAP id and VST3 class ids (identity tests, `port-scan/`), AU `aufx Hcmp OpGr` (bundle plist), LV2 URI `urn:opengrid:harmoniccompressor` (manifest), AAX `OpGr` / `Hcmp` / `jcbb` / `jccc` (JUCE's 'jcaa' plus the layout index in and out; the validator's `info.productids` aborts in its own DTT layer on this macOS, so not read back) |
| 4. Every format, universal (RL-5) | PASS: five bundles, `x86_64 arm64`, minos 11.0; `codesign --verify --deep --strict` and the build script's path scan pass (RL-3, ad-hoc) |
| 4. Developer ID signing, PACE, notarisation | NOT DONE: needs the owner (identity, iLok password) |
| 5. clap-validator 0.4.1, universal CLAP | PASS: 33 passed, 0 failed, 1 warning (scan time 103 ms at a load average of 180), 10 skipped (`validators/clap-validator-universal.txt`) |
| 5. pluginval 1.0.4, universal VST3, strictness 10, editor tests | PASS (`validators/pluginval-vst3-universal.txt`) |
| 5. `lv2_validate`, LV2 bundle | PASS: 0 errors |
| 5. AAX Validator 2024.6, unsigned bundle from its path | PASS 6/6, the tests 1.5.3 ran: describe_validation (warning: gaps in the algorithm context fields), parameters, parameter_traversal.random.fast, page_table.load, page_table.automation_list, load_unload. Output kept outside the repository (Avid material) |
| 5. x86_64 slice: `cargo test --release --target x86_64-apple-darwin` under Rosetta | PASS: 37 unit, 7 meter, conformance, host behaviour, every parameter |
| 5. auval, hosts (REAPER, Logic, Pro Tools) (RL-4) | SKIPPED: needs an installed build |
| 6. Tag, publish | NOT DONE |

Differences from 1.5.3 a release must say or settle:
- LV2: the bundle folder is `HarmonicCompressor.lv2` (1.5.3: `Harmonic Compressor.lv2`, same URI): an
  installer must remove the old folder. The SDK's LV2 has control ports; 1.5.3's (JUCE) used patch
  parameters, so an LV2 host session of 1.5.3 is not expected to restore (not tested).
- AAX: the page table is the SDK's generated one, not 1.5.3's `Resources/HarmonicCompressor.xml`; no
  AudioSuite type.
- AU: one input element, so no external sidechain in AU hosts (SDK shell).
- `Docs/UserManual.md` still describes 1.5.3's meters and tenth steps.

## Exceptions

- `tests/common/mod.rs`: `latency_depends_on_params` (latency follows oversampling) and
  `max_output` 2e5 (+96 dB designed range); reasons in the file.
- `null/run.sh`: two cases at 3e-5 instead of 1e-5 (fused multiply-adds in the C++ build).
