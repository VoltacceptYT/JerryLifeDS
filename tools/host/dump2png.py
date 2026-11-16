#!/usr/bin/env python3
"""Converts a raw 256x192 ARGB1555 framebuffer dump to PNG:  dump2png.py in.raw out.png [scale]"""
import sys
from PIL import Image
d = open(sys.argv[1], "rb").read()
w, h = 256, 192
n = len(d) // (w * h * 2)
sc = int(sys.argv[3]) if len(sys.argv) > 3 else 2
im = Image.new("RGB", (w, h * n))
px = im.load()
for i in range(w * h * n):
    v = d[2 * i] | (d[2 * i + 1] << 8)
    px[i % w, i // w] = (((v & 31) * 255) // 31, (((v >> 5) & 31) * 255) // 31, (((v >> 10) & 31) * 255) // 31)
im.resize((w * sc, h * n * sc), Image.NEAREST).save(sys.argv[2])
