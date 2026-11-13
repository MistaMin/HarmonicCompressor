# Harmonic Compressor 1.5.1 — macOS QA ledger (2026-11-08)

Source: commit c41fba4 (HEAD, pushed). Uncommitted: RELEASE_NOTES.md (doc text, not a build input),
tools/make_bundle_zip.sh, tools/package_crossplatform.py, Docs/Release-1.5.1-QA.md, workflow deletion.
No Source/, CMakeLists.txt or Designs/ changes since HEAD. Candidate: dist/native-plugins-1.5.1/.
Scope rule: user decisions 2026-11-08 — Plugin Manager ZIP without Standalone; no hosted runners.

## Scope

| Item | Evidence | Status |
|---|---|---|
| Slider click no longer jumps to max (13 linear sliders, 4 themes, 2 editor lifecycles) | CTest SliderClickTest, EditorSliderTest (ctest.log) | PASS |
| 25 APVTS parameters bound to GUI (static) | gui_coverage.py: 25/25 bound | PASS |
| All 25 parameters change sound, visible in GUI, state round trip (VST3, AU, CLAP in REAPER) | report-vst3.md, report-au.md, report-clap.md | PASS |
| LV2 parameters change sound and are visible (REAPER) | report-lv2.md | PASS |
| LV2 state save/reopen in REAPER | report-lv2.md: 21 parameters read blank after reopen | **FAIL (open)** |
| Input/output section (Input Gain, Output Gain, meters) | GUI bindings in gui_coverage; REAPER visual sweep | PASS (bindings); ballistics not re-verified (no meter code change in 1.5.1) |
| Meter ballistics (VU/PPM/RMS standards) | not part of 1.5.1 change | NOT RUN (no meter change; full ballistic test not repeated) |
| Windows 150% HiDPI layout (F1 from 1.5.0) | prior 2026-11-03-win-hidpi | UNVERIFIED — not claimed fixed |

## Builds

| Build | Evidence | Status |
|---|---|---|
| Release universal arm64+x86_64, min 11.0 (VST3, AU, CLAP, LV2, AAX) | lipo -archs, otool LC_BUILD_VERSION minos 11.0 | PASS |
| Installed copies = candidate bytes (VST3, AU, CLAP, LV2 dylib, AAX) | sha256 match (plugin-verification.json; re-checked VST3 and AU) | PASS |
| Signatures: VST3, AU, CLAP, AAX bundles; LV2 inner .so | codesign --verify --strict | PASS |
| PACE signing (AAX) | aax-verify.log: signed, not wrapped | PASS (signing only; wrapping not part of this release path) |
| Apple notarization of plugin-only ZIP | notary.log submission 663d6f26-0cd1-461f-9818-f8ee4f7bff97: Accepted | PASS |
| Manifest sha256/size match ZIP | 132173694cac…10e3f, 24010373 bytes | PASS |
| Manifest build_class/min_os consistent | modern, 11.0 | PASS |
| Standalone in Manager ZIP | excluded by user decision | OUT OF SCOPE (user decision) |
| Local /Applications Standalone repair | invalid seal from old moduleinfo.json; repair needs admin | NOT DONE |

## Validators and hosts

