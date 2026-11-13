# Plugin QA: LV2: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | LV2: Harmonic Compressor (OpenGrid) (4->2ch), 30 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -116.9 dB |
| editor opens | PASS | 0x0, 0.6% animates by itself |
| every parameter changes the sound | FAIL | Sidechain HP Freq |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | FAIL | Algorithm (5.21999979 -> 5.00000000), Ratio (60.22000122 -> 60.20000076), Release (1053.25000000 -> 1053.00000000), Knee (17.28000069 -> 17.30000114), Makeup Gain (-8.76000023 -> -8.80000019), Sidechain Enable (0.31000000 -> 0.00000000), Sidechain Listen (0.68000001 -> 1.00000000), Auto Release (0.41999999 -> 0.00000000), Auto Attack (0.79000002 -> 1.00000000), Input Gain (-16.31999969 -> -16.29999924), Output Gain (1.44000006 -> 1.40000033), Oversample (2.70000005 -> 3.00000000), Oversample Filter (0.27000001 -> 0.00000000), Transient Enable (0.63999999 -> 1.00000000), Transient Speed (37.62500000 -> 37.60000229) |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Free Wheeling |  | excused (-112.0 dB) | ok (16.349%) |
| 1 | Enabled |  | ok (-3.9 dB) | ok (16.697%) |
| 2 | Algorithm |  | ok (-18.1 dB) | ok (15.923%) |
| 3 | Threshold |  | ok (-7.2 dB) | ok (23.000%) |
| 4 | Ratio |  | ok (-13.3 dB) | ok (16.100%) |
| 5 | Attack |  | ok (-13.3 dB) | ok (15.756%) |
| 6 | Release |  | ok (-13.3 dB) | ok (15.985%) |
| 7 | Knee |  | ok (-13.3 dB) | ok (16.207%) |
| 8 | Makeup Gain |  | ok (-0.3 dB) | ok (19.806%) |
| 9 | Mix |  | ok (-7.0 dB) | ok (15.463%) |
| 10 | 2nd Harmonic |  | ok (-7.0 dB) | ok (15.892%) |
| 11 | 3rd Harmonic |  | ok (-7.0 dB) | ok (16.308%) |
| 12 | 4th Harmonic |  | ok (-7.0 dB) | ok (16.304%) |
| 13 | 5th Harmonic |  | ok (-7.0 dB) | ok (16.341%) |
| 14 | Sidechain Enable |  | ok (-7.0 dB) | ok (15.762%) |
| 15 | Sidechain Listen |  | ok (-7.0 dB) | ok (16.254%) |
| 16 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | DEAD (-240.0 dB) | ok (29.753%) |
| 17 | Auto Release |  | ok (-7.0 dB) | ok (16.257%) |
| 18 | Auto Attack |  | ok (-7.0 dB) | ok (16.580%) |
| 19 | Input Gain |  | ok (2.0 dB) | ok (19.734%) |
| 20 | Output Gain |  | ok (2.0 dB) | ok (19.116%) |
| 21 | Oversample |  | ok (-6.1 dB) | ok (15.944%) |
| 22 | Oversample Filter |  | ok (-6.1 dB) | ok (16.291%) |
| 23 | Transient Enable |  | ok (-6.1 dB) | ok (16.467%) |
| 24 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (13.650%) |
| 25 | Transient Sustain | Transient Enable=1 | ok (-2.2 dB) | ok (13.093%) |
| 26 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.7 dB) | ok (13.570%) |
| 27 | Bypass |  | ok (-6.1 dB) | ok (16.221%) |
| 28 | Wet |  | ok (-6.1 dB) | ok (15.789%) |
| 29 | Delta |  | ok (-6.1 dB) | ok (15.462%) |
