# Plugin QA: LV2: Harmonic Compressor (OpenGrid) (4->2ch)

| Check | Status | Detail |
|---|---|---|
| loads | PASS | LV2: Harmonic Compressor (OpenGrid) (4->2ch), 32 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1080x813, 13.2% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | FAIL | Free Wheeling (On -> Off), Algorithm (5.21999979 -> ), Threshold (-45.59999847 -> ), Ratio (60.22000122 -> ), Attack (294.00201416 -> ), Release (1053.25000000 -> ), Knee (17.28000069 -> ), Makeup Gain (-8.76000023 -> ), Mix (0.46000001 -> ), 2nd Harmonic (83.00000000 -> ), 3rd Harmonic (20.00000000 -> ), 4th Harmonic (57.00000000 -> ), 5th Harmonic (94.00000000 -> ), Sidechain Enable (0.31000000 -> ), Sidechain Listen (0.68000001 -> ), Sidechain HP Freq (44.00000000 -> ), Auto Release (0.41999999 -> ), Auto Attack (0.79000002 -> ), Input Gain (-16.31999969 -> ), Output Gain (1.44000006 -> ) |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Free Wheeling |  | excused (-7.0 dB) | ok (10.790%) |
| 1 | Enabled |  | ok (-29.4 dB) | ok (10.571%) |
| 2 | Algorithm |  | ok (-1.7 dB) | ok (10.555%) |
| 3 | Threshold |  | ok (-15.8 dB) | ok (20.132%) |
| 4 | Ratio |  | ok (-7.3 dB) | ok (10.419%) |
| 5 | Attack |  | ok (-1.7 dB) | ok (10.352%) |
| 6 | Release |  | ok (-7.2 dB) | ok (10.354%) |
| 7 | Knee |  | ok (-13.3 dB) | ok (10.326%) |
| 8 | Makeup Gain |  | ok (-14.0 dB) | ok (13.409%) |
| 9 | Mix |  | ok (-0.6 dB) | ok (10.121%) |
| 10 | 2nd Harmonic |  | ok (-7.4 dB) | ok (10.089%) |
| 11 | 3rd Harmonic |  | ok (-18.9 dB) | ok (10.089%) |
| 12 | 4th Harmonic |  | ok (-18.1 dB) | ok (10.082%) |
| 13 | 5th Harmonic |  | ok (-0.6 dB) | ok (10.085%) |
| 14 | Sidechain Enable |  | ok (-15.6 dB) | ok (10.369%) |
| 15 | Sidechain Listen |  | ok (-22.2 dB) | ok (10.374%) |
| 16 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | ok (2.6 dB) | ok (29.283%) |
| 17 | Auto Release |  | ok (-20.9 dB) | ok (10.237%) |
| 18 | Auto Attack |  | ok (-13.8 dB) | ok (10.236%) |
| 19 | Input Gain |  | ok (-24.8 dB) | ok (14.342%) |
| 20 | Output Gain |  | ok (-19.6 dB) | ok (13.743%) |
| 21 | Oversample |  | ok (-39.2 dB) | ok (9.805%) |
| 22 | Oversample Filter |  | ok (-26.1 dB) | ok (10.115%) |
| 23 | Transient Enable |  | ok (-7.4 dB) | ok (10.463%) |
| 24 | Transient Attack | Transient Enable=1 | ok (-1.2 dB) | ok (8.082%) |
| 25 | Transient Sustain | Transient Enable=1 | ok (1.2 dB) | ok (10.307%) |
| 26 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (1.2 dB) | ok (8.501%) |
| 27 | Auto Gain |  | ok (-3.5 dB) | ok (10.190%) |
| 28 | Delta Monitor |  | ok (-2.9 dB) | ok (9.959%) |
| 29 | Bypass |  | ok (-4.8 dB) | ok (8.844%) |
| 30 | Wet |  | ok (1.2 dB) | ok (10.155%) |
| 31 | Delta |  | ok (1.2 dB) | ok (10.133%) |
