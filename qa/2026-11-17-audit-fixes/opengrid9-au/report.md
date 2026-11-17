# Plugin QA: AU: Harmonic Compressor AUTest

| Check | Status | Detail |
|---|---|---|
| loads | PASS | Harmonic Compressor AUTest, 27 parameters |
| finite output | PASS | peak -6.1 dBFS |
| not silent | PASS | rms -18.9 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | SKIPPED | visual checks disabled |
| every parameter changes the sound | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | SKIPPED | headless: OpenGrid9 runs without an editor window |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Mix |  | ok (-13.6 dB) | -  |
| 1 | Knee |  | ok (-32.0 dB) | -  |
| 2 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (3.5 dB) | -  |
| 3 | Delta Monitor |  | ok (-5.1 dB) | -  |
| 4 | Ratio |  | ok (-17.3 dB) | -  |
| 5 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | ok (-12.5 dB) | -  |
| 6 | Transient Attack | Transient Enable=1 | ok (5.7 dB) | -  |
| 7 | Threshold |  | ok (-9.4 dB) | -  |
| 8 | Sidechain Listen |  | ok (-13.6 dB) | -  |
| 9 | Attack |  | ok (-12.4 dB) | -  |
| 10 | Output Gain |  | ok (17.3 dB) | -  |
| 11 | Oversample |  | ok (-6.8 dB) | -  |
| 12 | Release |  | ok (-6.0 dB) | -  |
| 13 | Algorithm |  | ok (-6.5 dB) | -  |
| 14 | Makeup Gain |  | ok (17.8 dB) | -  |
| 15 | Sidechain Enable |  | ok (-6.8 dB) | -  |
| 16 | Auto Attack |  | ok (-6.8 dB) | -  |
| 17 | Auto Gain |  | ok (-4.0 dB) | -  |
| 18 | Input Gain |  | ok (17.0 dB) | -  |
| 19 | 2nd Harmonic |  | ok (-6.4 dB) | -  |
| 20 | 3rd Harmonic |  | ok (-6.8 dB) | -  |
| 21 | 4th Harmonic |  | ok (-6.8 dB) | -  |
| 22 | 5th Harmonic |  | ok (-6.8 dB) | -  |
| 23 | Auto Release |  | ok (-6.8 dB) | -  |
| 24 | Transient Sustain | Transient Enable=1 | ok (-30.1 dB) | -  |
| 25 | Oversample Filter |  | ok (-6.8 dB) | -  |
| 26 | Transient Enable |  | ok (-6.8 dB) | -  |
