# Harmonic Compressor 1.5.3 release status

Status: macOS 1.5.3 built and verified on 2026-11-17; installer rebuilt with the new manual and licence and notarized on 2026-11-23, as a single `.pkg`
(no ZIP). Not yet published. Release text: `Docs/RELEASE_1.5.3.md`. Ledger and evidence:
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
- Publishing: push, tag, GitHub release, Plugin Manager staging and upload.
