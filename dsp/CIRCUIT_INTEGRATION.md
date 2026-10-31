# Circuit mode integration — provisional

User approved adding three modes and deferring DSP audit. Existing indices 0–4 retained; Bridge=5, BusVCA=6, FETCircuit=7. Expanding the existing choice changes normalized host automation mapping; old automation lanes need checking, despite saved integer indices remaining stable.

These are reduced integration candidates, NOT complete netlist implementations. FET uses the source small-signal channel conductance and resistor values; Bridge and Bus use provisional feedback control laws. All three currently share the plugin audio gain path. Transformer networks, amplifier stages, supplies, exact bridge junctions and rectifier/clamp networks remain unimplemented. Attack/release use plugin time settings. Bridge recovery memory references C36/R81; Bus tracking references 8.89/9.53. Auto timing, knee, extended ratios and sidechain semantics need circuit-specific mapping. Harmonic sliders remain a separate existing stage.

Deferred audit: voltage calibration, complete signal paths, source assumptions, stereo sample order/link, detector taps, RC networks, external sidechain, automation migration, stability and sound comparisons against SPICE. No circuit fidelity or host QA claimed.


## Additional references: Opto Leveler (8), RMS VCA Dynamics (9)

Added as reduced provisional candidates, with original mode indices retained. Opto preserves the reference panel 1ms lag, 5.1 light exponent, three cell states and 0.65467/0.2131/0.13223 weights, 0.9 conductance exponent, dark and light conductance, R6/R7 shunt approximation. Panel drive is normalized to plugin threshold, not physically calibrated; tube amplifiers, transformer loading and supply are not implemented. Ratio <10 selects Compress; >=10 selects Limit. Attack/release and knee controls are disabled for these two modes.

RMS uses an analytic power-domain transformation of the reference log-cap equation with constant-current release approximated at 126dB/s, paired 6mV/dB detector and 2.98mV/dB VCA constants, and 1% below-threshold rectifier leakage. Timing is coupled, not separate attack/release filters. Reference current limiting, log junction floor, nonconstant R35 current, transistor modules, amplifier, mute and supply stages remain unimplemented. This is a behavioural reduction, not the default transistor-level variant.

Stereo uses the existing provisional maximum-channel detector. Physical optical link and per-channel RMS linking remain unaudited. No hardware or SPICE fidelity claim. Full circuit implementation remains open; addition to the plugin is not proof of equivalence.
