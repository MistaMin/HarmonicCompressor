# Plugin QA: VST3: Harmonic Compressor Dev

| Check | Status | Detail |
|---|---|---|
| loads | PASS | Harmonic Compressor Dev, 28 parameters |
| finite output | PASS | peak -8.5 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | SKIPPED | visual checks disabled |
| every parameter changes the sound | PASS |  |
| state survives save and reopen | FAIL | Ratio (0.87 -> 0.869979), Attack (0.24 -> 0.240657), Release (0.61 -> 0.610146), Knee (0.98 -> 0.979167), Sidechain HP Freq (0.31 -> 0.31306), Input Gain (0.42 -> 0.420833), Output Gain (0.79 -> 0.789583), Transient Speed (0.01 -> 0) |
| editor reopens | SKIPPED | headless: OpenGrid9 runs without an editor window |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Algorithm |  | ok (-14.6 dB) | -  |
| 1 | Threshold |  | ok (-8.7 dB) | -  |
| 2 | Ratio |  | ok (-17.4 dB) | -  |
| 3 | Attack |  | ok (-16.7 dB) | -  |
| 4 | Release |  | ok (-19.3 dB) | -  |
| 5 | Knee |  | ok (-30.9 dB) | -  |
| 6 | Makeup Gain |  | ok (14.9 dB) | -  |
| 7 | Mix |  | ok (-14.7 dB) | -  |
| 8 | 2nd Harmonic |  | ok (-23.5 dB) | -  |
| 9 | 3rd Harmonic |  | ok (-15.4 dB) | -  |
| 10 | 4th Harmonic |  | ok (-24.4 dB) | -  |
| 11 | 5th Harmonic |  | ok (-21.6 dB) | -  |
| 12 | Sidechain Enable |  | ok (-15.5 dB) | -  |
| 13 | Sidechain Listen |  | ok (-14.7 dB) | -  |
| 14 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | ok (-11.8 dB) | -  |
| 15 | Auto Release |  | ok (-42.2 dB) | -  |
| 16 | Auto Attack |  | ok (-39.4 dB) | -  |
| 17 | Input Gain |  | ok (12.2 dB) | -  |
| 18 | Output Gain |  | ok (14.9 dB) | -  |
| 19 | Oversample |  | ok (-9.3 dB) | -  |
| 20 | Oversample Filter |  | ok (-9.3 dB) | -  |
| 21 | Transient Enable |  | ok (-68.1 dB) | -  |
| 22 | Transient Attack | Transient Enable=1 | ok (3.0 dB) | -  |
| 23 | Transient Sustain | Transient Enable=1 | ok (-30.0 dB) | -  |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (1.6 dB) | -  |
| 25 | Auto Gain |  | ok (-15.1 dB) | -  |
| 26 | Delta Monitor |  | ok (-5.1 dB) | -  |
| 27 | Bypass |  | ok (-3.9 dB) | -  |
