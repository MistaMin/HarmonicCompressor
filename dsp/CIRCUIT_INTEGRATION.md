# Circuit mode integration — provisional

User approved adding three modes and deferring DSP audit. Existing indices 0–4 retained; Bridge=5, BusVCA=6, FETCircuit=7. Expanding the existing choice changes normalized host automation mapping; old automation lanes need checking, despite saved integer indices remaining stable.

These are reduced integration candidates, NOT complete netlist implementations. FET uses the source small-signal channel conductance and resistor values; Bridge and Bus use provisional feedback control laws. All three currently share the plugin audio gain path. Transformer networks, amplifier stages, supplies, exact bridge junctions and rectifier/clamp networks remain unimplemented. Attack/release use plugin time settings. Bridge recovery memory references C36/R81; Bus tracking references 8.89/9.53. Auto timing, knee, extended ratios and sidechain semantics need circuit-specific mapping. Harmonic sliders remain a separate existing stage.

Deferred audit: voltage calibration, complete signal paths, source assumptions, stereo sample order/link, detector taps, RC networks, external sidechain, automation migration, stability and sound comparisons against SPICE. No circuit fidelity or host QA claimed.