| Check | Evidence | Status |
|---|---|---|
| CTest (Release) | ctest.log, rerun this session: 2/2 | PASS |
| auval (installed AU, aufx Hcmp OpGr) | auval -v run this session | PASS |
| clap-validator 0.4.1 on installed CLAP | clap-validator.log: 32 passed, 0 failed, 1 warning, 11 skipped | PASS with 1 WARNING (see below) |
| lilv/lv2info load of installed LV2 bundle | lv2info output: ports, UI, state interface present | PASS (metadata only) |
| lv2lint | not installed | SKIPPED |
| AAX Validator 2024.6 on installed AAX | aax-validator/aaxval_summary.txt: 5 pass, 1 FAIL (test.page_table.load) | **FAIL (open)**, see below |
| REAPER VST3 suite | report-vst3.md: every check PASS | PASS |
| REAPER AU suite | report-au.md: every check PASS | PASS |
| REAPER CLAP suite | report-clap.md: every check PASS | PASS |
| REAPER LV2 suite | report-lv2.md: state check FAIL | **FAIL (open)** |
| Pro Tools (AAX) host test | requires computer use with Pro Tools in foreground; user's hidden-window rule | NOT RUN |
| Logic (AU) host test | Logic is not installed on this Mac | NOT RUN (not installed) |
| Studio One host test | not requested for this release | NOT RUN |
| Controlled performance comparison (old vs new standalone, interleaved) | not measured; the 1.5.1 change is a hit-region/layout fix in Knob.h without new paint, timer or analyser work | NOT RUN |

## Open items and exceptions

1. **AAX test.page_table.load — FAIL (environment suspected, not cleared).** The same test fails on the
   installed, shipping HybridEQ.aaxplugin on this machine ("Failed to load page tables library"), so it
   looks like a validator/OS environment issue. The validator's own page-table library is what fails. It is
   still a validator FAIL and is not counted as pass. Needs a Windows/second-Mac or Avid support check.
2. **LV2 state round trip — FAIL in REAPER (not yet diagnosed).** The Mix probe and the full run show nearly every
   parameter reading blank after save and reopen, and "Free Wheeling" changed On -> Off. This may be the REAPER
   LV2 host's state handling or our LV2 state path. Not diagnosed; not claimed as a host-only issue. The LV2
   state interface is declared in dsp.ttl. Next step: dump the saved state directly with lilv and compare against
   the VST3 round trip.
3. **CLAP warning — state-invalid-random.** The plugin accepts 3 MB of random bytes through state load
   without crashing or rejecting them. Non-crash, reported as WARNING. Trailing-garbage ID test: PASSED.
4. **clap-validator skips (11)** are structural: the plugin has no preset-discovery, note ports, 64-bit
   audio, audio-ports-activation or configurable-audio-ports extension. Not failures.
5. **Pro Tools, Logic, Studio One** not run; AAX host behaviour beyond the validator is unverified.
6. **Performance comparison** not run. The wiki rule asks for it before release; this change does not touch
   paint, timers or analysers, but that is a judgement and not a measurement.
7. **Windows HiDPI 150%** from 1.5.0 remains unverified and not claimed fixed.
8. **RELEASE_NOTES.md** said GitHub pages carry no executable downloads, contradicting the approved
   distribution plan. Corrected (uncommitted). The candidate ZIP bundles an older copy of RELEASE_NOTES.md; the ZIP
   must be rebuilt before any release.

## Verdict

macOS plugin set: installed, signed and notarized candidate is consistent. **Not release-ready.** Two open FAILs
(AAX page-table validator test, LV2 state round trip in REAPER) and several NOT RUN checks remain. No RELEASE-READY marker
created. No commit, tag, upload or GitHub release made.

## Update 2026-11-08 (LV2 state investigation, Standalone removal)

Standalone: per user decision, this plugin ships no Standalone app. Removed from CMake's shipped
format list (`HARMONIC_COMPRESSOR_FORMATS VST3 AU LV2` + AAX when SDK present), README, User Manual
and RELEASE_NOTES. Dev-only Standalone option kept for UI work and not shipped. Configure re-run OK.
**The candidate binaries were built before this change: rebuild and re-verify before any release.**

LV2 state round trip — investigation (not fixed):
- Plugin-side state path is the same APVTS XML used by VST3, AU and CLAP. Those formats pass the
  REAPER state round trip for every parameter (report-vst3.md, report-au.md, report-clap.md).
