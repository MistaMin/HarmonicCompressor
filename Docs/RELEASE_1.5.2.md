Harmonic Compressor 1.5.2 adds Auto Gain and Delta monitoring and fixes parallel Mix with oversampling.

## What's new

- **Auto Gain:** matches the output to the input's K-weighted short-term loudness (ITU-R BS.1770, 3 s window), gated at -70 LUFS, ±24 dB range. The AUTO GAIN key shows the applied gain. The reference is the untouched input, so Input Gain works as a drive control.
- **Delta:** plays only what the plugin changes: the output minus the delay-compensated dry signal (the untouched input while Auto Gain is on).
- Two host parameters are appended (`autoGain`, `delta`). Existing parameter identifiers and saved sessions are unchanged; older sessions load with both off.
- The four main knobs are slightly narrower to make room for the new keys.

## Fixes

- **Parallel Mix with oversampling:** the dry signal now passes through the same oversampling filters as the processed signal. Before, it was dulled (-2 dB at 10 kHz, up to -8 dB at 18 kHz) and, in Min Phase modes, partly cancelled the processed signal (Mix 50 %: up to -10.7 dB at 18 kHz). Now Mix 50 % and 0 % match 100 % within 0.000 dB in every oversampling mode.
- **Crash:** a host removing the plugin right after a model or oversampling change could crash. Those changes, including from automation, no longer allocate memory on the calling thread.

## Download and install

macOS 11 or later, universal (Apple silicon and Intel).

Run `Harmonic-Compressor-1.5.2-macos.pkg`. It installs:

- VST3 to `/Library/Audio/Plug-Ins/VST3`;
- AU to `/Library/Audio/Plug-Ins/Components`;
- CLAP to `/Library/Audio/Plug-Ins/CLAP`;
- LV2 to `/Library/Audio/Plug-Ins/LV2`;
- AAX to `/Library/Application Support/Avid/Audio/Plug-Ins`;
- the manual and licences to `/Library/Application Support/Harmonic Compressor`.

An earlier version is replaced in place.

The installer is signed with a Developer ID, notarized by Apple and stapled. Every plug-in is Developer ID signed with the hardened runtime, and the AAX is PACE Eden signed for Pro Tools. No Standalone app is included.

The release is also available through the OpenGrid Plugin Manager.

## Checks this build passed

- Unit and regression tests, 4 of 4: Auto Gain loudness match within 0.03 LU, Delta null, dry-path response in all 7 oversampling modes, processor lifetime, slider and editor regression.
- On the installer's own plug-in copies:
  - pluginval 1.0.4, VST3, strictness 10: SUCCESS in 3 of 3 runs.
  - clap-validator 0.4.1: 32 passed, 0 failed, 1 warning (random state data is accepted without a crash, as in 1.5.1), 11 skipped.
  - Avid AAX Validator 2024.6: 6 of 6 PASS, including the control-surface page table.
- On the same source: REAPER (VST3), all 9 host checks: load, audio, determinism, editor, every parameter audible, every parameter visible, state round trip, editor reopening. OpenGrid9: the audio, parameter and Auto Gain/Delta checks.

## Known limits

- CPU use rises in oversampled modes because the dry signal is now filtered too. Per 256-sample block at 48 kHz: 4x Lin Phase 0.25 → 0.36 ms; 8x Lin Phase 0.49 → 0.67 ms.
- Not tested with this version: auval and pluginval on the AU, REAPER with the AU, CLAP and LV2 builds, an interactive Pro Tools session, Logic, Studio One.
- REAPER on macOS does not restore LV2 plug-in state for any plug-in. This is a host limitation, recorded in 1.5.1.
- In the Hyperdrive theme the DELTA key lights in the same cyan as AUTO GAIN.
- Windows and Linux builds are not available yet.

## Files

- `Harmonic-Compressor-1.5.2-macos.pkg` (24,641,380 bytes) and `.sha256`
- `HarmonicCompressor-1.5.2-User-Manual.pdf` and `.sha256`
- `opengrid-manifest.json` (Plugin Manager manifest)
