Harmonic Compressor 1.5.3 fixes Red and Hybrid in stereo, Hybrid's feedback path, external sidechain ducking and per-model memory, and lowers CPU use with oversampling.

## Sound changes in saved sessions

- **Red and Hybrid** now detect stereo-linked like the other five models: the louder channel sets one gain for both channels. Before, both channels shared one detector that ran twice per sample, so release ran about twice as fast as set (Release 100 ms: measured 50 ms before, 100 ms now), the gain stepped at every host buffer, and the result depended on the buffer size. Saved Red and Hybrid sessions sound different: slower, smoother release.
- **Hybrid** now blends its feedback path as described (75 % feed-forward, 25 % feedback). It was never fed, so Hybrid compressed less than its Ratio setting. Saved Hybrid sessions compress somewhat more (a drum loop at -24 dB threshold, 4:1: -6.85 dB before, -7.83 dB now).

## Fixes

- **Hybrid at ratios between 0:1 and 1:1** boosted loud signals without limit (+14 dB at 0.5:1, +128 dB at 0.1:1). It now only reduces gain there, like the other models.
- **External sidechain:** a sidechain that went silent between hits made the compressor detect the main input instead. Once the sidechain has carried signal, silence on it now means no compression.
- **Per-model memory** is saved with the session. Choosing a model in the plugin window recalls its settings; a model change from host automation changes only the model, so offline renders no longer depend on timing. Sessions saved by 1.5.2 and earlier load as before.
- **Knee** is enabled only in Red, the one model with a soft knee; in the other models it had no effect. The transfer curve now draws their hard knee.
- **Transfer curve drag** no longer resets an infinite or negative ratio to 20:1 or 1:1.
- **CPU:** Delta's reference filters run only while Delta and Auto Gain are on. 4x Lin Phase, 256-sample blocks at 48 kHz: 0.37 → 0.28 ms (6.99 → 5.26 % of the block), back under the 6 % budget.

## Download and install

macOS 11 or later, universal (Apple silicon and Intel).

Run `Harmonic-Compressor-1.5.3-macos.pkg`. It installs:

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

- Unit and regression tests, 4 of 4, on the release build. The processor test now includes buffer-size invariance (identical output at 64- and 512-sample blocks in all 7 models), Hybrid below 1:1, a silent external sidechain, and per-model memory across save and reload; the first three fail on 1.5.2's code.
- On the installer's own plug-in copies:
  - pluginval 1.0.4, VST3, strictness 10: SUCCESS in 3 of 3 runs, and with the editor tests.
  - clap-validator 0.4.1: 32 passed, 0 failed, 1 warning (random state data is accepted without a crash, as in 1.5.2), 11 skipped.
  - Avid AAX Validator 2024.6: 6 of 6 PASS, including the control-surface page table.
- On the same source: auval and pluginval (AU, strictness 10) SUCCESS; REAPER suite with the CLAP (9 of 9) and LV2 (8 of 9: REAPER's LV2 state limit below); OpenGrid9 with the AU (built under a test AU code so the host could not pick up an older installed copy): load, audio, determinism, every parameter audible and the state round trip pass (editor checks need a window and were skipped).

## Known limits

- Not tested with this version: REAPER with the AU (on this machine REAPER loads the AU installed system-wide instead of a test copy; the AU passed auval and pluginval, and the OpenGrid9 host checks under a test code), an interactive Pro Tools session, Logic, Studio One.
- REAPER on macOS does not restore LV2 plug-in state for any plug-in. This is a host limitation, recorded in 1.5.1.
- In the Hyperdrive theme the DELTA key lights in the same cyan as AUTO GAIN.
- Windows and Linux builds are not available yet.

## Files

- `Harmonic-Compressor-1.5.3-macos.pkg` (24,569,321 bytes) and `.sha256`
- `HarmonicCompressor-1.5.3-User-Manual.pdf` and `.sha256`
- `opengrid-manifest.json` (Plugin Manager manifest)