- LV2 saves one base64 StateString (JUCE LV2 wrapper, store/retrieve), so the state blob is restored.
- Full 30-parameter probe (lv2_state_probe.sh): 24 of 30 values come back changed or blank after reopen
  and a render. Values that changed are not simply stale: e.g. Attack 294.0 -> 116.5, Threshold -45.6 -> 0.0.
- Control-port values: JUCE's LV2 wrapper pushes host port values into parameters
  (juce_audio_plugin_client_LV2.cpp `setValueFromHost`, called when a port value differs from the parameter).
  The REAPER project chunk stores only one control port value (`P enabled`), so restored parameter values
  are overwritten by the host's port buffers.
- Same effect in a different shipped product: HybridEQ LV2 in the same REAPER loses 45 of 50 parameter
  values on reopen. So this is the JUCE LV2 wrapper / REAPER LV2 port-state interaction, not Harmonic
  Compressor's own state code.
- Not yet settled: whether another LV2 host (for example Ardour, Carla) restores these values correctly.
- The suite's LV2 failure is real for REAPER; the suite's fallback path (no parsable chunk) also reads
  the display immediately after reopen, which the flush render rule in plugin-daw-test is meant to prevent.

Status: LV2 state round trip FAIL (open). Options: patch the JUCE LV2 wrapper (vendored, applied by CMake),
ship without LV2 in this release, or verify in a second LV2 host before deciding.

## Update 2026-11-08 (AAX page table, OpenGrid9 attempt)

AAX page table — root cause of `test.page_table.load` FAIL:
- Harmonic Compressor (and sibling JUCE product Tube Channel) shipped no AAX page table. JUCE registers a
  page resource only when the plugin names one (`juce_audio_plugin_client_AAX.cpp` ~2575-2579, via
  `AAXClientExtensions::getPageFileName`). The validator then reports "Failed to load page tables library"
  for real-time effects and "No page tables are registered" for automation.
- Passing comparisons on this Mac: ArcVu, RadarVU, OpenPreamp (JUCE, each ships a page XML) and
  Tube-Tech CL 1B (ships Resources/Pagetables.xml). Both tests pass for the three OpenGrid products.

Change made (source, uncommitted):
- `Resources/HarmonicCompressor.xml`: PageTables for plugIDs `jccc` (stereo) and `jcbb` (mono), manID `OpGr`,
  prodID `Hcmp`; one 24-cell page plus one cell on page 2, covering all 25 parameters.
- `Source/PluginProcessor.h`: `HarmonicCompressorAAXExtensions` returning `HarmonicCompressor.xml`; override of
  `getAAXClientExtensions()`.
- `CMakeLists.txt`: POST_BUILD copy of the XML into `Contents/Resources` of HarmonicCompressor_AAX.

Result on the rebuilt (unsigned build-output) AAX:
- `test.page_table.load`: PASS.
- `test.page_table.automation_list`: FAIL, `AAXHShell_GetNumControlNamesForId`. Still failing after
  testing single cells with: APVTS IDs (threshold, attack, mix, transientSpeed, algorithmMode), numeric
  IDs (0, 1, 2, 3, 10), display names (Threshold, Attack), "Master Bypass", "Bypass", and layouts with
  empty knob cells removed. Cause not identified. The host function is not in the AAX SDK headers on this
  machine. Next step: compare how ArcVu registers parameter IDs against ours (both use managed IDs),
  or get the host's expectation from Avid. This test stays FAIL.

Installed system AAX is unchanged (still the candidate build without the page table). Any release needs the
rebuilt, PACE-signed AAX (iLok required) and re-verification.

OpenGrid9 LV2 test (option 3) — not completed:
- /Applications/OpenGrid9.app is an Ardour fork. Its Lua runner wrapper exists, but the binary it execs
  (Contents/lib/opengrid9-lua) is not in the install. The MCP HTTP module exposes plugin add, parameter set,
  and session save, but no session-open tool, and it did not listen after a hidden launch. Relaunching
  with a session file is the remaining route; not attempted. The launched instance was stopped.
