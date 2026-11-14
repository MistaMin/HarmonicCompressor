# Plugin QA: VST3: Harmonic Compressor Dev

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: Harmonic Compressor Dev (OpenGrid), 31 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1080x847, 0.0% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Algorithm |  | ok (-18.1 dB) | ok (18.330%) |
| 1 | Threshold |  | ok (-7.4 dB) | ok (22.221%) |
| 2 | Ratio |  | ok (-15.8 dB) | ok (17.262%) |
| 3 | Attack |  | ok (-14.0 dB) | ok (17.570%) |
| 4 | Release |  | ok (-18.9 dB) | ok (18.120%) |
| 5 | Knee |  | ok (-29.4 dB) | ok (17.407%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (20.228%) |
| 7 | Mix |  | ok (-13.3 dB) | ok (17.504%) |
| 8 | 2nd Harmonic |  | ok (-20.9 dB) | ok (18.162%) |
| 9 | 3rd Harmonic |  | ok (-13.8 dB) | ok (15.712%) |
| 10 | 4th Harmonic |  | ok (-24.8 dB) | ok (17.899%) |
| 11 | 5th Harmonic |  | ok (-19.6 dB) | ok (17.368%) |
| 12 | Sidechain Enable |  | ok (-15.6 dB) | ok (17.636%) |
| 13 | Sidechain Listen |  | ok (-13.3 dB) | ok (17.997%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | ok (-13.2 dB) | ok (36.143%) |
| 15 | Auto Release |  | ok (-39.2 dB) | ok (18.593%) |
| 16 | Auto Attack |  | ok (-24.8 dB) | ok (17.048%) |
| 17 | Input Gain |  | ok (-1.0 dB) | ok (19.858%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (20.267%) |
| 19 | Oversample |  | ok (1.2 dB) | ok (17.232%) |
| 20 | Oversample Filter |  | ok (-4.9 dB) | ok (17.108%) |
| 21 | Transient Enable |  | ok (-6.1 dB) | ok (18.087%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (13.690%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-26.1 dB) | ok (17.905%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (14.624%) |
| 25 | Auto Gain |  | ok (-13.4 dB) | ok (18.142%) |
| 26 | Delta Monitor |  | ok (-3.5 dB) | ok (14.994%) |
| 27 | Bypass |  | ok (-3.9 dB) | ok (15.132%) |
| 28 | Bypass |  | ok (-6.1 dB) | ok (17.341%) |
| 29 | Wet |  | ok (-6.6 dB) | ok (17.032%) |
| 30 | Delta |  | ok (-3.5 dB) | ok (17.032%) |
