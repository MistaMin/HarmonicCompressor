# Harmonic Compressor 1.5.1 release status

Status: candidate; not published. Canonical workflow: DSP Engineering Wiki,
Make-Plugin-Release. User decisions on 2026-11-08: no Standalone in the Plugin
Manager ZIP; no GitHub-hosted runners. Windows and Linux require user-owned
native build machines. Do not ship the earlier hosted-runner archives.

## Verified macOS candidate

Source: commit `8a5b2ce` on master (AAX page table, no Standalone in release
formats, packaging scripts). Candidate: `dist/release-1.5.1/`.

- Both Release CTest targets pass, including the editor regression covering all
  13 linear sliders across four themes and two editor lifecycles (1,144 checks).
- The Manager ZIP includes VST3, AU, CLAP, LV2 and PACE-signed AAX; no Standalone.
- The ZIP carries the AAX control-surface page table
  (`Resources/HarmonicCompressor.xml` with `<Editor><DiscCtrls>`), which the
  validator needs for `test.page_table.automation_list`.
- Extracted executable slices are universal arm64/x86_64; Apple and PACE
  signatures verify on every format inside the ZIP.
- Avid AAX Validator 2024.6 on the AAX bundle extracted from the ZIP: all six
  default tests PASS, including `page_table.load` and `page_table.automation_list`.
  The remaining warnings (algorithm context gaps; automatable parameters not in
  the default one-control-per-page table) also appear on the sibling plugins.
- Apple accepted the ZIP: submission
  `18667616-14fc-4111-8506-72aede2b0280`.
- Archive size 24012167 bytes, SHA-256
  `47b1617a973ae5a026f61f011806b7aabdf068576ef848939c59f1e023f18dee`, five
  install paths in the schema-2 manifest. Modern macOS 11.0+; legacy not published.

The AAX validator cannot load bundles from `/Volumes/Developer` (signed or
unsigned; the sibling RadarVU build fails the same way), while installed bundles
in `/Library` load. The validator run therefore used the ZIP extracted to `/tmp`.

Earlier macOS evidence is retained in `dist/wiki-candidate-1.5.1/QA/`: installed
AU validation, VST3 OpenGrid9 audio and exact state roundtrip for all 25 plugin
parameters, 25/25 GUI/manual bindings, minimum OS and original signing records.
These checks are distinct from native GUI interaction inside each DAW.

## Remaining release work

- Install the new ZIP to `/Library` (administrator authentication) and re-run the
  installed-plugin checks: the copies in `/Library` are still the earlier
  candidate, which has no page table. Pro Tools and control-surface behaviour
  have not been tested on the new build.
- LV2 state round trip: REAPER 7.80 on this Mac restores no LV2 plugin state at
  all. Harmonic Compressor, ArcVu, Blue Channel, DualSone and the third-party
  AudioGridder all reopen at defaults, while VST3 restores correctly in the same
  REAPER instance. Recorded as a host limitation, not a plugin defect; no second
  LV2 host is installed here to prove the plugin's own state interface.
- Obtain Windows and Linux machine connections; build and test the same clean
  source revision natively; package without Standalone. Confirm Windows signing.
- Complete or explicitly resolve outstanding hosted GUI/format-validator checks
  and the controlled performance comparison. Earlier results did not verify
  Pro Tools/Logic editor lifecycle or CLAP/LV2 validators. Historical Windows
  150% HiDPI clipping was not revalidated or claimed fixed.
- The locally installed Standalone has an invalid resource seal. It is excluded
  from the Manager ZIP. Its optional repair needs administrator authentication;
  it must not be described as successfully repaired.
- Finish QA, final source commit/push/tag and GitHub binary publication, then
  approved Release Binaries staging and Plugin Manager site upload/verification.
  The GitHub release is still a draft. No RELEASE-READY marker has been created.

The prior installer/DMG and hosted archives are retained as historical candidates,
not current approved release assets. Unrelated DSP evidence edits are preserved.
