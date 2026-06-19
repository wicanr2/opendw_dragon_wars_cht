#!/usr/bin/env python3
"""忠實模擬 0x32b4c blitter(含 16-bit dbra wrap、負 stride),把解碼 buffer 真正攤進 GVRAM。

blitter 0x32b4c 語意(逐指令):
  a0 = 0xC00000 + (dst & 0xFFFFFF) + [0x89bd4(=0)]
  a1 = src (0xD83000)
  src_row_step ($445e6) = arg5 << 1                  (arg5=0 -> 0)
  dst_row_step ($445e2) = (0x400 - w) << 1   (long 運算,但 a0 是 24-bit GVRAM)
  d1 = h - 1
  outer: d0 = w - 1; d2 = d0
    inner: *a0++ = *a1++ ; dbra d2  (d2 是 16-bit!)
    a0 += dst_row_step ; a1 += src_row_step
    dbra d1 (16-bit)
關鍵:dbra 只看低 16 bit。w=0xfff1 -> 內層 d2=0xfff0,跑 0xfff1 次;
但 16-bit dbra 從 0xfff0 遞減到 0xffff 再 -1... 實際 dbra 遞減直到 -1(0xffff)結束 =
跑 (d2+1) 次,d2=0xfff0 -> 65521 次。
本模擬照 16-bit counter 與 24-bit a0 wrap 真實重現。
"""
import sys, struct
from PIL import Image

GVBASE = 0xC00000
GVSIZE = 0x400000  # 4MB GVRAM 視窗(超出 wrap)

def sim(src_idx, dst_off, w, h, scroll=0, src_row_words=None, gv_words=512*512*4):
    """src_idx: list/bytes of 4bpp index (per word). 回傳 GVRAM(dict off->idx) 或 array。"""
    gv = bytearray(gv_words)  # 每元素 = 一個 word 的 4bpp index(0..15);GVRAM 以 word 定址
    # a0 以 word 為單位:gvram word index = ((dst_off)>>1) ... dst_off 是 byte
    a0 = (dst_off + scroll) >> 1            # word index into GVRAM
    a1 = 0                                   # src word index
    dst_row_step_words = (0x400 - w)         # words(原碼 <<1 byte = 此值 word)
    src_row_step_words = 0
    n = len(src_idx)
    GVW = gv_words
    reps_inner = (w - 1) & 0xFFFF            # dbra count low16 -> 跑 reps+1 次
    reps_outer = (h - 1) & 0xFFFF
    for _ in range(reps_outer + 1):
        for _ in range(reps_inner + 1):
            if a1 < n:
                idx = src_idx[a1] & 0x0F
            else:
                idx = 0
            if 0 <= a0 < GVW:
                gv[a0] = idx
            a0 = (a0 + 1)
            a1 += 1
        a0 += dst_row_step_words
        a1 += src_row_step_words
        if a1 >= n:
            break
    return gv

def sim_fast(src_idx, dst_off, w, h, scroll=0, gv_w=1024, gv_h=512):
    """pure-python slice 模擬。每列寫 reps_inner words,a0 每列淨進 reps_inner+dst_row_step。"""
    src = bytes((b & 0x0F) for b in src_idx)
    n = len(src)
    GVW = gv_w * gv_h
    gv = bytearray(GVW)
    a0 = (dst_off + scroll) >> 1
    dst_row_step = (0x400 - w)
    reps_inner = ((w - 1) & 0xFFFF) + 1
    reps_outer = ((h - 1) & 0xFFFF) + 1
    a1 = 0
    for _ in range(reps_outer):
        if a1 >= n:
            break
        take = min(reps_inner, n - a1)
        seg = src[a1:a1 + take]
        lo = a0
        if lo < 0:
            seg = seg[-lo:]; lo = 0
        if lo < GVW and seg:
            end = min(lo + len(seg), GVW)
            gv[lo:end] = seg[:end - lo]
        a1 += reps_inner
        a0 += reps_inner + dst_row_step
    # reshape
    rows = [gv[r*gv_w:(r+1)*gv_w] for r in range(gv_h)]
    return rows, gv_w, gv_h

PAL = [(0,0,0),(255,255,255),(255,0,0),(0,255,0),(0,0,255),(160,160,120),(255,255,0),(255,0,255),
       (0,255,255),(128,64,0),(200,150,100),(100,100,100),(0,128,0),(0,0,128),(128,0,0),(220,200,160)]

if __name__ == '__main__':
    idxfile = sys.argv[1]
    w = int(sys.argv[2]); h = int(sys.argv[3])
    dst_off = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0x24100
    out = sys.argv[5] if len(sys.argv) > 5 else '/w/r4/blit.png'
    src = open(idxfile, 'rb').read()
    rows, gw, gh = sim_fast(src, dst_off, w, h)
    img = Image.new('RGB', (gw, gh))
    p = img.load()
    for y in range(gh):
        row = rows[y]
        for x in range(gw):
            p[x, y] = PAL[row[x] & 0x0F]
    img.save(out)
    print(f"{out}: GVRAM {gw}x{gh} w={w} h={h} dst={dst_off:#x}")
