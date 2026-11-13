# Plugin QA: LV2: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | LV2: Harmonic Compressor (OpenGrid) (4->2ch), 30 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1120x813, 9.6% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | FAIL | Free Wheeling (On -> Off), Algorithm (5.21999979 -> ), Threshold (-45.59999847 -> ), Ratio (60.22000122 -> ), Attack (294.00201416 -> ), Release (1053.25000000 -> ), Knee (17.28000069 -> ), Makeup Gain (-8.76000023 -> ), Mix (0.46000001 -> ), 2nd Harmonic (83.00000000 -> ), 3rd Harmonic (20.00000000 -> ), 4th Harmonic (57.00000000 -> ), 5th Harmonic (94.00000000 -> ), Sidechain Enable (0.31000000 -> ), Sidechain Listen (0.68000001 -> ), Sidechain HP Freq (44.00000000 -> ), Auto Release (0.41999999 -> ), Auto Attack (0.79000002 -> ), Input Gain (-16.31999969 -> ), Output Gain (1.44000006 -> ) |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Free Wheeling |  | ok (-18.1 dB) | ok (3.326%) |
| 1 | Enabled |  | ok (-7.4 dB) | ok (2.865%) |
| 2 | Algorithm |  | ok (-15.8 dB) | ok (1.302%) |
| 3 | Threshold |  | ok (-14.0 dB) | ok (3.231%) |
| 4 | Ratio |  | ok (-18.9 dB) | ok (2.550%) |
| 5 | Attack |  | ok (-29.4 dB) | ok (3.326%) |
| 6 | Release |  | ok (-0.6 dB) | ok (2.615%) |
| 7 | Knee |  | ok (-7.0 dB) | ok (1.359%) |
| 8 | Makeup Gain |  | ok (-20.9 dB) | ok (2.520%) |
| 9 | Mix |  | ok (-13.8 dB) | ok (4.396%) |
| 10 | 2nd Harmonic |  | ok (-24.8 dB) | ok (2.962%) |
| 11 | 3rd Harmonic |  | ok (-19.6 dB) | ok (4.232%) |
| 12 | 4th Harmonic |  | ok (-15.6 dB) | ok (4.464%) |
| 13 | 5th Harmonic |  | ok (-13.3 dB) | ok (1.359%) |
| 14 | Sidechain Enable |  | ok (-13.3 dB) | ok (1.396%) |
| 15 | Sidechain Listen |  | ok (-39.2 dB) | ok (2.297%) |
| 16 | Sidechain HP Freq | Sidechain Enable=1 | ok (1.6 dB) | ok (2.523%) |
| 17 | Auto Release |  | ok (-1.0 dB) | ok (3.759%) |
| 18 | Auto Attack |  | ok (-0.6 dB) | ok (4.687%) |
| 19 | Input Gain |  | ok (1.2 dB) | ok (2.863%) |
| 20 | Output Gain |  | ok (-4.9 dB) | ok (2.566%) |
| 21 | Oversample |  | ok (-6.1 dB) | ok (4.465%) |
| 22 | Oversample Filter |  | ok (-1.7 dB) | ok (4.186%) |
| 23 | Transient Enable |  | ok (-26.1 dB) | ok (3.398%) |
| 24 | Transient Attack | Transient Enable=1 | ok (-1.8 dB) | ok (1.434%) |
| 25 | Transient Sustain | Transient Enable=1 | ok (3.3 dB) | ok (1.359%) |
| 26 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (2.0 dB) | ok (3.030%) |
| 27 | Bypass |  | ok (-6.6 dB) | ok (3.326%) |
| 28 | Wet |  | ok (-3.5 dB) | ok (1.396%) |
| 29 | Delta |  | ok (1.6 dB) | ok (1.332%) |
