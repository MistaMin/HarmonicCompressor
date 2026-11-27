# Plugin QA: LV2: Harmonic Compressor Test (OpenGrid)

| Check | Status | Detail |
|---|---|---|
| loads | PASS | LV2: Harmonic Compressor Test (OpenGrid) (4->2ch), 31 parameters |
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
| 0 | Algorithm |  | ok (-19.5 dB) | ok (0.792%) |
| 1 | Threshold |  | ok (-7.5 dB) | ok (1.654%) |
| 2 | Ratio |  | ok (-15.5 dB) | ok (1.058%) |
| 3 | Attack |  | ok (-11.9 dB) | ok (0.751%) |
| 4 | Release |  | ok (-19.1 dB) | ok (0.757%) |
| 5 | Knee |  | ok (-30.0 dB) | ok (0.924%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (1.510%) |
| 7 | Mix |  | ok (-11.8 dB) | ok (0.947%) |
| 8 | 2nd Harmonic |  | ok (-19.9 dB) | ok (0.899%) |
| 9 | 3rd Harmonic |  | ok (-13.1 dB) | ok (0.900%) |
| 10 | 4th Harmonic |  | ok (-25.0 dB) | ok (0.901%) |
| 11 | 5th Harmonic |  | ok (-19.7 dB) | ok (0.902%) |
| 12 | Sidechain Enable |  | ok (-15.1 dB) | ok (0.876%) |
| 13 | Sidechain Listen |  | ok (-11.8 dB) | ok (0.880%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | ok (-12.6 dB) | ok (1.890%) |
| 15 | Auto Release |  | ok (-41.8 dB) | ok (0.756%) |
| 16 | Auto Attack |  | ok (-23.1 dB) | ok (0.751%) |
| 17 | Input Gain |  | ok (-0.8 dB) | ok (0.795%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (0.794%) |
| 19 | Oversample |  | ok (1.3 dB) | ok (0.751%) |
| 20 | Oversample Filter |  | ok (-7.7 dB) | ok (0.769%) |
| 21 | Transient Enable | random state 1 | CONDITIONAL (3.5 dB) | ok (1.153%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (1.159%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-25.7 dB) | ok (1.161%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (1.164%) |
| 25 | Auto Gain |  | ok (-12.0 dB) | ok (1.085%) |
| 26 | Delta Monitor |  | ok (-3.3 dB) | ok (1.132%) |
| 27 | Freewheel |  | ok (-6.1 dB) | ok (0.748%) |
| 28 | Bypass |  | ok (-7.4 dB) | ok (1.064%) |
| 29 | Wet |  | ok (-7.7 dB) | ok (0.748%) |
| 30 | Delta |  | ok (-3.5 dB) | ok (0.775%) |
