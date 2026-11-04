# Plugin QA: VST3: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: Harmonic Compressor (OpenGrid), 29 parameters |
| finite output | PASS | peak -7.0 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1702x1032, 0.0% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Algorithm |  | ok (-15.5 dB) | ok (12.297%) |
| 1 | Threshold |  | ok (-7.2 dB) | ok (19.145%) |
| 2 | Ratio |  | ok (-15.8 dB) | ok (9.190%) |
| 3 | Attack |  | ok (-15.2 dB) | ok (14.809%) |
| 4 | Release |  | ok (-18.5 dB) | ok (11.406%) |
| 5 | Knee |  | ok (-29.5 dB) | ok (10.969%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (8.751%) |
| 7 | Mix |  | ok (-8.8 dB) | ok (10.977%) |
| 8 | 2nd Harmonic |  | ok (-20.9 dB) | ok (10.361%) |
| 9 | 3rd Harmonic |  | ok (-14.3 dB) | ok (10.187%) |
| 10 | 4th Harmonic |  | ok (-24.6 dB) | ok (12.097%) |
| 11 | 5th Harmonic |  | ok (-19.8 dB) | ok (10.572%) |
| 12 | Sidechain Enable |  | ok (-15.6 dB) | ok (15.840%) |
| 13 | Sidechain Listen |  | ok (-13.2 dB) | ok (11.582%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1 | ok (-15.7 dB) | ok (12.480%) |
| 15 | Auto Release |  | ok (-40.4 dB) | ok (11.765%) |
| 16 | Auto Attack |  | ok (-24.9 dB) | ok (11.152%) |
| 17 | Input Gain |  | ok (-0.9 dB) | ok (5.626%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (5.393%) |
| 19 | Oversample |  | ok (-9.3 dB) | ok (15.062%) |
| 20 | Oversample Filter |  | ok (-9.3 dB) | ok (11.219%) |
| 21 | Transient Enable | random state 1 | CONDITIONAL (-5.0 dB) | ok (11.684%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (11.561%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-26.6 dB) | ok (11.842%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (9.324%) |
| 25 | Bypass |  | ok (-3.7 dB) | ok (10.677%) |
| 26 | Bypass |  | ok (-7.5 dB) | ok (12.645%) |
| 27 | Wet |  | ok (-7.5 dB) | ok (12.299%) |
| 28 | Delta |  | ok (-3.2 dB) | ok (14.261%) |
