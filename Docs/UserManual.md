# Harmonic Compressor (version 1.5.1)

Harmonic Compressor is a mastering-grade, multi-topology analogue dynamics processor and harmonic saturation engine. Built with physical-circuit component modeling and first-order Antiderivative Anti-Aliasing (ADAA), it combines seven distinct analogue compression topologies, an integrated transient designer, a multi-order harmonic saturation stage, and an interactive real-time transfer curve display.

![Harmonic Compressor Soft White Interface](harmonic-compressor-soft-white.png)

## Architecture & Signal Flow

Harmonic Compressor implements a pristine, zero-latency direct signal path designed for both individual tracks and master bus processing.

1. **Input Gain:** Input trim precedes the dry/wet split and compressor processing.
2. **Compression and Sidechain:** The selected model computes gain reduction. Sidechain filtering affects detection; the Listen modes audition the detector input.
3. **Harmonic Saturation:** Four independent controls set 2nd, 3rd, 4th and 5th harmonic contributions after compression. Even-order coloration includes DC blocking. The smooth rail stage is active with harmonic coloration; it is not a guaranteed output ceiling after later gain stages.
4. **Dry/Wet Mix:** The processed signal is blended with the dry signal. With oversampling on, the dry signal passes through the same oversampling filters as the processed signal, so blending causes no phase cancellation or treble loss.
5. **Transient Designer:** The transient designer runs after the mix.
6. **Auto Gain and Delta:** Auto Gain, when on, matches the output loudness to the untouched plugin input. Delta, when on, replaces the output with the difference between the output and the dry signal.
7. **Output Gain:** Final trim, after Auto Gain and Delta. Output gain can raise the level above digital full scale; leave appropriate headroom.

## The Seven Analogue Topologies

Harmonic Compressor features seven meticulously modeled compressor engines, covering the most iconic dynamics circuits in recording history:

- **Red:** Discrete Class-A VCA console channel compressor inspired by classic British high-end broadcast desks. Features a progressive soft knee and reactive punch relief that preserves punchy drum transients while heavily flattening sustained decay.
- **Hybrid:** A state-of-the-art modern topology blending feed-forward speed with smooth feedback release envelope ballistics. Ideal for complex full mixes and stems where transparency and precise control are demanded.
- **Bridge:** Classic German diode-bridge feedback compressor. Characterized by a distinctive non-linear transfer knee, organic harmonic thickness, and smooth, musical density across percussion and acoustic instruments.
- **Bus VCA:** The legendary British console master bus compressor. Delivers signature "glue" and punch across drum busses and full stereo master mixes with auto-release timing and logarithmic release decay.
- **76:** Ultra-fast discrete Field Effect Transistor (FET) peak limiter. Delivers lightning attack speeds down to 20 microseconds and a program-dependent dual-time-constant release that adds aggression, excitement, and presence to vocals, snares, and bass.
- **Opto Leveler:** Electro-luminescent optical attenuator circuit modeled after the classic T4B opto-cell. Features a smooth, two-stage memory-dependent release curve where initial gain recovery is swift while deep compression decays slowly, yielding transparent, effortless vocal and bass leveling.
- **VCA 160:** Legendary feed-forward VCA compressor with RMS level detection and a punchy hard knee. World-renowned for its snappy attack on kicks, snares, slap bass, and percussive synths.

The shared Attack and Release controls expose 0.1-300 ms and 5-3000 ms respectively. Individual models interpret timing differently; circuit-timed models may disable automatic timing switches. Model names describe the intended character, rather than a claim of measured equivalence to a particular hardware unit.


## ADAA Harmonic Saturation Engine

Harmonic saturation in the digital domain typically causes severe high-frequency mirror aliasing that impairs clarity and adds harshness. Harmonic Compressor overcomes this using continuous first-order Antiderivative Anti-Aliasing (ADAA) combined with selectable 1x, 2x, 4x or 8x oversampling and Minimum Phase or Linear Phase filtering.

The continuous waveshaping polynomial evaluates analytical antiderivatives across sample steps:

The anti-aliased output uses the difference between the antiderivative at consecutive input values, divided by their input difference.

The implementation handles small differences between adjacent samples to avoid unstable division.

