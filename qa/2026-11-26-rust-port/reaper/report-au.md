# Plugin QA: AU: Harmonic Compressor Test (OpenGrid)

| Check | Status | Detail |
|---|---|---|
| loads | PASS | AU: Harmonic Compressor Test (OpenGrid), 30 parameters |
| finite output | PASS | peak -6.0 dBFS |
| not silent | PASS | rms -18.9 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1120x813, 0.0% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Algorithm |  | ok (-19.5 dB) | ok (22.002%) |
| 1 | Threshold |  | ok (-7.5 dB) | ok (31.411%) |
| 2 | Ratio |  | ok (-15.5 dB) | ok (20.912%) |
| 3 | Attack |  | ok (-11.9 dB) | ok (21.491%) |
| 4 | Release |  | ok (-19.1 dB) | ok (23.686%) |
| 5 | Knee |  | ok (-30.0 dB) | ok (22.055%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (24.487%) |
| 7 | Mix |  | ok (-11.8 dB) | ok (22.892%) |
| 8 | 2nd Harmonic |  | ok (-19.9 dB) | ok (22.172%) |
| 9 | 3rd Harmonic |  | ok (-13.1 dB) | ok (19.418%) |
| 10 | 4th Harmonic |  | ok (-25.0 dB) | ok (22.028%) |
| 11 | 5th Harmonic |  | ok (-19.7 dB) | ok (21.386%) |
| 12 | Sidechain Enable |  | ok (-15.1 dB) | ok (22.098%) |
| 13 | Sidechain Listen |  | ok (-11.8 dB) | ok (22.810%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | ok (-12.6 dB) | ok (39.226%) |
| 15 | Auto Release |  | ok (-41.8 dB) | ok (21.905%) |
| 16 | Auto Attack |  | ok (-23.1 dB) | ok (22.385%) |
| 17 | Input Gain |  | ok (-0.8 dB) | ok (24.126%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (23.725%) |
| 19 | Oversample |  | ok (0.2 dB) | ok (21.476%) |
| 20 | Oversample Filter |  | ok (-5.4 dB) | ok (21.880%) |
| 21 | Transient Enable |  | ok (-2.3 dB) | ok (22.850%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (17.522%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-25.7 dB) | ok (22.736%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (18.209%) |
| 25 | Auto Gain |  | ok (-12.0 dB) | ok (22.925%) |
| 26 | Delta Monitor |  | ok (-3.3 dB) | ok (20.347%) |
| 27 | Bypass |  | ok (-6.1 dB) | ok (20.603%) |
| 28 | Wet |  | ok (-7.7 dB) | ok (21.884%) |
| 29 | Delta |  | ok (-3.5 dB) | ok (21.862%) |
