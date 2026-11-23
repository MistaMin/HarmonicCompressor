# Harmonic Compressor 1.5.3 release status

Status: macOS 1.5.3 built and verified on 2026-11-17; installer rebuilt with the new manual and licence and notarized on 2026-11-23, as a single `.pkg`
(no ZIP), published on GitHub as tag `v1.5.3`. Release text: `Docs/RELEASE_1.5.3.md`. Ledger and evidence:
`qa/2026-11-17-release-1.5.3/` (fix QA in `qa/2026-11-17-audit-fixes/`).

## Verified

- Universal arm64/x86_64, macOS 11.0 minimum, VST3, AU, CLAP, LV2, AAX; no Standalone.
- CTest 4/4 on the release build; no build warnings in project sources.
- Developer ID signatures with hardened runtime on every format; AAX PACE-signed (product GUID
  `56C67100-D558-11F1-804C-005056928F3B`).
- Installer signed with Developer ID Installer, notarized (submission
  `cd12a44f-093a-4009-bce5-77ee65ca62c8`, Accepted), stapled, Gatekeeper accepted.
- The installer's payload is identical to the signed bundles.
- On the installer payload: pluginval VST3 strictness 10 SUCCESS 3/3 and with editor tests;
  clap-validator 32 passed, 0 failed, 1 warning (as 1.5.2); AAX Validator 2024.6 6/6 PASS.
- On the same source: auval and pluginval AU SUCCESS; REAPER CLAP 9/9, LV2 8/9 (host limit);
  OpenGrid9 AU checks PASS under a test AU code (editor checks skipped).
- `Harmonic-Compressor-1.5.3-macos.pkg`: 24,569,321 bytes, SHA-256
  `71c24cf3e0a10f4c04a20298af0ca3faa3a19f740de8fb9407c94fb97cf925cd`.

## Not done

- REAPER with the AU: on this machine REAPER loads the AU installed in `/Library` (an older version).
- Pro Tools interactive session, Logic, Studio One.
- Windows and Linux builds (native machines required).
- Plugin Manager site upload (`upload_release_binaries.sh`, run by the owner in their own Terminal).

## Published (2026-11-23)

- GitHub release `v1.5.3` (tag on 2756d2e), marked Latest. Assets: the `.pkg`, the manual PDF, their
  `.sha256` files and `opengrid-manifest.json`. Downloaded back: both checksums OK, manifest identical.
- README download links and pictures on master return 200.
- Commits bf1ec03, 7a8a1bf, 0314faa, 2756d2e show author login `MistaMin` on GitHub; no assistant
  attribution lines.
- Repository description, homepage and topics match the shipped formats (unchanged).
- Staged in `Release Binaries/MistaMin/HarmonicCompressor/1.5.3/` with `RELEASE-READY`; the upload
  script's dry run lists it as approved.
