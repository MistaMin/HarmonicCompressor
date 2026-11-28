# Harmonic Compressor 1.6.0 release status

Status: macOS 1.6.0 built, signed, notarized and verified on 2026-11-27 as a single `.pkg` (no ZIP).
**Not published:** no tag, no GitHub release, nothing staged for the Plugin Manager. Release text:
`Docs/RELEASE_1.6.0.md`. Ledger and evidence: `qa/2026-11-27-release-1.6.0/`; the port's own QA:
`qa/2026-11-26-rust-port/`.

## Verified

- Universal arm64/x86_64, macOS 11.0 minimum, VST3, AU, CLAP, LV2, AAX; no Standalone.
- Built from commit `0fea632` (clean worktree) on OpenPluginSDK `2969ebb`.
- Test list on that commit: full tier 6 of 6 passed (realtime lint, clippy, tests, soak, strict fuzz,
  benchmark against the baseline), then the soak at 200 seeds passed.
- Developer ID signatures with hardened runtime on every format; AAX PACE-signed (product GUID
  `56C67100-D558-11F1-804C-005056928F3B`, as 1.5.3).
- Installer signed with Developer ID Installer, notarized (submission
  `25aa20b5-d059-41b3-9ad7-e2c9f442cc2c`, Accepted), stapled, Gatekeeper accepted.
- The installer's payload is identical to the validated bundles.
- On those bundles: clap-validator 34 passed, 0 failed, 0 warnings; pluginval VST3 strictness 10 with
  editor tests SUCCESS; `lv2_validate` 0 errors; AAX Validator 2024.6 6/6 PASS on the PACE-signed AAX.
- On the same source under a test identity: auval and pluginval AU SUCCESS; REAPER 7.80 AU, VST3, CLAP
  and LV2 9/9 each; external sidechain in REAPER in all four.
- `Harmonic-Compressor-1.6.0-macos.pkg`: 14,413,832 bytes, SHA-256
  `fd89daf6a8eac1d6a0c6afe18b32da5d1631cce2a45d08738c1f9ba54415261f`.

## Not done

- OpenPluginSDK: the build uses two commits that are not in the SDK's `main` yet (branch
  `editor-pointer-left`). The README's build instructions need them merged.
- Pro Tools (AAX in a host, its key input), Logic, Studio One, OpenGrid9.
- auval and hosts under the shipped AU code; an upgrade install over 1.5.3; an LV2 session of 1.5.x.
- VoiceOver pass; editor timings on a quiet machine.
- Windows and Linux builds (native machines required).
- Publishing: tag `v1.6.0`, GitHub release, `Release Binaries/` with `RELEASE-READY`, Plugin Manager
  upload (`upload_release_binaries.sh`, run by the owner in their own Terminal), repository page check.