- **Tube (Even Order):** Emphasizes 2nd and 4th order harmonics, imparting three-dimensional warmth, physical depth, and body to thin recordings. An integrated sub-audio 3.5 Hz DC blocker cancels asymmetrical voltage drift without degrading sub-bass frequency response.
- **Tape (Odd Order):** Emphasizes 3rd and 5th order odd harmonics, producing tape-style compression, high-frequency rounding, and punchy saturation.
- **Smooth Rail Stage:** ADAA-buffered soft clipping shapes the harmonic stage. Later makeup, mixing, transient shaping and output gain still require appropriate gain staging.

## Transient Designer & Sidechain Relief

Harmonic Compressor integrates a stereo-linked differential-envelope transient processor after the dry/wet mix:

- **Attack (-100% to +100%):** Emphasizes or softens transient onsets after compression and mixing.
- **Sustain (-100% to +100%):** Extends or tightens decay and room tails. Speed (0.5-50 ms) adjusts the detector response.
- **Sidechain High-Pass Filter (20-500 Hz, with bypass):** Prevents heavy bass and kick transients from excessively triggering pumping artifacts on stereo busses.

## Auto Gain & Delta

The **AUTO GAIN** and **DELTA** keys sit at the right end of the knob bar. Both are global: they do not change when you switch compressor model.

**Auto Gain** keeps the output as loud as the plugin's input, so you judge compression and saturation by their sound, not by a level change. It measures loudness the way the ear weighs it: K-weighted short-term loudness (ITU-R BS.1770, the 3-second window of EBU R128), which counts low bass less and the presence region more than a plain RMS meter. The key shows the gain it is applying, for example `AUTO +4.2 dB`.

- The reference is the untouched input, before Input Gain. With Auto Gain on, Input Gain works as a drive control: push it to compress or saturate harder at the same output loudness.
- Makeup still changes the sound with Auto Gain on, because it sets how hard the signal drives the harmonics stage.
- The 3-second window follows the average level and does not undo the compression itself. After you change a control, the level settles within about 3 seconds.
- Passages below -70 LUFS (silence, fades) hold the current gain instead of raising it.
- The range is +/-24 dB. Settings that remove more than 24 dB of loudness stay quieter than the input.
- Output Gain applies after Auto Gain, as a final trim.

**Delta** plays only what the plugin changes: the output minus the dry signal, delay-compensated.

- With Auto Gain off, the dry signal is taken after Input Gain, so you hear what compression, makeup, saturation and the transient designer add or remove.
- With Auto Gain on, the dry signal is the untouched input, so you hear the level-matched difference from bypass.
- The dry signal passes through the same oversampling filters as the processed signal, so with neutral settings Delta is silent at every oversampling setting.
- Sidechain Listen takes priority over Delta.

![Dynamic Auto-Scaling Real-Time Metering](harmonic-compressor-autoscale.png)

## Dynamic Auto-Scaling Metering & Real-Time Transfer Curve

The central visualization display combines an interactive compression transfer curve and auto-scaling peak/VU/GR meters:

- **Real-Time Transfer Curve:** Graphically renders the active threshold, ratio slope, and knee curvature. Live input and output audio levels are traced across the transfer line in real time.
- **Auto-Scale GR Metering:** When working with subtle bus compression (-1 dB to -3 dB) or extreme limiting (-20 dB), the meter display dynamically scales to provide high-resolution visual feedback.
- **Side-by-Side In/Out Peak & RMS Bars:** Precision PPM-style level meters with integrated clip indicators let you match gain staging and evaluate perceptual loudness effortlessly.


## Interface Customization & Themes

Harmonic Compressor features four distinct UI themes crafted for long studio sessions and different ambient lighting conditions:

- **Soft White:** Pale surface with dark labels, curves and controls.
- **Matrix Phosphor:** Retro CRT green glow reminiscent of vintage lab oscilloscopes and hardware spectrum analyzers.
- **Cryo Plasma:** Futuristic deep-space cobalt and cyan neon accents designed for high-resolution dark environments.
- **Hyperdrive:** Ultraviolet background with magenta, cyan and acid-yellow highlights.

![All Four Colour Themes](harmonic-compressor-themes-grid.png)

### Floating Modular Windows

The Compression, Harmonics, Sidechain and Transient modules can be expanded, collapsed and moved inside the plugin window. Drag a module header to reposition it; click the header to expand or collapse it. Linear sliders map clicks to the position in the visible groove. Drag left to reduce a value, including after reaching the maximum.

![Floating Modular Control Windows](harmonic-compressor-modules.png)

## DAW Host Integration & Performance

