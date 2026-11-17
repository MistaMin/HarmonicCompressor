# Audit fixes — ledger (2026-11-17)

Source of scope: user request 2026-11-17, "fix all and audit" (findings 1–15 of the repository audit of the same
day, then an audit of the parts it had not read). Build: `build-autogain` (Release, arm64, macOS 15.7.5). Not
released; not committed.

| ID | Finding | Change | Evidence | Status |
|---|---|---|---|---|
| A1 | Red/Hybrid shared one envelope between channels | All models use the stereo-linked loop (`PluginProcessor.cpp`) | `processor-fixes-before.log`: Red 0.090, Hybrid 0.081 max difference between 64- and 512-sample blocks, linked models 0; `processor-fixes-after.log`: all 7 models 0.00000000 | verified |
| A2 | Hybrid ratio 0–1:1 unbounded boost | `HybridAlgorithm::gainReductionDb` clamps to ≤ 0 dB | A2: 0.00 dB now; `old-code-new-tests.log`: +14.18 dB (0.5:1) and +127.59 dB (0.1:1) on the old code | verified |
| A3 | Installer deleted its whole output folder | Removes only the files it writes | `installer-e2e.log`: real run, an unrelated file in the output folder survived | verified |
| A4 | Silent sidechain fell back to the main input | Latches the external sidechain once it has carried signal | A4: 0.00 dB now; -17.44 dB on the old code (`old-code-new-tests.log`) | verified |
| A5 | Per-model memory lost on reload; recalled asynchronously from host changes | `selectMode` (editor only), memory saved as `ModeSnapshots` child; queue, restore generation and restoring flag removed | A5 checks: recall, host change keeps values, save/reload recalls, old-format session loads | verified |
| A7 | Hybrid feedback path never fed | Fed from the detector level after the gain, inside `processSidechain` | `ab-render.log`: drum loop -6.85 → -7.83 dB level change; A/B files in `ab/` (gitignored) | verified (behaviour); owner listening pending |
| A8 | No buffer-size test | `testBlockSizeInvariance` in `ProcessorFixesTest` | fails on old code, passes now (A1) | verified |
| A9 | Delta reference filtered always | Own filter chain, run only while Delta and Auto Gain are on | `bench-before.txt` / `bench-after.txt`: 4x Lin 0.3726 → 0.2805 ms (6.99 → 5.26 %); AutoGainDeltaTest 64/64 | verified |
| A10 | Dead code (engine, processor, resampler, loudness, transient) | Deleted | build without project warnings; ctest 4/4 | verified |
| A11 | ZIP tool and Standalone staging | `tools/make_bundle_zip.sh` deleted; installer writes the `.installs.json` sidecar (architecture read from the binary) and uses the published `.pkg` name | real run: sidecar `universal`, 4 formats; `write_opengrid_manifest.py` accepts it | verified |
| A12 | `std::exp` per sample in the DC blocker | Coefficient in `prepare()` | bench | verified |
| A13 | log per sample for the GR meter | Minimum gain, one conversion per block | bench | verified |
| A14 | README submodule step | Real dependency steps | read | verified |
| A15 | `build_win.bat` personal path | `AAX_SDK_PATH` from the environment | not run (no Windows machine) | unverified |
| A1b | Release time in stereo (Red/Hybrid) | — | `ab-render.log`: old 50 ms, new 100 ms with Release at 100 ms (1/e recovery) | verified |
| A16 | Knee control dead in 4 models | Knee enabled only in Red; curve draws a hard knee elsewhere | build; not seen on a screenshot | verified in code |
| A17 | Curve drag forced ratio into 1–20 | Parameter range clamps instead | build | verified in code |
| A18 | PDF merged numbered lists | Numbered lines are list items | `pdftotext`: items 1–7 on their own lines | verified |
| A19 | PDF builder skipped missing images | Exits with the image line | missing image → exit 1 | verified |
| A20 | Dead ADAA code | `logCosh`, `Mode`, `Residual` deleted | build, ctest 4/4 | verified |
| A21 | `package_crossplatform.py` (ZIP for Windows) | deleted (unreferenced) | `git grep` | verified |
| A22 | "zero-latency" claim | "zero latency at 1x" | read | verified |
| A23 | Installer left empty Avid folders without an AAX | `rmdir -p` | shape check | verified (not rerun end to end) |
| I1 | Installer end to end | `Scripts/build_installer.sh` on Developer ID-signed universal VST3/AU/CLAP/LV2 (no AAX: PACE signing not part of this run) | notarization da61d46f-3d62-465b-ada0-6070e64e7bdc Accepted, stapled, Gatekeeper accepted; payload identical to the signed bundles | verified |
| H1 | auval (new build in the user folder) | — | `auval.log`: Component Version 1.5.2, AU VALIDATION SUCCEEDED | verified |
| H2 | pluginval AU strictness 10 | — | `pluginval-au.log`: SUCCESS (same 2 warnings as 1.5.1) | verified |
| H3 | clap-validator 0.4.1 | — | `clap-validator.log`: 32 passed, 0 failed, 1 warning (as 1.5.2) | verified |
| H4 | REAPER suite CLAP (loaded binary: user-folder test build, `lsof`) | — | `reaper/suite-clap.out`: 9/9 PASS | verified |
| H5 | REAPER suite LV2 (loaded binary: test build) | — | `reaper/suite-lv2.out`: 8/9; state FAIL identical to 1.5.1 (REAPER macOS restores no LV2 state, `Docs/Release-1.5.1-QA.md`) | verified, known host limit |
| H6 | REAPER suite AU | — | ran, but `lsof` shows REAPER loaded the `/Library` 1.5.1 AU (shared AU cache); result not counted | unverified: needs `/Library` install (password dialog cancelled) |
| H7 | OpenGrid9 suite AU (new build) | Test build from the same source with its own AU code and name (`Hcmt`, "Harmonic Compressor AUTest", scratch copy of CMakeLists only), because every host resolves `aufx Hcmp OpGr` to the `/Library` 1.5.1 copy; indexed with OpenGrid9's `ardour-au-scanner -f` called directly (its helper's `auval -a` hangs on this Mac) | `opengrid9-au/suite-au.out`: loads (27 parameters), finite, not silent, deterministic, every parameter changes the sound, state survives save and reopen: PASS; editor opens and editor reopens SKIPPED (headless). `lsof`: the AUTest binary was loaded. `auval-autest.log`: AU VALIDATION SUCCEEDED | verified (AU code differs from the shipped one) |
| V1 | Plug-in validity after the changes | pluginval 1.0.4 VST3 strictness 10, in-process, with and without editor tests | `pluginval-vst3.log`, `pluginval-vst3-gui.log`: SUCCESS | verified |

Not done: a REAPER AU run with the shipped AU code (H6: needs the `/Library` copy replaced), Pro Tools/Logic/Studio One, `build_win.bat` (no Windows machine), listening to the Red/Hybrid change (A/B files ready in `ab/`).

Cleanup: the user-folder test copies were removed; `/Library` still holds 1.5.1 (backed up before, never changed); the session secret was cleared; OpenGrid9's AU scan cache was backed up and is identical to the backup after the run.
