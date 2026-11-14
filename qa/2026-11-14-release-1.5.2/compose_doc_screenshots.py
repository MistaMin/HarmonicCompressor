#!/usr/bin/env python3
"""Paste the 1.5.2 knob bar (rendered by DocKnobBarRender with each screenshot's settings) into the
user-manual screenshots. Only the knob bar rectangle (editor bounds 116,672 888x76) is replaced; the
position and scale of the editor in each image are found by matching the unchanged IN GAIN panel.
Usage: compose_doc_screenshots.py <knobbar dir> <editor-theme snapshot dir> <Docs dir> <out dir>"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image

KB = (116, 672, 888, 76)          # knob bar, editor logical px
IG = (12, 672, 92, 76)            # IN GAIN panel, identical before and after

knob, snaps, docs, out = map(Path, sys.argv[1:5])
out.mkdir(parents=True, exist_ok=True)

def arr(img):
    return np.asarray(img.convert("RGB"), dtype=np.float32)

def locate(target, theme, scales, x0, y0, radius):
    """Find editor origin (x, y) and scale in target by matching the IN GAIN panel of the theme."""
    ref = Image.open(snaps / f"editor-theme-{theme}-off.png")  # rendered at 2x
    tgt = arr(target)
    best = None
    for s in scales:
        w, h = round(IG[2] * s), round(IG[3] * s)
        tpl = arr(ref.crop((IG[0] * 2, IG[1] * 2, (IG[0] + IG[2]) * 2, (IG[1] + IG[3]) * 2)).resize((w, h), Image.LANCZOS))
        ex, ey = x0 + IG[0] * s, y0 + IG[1] * s
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                X, Y = round(ex) + dx, round(ey) + dy
                if X < 0 or Y < 0 or Y + h > tgt.shape[0] or X + w > tgt.shape[1]:
                    continue
                e = float(np.mean((tgt[Y:Y + h, X:X + w] - tpl) ** 2))
                if best is None or e < best[0]:
                    best = (e, X - IG[0] * s, Y - IG[1] * s, s)
    return best

def paste(target, crop_name, ox, oy, s):
    bar = Image.open(knob / f"knobbar-{crop_name}.png").convert("RGB")
    w, h = round(KB[2] * s), round(KB[3] * s)
    if bar.size != (w, h):
        bar = bar.resize((w, h), Image.LANCZOS)
    target.paste(bar, (round(ox + KB[0] * s), round(oy + KB[1] * s)))

log = []
# Full-size 2x screenshots of the editor alone.
for doc, crop, theme in [("harmonic-compressor-soft-white.png", "soft-white", 1),
                         ("harmonic-compressor-modules.png", "modules", 1)]:
    img = Image.open(docs / doc).convert("RGB")
    e, ox, oy, s = locate(img, theme, [2.0], 0, 0, 3)
    paste(img, crop, ox, oy, s)
    img.save(out / doc)
    log.append(f"{doc}: origin ({ox:.1f},{oy:.1f}) scale {s:.4f} IN GAIN mse {e:.1f}")

# REAPER window screenshot, editor at 1x.
img = Image.open(docs / "harmonic-compressor-reaper.png").convert("RGB")
e, ox, oy, s = locate(img, 0, [1.0], 0, img.height - 760, 12)
paste(img, "reaper", ox, oy, s)
img.save(out / "harmonic-compressor-reaper.png")
log.append(f"harmonic-compressor-reaper.png: origin ({ox:.1f},{oy:.1f}) scale {s:.4f} IN GAIN mse {e:.1f}")

# Theme grid: four tiles, editor scaled down.
img = Image.open(docs / "harmonic-compressor-themes-grid.png").convert("RGB")
W, H = img.size
scales = [x / 10000 for x in range(11950, 12251, 25)]
for theme, (tx, ty) in enumerate([(0, 0), (1, 0), (0, 1), (1, 1)]):
    # approximate tile origins measured on the 2798x1996 grid
    gx = [25, 1413][tx]
    gy = [62, 1047][ty]
    e, ox, oy, s = locate(img, theme, scales, gx, gy, 14)
    e, ox, oy, s = locate(img, theme, [s + d / 10000 for d in range(-24, 25, 4)], ox, oy, 3)
    paste(img, f"grid-{theme}", ox, oy, s)
    log.append(f"themes-grid tile {theme}: origin ({ox:.1f},{oy:.1f}) scale {s:.4f} IN GAIN mse {e:.1f}")
img.save(out / "harmonic-compressor-themes-grid.png")
print("\n".join(log))
