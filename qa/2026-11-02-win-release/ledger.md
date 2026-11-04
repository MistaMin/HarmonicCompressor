# Harmonic Compressor 1.5.0 — Windows QA ledger (2026-11-02)

Scope: Windows release build of tag v1.5.0 (commit 407b114) and host verification.

| Item | Evidence | Status |
|---|---|---|
| Build Release x64, commit 407b114 (tag v1.5.0 == master HEAD), clean tree | build.log | PASS |
| VST3 loads in REAPER 7.80 x64, 29 parameters | report.md "loads" | PASS |
| Finite, non-silent, deterministic render | report.md | PASS |
| Editor opens, 5 close/open cycles | report.md | PASS |
| State round trip (save/reopen) | report.md | PASS |
| Every parameter changes the sound | report.md + exceptions.txt (4 conditional params retested with their section enabled) | PASS |
| Every parameter shown in GUI (per-pixel diff) | report.md | PASS |
| Full-layout GUI visual check at 150% DPI | editor_*.png, standalone_*.png | PARTIAL — see finding F1 |

## Finding F1 (needs user decision)
On this machine (1920x1080 at 150% DPI), the editor content renders 1.5x larger than its
window in both REAPER (VST3) and the Standalone, and cannot be brought to fit by resizing
(editor max width 1600 logical < 1680 physical needed). Bottom knob row (THRESHOLD, RATIO,
MAKEUP, MIX, IN/OUT GAIN) and the right OUT/GR meter column are not reachable on screen.
HybridEQ.vst3 (previously shipped build) shows the identical behaviour in the same REAPER —
this is a standing cross-project Windows HiDPI issue, not a v1.5.0 regression.
The per-parameter visual sweep still PASSed (each control affects pixels), and the top-left
region was verified against Docs/harmonic-compressor-reaper.png; the bottom/right regions
could not be captured on this display configuration.

## Exceptions
qa/exceptions.txt — 4 context entries (Sidechain HP Freq, Transient Attack/Sustain/Speed),
each retested with its section enabled; none excused.

## Untracked files left in the working tree (not committed)
- build_win.bat (local build helper, mirrors HybridEQ)
- build.log, build.pid
- qa/2026-11-02-win-release/ (this evidence), qa/exceptions.txt
- JUCE/ (gitignored dependency checkout, tag 8.0.17-security)
- ../clap-juce-extensions (sibling checkout at pinned commit 55525c9)
