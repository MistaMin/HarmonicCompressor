# ADR-0001: Auto Gain measures K-weighted short-term loudness; Delta subtracts the compared dry signal

Date: 2026-11-14. Status: accepted. Evidence: `qa/2026-11-14-autogain-delta/`.

## Context

The user asked for an automatic gain function and a delta mode, and whether automatic gain should be
measured in loudness units or RMS.

## Decision

**Measure.** K-weighted loudness (ITU-R BS.1770 pre-filter: +4 dB high shelf, ~38 Hz RLB high-pass),
channel mean squares summed, over a 3 s rectangular window in 100 ms chunks (EBU R128 short-term).
A chunk whose reference loudness is below -70 LUFS (BS.1770 absolute gate) is discarded, so silence holds
the gain. Coefficients are designed per sample rate from the analogue prototype; at 48 kHz they equal
the BS.1770-4 table (difference < 1e-12).

**Why loudness over RMS.** RMS weights all frequencies equally, so it is dominated by bass energy that the
ear rates as quieter, and it counts presence-region energy (2-5 kHz) at less than its perceived weight.
K-weighting is the standard, validated approximation of that and costs two biquads per channel. The gate
and window length matter as much as the weighting: a fast RMS follower would undo the compression it is
compensating, and an ungated one would ramp up through silence.

Measured on this plugin (`autogain-delta-test.log`, INFO lines): compression and the harmonics stage
change the spectral balance only a little, so an RMS match would land within about 0.3 LU on program
material. The weighting is the right basis, but its audible advantage here is small. Bigger spectral
changes (heavy sidechain-filtered compression of bass-heavy material, strong upper harmonics) widen the gap.

**Reference.** The untouched plugin input (before Input Gain), measured unfiltered. With Auto Gain on,
the output sounds as loud as bypass, and Input Gain acts as drive. The copy aligned for Delta is not
used for measurement: it is band-limited by the resampler filters, and bypass is not. (The first
implementation measured the copy from the old interpolated delay line and read up to 1.4 LU low at 4x;
the per-mode test caught it.)

**Topology.** Feed-forward: gain = reference loudness - processed loudness (before compensation),
clamped to +/-24 dB, updated every 100 ms and ramped over 100 ms. It cannot oscillate. Placed after the
transient designer, before Delta and Output Gain.

**Delta.** Output minus the latency-aligned dry signal the output is compared with: the dry after Input
Gain when Auto Gain is off (what the processing changed), the untouched input when Auto Gain is on (the
level-matched difference from bypass). The switch and the reference change are crossfaded (20 ms, 50 ms).
Sidechain Listen overrides Delta and freezes the Auto Gain measurement.

**Scope.** Both parameters are global, not stored per compressor model. Appended as parameters 26 and 27;
the host name of `delta` is "Delta Monitor" to avoid REAPER's own per-FX "Delta" parameter.

## Consequences

- Delta nulls in every oversampling mode (-280 dB, the measurement floor). The first implementation used
  the interpolated dry delay line and left -7 to -9 dB (Min Phase) and -14 to -28 dB (Lin Phase) of
  residue; ADR-0002 replaced that delay line with a matched resampler chain.
- Settings removing more than 24 dB of loudness (e.g. the 76 model at -30 dB threshold, 8:1) stay
  quieter than the input.
- After a control change the level settles within the 3 s window.
