# Plugin QA: VST3: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: Harmonic Compressor (OpenGrid), 29 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1120x813, 0.1% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Algorithm |  | ok (-18.1 dB) | ok (14.932%) |
| 1 | Threshold |  | ok (-7.2 dB) | ok (21.441%) |
| 2 | Ratio |  | ok (-15.8 dB) | ok (15.006%) |
| 3 | Attack |  | ok (-14.0 dB) | ok (14.085%) |
| 4 | Release |  | ok (-18.9 dB) | ok (15.023%) |
| 5 | Knee |  | ok (-29.4 dB) | ok (14.788%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (18.262%) |
| 7 | Mix |  | ok (-7.0 dB) | ok (15.129%) |
| 8 | 2nd Harmonic |  | ok (-20.9 dB) | ok (14.875%) |
| 9 | 3rd Harmonic |  | ok (-13.8 dB) | ok (12.718%) |
| 10 | 4th Harmonic |  | ok (-24.8 dB) | ok (15.119%) |
| 11 | 5th Harmonic |  | ok (-19.6 dB) | ok (14.092%) |
| 12 | Sidechain Enable |  | ok (-15.6 dB) | ok (15.165%) |
| 13 | Sidechain Listen |  | ok (-13.3 dB) | ok (15.695%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1 | ok (-15.5 dB) | ok (14.801%) |
| 15 | Auto Release |  | ok (-39.2 dB) | ok (14.659%) |
| 16 | Auto Attack |  | ok (-22.2 dB) | ok (14.858%) |
| 17 | Input Gain |  | ok (-0.9 dB) | ok (18.252%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (17.799%) |
| 19 | Oversample |  | ok (-7.4 dB) | ok (13.823%) |
| 20 | Oversample Filter |  | ok (-7.4 dB) | ok (14.730%) |
| 21 | Transient Enable | Transient Attack=0.85 | ok (-3.1 dB) | ok (11.459%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (11.473%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-26.1 dB) | ok (15.150%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=0.85, Transient Sustain=0.85 | ok (-3.7 dB) | ok (100.000%) |
| 25 | Bypass |  | ok (-3.9 dB) | ok (100.000%) |
| 26 | Bypass |  | ok (-6.1 dB) | ok (100.000%) |
| 27 | Wet |  | ok (-6.1 dB) | ok (14.672%) |
| 28 | Delta |  | ok (-3.5 dB) | ok (17.494%) |
