# Plugin QA: AU: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | AU: Harmonic Compressor (OpenGrid), 28 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1080x813, 0.9% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Mix |  | ok (-7.0 dB) | ok (16.082%) |
| 1 | Knee |  | ok (-29.4 dB) | ok (15.506%) |
| 2 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (12.007%) |
| 3 | Ratio |  | ok (-15.8 dB) | ok (15.601%) |
| 4 | Sidechain HP Freq | Sidechain Enable=1 | ok (-15.5 dB) | ok (15.391%) |
| 5 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (12.068%) |
| 6 | Threshold |  | ok (-7.2 dB) | ok (21.512%) |
| 7 | Sidechain Listen |  | ok (-13.3 dB) | ok (16.100%) |
| 8 | Attack |  | ok (-14.0 dB) | ok (14.844%) |
| 9 | Output Gain |  | ok (-0.6 dB) | ok (18.263%) |
| 10 | Oversample |  | ok (-7.4 dB) | ok (14.793%) |
| 11 | Release |  | ok (-18.9 dB) | ok (15.970%) |
| 12 | Algorithm |  | ok (-18.1 dB) | ok (15.502%) |
| 13 | Makeup Gain |  | ok (-0.6 dB) | ok (19.053%) |
| 14 | Sidechain Enable |  | ok (-15.6 dB) | ok (14.872%) |
| 15 | Auto Attack |  | ok (-22.2 dB) | ok (15.274%) |
| 16 | Input Gain |  | ok (-0.9 dB) | ok (18.797%) |
| 17 | 2nd Harmonic |  | ok (-20.9 dB) | ok (15.072%) |
| 18 | 3rd Harmonic |  | ok (-13.8 dB) | ok (12.384%) |
| 19 | 4th Harmonic |  | ok (-24.8 dB) | ok (15.704%) |
| 20 | 5th Harmonic |  | ok (-19.6 dB) | ok (14.169%) |
| 21 | Auto Release |  | ok (-39.2 dB) | ok (15.074%) |
| 22 | Transient Sustain | Transient Enable=1 | ok (-26.1 dB) | ok (15.489%) |
| 23 | Oversample Filter |  | ok (-7.4 dB) | ok (15.419%) |
| 24 | Transient Enable | random state 1 | CONDITIONAL (-54.3 dB) | ok (15.408%) |
| 25 | Bypass |  | ok (-6.1 dB) | ok (16.275%) |
| 26 | Wet |  | ok (-6.1 dB) | ok (15.438%) |
| 27 | Delta |  | ok (-3.5 dB) | ok (15.096%) |
