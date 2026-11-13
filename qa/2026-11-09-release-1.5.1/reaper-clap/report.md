# Plugin QA: CLAP: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | CLAP: Harmonic Compressor (OpenGrid), 28 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1080x813, 0.0% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Algorithm |  | ok (-18.1 dB) | ok (20.097%) |
| 1 | Threshold |  | ok (-7.4 dB) | ok (25.862%) |
| 2 | Ratio |  | ok (-15.8 dB) | ok (20.454%) |
| 3 | Attack |  | ok (-14.0 dB) | ok (19.252%) |
| 4 | Release |  | ok (-18.9 dB) | ok (20.686%) |
| 5 | Knee |  | ok (-29.4 dB) | ok (19.918%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (22.880%) |
| 7 | Mix |  | ok (-7.0 dB) | ok (20.260%) |
| 8 | 2nd Harmonic |  | ok (-20.9 dB) | ok (19.826%) |
| 9 | 3rd Harmonic |  | ok (-13.8 dB) | ok (17.367%) |
| 10 | 4th Harmonic |  | ok (-24.8 dB) | ok (19.692%) |
| 11 | 5th Harmonic |  | ok (-19.6 dB) | ok (18.940%) |
| 12 | Sidechain Enable |  | ok (-15.6 dB) | ok (19.482%) |
| 13 | Sidechain Listen |  | ok (-13.3 dB) | ok (20.647%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1 | ok (-15.5 dB) | ok (19.302%) |
| 15 | Auto Release |  | ok (-39.2 dB) | ok (19.674%) |
| 16 | Auto Attack |  | ok (-22.6 dB) | ok (19.580%) |
| 17 | Input Gain |  | ok (-1.0 dB) | ok (22.161%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (21.941%) |
| 19 | Oversample |  | ok (1.2 dB) | ok (18.852%) |
| 20 | Oversample Filter |  | ok (-4.9 dB) | ok (19.960%) |
| 21 | Transient Enable |  | ok (-4.0 dB) | ok (20.313%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (15.675%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-26.1 dB) | ok (19.786%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (15.939%) |
| 25 | Bypass |  | ok (-6.1 dB) | ok (20.265%) |
| 26 | Wet |  | ok (-6.6 dB) | ok (19.773%) |
| 27 | Delta |  | ok (-3.5 dB) | ok (19.855%) |
