# Port to OpenPluginSDK 1.0.0-beta (2026-11-26)

`plugin/` is Harmonic Compressor on OpenPluginSDK, in Rust: the DSP was ported on `19fac89`, and since
2026-11-27 everything is built and checked on the SDK branch `editor-pointer-left` (`2969ebb`: `main`
`07f97e1` plus two commits, not merged yet: the view tells an editor the pointer left, and an Audio
Unit effect starts with its sidechain element). The release QA of 1.6.0 is in
`qa/2026-11-27-release-1.6.0/`.
It exports CLAP, LV2 and the SDK's VST3, Audio Unit and AAX shells; all five were built universal
and checked here (`ledger.md`, "Release preparation"). It replaces the JUCE processor and editor of 1.5.3 (`Source/`), which are
untouched. The editor is macOS only, as the SDK's editor hosting is.

Build and test:

```sh
. Scripts/sdk_env.sh                      # finds OpenPluginSDK, writes the untracked .cargo/config.toml
cd plugin && cargo test --release         # unit tests, full conformance kit, host behaviour
cargo clippy --all-targets -- -D warnings
VST3_SDK=... AU_SDK=... AAX_SDK=... "$SDK_DIR/scripts/build_plugin.sh" --manifest-path plugin/Cargo.toml \
  --name "Harmonic Compressor" --bundle-id com.opengrid.harmoniccompressor --manufacturer OpenGrid \
  --editor --tail --aax-pages --ttl-bin lv2_ttl --formats clap,lv2,vst3,au,aax --archs universal \
  --out plugin/target/dist-universal --work-dir plugin/target/openplugin-build-universal
OPENPLUGIN_PORT=.../openplugin-port qa/2026-11-26-rust-port/null/run.sh   # null against 1.5.3
# editor against 1.5.3: render both in the states of gui/scenarios.txt, then compare
ninja -C build-release GuiReferenceRender && ./build-release/GuiReferenceRender qa/2026-11-26-rust-port/gui/scenarios.txt qa/2026-11-26-rust-port/gui/ref
cd plugin && cargo run --release --example gui_render -- ../qa/2026-11-26-rust-port/gui/scenarios.txt ../qa/2026-11-26-rust-port/gui/port
python3 ../qa/2026-11-26-rust-port/gui/compare.py          # diff/ images, table as gui/report.txt
cargo run --release --example gui_window                   # the editor in a window, with audio running
cargo run --release --example frame_time                   # CPU per frame
```

## What is in it

| File | From 1.5.3 |
|---|---|
| `plugin/src/dsp.rs` | `CompressorCore.h` (seven models, ADAA harmonics, sidechain high-pass), `DSP/Adaa.h`, `DSP/TransientDesigner.h`, `DSP/LoudnessMatch.h`, `juce::SmoothedValue`; same arithmetic order and precision |
| `plugin/src/lib.rs` | `PluginProcessor.cpp` `processBlock`, `prepareToPlay`, `reset`, bus layouts, identity |
| `plugin/src/params.rs` | the parameter layout: ids, order, ranges, skews, defaults, intervals, value text |
| `plugin/src/state.rs` | `get/setStateInformation`, including the per-model memory (`ModeSnapshots`) |
| `plugin/src/multirate.rs` | replaces `DSP/Resampler.h` (`juce::dsp::Oversampling`): HybridEQ 2.0's oversampler |
| `plugin/src/editor/` | `PluginEditor.cpp`, `TransferCurveDisplay.cpp`, `UI/*.h`: every component, colour, string, layout constant and mouse behaviour (slider drags, double-click and Option-click reset, wheel, keys, menus, module drag and open, curve handles, corner resizer, per-model recall) |
| `plugin/src/gfx.rs` | JUCE's `Graphics` on CoreGraphics as `juce_CoreGraphicsContext_mac.mm` draws: same CoreGraphics calls, JUCE's colour arithmetic, arcs and dashes, and its text layout in Lucida Grande |

## Evidence

