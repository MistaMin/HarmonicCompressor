#!/usr/bin/env python3
"""Put the 1.5.3 editor renders (DocScreenshotRender) into the user-manual pictures.
soft-white and modules are full 2x editor renders and replace the pictures. In the theme grid and the
host-window picture the whole editor is replaced in place; its position and scale are found by matching
the IN GAIN panel, which did not change between versions.
Usage: compose_doc_screenshots.py <render dir> <Docs dir> <out dir>"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image

EDITOR = (1120, 760)      # editor logical px
IG = (12, 672, 92, 76)    # IN GAIN panel, logical px

renders, docs, out = map(Path, sys.argv[1:4])
out.mkdir(parents=True, exist_ok=True)


def arr(img):
    return np.asarray(img.convert("RGB"), dtype=np.float32)


def locate(target, ref, scales, x0, y0, radius):
    """Editor origin (x, y) and scale in target, by matching ref's IN GAIN panel (ref rendered at 2x)."""
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


def replace(target, editor, ox, oy, s):
    size = (round(EDITOR[0] * s), round(EDITOR[1] * s))
    target.paste(editor.convert("RGB").resize(size, Image.LANCZOS), (round(ox), round(oy)))


for name in ("soft-white", "modules"):
    Image.open(renders / f"editor-{name}.png").convert("RGB").save(out / f"harmonic-compressor-{name}.png", optimize=True)
    print(name, "replaced")

# Theme grid: four editors, found near the cell origins of the 1.5.2 grid (2798x1996).
grid = Image.open(docs / "harmonic-compressor-themes-grid.png").convert("RGB")
for theme, (x0, y0) in enumerate([(25, 60), (1415, 60), (25, 1047), (1415, 1047)]):
    editor = Image.open(renders / f"editor-grid-{theme}.png")
    e, ox, oy, s = locate(grid, editor, [k / 1000 for k in range(1196, 1236, 2)], x0, y0, 40)
    print(f"grid-{theme}: origin {ox:.1f},{oy:.1f} scale {s:.3f} error {e:.1f}")
    replace(grid, editor, ox, oy, s)
grid.save(out / "harmonic-compressor-themes-grid.png", optimize=True)

# Host window (1120x813): the editor at 1x below the host's title and toolbar.
host = Image.open(docs / "harmonic-compressor-reaper.png").convert("RGB")
ref = Image.open(renders / "editor-grid-0.png")  # Matrix theme at 2x, same IN GAIN panel
e, ox, oy, s = locate(host, ref, [1.0], 0, 53, 8)
print(f"reaper: origin {ox:.1f},{oy:.1f} error {e:.1f}")
host.paste(Image.open(renders / "editor-reaper.png").convert("RGB"), (round(ox), round(oy)))
host.save(out / "harmonic-compressor-reaper.png", optimize=True)
