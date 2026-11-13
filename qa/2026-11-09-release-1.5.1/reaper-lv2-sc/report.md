# Plugin QA: LV2: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | LV2: Harmonic Compressor (OpenGrid) (4->2ch), 30 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1080x813, 0.0% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | FAIL | Sidechain HP Freq (82.40000153 -> ) |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 16 | Sidechain HP Freq | Sidechain Enable=1, Threshold=0.2, Ratio=0.8 | ok (-13.2 dB) | ok (41.379%) |
