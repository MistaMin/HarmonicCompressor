Harmonic Compressor 1.6.0 is the plugin rebuilt in Rust on OpenPluginSDK, with new PPM, RMS and VU meters, controls in whole steps, and an external sidechain in every format. It keeps its name and identifiers, so sessions, presets and automation of 1.5.x load.

## What is new

- **Meters:** PPM in dBFS (IEC 60268-10 Type I, the default), RMS over 300 ms in dBFS, or VU (IEC 60268-17, 0 VU = -18 dBFS). The selector is on the input meter and the choice is saved with the session. Meter readouts, the gain-reduction readout and the gain on the AUTO GAIN key are whole numbers.
- **Whole steps:** Threshold, Ratio, Knee, Makeup, Input and Output Gain, the four harmonics and the transient Attack and Sustain move in whole dB, ratios and percent. A session keeps its 1.5.x values until the control is moved. Attack and Speed below 1 ms read in microseconds; hosts get Mix in percent and the times with their unit.
- **External sidechain in every format.** The Audio Unit has its second input ("Sidechain") from the start, so a host that only looks for it offers it.
- **The editor** uses no CPU while nothing moves, and with the editor closed the meters cost nothing. Screen readers get every control's name and value. The smallest text is 8 pt. Menus open inside the plugin window.

## Sound

- At 1x oversampling 1.6.0 sounds the same as 1.5.3: in 41 test cases (every model, automation, harmonics, sidechain, transient designer, Auto Gain, Delta) the difference is below -115 dB.
- 2x, 4x and 8x use new anti-alias filters (a Kaiser FIR for Lin Phase, a minimum-phase FIR for Min Phase) and sound slightly different from 1.5.3. Reported latency: 0 at 1x; 3, 4 and 5 samples at 2x, 4x and 8x Min Phase; 65, 71 and 73 samples in Lin Phase.
- The plugin reports a 0.7 s tail (the harmonics' DC blocker) instead of none.

## Sessions from 1.5.x

VST3, AU, CLAP and AAX sessions load with every value, including the per-model memory. Three things do not carry over:

- Automation of the VST3 host bypass recorded with 1.5.x does not replay (the bypass parameter has a new identifier).
- An LV2 host session saved with 1.5.x is not expected to restore its settings: the LV2 version now has control ports. This was not tested.
- The AAX has no AudioSuite version. Its control-surface page table is generated from the parameter list.

## Download and install

macOS 11 or later, universal (Apple silicon and Intel).

Run `Harmonic-Compressor-1.6.0-macos.pkg`. It installs:

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

- The plugin's test list on the release commit: realtime lint, clippy, unit tests, the full conformance kit at every sample rate from 22.05 to 384 kHz, host-behaviour tests, a soak over 200 seeds, strict fuzzing and a benchmark against the stored baseline.
- On the installer's own plug-in copies:
  - clap-validator 0.4.1: 34 passed, 0 failed, 0 warnings, 10 skipped.
  - pluginval 1.0.4, VST3, strictness 10, with the editor tests: SUCCESS.
  - `lv2_validate`: 0 errors.
  - Avid AAX Validator 2024.6 on the PACE-signed AAX: 6 of 6 PASS (one warning: gaps between the algorithm context fields, which may matter on older hosts).
- On the same source, built under a test name and test codes so it could sit beside an installed copy:
  - auval and pluginval (AU, strictness 10): SUCCESS; auval lists two input buses, the second named "Sidechain".
  - REAPER 7.80 with the AU, VST3, CLAP and LV2: 9 of 9 checks each (loads, renders, every parameter changes the sound and is on the editor, state survives save and reopen, the editor reopens).
  - External sidechain in REAPER with the AU, VST3, CLAP and LV2: a key on the sidechain input ducks a steady tone by 38 dB and releases it between hits.

## Known limits

- Not tested with this version: Pro Tools (the AAX passed Avid's validator; its key input was not exercised in a host), Logic, Studio One, and an upgrade install over 1.5.3.
- The Audio Unit passed auval under a test code only; on the build machine an older release holds the shipped code.
- The editor is tested on macOS only; Windows and Linux builds are not available yet.
- The pointer keeps its arrow over the module headers and the curve handles, and the mouse wheel follows the system's scroll direction.
- In the Hyperdrive theme the DELTA key lights in the same cyan as AUTO GAIN.

## Files

- `Harmonic-Compressor-1.6.0-macos.pkg` (14,413,832 bytes) and `.sha256`
- `HarmonicCompressor-1.6.0-User-Manual.pdf` and `.sha256`
- `opengrid-manifest.json` (Plugin Manager manifest)
