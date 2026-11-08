# Harmonic Compressor 1.5.1 release status

Status: candidate; not published. Canonical workflow: DSP Engineering Wiki,
Make-Plugin-Release. User decisions on 2026-11-08: no Standalone in the Plugin
Manager ZIP; no GitHub-hosted runners. Windows and Linux require user-owned
native build machines. Do not ship the earlier hosted-runner archives.

## Verified macOS candidate

Source binary baseline: c41fba4675cc4b013cd3d9a8628c644526af27e7.
The pending packaging-script changes affect archive contents, not plugin build inputs.
Candidate: `dist/native-plugins-1.5.1/`.

- Both Release CTest targets pass, including the editor regression covering all
  13 linear sliders across four themes and two editor lifecycles (1,144 checks).
- The Manager ZIP includes VST3, AU, CLAP, LV2 and PACE-signed AAX; no Standalone.
- Extracted executable slices are universal arm64/x86_64. Signatures verify and
  executable hashes match the five installed plugin formats.
- Apple accepted the plugin-only ZIP: submission
  `663d6f26-0cd1-461f-9818-f8ee4f7bff97`.
- Archive size, SHA-256 and five install paths are recorded in the candidate
  schema-2 manifest. Modern macOS 11.0+; legacy build not published.

Earlier macOS evidence is retained in `dist/wiki-candidate-1.5.1/QA/`: installed
AU validation, VST3 OpenGrid9 audio and exact state roundtrip for all 25 plugin
parameters, 25/25 GUI/manual bindings, minimum OS and original signing records.
These checks are distinct from native GUI interaction inside each DAW.

## Remaining release work

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
