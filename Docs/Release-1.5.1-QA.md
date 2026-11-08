# Harmonic Compressor 1.5.1 release status

Status: macOS 1.5.1 published on GitHub (tag `v1.5.1`, commit `0439514`) and staged in
`Release Binaries/MistaMin/HarmonicCompressor/1.5.1/`; website sync pending. Canonical
workflow: DSP Engineering Wiki, Make-Plugin-Release. User decisions on 2026-11-08:
no Standalone in the Plugin Manager ZIP; no GitHub-hosted runners. Windows and
Linux require user-owned native build machines. Do not ship the earlier
hosted-runner archives.

## Verified macOS candidate

Source: master with the 1.5.1 changes plus the state-restore race fix. Candidate:
`dist/release-1.5.1/`. QA ledger: `qa/2026-11-09-release-1.5.1/ledger.md`.

- Both Release CTest targets pass, including the editor regression covering all
  13 linear sliders across four themes and two editor lifecycles (1,144 checks).
- The Manager ZIP includes VST3, AU, CLAP, LV2 and PACE-signed AAX; no Standalone.
- The ZIP carries the AAX control-surface page table
  (`Resources/HarmonicCompressor.xml` with `<Editor><DiscCtrls>`), which the
  validator needs for `test.page_table.automation_list`.
- Extracted executable slices are universal arm64/x86_64; Apple and PACE
  signatures verify on every format inside the ZIP. The installed copies were
  re-installed from the ZIP and are byte-identical to it.
- Validators on the installed build: auval PASS; clap-validator 44 tests, 32
  passed, 0 failed, 1 warning, 11 skipped; pluginval strictness 10 SUCCESS on
  VST3 (3/3 runs) and AU; AAX Validator 2024.6 6/6 PASS, including
  `page_table.load` and `page_table.automation_list`.
- REAPER 7.80 host suite on the installed build: VST3, AU and CLAP pass all nine
  checks (load, audio, determinism, editor, every parameter audible, every
  parameter visible, state round trip, editor lifecycle). 25/25 parameters are
  bound to the GUI.
- pluginval's strictness-10 state test exposed a race: a mode switch queued just
  before a state restore could overwrite the restored parameters. Fixed in
  `PluginProcessor` with a restore-generation guard; the failure was 1–3
  parameters per run before the fix and 0 in 3/3 runs after it.
- Apple accepted the ZIP: submission
  `0929bda7-3c25-475f-be2f-fd165afcbcd5`.
- Archive size 24012245 bytes, SHA-256
  `3216d42c1f0f41ae35402ca1afe1ed5cb7eee76f6d0899f23d8582bd9ebfa2bf`, five
  install paths in the schema-2 manifest. Modern macOS 11.0+; legacy not published.

## LV2 state: host limitation, not a plugin defect

REAPER 7.80 on this Mac restores no LV2 plugin state on project reopen. The same
flow that restores VST3 exactly (audio difference -240 dB) leaves LV2 at defaults
(-5.6 dB); Harmonic Compressor, ArcVu, Blue Channel, DualSone and the third-party
AudioGridder all fail the same way. The state is written into the project under
`urn:opengrid:harmoniccompressor:StateString`, so the plugin saves it; REAPER does
not pass it back. The LV2 parameter path itself works (sidechain HP filter moves
the output by 13 dB). No second LV2 host is installed here, so the plugin's own
LV2 state interface is not independently proven; the suite's LV2 state FAIL is
expected on this host and is recorded as an exception, not as a pass.

The AAX validator cannot load bundles from `/Volumes/Developer` (signed or
unsigned; the sibling RadarVU build fails the same way), while installed bundles
in `/Library` load; AAX validation therefore used the installed copy and the ZIP
extracted to `/tmp`.

## Remaining release work

- Website sync (`opengrid.studio/ogpm`) of the 1.5.1 manifest and archive: the local
  `Release Binaries` staging and `index.json` entry are done, the upload is not.
- Pro Tools interactive test and control-surface behaviour on the new build: the
  AAX Validator suite is the automated evidence; a foreground Pro Tools session
  test has not been run.
- Obtain Windows and Linux machine connections; build and test the same clean
  source revision natively; package without Standalone. Confirm Windows signing.
- Controlled performance comparison was not measured; 1.5.1 changes hit-testing
  and state handling, not painting, timers or the audio path.
- The locally installed Standalone has an invalid resource seal. It is excluded
  from the Manager ZIP. Its optional repair needs administrator authentication;
  it must not be described as successfully repaired. Historical Windows 150%
  HiDPI clipping was not revalidated or claimed fixed.

The prior installer/DMG and hosted archives are retained as historical candidates,
not current approved release assets. Unrelated DSP evidence edits are preserved.
