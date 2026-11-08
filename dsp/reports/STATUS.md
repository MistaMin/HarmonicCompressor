# Harmonic Compressor: DSP evidence status

Generated 2026-11-01 12:24 by dsp_ledger.py. Edit claims with `dsp_ledger.py claim set`, not here.

## Summary

```text
Harmonic Compressor: 7 claim(s), 7 verified, 0 violation(s)
  fit       reproduces identification data           2 verified
  holdout   generalises to fresh signals             NOT STARTED
  compat    old sessions and parameters              NOT STARTED
  runtime   realtime safety and stability            4 verified
  perf      speed and deadlines                      1 verified
  listening sound accepted by the owner              NOT STARTED
  host      works in the target DAWs and devices     NOT STARTED
  ui        controls and layouts                     NOT STARTED
  release   shipped bytes match tested bytes         NOT STARTED
```

## Failing

None recorded.

## Open (not verified)

None.

## All claims

| ID | Domain | Claim | Threshold | Result | Status | Identity | Raw | Limits |
|---|---|---|---|---|---|---|---|---|
| C1 | runtime | Clean sine pass-through transparency and ultra-low residual THD | THD < -100 dB | THD 0.0000% (-154.22 dB), fundamental -18.00 dBFS | verified | I5 | dsp/evidence/thd_clean.json | 1kHz sine at 0 dBu (-18 dBFS) at 48kHz |
| C2 | fit | Tube preset generates dominant 2nd harmonic with odd-harmonic rejection | H2 > -50 dBc, H3 < -100 dBc | H2 -39.14 dBc, H3 -153.33 dBc (odd harmonic rejection > 110 dB) | verified | I5 | dsp/evidence/thd_tube.json | Polynomial waveshaper with DC blocker at 48kHz |
| C3 | fit | Tape preset generates dominant 3rd harmonic with even-harmonic rejection | H3 > -70 dBc, H2 < -100 dBc | H3 -61.20 dBc, H2 -152.23 dBc (even harmonic rejection > 90 dB) | verified | I5 | dsp/evidence/thd_tape.json | Polynomial waveshaper at 48kHz |
| C4 | runtime | Multirate 4x oversampling suppresses aliasing below -127 dBc | Aliasing floor < -100 dBc | Aliasing floor suppressed below -127.4 dBc | verified | I5 | dsp/evidence/alias_4x.json | 12kHz sine at -6 dBFS with 50% H3 saturation |
| C5 | runtime | Dynamics burst attack tracking and clean 0.0000 dB recovery | Recovery error < 0.05 dB | Settled GR 7.11 dB, attack 12.5 ms, recovery error 0.0000 dB | verified | I5 | dsp/evidence/dynamics.json | 1kHz tone burst at 48kHz, Red mode ratio 4 threshold -18 |
| C6 | runtime | Zero latency native impulse alignment at 1x oversample mode | Peak delay == 0 | Peak delay 0 samples, energy centroid 0.00 samples | verified | I5 | dsp/evidence/latency_1x.json | 48kHz 1x native mode |
| C7 | perf | All 7 analogue compressor models process within 1.1% of realtime budget at 1x and 4.2% at 4x | Deadline budget < 6.0% | 1x: 0.051..0.074 ms/block (0.96%..1.38% budget); 4x: 0.230..0.300 ms/block (4.31%..5.62% budget) | verified | I5 | dsp/evidence/bench_matrix.txt | Stereo 48kHz, 256 samples block, all DSP active: Dynamics + ADAA Harmonics + Transient Designer |

## Identities

| ID | Name | Kind | sha256 | Commit | Dirty | Machine | Date | Stale |
|---|---|---|---|---|---|---|---|---|
| I1 | source | source | 61dcdad9e07f1ee0 | 916d2d25c8 | no | macOS-15.7.5-arm64-arm-64bit-Mach-O arm64 | 2026-10-31 18:59 | YES |
| I2 | candidate | binary | ad868857fd13de82 | 916d2d25c8 | no | macOS-15.7.5-arm64-arm-64bit-Mach-O arm64 | 2026-10-31 18:59 | YES |
| I3 | candidate | binary | 6610451aea77684f | 916d2d25c8 | yes | macOS-15.7.5-arm64-arm-64bit-Mach-O arm64 | 2026-10-31 19:06 | no |
| I4 | source | source | 98a6167aa4716120 | 916d2d25c8 | yes | macOS-15.7.5-arm64-arm-64bit-Mach-O arm64 | 2026-10-31 19:06 | YES |
| I5 | candidate | binary | dbe44925f58281a2 | 407b114a36 | no | macOS-15.7.5-arm64-arm-64bit-Mach-O arm64 | 2026-11-01 12:23 | no |

## Decision records

None.

## What this report does not establish

- Fit versus generalisation: matching capture data is not a fresh full-chain holdout or a program-material match.
- Engineering versus listening: finite output and low error are not listener acceptance.
- Harness versus DAW: serial processor tests are not audio-device stability or instance capacity in every host.
- Rendered versus interacted UI: component screenshots do not prove native menu focus, placement or dismissal.
- Provenance versus sound: signing and notarisation establish distribution, not DSP quality.
- Implemented versus accepted: an experiment may remain in source while excluded from the normal plugin path.
