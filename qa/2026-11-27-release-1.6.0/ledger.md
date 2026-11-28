# Harmonic Compressor 1.6.0 — release ledger (2026-11-27)

Source of scope: the owner's instructions of 2026-11-27 after the first release preparation
(`qa/2026-11-26-rust-port/ledger.md`, "Release preparation"): build on the latest OpenPluginSDK, sign,
notarise and package as the release procedure says (Make-Plugin-Release), give the Audio Unit and every
other format an external sidechain, update all documentation, redo the QA on the result, remove the
test builds. 1.5.3 (JUCE) → 1.6.0 (Rust, OpenPluginSDK). macOS ships as one signed, notarized `.pkg`;
no ZIP, no Standalone.

Build: Mac (arm64, macOS 15.7.5, Xcode 16.0), release, universal arm64 + x86_64, macOS 11.0 minimum,
from a clean worktree of commit `0fea632` (`plugin/` is unchanged since `b88e429`; later commits change
documents and QA files only). SDK and tool revisions: `build-env.txt`. Artifacts outside git:
`plugin/target/dist-1.6.0/` (bundles), `plugin/target/release-1.6.0/` (installer, manual, manifest).

Not released: nothing is tagged, uploaded or staged in `Release Binaries/`.

| ID | Item | Evidence | Status |
|---|---|---|---|
| R1 | Version 1.6.0 in `plugin/Cargo.toml`, `VERSION`, bundle plists, README, manual | `PluginInfo` reads the crate version; AU `CFBundleShortVersionString` 1.6.0, component version 67072 (1.5.3: 66819); `Scripts/build_installer.sh` stops when `VERSION` and the crate differ | verified |
| R2 | Latest OpenPluginSDK | `main` was `07f97e1` on 2026-11-27 (fetched); the build uses branch `editor-pointer-left` `2969ebb` = that `main` plus two commits (below). Local branch, not pushed or merged | verified; the merge is the owner's |
| R3 | Audio Unit external sidechain (owner: "AU needs an external sidechain") | SDK `2969ebb`: an effect with a sidechain layout starts with two input elements, the second named "Sidechain" (before: one, until a host set the count). SDK proofs `au_*` 11 of 11, with a new check of the count. auval: "Input Scope Bus Configuration: Default Bus Count:2", "Bus Name: Sidechain", `AU VALIDATION SUCCEEDED` (`validators/auval-test-identity.txt`); pluginval AU lists two stereo input buses | verified (auval, pluginval, REAPER); not in Logic |
| R4 | External sidechain in every format | REAPER 7.80, `sidechain/reaper_sidechain.py`: a key on the plugin's second input, a steady tone on the main input; the output drops 38 dB during each burst and returns between them, and no key reaches the output, equal in AU, VST3, CLAP and LV2 (`sidechain/reaper.txt`). AAX: the built bundle's description, read through Avid's host layer, has `AAX_eProperty_SupportsSideChainInput` = 1 on the mono and on the stereo component (`validators/aax-description-1.6.0.txt`); AAX Validator `describe_validation` PASS | verified in four formats; AAX declared and validated, its key input not exercised in a host (needs Pro Tools) |
| R5 | Manual matches the code | `Docs/UserManual.md`: meters (PPM, RMS, VU), whole steps, sidechain per format, latency per oversampling setting (measured: 0; 3, 4, 5; 65, 71, 73 samples), sessions from 1.5.x, AAX without AudioSuite. PDF rebuilt, 8 pages looked at | verified |
| R6 | Manual pictures show the 1.6.0 editor | four pictures rendered by `plugin/examples/gui_render.rs` with audio running (`docs/scenarios.txt`, the settings of 1.5.3's pictures) and composed with `qa/2026-11-23-docs-1.5.3/compose_doc_screenshots.py`; looked at. The REAPER picture is a capture of a hidden REAPER 7.80 window with the VST3 of this source (the test-identity build; its instance renamed in the project to the shipped name, so the title bar reads as the shipped plugin's) | verified |
| R7 | README, release notes, third-party notices, binary licence, release text | `README.md` (version, download, build from source), `RELEASE_NOTES.md`, `THIRD_PARTY_LICENSES.md` and `Licenses/third-party/` (OpenPluginSDK, VST 3 SDK, AudioUnitSDK, the 22 linked crates from `cargo tree -e normal`), `LICENSE-BINARIES.txt` (the component paragraph: the owner should read it), `Docs/RELEASE_1.6.0.md`, `Docs/Release-1.6.0-QA.md` | verified |
| R8 | Full tier on the release commit (RL-1) | clean worktree of `0fea632` on SDK `2969ebb`: 6 passed, 0 failed, 0 skipped, 923 s: T-LINT, T-CLIPPY, T-TEST (49 tests, the full conformance run and the host-behaviour tests), T-SOAK (24 seeds), T-FUZZ, T-BENCH (no regression against the baseline); `commit-test/commit-0fea632-full.txt` | verified |
| R9 | Soak at 200 seeds on the release commit (RL-1) | `OPENPLUGIN_SOAK_SEEDS=200`, same worktree: T-SOAK passed, 6305 s (the 24-seed run took 757 s); `commit-test/commit-0fea632-soak200.txt` | verified |
| R10 | Identities unchanged (RL-2, PR-2, PR-3) | as `qa/2026-11-26-rust-port/ledger.md`: CLAP id, VST3 class ids, AU `aufx Hcmp OpGr`, LV2 URI, AAX codes `OpGr` / `Hcmp` / `jcbb`, `jccc` (now read back from the built bundle, `validators/aax-description-1.6.0.txt`), 27 parameter ids (`params::tests` on every `cargo test`); PACE product GUID 56C67100-D558-11F1-804C-005056928F3B as 1.5.3 (`aax-sign.log`) | verified |
| R11 | Universal binaries, min macOS 11.0, all five formats (RL-5) | installer payload: `lipo -archs` x86_64 arm64 and `vtool` minos 11.0 for VST3, AU, CLAP, LV2, AAX | verified |
| R12 | Developer ID signatures, hardened runtime, timestamp; no build or home paths (RL-3) | build script: `codesign ok` on five bundles, "no build path found in any binary"; `codesign --verify --deep --strict` on the installer payload | verified |
| R13 | AAX PACE Eden signature (Signing Only) | `aax-sign.log`: product "Harmonic Compressor", wrap configuration 7EFEC460-D558-11F1-804C-005056928F3B; `wraptool verify` on the installer payload; `codesign` still valid | verified |
| R14 | Validators on the final signed bundles (RL-4) | clap-validator 0.4.1: 34 passed, 0 failed, 0 warnings, 10 skipped (`validators/clap-validator-1.6.0.txt`); pluginval 1.0.4 VST3 strictness 10 with editor tests: SUCCESS, input buses mono/stereo main and mono/stereo sidechain (`validators/pluginval-vst3-1.6.0.txt`); `lv2_validate` on the bundle under its installed folder name: 0 errors (`validators/lv2-validate-1.6.0.txt`); AAX Validator 2024.6 on the PACE-signed bundle: 6 of 6 PASS, one warning (gaps between the algorithm context fields, "may be a problem on older hosts"; log outside git) | verified |
| R15 | Audio Unit validation | auval and pluginval AU (strictness 10) on the test-identity build, `aufx Hcmt OpGr`, user folder: PASS (`validators/`). The shipped code `Hcmp` is not validated here: `/Library` holds an older release under it | verified on the same source; shipped code unverified |
| R16 | Host suite (RL-4) | REAPER 7.80, a separate hidden instance, test-identity build in the user folders: 9 of 9 for AU, VST3, CLAP and LV2 (loads, finite, not silent, deterministic, editor opens, every parameter changes the sound, every parameter is shown, state survives save and reopen, editor reopens); `reaper/suite-*.txt`, `reaper/report-*.md`; the binaries REAPER had loaded: `reaper/loaded-binaries.txt` | verified |
| R17 | Installer | `Harmonic-Compressor-1.6.0-macos.pkg`, 14,413,832 bytes, SHA-256 `fd89daf6a8eac1d6a0c6afe18b32da5d1631cce2a45d08738c1f9ba54415261f`. Developer ID Installer signed, notarized (submission `25aa20b5-d059-41b3-9ad7-e2c9f442cc2c`, Accepted), stapled, Gatekeeper "accepted, Notarized Developer ID" (`pkg-build.log`). Expanded: the five bundles are byte-identical to the validated ones; manual, licences and notices present; no AppleDouble files | verified; not installed on any machine |
| R18 | Upgrade from 1.5.x | the LV2 installs as `Harmonic Compressor.lv2`, 1.5.x's folder, and a preinstall script removes the old folder first (an LV2 folder is not a bundle to the installer, so files would merge). The other four are bundles with `BundleOverwriteAction` upgrade, as since 1.5.1 | script and payload read; an upgrade install was not run |
| R19 | Plugin Manager manifest, checksums | `opengrid-manifest.json` (schema 2, modern, min macOS 11.0, five installs, the paths of 1.5.3), `.sha256` files for the installer and the manual, in `plugin/target/release-1.6.0/` | written; `released` is 2026-11-27 and must be regenerated if the date differs |
| R20 | Intel slice | `cargo test --release --target x86_64-apple-darwin` under Rosetta on the worktree of `0fea632`: 49 passed, 0 failed (unit tests, the full conformance run, every parameter, host behaviour, meters); `commit-test/commit-0fea632-x86_64-tests.txt`. The universal bundles carry an x86_64 slice (R11) | verified under Rosetta; not run on an Intel Mac |
| R21 | Null against 1.5.3 | 41 PASS, 0 failed on SDK `f2e8d15` with this plugin source (`qa/2026-11-26-rust-port/null/`). Not rerun on `2969ebb`: that commit changes the Audio Unit shell's C++ and documents, no Rust | carried over |
| R22 | Test builds removed | the five bundles of the first preparation removed before this run ("Harmonic Compressor Test" CLAP, VST3, AU, LV2 and an older "Harmonic Compressor Dev" VST3 1.5.1); the four of this run removed after the host tests | verified |

