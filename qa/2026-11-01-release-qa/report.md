# Plugin QA: AU: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | AU: Harmonic Compressor (OpenGrid), 28 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1120x813, 4.3% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Mix |  | ok (-7.0 dB) | ok (20.471%) |
| 1 | Knee |  | ok (-29.4 dB) | ok (19.165%) |
| 2 | Transient Speed | Transient Enable=1, Transient Attack=0.85, Transient Sustain=0.85 | ok (-3.7 dB) | ok (16.380%) |
| 3 | Ratio |  | ok (-15.8 dB) | ok (100.000%) |
| 4 | Sidechain HP Freq | Sidechain Enable=1 | ok (-15.5 dB) | ok (100.000%) |
| 5 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (14.281%) |
| 6 | Threshold |  | ok (-7.2 dB) | ok (32.318%) |
| 7 | Sidechain Listen |  | ok (-13.3 dB) | ok (19.586%) |
| 8 | Attack |  | ok (-14.0 dB) | ok (18.965%) |
| 9 | Output Gain |  | ok (-0.6 dB) | ok (21.759%) |
| 10 | Oversample |  | ok (-7.4 dB) | ok (16.073%) |
| 11 | Release |  | ok (-18.9 dB) | ok (16.287%) |
| 12 | Algorithm |  | ok (-18.1 dB) | ok (16.501%) |
| 13 | Makeup Gain |  | ok (-0.6 dB) | ok (20.275%) |
| 14 | Sidechain Enable |  | ok (-15.6 dB) | ok (16.265%) |
| 15 | Auto Attack |  | ok (-22.2 dB) | ok (16.746%) |
| 16 | Input Gain |  | ok (-0.9 dB) | ok (100.000%) |
| 17 | 2nd Harmonic |  | ok (-6.9 dB) | ok (8.116%) |
| 18 | 3rd Harmonic |  | ok (-6.9 dB) | ok (100.000%) |
| 19 | 4th Harmonic |  | ok (-6.9 dB) | ok (2.366%) |
| 20 | 5th Harmonic |  | ok (-19.6 dB) | ok (15.663%) |
| 21 | Auto Release |  | ok (-39.2 dB) | ok (16.370%) |
| 22 | Transient Sustain | Transient Enable=1 | ok (-26.1 dB) | ok (16.487%) |
| 23 | Oversample Filter |  | ok (-7.4 dB) | ok (100.000%) |
| 24 | Transient Enable | Transient Attack=0.85random state 1 | CONDITIONAL (-54.3 dB) | ok (2.568%) |
| 25 | Bypass |  | ok (-6.9 dB) | ok (2.206%) |
| 26 | Wet |  | ok (-6.9 dB) | ok (2.203%) |
| 27 | Delta |  | ok (-3.5 dB) | ok (100.000%) |
