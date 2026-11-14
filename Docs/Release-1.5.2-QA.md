# Harmonic Compressor 1.5.2 release status

Status: macOS 1.5.2 built, signed, notarized and verified on 2026-11-14 as a single installer
`.pkg` (no ZIP), published on GitHub as tag `v1.5.2`. Release text: `Docs/RELEASE_1.5.2.md`.
Ledger and evidence: `qa/2026-11-14-release-1.5.2/` (feature QA in `qa/2026-11-14-autogain-delta/`
and `qa/2026-11-14-processor-fixes/`).

## Verified

- Universal arm64/x86_64, macOS 11.0 minimum, VST3, AU, CLAP, LV2, AAX; no Standalone.
- CTest 4/4 on the release build.
- Developer ID signatures with hardened runtime on every format; AAX PACE-signed (product GUID
  `56C67100-D558-11F1-804C-005056928F3B`).
- Installer signed with Developer ID Installer, notarized (submission
  `f5a77ef9-f960-4e0f-9252-8753feb09cbf`, Accepted), stapled, Gatekeeper accepted.
- The installer's payload is byte-identical to the signed bundles.
- On the installer payload: pluginval VST3 strictness 10 SUCCESS 3/3; clap-validator 32 passed,
  0 failed, 1 warning (as 1.5.1); AAX Validator 2024.6 6/6 PASS.
- `Harmonic-Compressor-1.5.2-macos.pkg`: 24,641,380 bytes, SHA-256
  `2434ac8cfc2decd15b43cb6c85795c2ae76c66a9f7be60bd64e483e9aafd6a2d`.

## Not done in this run

- auval, pluginval AU, and the REAPER suite on AU/CLAP/LV2: they need this build installed over
  1.5.1 in `/Library`, which needs the administrator password. Not installed.
- Pro Tools interactive session, Logic, Studio One.
- Plugin Manager site upload (`upload_release_binaries.sh`, run by the owner in their own Terminal).
- Windows and Linux builds (native machines required).

## Published (2026-11-14)

- GitHub release `v1.5.2` (tag on e0f615e), marked Latest. Assets: the `.pkg`, the manual PDF, their
  `.sha256` files and `opengrid-manifest.json`. Downloaded back: both checksums OK, manifest identical.
- README download links and pictures on master return 200.
- Commits 8b21e93, 3d4d99b, e0f615e show author login `MistaMin` on GitHub; no assistant attribution
  lines. Pull request #1 (the same changes, earlier commit identity) was closed with a pointer to 8b21e93;
  its branch `feature/autogain-delta` still exists on GitHub.
- Repository description corrected to the formats and platform actually shipped (macOS only).
- Staged in `Release Binaries/MistaMin/HarmonicCompressor/1.5.2/` with `RELEASE-READY`; the upload
  script's dry run lists it as approved. The site upload itself is still to be run by the owner.