- **Null against the shipped 1.5.3 VST3** (`null/run.sh`, `null/report.txt`; values on both
  versions' grids since the display rules): 41 cases, every model
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

- **Editor against 1.5.3** (`gui/`): 1.5.3's editor (`GuiReferenceRender`, JUCE's own snapshot) and
  the port's (`gui_render`) rendered in 10 states (four colour modes, scale 1 and 2, all modules
  open, disabled controls, curve off, other values): every pixel within 1 of 255, except the text
  the display rules changed and the new meter selector and readouts (`gui/report.txt`, `gui/diff/`;
  the one level is CoreGraphics' gradient dither). With audio running, the port's
  render matches the 1.5.3 manual picture (same readings, scale and traces). Repainting only changed
  regions, with the history field's static underlay cached, equals a whole repaint to the same dither
  (`partial_repaints_equal_a_whole_repaint`). Mouse, menu, model recall, screen-reader and idle tests
  in `editor::tests`. The editor opened in a real window through the SDK's Cocoa view, and pluginval's
  editor tests (strictness 10) pass on the VST3 (`validators/pluginval-vst3-editor.txt`).
- **Editor cost**, CPU per frame at 2x on this machine while it was heavily loaded (`frame_time`,
  `GUI_TIME=1 GuiReferenceRender`): a whole repaint 58.4 ms (1.5.3: 57.7 ms); idle 0 (1.5.3 redrew its
  meters and history field 30 times a second even then); with audio about 51 ms (1.5.3 redrew the
  same parts). Numbers on a quiet machine are owed.

## What differs from 1.5.3

- **2x, 4x and 8x sound slightly different**: the anti-alias filters are new designs (Kaiser FIR
  for Lin Phase, minimum-phase FIR for Min Phase) instead of JUCE's polyphase IIR and equiripple FIR,
  so 2x to 8x do not null (1x does). Latency at 2x Min Phase is 3 samples in both; the other
  settings report their own latency.
- **Editor popup menus** are drawn inside the editor: JUCE opened them as windows of their own, with
  a system shadow, that could pass the editor's edge (the colour menu did). Same look and items.
- **The pointer** keeps its arrow: the SDK's editor contract has no cursor, where 1.5.3 showed a hand
  over the module headers and the curve's handles.
- **Screen readers** now get every control's name and value (1.5.3's controls had no names).
- **Mouse wheel** direction follows the system's natural-scrolling setting (JUCE undid it); the
  SDK does not say which way the device was turned.
- **Audio Unit sidechain**: fixed in the SDK (`2969ebb`): an effect starts with two input elements,
  the second named "Sidechain", as 1.5.3 offered. Before, the shell added it only when a host asked.
  Checked with auval and in REAPER (`qa/2026-11-27-release-1.6.0/sidechain/`); not in Logic.
- **VST3 bypass parameter** has the SDK shell's id, not JUCE's (1652125811): automation of the host
  bypass saved with a 1.5.3 VST3 session does not replay. The SDK has no pin for it.
- **VST3 choice parameters** report plain index ranges (0..6, 0..3) to hosts that ask for plain
  values; automation is normalized in VST3 either way, with the same step counts.
- **Choice text at exact half-way host values** (Algorithm at 0.75, Oversample Filter at 0.5) picks
  the upper choice where 1.5.3 picked the lower.
- Red's auto-attack/release memory is now cleared by a reset (1.5.3 kept it).
- Tail is reported as 0.7 s (the harmonics' DC blocker), where 1.5.3 reported 0.
- **Display rules** (owner, 2026-11-27; table in `ledger.md`): Threshold, Ratio, Knee, Makeup, the
  gains, the harmonics and the transient amounts move in whole steps (1.5.3: tenths); Attack and
  Speed under 1 ms read in µs; hosts get Mix in percent and Attack, Release and Speed with their unit.
  Sessions keep 1.5.3's tenths until the control moves. Steps on a 0.1 grid snap with a fused
  multiply-add as 1.5.3's did (arm64).
- **Text sizes**: the meter scale labels, axis captions and curve labels 1.5.3 drew at 7.2 to 7.9 pt
  are 8 pt (display rule 5).
- **Meters** (owner, 2026-11-27): 1.5.3's peak bars (sample peak with a 0.8 s fall) and its
  "dBu / dBFS" readouts are replaced by the SDK's meters, chosen on the input meter and saved with the
  session: PPM after IEC 60268-10 Type I in dBFS (default), RMS over 300 ms in dBFS, or VU after
  IEC 60268-17 with 0 VU = -18 dBFS. Readouts in whole numbers, as is the GR readout ("3 dB GR").
  Ballistics measured in `tests/meters.rs` (`ledger.md`). The engine runs a meter only while an
  editor shows it (the PPM's 8x rectifier is a fifth of the plugin's cost): with the editor closed
  the audio cost is 1.5.3's.
- The shipped 1.5.3 CLAP crashes the porting tool's render process (SIGSEGV) at its defaults, the
  VST3 does not; probably the unconnected sidechain buffer read in `processBlock`. 1.5.3 issue, not
  the port's; the null uses the VST3.

## Not done yet

PACE signing and a Pro Tools test of the AAX; notarisation and the installer; a Logic test (LV2 ran in REAPER); OpenGrid9 host
tests; a VoiceOver pass; editor timings on a quiet machine; the user manual (still 1.5.3's meters
and values). The version is 1.6.0 (owner, 2026-11-27). `--features test-identity` builds
"Harmonic Compressor Test" with its own ids (AU `Hcmt`), to install beside a shipped copy for host
tests; it is never shipped.
