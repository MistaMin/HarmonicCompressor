# Harmonic Compressor 1.5.3 — release ledger (2026-11-17)

Source of scope: user request 2026-11-17, "run the make plugin release wiki" (Make-Plugin-Release, DSP
Engineering Wiki copy), after the audit fixes of the same day (`qa/2026-11-17-audit-fixes/ledger.md`).
1.5.2 → 1.5.3. macOS ships as one signed, notarized `.pkg`; no ZIP, no Standalone.

Build: Mac (arm64, macOS 15.7.5, Xcode 16.0), Release, universal arm64 + x86_64, macOS 11.0 minimum, from
commit bf1ec03 (clean tree). Later commits change no plug-in build input (documents, QA files, a doc render
tool target). The installer was rebuilt on 2026-11-23 with the same signed plug-ins and the new manual and
binary licence. Dependency
revisions: `build-env.txt`. Artifacts outside git (`dist/release-1.5.3/`).

| ID | Item | Evidence | Status |
|---|---|---|---|
| R1 | Version 1.5.3 in VERSION, CMake project, bundle plists, README, manual | AU `CFBundleShortVersionString` 1.5.3; manual title and page footers | verified |
| R2 | Release notes, README version and download section, GitHub release text | `RELEASE_NOTES.md`, `README.md`, `Docs/RELEASE_1.5.3.md` | verified |
| R3 | Manual matches the code: stereo-linked models, per-model memory saved with the session, external sidechain, Knee Red only | `Docs/UserManual.md`; PDF rebuilt, all 7 pages looked at | verified |
| R4 | Manual screenshots show the 1.5.3 curve (hard knee outside Red) | user decision 2026-11-23: re-render. `qa/2026-11-23-docs-1.5.3/doc_screenshots.cpp` (`DocScreenshotRender`) renders the four Opto Leveler pictures offscreen with live tone bursts; `compose_doc_screenshots.py` replaces the main and modules pictures and pastes the editor into the theme grid and the host-window picture (located by the unchanged IN GAIN panel). The host-window frame is the old capture, so its title still reads the former manufacturer name. Looked at each picture and the PDF pages | verified |
| R5 | Universal binaries, min macOS 11.0, all five formats | `lipo -archs`, `vtool`: x86_64 arm64, minos 11.0 for VST3, AU, CLAP, LV2, AAX | verified |
| R6 | CTest on the release build | `ctest.log`: 4/4 | verified |
| R7 | Build warnings in project sources | `build.log`: none (all warnings are in JUCE and other dependencies) | verified |
| R8 | Developer ID signatures, hardened runtime, timestamp | `sign.log`; `codesign --verify --deep --strict` on the `.pkg` payload | verified |
| R9 | AAX PACE Eden signature (Signing Only) | `aax-sign.log`: product "Harmonic Compressor", GUID 56C67100-D558-11F1-804C-005056928F3B (same as 1.5.2) | verified |
| R10 | Installer: Developer ID Installer, notarized, stapled, Gatekeeper | `pkg-build.log`: submission cd12a44f-093a-4009-bce5-77ee65ca62c8 Accepted; staple validated; `spctl` accepted, Notarized Developer ID | verified |
| R11 | Payload = signed bundles; no AppleDouble, .DS_Store or Standalone | `pkgutil --expand-full` + `diff -r` per format: identical; 0 `._`/.DS_Store; no `.app` | verified |
| R12 | pluginval 1.0.4 VST3 strictness 10, in-process (payload) | `validators/pluginval-vst3-{1,2,3}.log` (no editor), `pluginval-vst3-gui.log`: SUCCESS | verified |
| R13 | clap-validator 0.4.1 (payload) | `validators/clap-validator.log`: 44 run, 32 passed, 0 failed, 1 warning, 11 skipped (as 1.5.2) | verified |
| R14 | AAX Validator 2024.6 (payload) | 6/6 PASS: describe_validation, parameters, parameter_traversal.random.fast, page_table.load, page_table.automation_list, load_unload. Output kept outside the repository (Avid SDK material) | verified |
| R15 | auval, pluginval AU | on the same source (pre-version-bump build): `qa/2026-11-17-audit-fixes/auval.log`, `pluginval-au.log` SUCCESS. `validators/pluginval-au.log` (payload path) SUCCESS, but which installed copy the AU registry loaded was not confirmed | verified on the same source |
| R16 | Host suites | same source: REAPER CLAP 9/9, LV2 8/9 (host state limit), OpenGrid9 AU under test code Hcmt PASS with editor checks skipped (`qa/2026-11-17-audit-fixes/`) | verified on the same source; REAPER AU unverified |
| R17 | Manifest (schema 2) from the final stapled `.pkg` | `opengrid-manifest.json`: 24,569,321 bytes, SHA-256 71c24cf3e0a10f4c04a20298af0ca3faa3a19f740de8fb9407c94fb97cf925cd, universal, 11.0, modern, 5 installs | verified |
| R18 | Commits as MistaMin, no assistant attribution lines | `git log --format='%an %ae%n%B'` | verified locally; GitHub check after push |
| R19 | No third-party product names in source | two comments in `CompressorCore.h` renamed | verified |
| R19b | Copyright holder and licence files (Section 7.3) | `grep` over first-party files: only Marcos Quirino Deida. `LICENSE-BINARIES.txt` corrected 2026-11-23: holder line added; formats (no Standalone); JUCE 8 has no splash screen, so the splash and licence-tier text was removed (user decision); MIT file name as installed | verified |
| R20 | Windows and Linux builds | need user-owned native machines | out of scope (user decision 2026-11-08) |
| R21 | Pro Tools interactive session, Logic, Studio One; listening to the Red/Hybrid change | not run | unverified |
| R22 | Push, tag `v1.5.3`, GitHub release, `RELEASE-READY`, Plugin Manager upload | waits for the owner's approval | todo |
