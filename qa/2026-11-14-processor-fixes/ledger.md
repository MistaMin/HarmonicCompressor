# Processor fixes — scope ledger (2026-11-14)

Source of scope: user request in session 2026-11-14, "fix both problems", referring to the two issues found
while adding Auto Gain and Delta (`qa/2026-11-14-autogain-delta/ledger.md`, "Found on the way").
Design: `dsp/decisions/ADR-0002-dry-path-and-async-work.md`.
Test: `ProcessorFixesTest` (`processor_fixes_test.cpp`); the same file built against master @ d5b833c gives
`before.log`. Build: `build-autogain/` (Release, arm64).

| ID | Item | Source | Code | GUI | Test | Evidence | Status |
|---|---|---|---|---|---|---|---|
| F1a | Queued model/oversampling work never runs on a destroyed processor | request | PluginProcessor.cpp `parameterChanged`, `handleAsyncUpdate`, destructor | none | `[F1]` lifetime, 50 instances | before: exit 139 (segfault); after: PASS | verified |
| F1b | Mode memory unchanged (switch, return, coalesced switches) | regression | `handleAsyncUpdate` | Model selector | `[F1]` mode checks | before and after: PASS | verified |
| F1c | 1.5.1 restore-race fix kept | regression | `pendingModeSwitch` generation | none | `[F1]` restore race | before and after: PASS | verified |
| F1d | Latency report follows Oversample changes | regression | `handleAsyncUpdate` | OS selectors | `[F1]` latency | 64 at 8x Lin, 0 at 1x | verified |
| F1e | No allocation when a parameter change is queued from the audio thread | ADR | `triggerAsyncUpdate` | none | code review (JUCE AsyncUpdater posts a pre-allocated message) | — | verified by review |
| F2a | Parallel Mix (50 %, 0 %) matches 100 % wet at every frequency in every oversampling mode | request | Resampler.h `alignDryBuffers`; PluginProcessor.cpp dry capture | MIX knob | `[F2]` 7 modes x 6 frequencies | before: up to -10.7 dB at 18 kHz; after: 0.000 dB | verified |
| F2b | Delta nulls on neutral settings in every oversampling mode | follow-on | same | DELTA key | AutoGainDeltaTest `[S11]` | before: -7..-28 dB; after: -280 dB | verified |
| F2c | Auto Gain still matches bypass loudness in every mode | regression | loudness reference unfiltered | AUTO GAIN key | AutoGainDeltaTest `[S3]` | within 0.02 LU | verified |
| F2d | CPU cost measured and reported | engineering | — | — | `ProcessorBench` before/after | `bench.log`; 4x Lin 6.70 % exceeds STATUS C7's 6 % budget | verified (cost reported; over budget at 4x Lin) |
| F3 | Existing tests, all formats build | regression | — | — | CTest 4/4; VST3/AU/CLAP/LV2/AAX build | `ctest.log` | verified |
| F4 | Host validators on this build | regression | — | — | pluginval, auval, AAX Validator | — | unverified: pluginval is not installed; auval needs this build installed over 1.5.1 |
