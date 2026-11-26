# Port to OpenPluginSDK 1.0.0-beta (2026-11-26)

`plugin/` is Harmonic Compressor on OpenPluginSDK (checkout `19fac89`, the current `main`), in Rust.
It exports CLAP, LV2 and the SDK's VST3, Audio Unit and AAX shells; CLAP, VST3 and AU were built
and checked here. It replaces the JUCE processor
of 1.5.3 (`Source/`), which is untouched. **The editor is not ported yet**: the plugin has no GUI.

Build and test:

```sh
. Scripts/sdk_env.sh                      # finds OpenPluginSDK, writes the untracked .cargo/config.toml
cd plugin && cargo test --release         # unit tests, full conformance kit, host behaviour
cargo clippy --all-targets -- -D warnings
VST3_SDK=... AU_SDK=... "$SDK_DIR/scripts/build_plugin.sh" --manifest-path plugin/Cargo.toml \
  --name "Harmonic Compressor" --bundle-id com.opengrid.harmoniccompressor --manufacturer OpenGrid \
  --tail --formats clap,vst3,au --out plugin/target/dist
OPENPLUGIN_PORT=.../openplugin-port qa/2026-11-26-rust-port/null/run.sh   # null against 1.5.3
```

## What is in it

| File | From 1.5.3 |
|---|---|
| `plugin/src/dsp.rs` | `CompressorCore.h` (seven models, ADAA harmonics, sidechain high-pass), `DSP/Adaa.h`, `DSP/TransientDesigner.h`, `DSP/LoudnessMatch.h`, `juce::SmoothedValue`; same arithmetic order and precision |
| `plugin/src/lib.rs` | `PluginProcessor.cpp` `processBlock`, `prepareToPlay`, `reset`, bus layouts, identity |
| `plugin/src/params.rs` | the parameter layout: ids, order, ranges, skews, defaults, intervals, value text |
| `plugin/src/state.rs` | `get/setStateInformation`, including the per-model memory (`ModeSnapshots`) |
| `plugin/src/multirate.rs` | replaces `DSP/Resampler.h` (`juce::dsp::Oversampling`): HybridEQ 2.0's oversampler |

## Evidence

- **Null against the shipped 1.5.3 VST3** (`null/run.sh`, `null/report.txt`): 41 cases, every model
  on noise, sweep and bursts with automation, auto attack/release in every model, harmonics, mix,
  knee, input/output gain, sidechain high-pass and listen, transient designer, Auto Gain, Delta,
  Delta with Auto Gain, off-grid values. All pass at 1e-5 (two at 3e-5, see the script); RMS of the
  difference -115 to -202 dB. This is float rounding (the C++ build fuses multiply-adds).
- **Identity** (`scan/` is 1.5.3, `port-scan/` the port's bundles): CLAP id, VST3 class ids, AU
  `aufx Hcmp OpGr`, all 27 parameter ids, ranges, defaults, step counts and value text match in
  CLAP, VST3 and AU; latency 3 samples at the default 2x in both. AAX codes `OpGr Hcmp jcbb/jccc`
  and LV2 URI `urn:opengrid:harmoniccompressor` are pinned in code (not scanned: AAX cannot be hosted
  offline). `params::tests` checks the ids against the scans on every `cargo test`.
- **Old sessions** (`capture/` is 1.5.3): its CLAP, VST3 and AU states load with every value
  (`state::tests`); through the port's real VST3 shell, the 1.5.3 component state restores
  Bus VCA, Threshold -45 dB, Auto Gain on (`port-capture/`). Damaged states change nothing.
- **Conformance kit** (release, every rate 22.05 to 384 kHz and block size): pass, with
  `latency_depends_on_params` and `max_output` 2e5 (the designed +96 dB range; reasons in
  `tests/common/mod.rs`, shared with the soak tests). Full tier of the openplugin-dev test list
  passes: realtime lint, clippy, release tests, soak over 24 seeds, strict fuzz (NaN and denormals
  forbidden even during a burst: the input is cleaned of both before any filter), bench.
  `plugin/bench/baseline.json` was written on this machine while it was busy: rewrite it on a
  quiet one before using it as a gate.

## What differs from 1.5.3

- **2x, 4x and 8x sound slightly different**: the anti-alias filters are new designs (Kaiser FIR
  for Lin Phase, minimum-phase FIR for Min Phase) instead of JUCE's polyphase IIR and equiripple FIR,
  so 2x to 8x do not null (1x does). Latency at 2x Min Phase is 3 samples in both; the other
  settings report their own latency.
- **No editor.** Next step in the porting order (SDK `docs/EDITOR.md`, GoodLookin DRI).
- **Audio Unit sidechain**: the SDK's AU shell starts with one input element and adds the sidechain
  only when a host asks for a second; 1.5.3 offered two. Logic may not show the sidechain menu.
  Needs a test in Logic, and probably an SDK change.
- **VST3 bypass parameter** has the SDK shell's id, not JUCE's (1652125811): automation of the host
  bypass saved with a 1.5.3 VST3 session does not replay. The SDK has no pin for it.
- **VST3 choice parameters** report plain index ranges (0..6, 0..3) to hosts that ask for plain
  values; automation is normalized in VST3 either way, with the same step counts.
- **Choice text at exact half-way host values** (Algorithm at 0.75, Oversample Filter at 0.5) picks
  the upper choice where 1.5.3 picked the lower.
- Red's auto-attack/release memory is now cleared by a reset (1.5.3 kept it).
- Tail is reported as 0.7 s (the harmonics' DC blocker), where 1.5.3 reported 0.
- The model selector's per-model recall (`selectMode`) belongs to the editor and is not wired yet;
  the remembered settings are kept and saved.
- The shipped 1.5.3 CLAP crashes the porting tool's render process (SIGSEGV) at its defaults, the
  VST3 does not; probably the unconnected sidechain buffer read in `processBlock`. 1.5.3 issue, not
  the port's; the null uses the VST3.

## Not done yet

Editor; AAX build, PACE signing and Pro Tools test; LV2 bundle (needs a ttl bin); validators
(pluginval, clap-validator, auval) and REAPER/Logic tests of the port's bundles; version number (crate says 1.6.0: owner's decision).