Use a host that supports the selected plugin format. Compatibility and validation results are recorded per build; the included REAPER image illustrates host use and is not certification for every DAW.

![Harmonic Compressor in Cockos REAPER](harmonic-compressor-reaper.png)

- **True Zero Latency:** In standard 1x mode, processing latency is exactly 0 samples (0 ms), making it suitable for live tracking and low-latency monitoring.
- **CPU Use:** Depends on model, sample rate, oversampling factor, host and processor. Measure at your session settings.
- **Parameter Automation:** Every knob, toggle switch, and mode selector is exposed to the host for automation; timing depends on the format, host and parameter smoothing.


## System Requirements & Formats

- **macOS:** macOS 11.0 Big Sur or later (Native Apple Silicon arm64 and Intel x86_64 Universal Binary).
- **Supported Formats:**
  - **VST3:** 64-bit compatible host (DAW).
  - **Audio Units (AU):** 64-bit AUv2 host.
  - **AAX Native:** Pro Tools 2022+ (PACE Eden Signed).
  - **CLAP:** CLAP 1.2+ host.
  - **LV2:** LV2 compatible host.
- **RAM:** 4 GB minimum (8 GB recommended).
- **Display:** 1280 x 800 minimum screen resolution (Retina / 4K Ultra-HD supported).

<!-- pagebreak -->

## Parameter Reference

| Parameter ID | Parameter Name | Range | Default | Unit | Description |
|---|---|---|---|---|---|
| `algorithmMode` | Algorithm | Red, Hybrid, Bridge, Bus VCA, 76, Opto Leveler, VCA 160 | Red | choice | Compression topology |
| `threshold` | Threshold | -60 to 0 | -18 | dB | Compression threshold |
| `ratio` | Ratio | -2 to 100 | 4 | :1 | Shared ratio control; top of range displays infinity |
| `attack` | Attack | 0.1 to 300 | 10 | ms | Model attack control |
| `release` | Release | 5 to 3000 | 100 | ms | Model release control |
| `knee` | Knee | 0 to 24 | 6 | dB | Soft-knee width |
| `makeupGain` | Makeup Gain | -12 to +24 | 0 | dB | Processed-signal makeup |
| `mix` | Mix | 0 to 1 | 1 | fraction | Displayed as 0-100% wet |
| `harmonic2` | 2nd Harmonic | 0 to 100 | 0 | % | Even-order coloration |
| `harmonic3` | 3rd Harmonic | 0 to 100 | 0 | % | Odd-order coloration |
| `harmonic4` | 4th Harmonic | 0 to 100 | 0 | % | Even-order coloration |
| `harmonic5` | 5th Harmonic | 0 to 100 | 0 | % | Odd-order coloration |
| `scEnabled` | Sidechain Enable | Off / On | Off | bool | Enable sidechain HPF |
| `scListen` | Sidechain Listen | Off / On | Off | bool | Audition detector input |
| `scHPFreq` | Sidechain HP Freq | 20 to 500 | 150 | Hz | Sidechain cutoff |
| `autoAttack` | Auto Attack | Off / On | Off | bool | Program-dependent attack where supported |
| `autoRelease` | Auto Release | Off / On | Off | bool | Program-dependent release where supported |
| `inputGain` | Input Gain | -24 to +24 | 0 | dB | Input trim |
| `outputGain` | Output Gain | -24 to +24 | 0 | dB | Final output trim |
| `oversampleMode` | Oversample | 1x, 2x, 4x, 8x | 2x | choice | Processing sample-rate multiplier |
| `oversampleFilter` | Oversample Filter | Min Phase / Lin Phase | Min Phase | choice | Resampling filter |
| `transientEnabled` | Transient Enable | Off / On | Off | bool | Enable post-mix transient shaping |
| `transientAttack` | Transient Attack | -100 to +100 | 0 | % | Onset emphasis or softening |
| `transientSustain` | Transient Sustain | -100 to +100 | 0 | % | Tail emphasis or tightening |
| `transientSpeed` | Transient Speed | 0.5 to 50 | 10 | ms | Transient detector timing |
| `autoGain` | Auto Gain | Off / On | Off | bool | Match output loudness to the untouched input |
| `delta` | Delta Monitor | Off / On | Off | bool | Play output minus dry signal |

The colour theme, curve visibility and module positions are saved interface settings. They are not additional host parameters. The Harmonics preset selector sets the four harmonic controls; the Sidechain selector combines its enable and listen switches.
