# Plugin QA: VST3: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: Harmonic Compressor (OpenGrid), 29 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 0x0, 0.3% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Algorithm |  | ok (-18.1 dB) | ok (14.110%) |
| 1 | Threshold |  | ok (-7.2 dB) | ok (19.558%) |
| 2 | Ratio |  | ok (-15.8 dB) | ok (13.753%) |
| 3 | Attack |  | ok (-14.0 dB) | ok (13.218%) |
| 4 | Release |  | ok (-18.9 dB) | ok (14.180%) |
| 5 | Knee |  | ok (-29.4 dB) | ok (14.195%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (17.543%) |
| 7 | Mix |  | ok (-7.0 dB) | ok (14.233%) |
| 8 | 2nd Harmonic |  | ok (-20.9 dB) | ok (14.137%) |
| 9 | 3rd Harmonic |  | ok (-13.8 dB) | ok (11.522%) |
| 10 | 4th Harmonic |  | ok (-24.8 dB) | ok (14.034%) |
| 11 | 5th Harmonic |  | ok (-19.6 dB) | ok (13.042%) |
| 12 | Sidechain Enable |  | ok (-15.6 dB) | ok (14.218%) |
| 13 | Sidechain Listen |  | ok (-13.3 dB) | ok (14.692%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1 | ok (-15.5 dB) | ok (14.076%) |
| 15 | Auto Release |  | ok (-39.2 dB) | ok (14.019%) |
| 16 | Auto Attack |  | ok (-22.2 dB) | ok (13.879%) |
| 17 | Input Gain |  | ok (-0.9 dB) | ok (17.286%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (16.680%) |
| 19 | Oversample |  | ok (-7.4 dB) | ok (13.929%) |
| 20 | Oversample Filter |  | ok (-7.4 dB) | ok (13.793%) |
| 21 | Transient Enable | random state 1 | CONDITIONAL (-4.4 dB) | ok (14.193%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (10.792%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-26.1 dB) | ok (14.372%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (11.019%) |
| 25 | Bypass |  | ok (-3.9 dB) | ok (14.498%) |
| 26 | Bypass |  | ok (-6.1 dB) | ok (16.154%) |
| 27 | Wet |  | ok (-6.1 dB) | ok (14.142%) |
| 28 | Delta |  | ok (-3.5 dB) | ok (14.029%) |
