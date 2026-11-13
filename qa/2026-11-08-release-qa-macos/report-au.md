# Plugin QA: AU: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | AU: Harmonic Compressor (OpenGrid), 28 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1120x813, 0.0% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Mix |  | ok (-18.1 dB) | ok (17.118%) |
| 1 | Knee |  | ok (-7.4 dB) | ok (16.427%) |
| 2 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-0.9 dB) | ok (13.143%) |
| 3 | Ratio |  | ok (-14.0 dB) | ok (16.648%) |
| 4 | Sidechain HP Freq | Sidechain Enable=1 | ok (-3.0 dB) | ok (16.627%) |
| 5 | Transient Attack | Transient Enable=1 | ok (-3.3 dB) | ok (12.760%) |
| 6 | Threshold |  | ok (-0.6 dB) | ok (23.768%) |
| 7 | Sidechain Listen |  | ok (-7.0 dB) | ok (17.126%) |
| 8 | Attack |  | ok (-20.9 dB) | ok (16.508%) |
| 9 | Output Gain |  | ok (-13.8 dB) | ok (19.216%) |
| 10 | Oversample |  | ok (-24.8 dB) | ok (16.208%) |
| 11 | Release |  | ok (-19.6 dB) | ok (16.640%) |
| 12 | Algorithm |  | ok (-15.6 dB) | ok (7.644%) |
| 13 | Makeup Gain |  | ok (-13.3 dB) | ok (2.903%) |
| 14 | Sidechain Enable |  | ok (-13.3 dB) | ok (1.861%) |
| 15 | Auto Attack |  | ok (-39.2 dB) | ok (1.864%) |
| 16 | Input Gain |  | ok (-23.7 dB) | ok (1.939%) |
| 17 | 2nd Harmonic |  | ok (-1.0 dB) | ok (1.998%) |
| 18 | 3rd Harmonic |  | ok (-0.6 dB) | ok (1.998%) |
| 19 | 4th Harmonic |  | ok (1.2 dB) | ok (1.998%) |
| 20 | 5th Harmonic |  | ok (-4.9 dB) | ok (1.998%) |
| 21 | Auto Release |  | ok (-6.1 dB) | ok (1.876%) |
| 22 | Transient Sustain | Transient Enable=1 | ok (-1.7 dB) | ok (1.654%) |
| 23 | Oversample Filter |  | ok (-26.1 dB) | ok (1.882%) |
| 24 | Transient Enable |  | ok (-1.7 dB) | ok (1.648%) |
| 25 | Bypass |  | ok (-3.9 dB) | ok (1.833%) |
| 26 | Wet |  | ok (-6.1 dB) | ok (1.861%) |
| 27 | Delta |  | ok (-6.6 dB) | ok (1.886%) |
