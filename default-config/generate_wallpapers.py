#!/usr/bin/env python3
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
"""Regenerate the original, package-free geometric wallpapers from palettes."""

import importlib.util
import json
import math
from pathlib import Path

root = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('desktop', root / 'capture.py')
desktop = importlib.util.module_from_spec(spec)
spec.loader.exec_module(desktop)
palettes = json.loads((root / 'palettes.json').read_text())
output = root / 'wallpapers'
output.mkdir(exist_ok=True)
width, height = 1600, 1000


def rgb(value):
    return tuple(int(value[i:i + 2], 16) for i in (1, 3, 5))


def blend(a, b, amount):
    return bytes(round(x * (1 - amount) + y * amount) for x, y in zip(a, b))


for name, variants in palettes.items():
    for mode, p in variants.items():
        background, accent, surface = map(rgb, (p['background'], p['accent'], p['surface']))
        rows = []
        for y in range(height):
            row = bytearray()
            # Broad facets leave quiet space behind windows. Dune uses gentle
            # curved strata; the other palettes share Slate's angular geometry.
            curve = int(100 * math.sin(y / height * math.pi)) if name == 'dune' else 0
            cuts = [max(0, min(width, int(width * .18 + y * .65) + curve)),
                    max(0, min(width, int(width * .70 - y * .40) + curve)),
                    max(0, min(width, int(width * .90 + y * .12)))]
            boundaries = sorted(set([0, width] + cuts))
            for i, (left, right) in enumerate(zip(boundaries, boundaries[1:])):
                amount = [.06, .15, .28, .10][i % 4]
                color = blend(background, accent, amount * (.7 if mode == 'light' else 1))
                row.extend(color * (right - left))
            rows.append(row)
        (output / f'{name}-{mode}.png').write_bytes(desktop.png_bytes(width, height, rows))

# A small original session badge avoids references to the removed legacy icons.
p = palettes['slate']['dark']
rows = []
for y in range(48):
    row = bytearray()
    for x in range(48):
        outer = abs(x - 23.5) + abs(y - 23.5) <= 18
        inner = abs(x - 23.5) + abs(y - 23.5) < 11
        row.extend(rgb(p['accent'] if outer and not inner else p['surface']))
    rows.append(row)
(root / 'fvwm-x.png').write_bytes(desktop.png_bytes(48, 48, rows))
