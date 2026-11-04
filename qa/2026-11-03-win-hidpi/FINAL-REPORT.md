# Windows HiDPI 1.5× "clipping" investigation — final report

Date: 2026-11-04 · Machine: Windows 11 25H2, 1920×1080 @ 150 % (DPI 144) · Target: HarmonicCompressor (JUCE 8.0.17)

## Verdict

**There is no rendering bug in the plugin or in JUCE.** The editor renders completely and
correctly at 150 % scaling, both Standalone and as VST3 in REAPER. The "content drawn 1.5×
too large, clipped at right/bottom" symptom was an **artifact of the QA capture script**
(`winlist_win.py`), which ran as a DPI-*unaware* process.

## Root cause (with evidence)

`plugin-qa-skills/skills/plugin-daw-test/scripts/winlist_win.py` (before fix):

1. `GetWindowRect(hwnd)` in a DPI-unaware python returns **logical** pixels: 1122×818
   (observed in `list` output before the fix).
2. It creates a 1122×818 bitmap and calls `PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT)`.
3. The JUCE app is genuinely per-monitor DPI aware (in-app diag: `platScale=1.5
   dpiForWindow=144 clientRect=1683x1227`, LOGPIXELSX=144). Its window surface is the full
   **physical** 1683×1227. `PrintWindow` makes the app blit that surface 1:1 into the DC.
4. Only the top-left 1122/1683 = 2/3 (≈ 1/1.5² in area) fits → the capture shows the UI
   "zoomed 1.5×, clipped at right and bottom". Exactly the reported symptom.

Counter-evidence that the app itself is correct:

- `DwmQueryThumbnailSourceSize` on the live window returns **1683×1227** = the physical
  window size → DWM presents the window surface 1:1, no extra scaling at presentation.
- JUCE-internal paint diag (temporary, since reverted): `rcPaint=0,98 1683x1080
  client=1683x1227 platScale=1.5` — paint target is physical, transform 1.5, output correct.
- `Component::createComponentSnapshot(top, 1.0)` produced the complete editor
  (`captures/hc_snapshot.png`) — component tree and layout are fine.
- A capture taken from a **DPI-aware** process shows the full editor
  (`captures/dpiaware_capture.png`), pixel-identical to the fixed script's output.
- Both renderers (default Direct2D and forced GDI engine 0) showed the identical "clipping"
  in the old captures — impossible for a renderer bug, consistent with a capture artifact.

## Fix (the only code change in this whole task)

`plugin-qa-skills/skills/plugin-daw-test/scripts/winlist_win.py`, right after
`PW_RENDERFULLCONTENT = 2`:

```python
# Become per-monitor-V2 DPI aware before reading any window metrics. Without this,
# on a >100% scaled display GetWindowRect returns logical (scaled-down) pixels while
# a DPI-aware target paints its full physical surface into the PrintWindow DC, so the
# capture is clipped to the top-left 1/scale portion and looks "zoomed in".
try:
    user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))  # PER_MONITOR_AWARE_V2
except Exception:
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)  # PROCESS_PER_MONITOR_DPI_AWARE
    except Exception:
        user32.SetProcessDPIAware()
```

Effect: `list` now reports physical pixels (1683×1227), the capture bitmap matches the
window surface, and the ImageGrab fallback bbox (also physical) stays consistent.

**HarmonicCompressor and its JUCE checkout have zero net changes** (`git diff` empty in
both). All diagnostic instrumentation was reverted; the shipped binary is a clean rebuild.

## Before / after

| | capture size | result |
|---|---|---|
| before (DPI-unaware winlist) | 1122×818 | UI zoomed 1.5×, SIDECHAIN tab cut, knobs clipped |
| after (fixed winlist) | 1683×1227 | full editor: `captures/winlist_fixed.png`, `captures/standalone_clean_fixed.png` |

## Verification

- Standalone (clean rebuild): fixed script captures the complete editor incl. title bar,
  all 5 bottom knobs, OUT/GR meters — `captures/standalone_clean_fixed.png`.
- REAPER VST3 full suite (`reaper_qa.py suite --fx "VST3: Harmonic Compressor"
  --exceptions qa/exceptions.txt`): **all checks PASS** — loads, finite output, not silent,
  deterministic render, editor opens (1702×1032 physical), every parameter changes the
  sound, every parameter shown in GUI, state survives save/reopen, 5 editor close/open
  cycles. Identical to the 2026-11-02-win-release baseline (`report.md`). Editor captures
  inside REAPER (`editor_p*.png`) show the full UI at 150 %.

## Per-repo recipe for the other 6 plugins (HybridEQ, BlueChannel, DualSone, ArcVu, OpenPreamp, TubeChannel)

**No source changes are needed in any repo — the bug was never in plugin code or JUCE.**
Apply this to each repo's QA:

1. Use the fixed `winlist_win.py` (single shared copy in `plugin-qa-skills`; if a repo
   vendors its own capture script, port the same `SetProcessDpiAwarenessContext(-4)` block).
2. Delete any QA conclusions/baselines drawn from DPI-unaware captures on >100 % displays;
   re-capture baselines. Expect `editor opens` sizes ~1.5× larger than before (physical px).
3. Re-run the REAPER suite per repo; compare against the pre-HiDPI baseline, not against
   the clipped captures.
4. If a repo's suite flags "visual" diffs only because baseline PNGs were clipped, re-record
   those baselines — do not touch plugin code.

Reference data gathered per repo (only relevant if a genuine HiDPI issue ever appears —
all four JUCE versions 8.0.15/8.0.17/9.0.3/9.0.4 share the identical renderer structure):
- HybridEQ: editor `Source/PluginEditor.h` (`HybridEQEditor`), JUCE 9.0.3 at `build3/_deps/juce-src`
- BlueChannel: shares HybridEQ's JUCE via `FETCHCONTENT_SOURCE_DIR_JUCE`, editor `Source/Plugin/PluginEditor.h` (`BlueChannelEditor`), build dir `build/release-windows`
- DualSone: JUCE 8.0.15 `build/_deps/juce-src`, editor `Source/PluginEditor.h` (`DualLoudnessEditor`)
- ArcVu: JUCE 9.0.4 sibling `audiogridder/JUCE` (CMakeLists JUCE_DIR), editor `plugin/PluginEditor.h` (`ArcVuEditor`)
- OpenPreamp: JUCE 9.0.3 `build/_deps/juce-src`, editor `Source/PluginEditor.h` (`HybridEQEditor`)
- TubeChannel: JUCE 9.0.3 `build/_deps/juce-src`, editor `Source/Plugin/PluginEditor.h` (`TubeChannelEditor`)

## Risks / notes

- Fix is process-local to the python capture process; zero impact on plugins, REAPER, or
  100 %-DPI machines (no-op there). Per-monitor V2 is correct for multi-monitor setups.
- PrintWindow relies on the target repainting; a genuinely hung plugin still yields a flat
  image and the ImageGrab fallback still applies (now with physical coords on both paths).
- This session is headless/remote: screen-pixel ground truth was established via DWM API
  (`DwmQueryThumbnailSourceSize` = physical window size ⇒ 1:1 presentation), not by looking
  at the physical panel. Recommend one glance at the real display to visually confirm.
- The misleading captures are what made the bug appear in Standalone *and* REAPER alike —
  both were captured by the same script. No host-specific HiDPI issue was found in REAPER.