## Exceptions

`qa/exceptions.txt`, unchanged in this run (the context entries for Sidechain HP Freq and the transient
controls; `audio: Free Wheeling` and `state: Freewheel` for the LV2 host port). None is new.

## Notes

- The test runner's `--commit` mode cannot find the SDK (its worktree lacks the untracked
  `.cargo/config.toml`), so the worktree was made by hand with `Scripts/sdk_env.sh`; the report says
  "the working tree" and the log's first lines carry the plugin and SDK hashes.
- The commit-test hook also ran the plugin test list on the SDK commit `2969ebb` and reported lint
  findings in `openplugin-dsp`, `openplugin-host` and `openplugin-port`. That commit changes none of those
  files (only `native/au/` and documents), and the lint's rules are written for plugin crates; the SDK's
  own Audio Unit proofs pass.
- REAPER shows the VST3 and AU with "2/4 in 2 out": the sidechain is input channels 3/4.

## Not done / needs the owner

- Push and merge the SDK branch `editor-pointer-left` (two commits); the public build instructions in the
  README work only once it is in the SDK's `main`.
- Hosts: Pro Tools (the PACE-signed AAX must be installed in the system folder), Logic, Studio One,
  OpenGrid9. The AAX key input is therefore proven only by the shell's description and the validator.
- auval and a host run under the shipped AU code (needs the `/Library` copy replaced, that is, the
  installer run on this machine).
- An upgrade install over 1.5.3, and an LV2 session of 1.5.x opened in 1.6.0.
- A VoiceOver pass; editor timings on a quiet machine (the machine's load average was 17 to 72 during
  this run).
- Windows and Linux builds (native machines).
- Approval, then: tag `v1.6.0`, the GitHub release, `Release Binaries/` with `RELEASE-READY`, the
  Plugin Manager upload, the repository page check.
