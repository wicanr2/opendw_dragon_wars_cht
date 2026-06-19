#!/usr/bin/env python3
"""渲染 index 流的指定 word 區間,獨立找該區寬度。"""
import sys
from PIL import Image
PAL = [(0,0,0),(255,255,255),(255,0,0),(0,255,0),(0,0,255),(160,160,120),(255,255,0),(255,0,255),
       (0,255,255),(128,64,0),(200,150,100),(100,100,100),(0,128,0),(0,0,128),(128,0,0),(220,200,160)]
idxfile, lo, hi, w, out = sys.argv[1], int(sys.argv[2],0), int(sys.argv[3],0), int(sys.argv[4]), sys.argv[5]
px = open(idxfile,'rb').read()[lo:hi]
h = len(px)//w
img = Image.new('RGB',(w,h)); p=img.load()
for y in range(h):
    for x in range(w):
        p[x,y]=PAL[px[y*w+x]&0x0F]
img.save(out); print(f"{out}: {w}x{h} (words {lo}..{hi})")
