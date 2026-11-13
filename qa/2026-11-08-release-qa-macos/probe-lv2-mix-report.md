# Plugin QA: LV2: Harmonic Compressor

| Check | Status | Detail |
|---|---|---|
| loads | PASS | LV2: Harmonic Compressor (OpenGrid) (4->2ch), 30 parameters |
| finite output | PASS | peak -6.9 dBFS |
| not silent | PASS | rms -18.3 dBFS |
| deterministic render | PASS | repeat differs by -240.0 dB |
| editor opens | PASS | 1120x813, 8.1% animates by itself |
| every parameter changes the sound | PASS |  |
| every parameter is shown in the GUI | PASS |  |
| state survives save and reopen | FAIL | Mix (0.13000000 -> ) |
| editor reopens | PASS | 5 close/open cycles |

| # | Parameter | Context | Audio | Visual |
|---|---|---|---|---|
| 9 | Mix |  | ok (-13.8 dB) | ok (2.083%) |
