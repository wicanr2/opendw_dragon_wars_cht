#!/usr/bin/env python3
"""把權威解碼 index 流以指定寬度渲染成 PNG(generic 16-color palette,看結構)。"""
import sys
from PIL import Image

# X68000-ish 16 色 generic palette(僅供目視結構,非真盤)
PAL = [
    (0,0,0),(255,255,255),(255,0,0),(0,255,0),(0,0,255),(160,160,120),(255,255,0),(255,0,255),
    (0,255,255),(128,64,0),(200,150,100),(100,100,100),(0,128,0),(0,0,128),(128,0,0),(220,200,160),
]

idxfile = sys.argv[1]
w = int(sys.argv[2])
out = sys.argv[3]
px = open(idxfile, 'rb').read()
h = len(px) // w
img = Image.new('RGB', (w, h))
pix = img.load()
for y in range(h):
    base = y * w
    for x in range(w):
        v = px[base + x] & 0x0F
        pix[x, y] = PAL[v]
img.save(out)
print(f"{out}: {w}x{h}")
