# Plugin QA: VST3: Harmonic Compressor (OpenGrid)

| Check | Status | Detail |
|---|---|---|
| loads | PASS | VST3: Harmonic Compressor (OpenGrid), 29 parameters |
| finite output | PASS | peak -7.0 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1135x688, 0.0% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | PASS |  |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 0 | Algorithm |  | ok (-15.5 dB) | ok (21.959%) |
| 1 | Threshold |  | ok (-7.2 dB) | ok (21.873%) |
| 2 | Ratio |  | ok (-15.8 dB) | ok (20.023%) |
| 3 | Attack |  | ok (-15.2 dB) | ok (20.269%) |
| 4 | Release |  | ok (-18.5 dB) | ok (21.290%) |
| 5 | Knee |  | ok (-29.5 dB) | ok (21.473%) |
| 6 | Makeup Gain |  | ok (-0.6 dB) | ok (29.614%) |
| 7 | Mix |  | ok (-8.8 dB) | ok (21.098%) |
| 8 | 2nd Harmonic |  | ok (-20.9 dB) | ok (20.951%) |
| 9 | 3rd Harmonic |  | ok (-14.3 dB) | ok (17.779%) |
| 10 | 4th Harmonic |  | ok (-24.6 dB) | ok (21.675%) |
| 11 | 5th Harmonic |  | ok (-19.8 dB) | ok (20.435%) |
| 12 | Sidechain Enable |  | ok (-15.6 dB) | ok (21.775%) |
| 13 | Sidechain Listen |  | ok (-13.2 dB) | ok (22.712%) |
| 14 | Sidechain HP Freq | Sidechain Enable=1 | ok (-15.7 dB) | ok (21.974%) |
| 15 | Auto Release |  | ok (-40.4 dB) | ok (21.073%) |
| 16 | Auto Attack |  | ok (-24.9 dB) | ok (21.272%) |
| 17 | Input Gain |  | ok (-0.9 dB) | ok (29.626%) |
| 18 | Output Gain |  | ok (-0.6 dB) | ok (29.279%) |
| 19 | Oversample |  | ok (-9.3 dB) | ok (20.638%) |
| 20 | Oversample Filter |  | ok (-9.3 dB) | ok (21.435%) |
| 21 | Transient Enable | random state 1 | CONDITIONAL (-5.0 dB) | ok (21.351%) |
| 22 | Transient Attack | Transient Enable=1 | ok (-1.7 dB) | ok (17.528%) |
| 23 | Transient Sustain | Transient Enable=1 | ok (-26.6 dB) | ok (21.584%) |
| 24 | Transient Speed | Transient Enable=1, Transient Attack=1 | ok (-1.8 dB) | ok (17.705%) |
| 25 | Bypass |  | ok (-3.7 dB) | ok (22.107%) |
| 26 | Bypass |  | ok (-7.5 dB) | ok (25.015%) |
| 27 | Wet |  | ok (-7.5 dB) | ok (21.576%) |
| 28 | Delta |  | ok (-3.2 dB) | ok (20.613%) |
