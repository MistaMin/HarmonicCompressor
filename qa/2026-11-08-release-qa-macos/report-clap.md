# Plugin QA: CLAP: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | CLAP: Harmonic Compressor (OpenGrid), 28 parameters |
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
| 0 | Algorithm |  | ok (-18.1 dB) | ok (18.940%) |
| 1 | Threshold |  | ok (-7.4 dB) | ok (20.654%) |
| 2 | Ratio |  | ok (-15.8 dB) | ok (18.984%) |
| 3 | Attack |  | ok (-14.0 dB) | ok (17.869%) |
| 4 | Release |  | ok (-18.9 dB) | ok (19.653%) |
| 5 | Knee |  | ok (-29.4 dB) | ok (18.872%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (21.868%) |
| 7 | Mix |  | ok (-7.0 dB) | ok (18.867%) |
| 8 | 2nd Harmonic |  | ok (-20.9 dB) | ok (18.878%) |
| 9 | 3rd Harmonic |  | ok (-13.8 dB) | ok (16.420%) |
| 10 | 4th Harmonic |  | ok (-24.8 dB) | ok (18.685%) |
| 11 | 5th Harmonic |  | ok (-19.6 dB) | ok (17.924%) |
| 12 | Sidechain Enable |  | ok (-15.6 dB) | ok (18.449%) |
| 13 | Sidechain Listen |  | ok (-13.3 dB) | ok (19.515%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1 | ok (-15.5 dB) | ok (18.299%) |
| 15 | Auto Release |  | ok (-39.2 dB) | ok (18.551%) |
| 16 | Auto Attack |  | ok (-23.7 dB) | ok (18.550%) |
| 17 | Input Gain |  | ok (-1.0 dB) | ok (21.389%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (21.048%) |
| 19 | Oversample |  | ok (1.2 dB) | ok (19.225%) |
| 20 | Oversample Filter |  | ok (-4.9 dB) | ok (18.546%) |
| 21 | Transient Enable |  | ok (-6.1 dB) | ok (18.936%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (14.275%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-26.1 dB) | ok (18.776%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (14.706%) |
| 25 | Bypass |  | ok (-3.9 dB) | ok (18.858%) |
| 26 | Wet |  | ok (-6.1 dB) | ok (18.520%) |
| 27 | Delta |  | ok (-6.6 dB) | ok (18.545%) |
