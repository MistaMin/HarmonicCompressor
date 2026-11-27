#!/usr/bin/env python3
"""Compares the port's editor renders (port/*.ppm) with 1.5.3's (ref/*.png), pixel by pixel.
Writes diff/<name>.png (differences amplified x8 over a dimmed reference) and prints per state:
the largest channel difference, the share of pixels that differ by more than 8 and by more than 32,
and the mean absolute difference.  usage: compare.py [gui dir]"""
import sys, os
import numpy as np
from PIL import Image

here = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, 'diff'), exist_ok=True)
rows = []
for f in sorted(os.listdir(os.path.join(here, 'ref'))):
    if not f.endswith('.png'):
        continue
    name = f[:-4]
    port_path = os.path.join(here, 'port', name + '.ppm')
    if not os.path.exists(port_path):
        print(f'{name}: no port render'); continue
    a = np.asarray(Image.open(os.path.join(here, 'ref', f)).convert('RGB')).astype(int)
    b = np.asarray(Image.open(port_path).convert('RGB')).astype(int)
    if a.shape != b.shape:
        print(f'{name}: size {a.shape} vs {b.shape}'); continue
    d = np.abs(a - b).max(axis=2)
    over8, over32 = (d > 8).mean() * 100, (d > 32).mean() * 100
    rows.append((name, d.max(), over8, over32, np.abs(a - b).mean()))
    vis = (a * 0.35).astype(np.uint8)
    vis[d > 8] = np.clip(np.stack([d[d > 8] * 8, d[d > 8] * 2, d[d > 8] * 0], axis=1), 0, 255)
    Image.fromarray(vis).save(os.path.join(here, 'diff', name + '.png'))
print(f'{"state":16} {"max":>4} {">8 %":>8} {">32 %":>8} {"mean":>6}')
for r in rows:
    print(f'{r[0]:16} {r[1]:4d} {r[2]:8.3f} {r[3]:8.3f} {r[4]:6.3f}')
