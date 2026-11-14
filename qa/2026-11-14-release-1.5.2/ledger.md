# Harmonic Compressor 1.5.2 — release ledger (2026-11-14)

Source of scope: user request 2026-11-14, "up the version to 0.0.+1 and make plugin release, commit and
push to github" (1.5.1 → 1.5.2), following Make-Plugin-Release (macOS ships as a signed, notarized
`.pkg`; no ZIP). Feature scope and its evidence: `qa/2026-11-14-autogain-delta/ledger.md`,
`qa/2026-11-14-processor-fixes/ledger.md`.

Build: Mac (arm64, macOS 15.7.5, Xcode 16.0), Release, universal arm64 + x86_64, macOS 11.0 minimum,
from commit 7e8b956. Its source tree is identical to the published commit `Prepare 1.5.2` apart from the
scratch-folder path in five OpenGrid9 QA logs, so no build input differs. Dependency revisions:
`build-env.txt`. Artifacts were kept outside git (`dist/release-1.5.2/`).

| ID | Item | Evidence | Status |
|---|---|---|---|
| R1 | Version 1.5.2 in VERSION, CMake project, bundle plists, README, manual | AU `CFBundleShortVersionString` 1.5.2; manual title | verified |
| R2 | Release notes, README version and download section | `RELEASE_NOTES.md`, `README.md`, `Docs/RELEASE_1.5.2.md` | verified |
| R3 | Manual matches the code: 27 parameters, Auto Gain & Delta section | `Docs/UserManual.md`; PDF rebuilt, pages 3–4 checked | verified |
| R4 | Manual screenshots show the 1.5.2 knob bar (AUTO GAIN, DELTA) | `doc_knobbar.cpp` + `compose_doc_screenshots.py` (knob bar re-rendered with each picture's settings and theme, pasted in place; editor located by the unchanged IN GAIN panel, error 0.0 on the 2x Soft White shot); metering picture is a new render with audio and Auto Gain on | verified (looked at each picture) |
| R5 | Universal binaries, min macOS 11.0, all five formats | `lipo -archs` + `vtool` per format; `plugin-verification.json` | verified |
| R6 | CTest on the release build | `ctest.log`: 4/4 (SliderClick, EditorSlider, AutoGainDelta, ProcessorFixes) | verified |
| R7 | Developer ID signatures, Hardened Runtime, timestamp (VST3, AU, CLAP, LV2 binary) | `codesign --verify --deep --strict` on the copies inside the `.pkg` | verified |
| R8 | AAX PACE Eden signature (Signing Only) | `aax-verify.log`: Product "Harmonic Compressor", GUID 56C67100-D558-11F1-804C-005056928F3B (same as 1.5.1) | verified |
| R9 | Installer `.pkg`: Developer ID Installer, notarized, stapled, Gatekeeper | `pkg-build.log`: submission f5a77ef9-f960-4e0f-9252-8753feb09cbf Accepted; `stapler validate` OK; `spctl`: accepted, Notarized Developer ID | verified |
| R10 | `.pkg` payload = signed bundles; no AppleDouble or .DS_Store | `pkgutil --expand-full`, `diff -r` per format: identical | verified |
| R11 | No Standalone in the package | decision of 2026-11-08 (`Docs/Release-1.5.1-QA.md`); payload has none | verified |
| R12 | pluginval 1.0.4, VST3, strictness 10, in-process, 3 runs (`.pkg` payload) | `validators/pluginval-vst3-{1,2,3}.log`: SUCCESS | verified |
| R13 | clap-validator 0.4.1 (`.pkg` payload) | `validators/clap-validator.log`: 44 run, 32 passed, 0 failed, 1 warning (`state-invalid-random`, as 1.5.1), 11 skipped | verified |
| R14 | AAX Validator 2024.6 (`.pkg` payload) | `validators/aax/aaxval_summary.txt`: 6/6 PASS incl. page_table.load and automation_list; Auto Gain and Delta are listed as automatable | verified |
| R15 | auval, pluginval AU | need the AU installed over the 1.5.1 copy in `/Library` (administrator password) | unverified: not installed in this run |
| R16 | REAPER suite AU, CLAP, LV2 on this build | same reason as R15; VST3 9/9 PASS on the same source (`qa/2026-11-14-autogain-delta/reaper-vst3/`) | unverified: not installed in this run |
| R17 | Manifest (schema 2) from the final stapled `.pkg` | `opengrid-manifest.json`: 24,641,380 bytes, SHA-256 2434ac8cfc2decd15b43cb6c85795c2ae76c66a9f7be60bd64e483e9aafd6a2d | verified |
| R18 | Commits as MistaMin, no assistant attribution lines | `git log --format='%an %ae%n%B'` | verified before push; GitHub author login checked after push (Docs/Release-1.5.2-QA.md) |
| R19 | Windows and Linux builds | need user-owned native machines | out of scope (user decision 2026-11-08) |
| R20 | Pro Tools interactive session, Logic, Studio One | not run; AAX Validator is the automated AAX evidence | unverified |

A notarized ZIP (`Harmonic-Compressor-1.5.2-macos.zip`, submission 24e2eedc-6bcb-4774-86d2-350df49e4db5,
`notary-zip-not-shipped.log`) was built before the `.pkg`-only rule was found in the release procedure.
It is not published anywhere.
