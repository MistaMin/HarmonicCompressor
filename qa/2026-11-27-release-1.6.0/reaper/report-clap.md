# Plugin QA: CLAP: Harmonic Compressor Test (OpenGrid)

| Check | Status | Detail |
|---|---|---|
| loads | PASS | CLAP: Harmonic Compressor Test (OpenGrid), 30 parameters |
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
| 0 | Algorithm |  | ok (-19.5 dB) | ok (21.956%) |
| 1 | Threshold |  | ok (-7.5 dB) | ok (32.230%) |
| 2 | Ratio |  | ok (-15.5 dB) | ok (20.935%) |
| 3 | Attack |  | ok (-11.9 dB) | ok (21.202%) |
| 4 | Release |  | ok (-19.1 dB) | ok (23.221%) |
| 5 | Knee |  | ok (-30.0 dB) | ok (22.578%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (24.489%) |
| 7 | Mix |  | ok (-11.8 dB) | ok (22.649%) |
| 8 | 2nd Harmonic |  | ok (-19.9 dB) | ok (22.157%) |
| 9 | 3rd Harmonic |  | ok (-13.1 dB) | ok (19.449%) |
| 10 | 4th Harmonic |  | ok (-25.0 dB) | ok (22.059%) |
| 11 | 5th Harmonic |  | ok (-19.7 dB) | ok (21.293%) |
| 12 | Sidechain Enable |  | ok (-15.1 dB) | ok (21.800%) |
| 13 | Sidechain Listen |  | ok (-11.8 dB) | ok (22.925%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | ok (-12.6 dB) | ok (38.704%) |
| 15 | Auto Release |  | ok (-41.8 dB) | ok (22.417%) |
| 16 | Auto Attack |  | ok (-23.1 dB) | ok (21.931%) |
| 17 | Input Gain |  | ok (-0.8 dB) | ok (23.989%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (23.786%) |
| 19 | Oversample |  | ok (-6.7 dB) | ok (21.105%) |
| 20 | Oversample Filter |  | ok (-4.8 dB) | ok (21.472%) |
| 21 | Transient Enable |  | ok (-2.3 dB) | ok (22.487%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (17.554%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-25.7 dB) | ok (22.734%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (18.400%) |
| 25 | Auto Gain |  | ok (-12.0 dB) | ok (22.934%) |
| 26 | Delta Monitor |  | ok (-3.3 dB) | ok (20.385%) |
| 27 | Bypass |  | ok (-6.1 dB) | ok (20.365%) |
| 28 | Wet |  | ok (-7.7 dB) | ok (21.894%) |
| 29 | Delta |  | ok (-3.5 dB) | ok (21.910%) |
