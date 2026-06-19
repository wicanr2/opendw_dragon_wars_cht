#!/usr/bin/env python3
# measure_text_px — 量測 640×480 dump 的視窗尺寸 / letterbox 黑邊 / 文字 ink 高度。
#
# 用途:驗證 docs/assessment/47 方案 3 的目標 —— 視窗確為 640×480、像素層 320×200 ×2 垂直置中
#   (上下各 40px 黑邊)、文字層字級固定(CJK 內文 ≈24px、UI ≈16px)。
#
# 量法(非字形 metrics,而是「實際畫到畫面上的 ink」):
#   - letterbox:掃描全黑列(整列 RGB=0)在頂端 / 底端連續段長度。
#   - 文字 ink 高:在指定「掃描帶」(text band)內,把「非黑且非背景」的列視為 ink 列,
#     找連續 ink 列段(允許 <=gap 列空隙黏合),回報每段高度 → 即一行文字的 ink 高度。
#   背景判定用該帶最常見顏色(底框 / 黑底)為背景。
import sys, struct
from collections import Counter

def read_ppm(path):
    with open(path, 'rb') as f:
        data = f.read()
    assert data[:2] == b'P6', "not P6 ppm"
    # 解析 header(magic, w, h, maxval),容許註解 / 多空白。
    idx = 2
    toks = []
    while len(toks) < 3:
        while idx < len(data) and data[idx:idx+1].isspace():
            idx += 1
        if data[idx:idx+1] == b'#':
            while idx < len(data) and data[idx:idx+1] != b'\n':
                idx += 1
            continue
        start = idx
        while idx < len(data) and not data[idx:idx+1].isspace():
            idx += 1
        toks.append(int(data[start:idx]))
    w, h, maxv = toks
    idx += 1  # 跳過 maxval 後的單一空白
    px = data[idx:idx + w*h*3]
    return w, h, px

def row_is_black(px, w, y):
    base = y*w*3
    for x in range(w):
        o = base + x*3
        if px[o] or px[o+1] or px[o+2]:
            return False
    return True

def measure(path, bands):
    w, h, px = read_ppm(path)
    print(f"{path}: window = {w}x{h}")
    # letterbox 黑邊
    top = 0
    while top < h and row_is_black(px, w, top):
        top += 1
    bot = 0
    while bot < h and row_is_black(px, w, h-1-bot):
        bot += 1
    print(f"  letterbox: top black rows = {top}, bottom black rows = {bot}")
    # 各 band 文字 ink 高
    for name, y0, y1, x0, x1 in bands:
        # 該帶背景色 = 最常見像素
        cnt = Counter()
        for y in range(y0, y1):
            for x in range(x0, x1, 2):
                o = (y*w+x)*3
                cnt[(px[o], px[o+1], px[o+2])] += 1
        bg = cnt.most_common(1)[0][0]
        ink_rows = []
        for y in range(y0, y1):
            ink = 0
            for x in range(x0, x1):
                o = (y*w+x)*3
                c = (px[o], px[o+1], px[o+2])
                # ink = 非背景 且 非全黑(letterbox / 框邊)
                if c != bg and c != (0,0,0):
                    ink += 1
            ink_rows.append(ink > 2)  # 至少幾個 ink 像素才算一列文字
        # 連續 ink 段(gap<=2 黏合)
        segs = []
        run = 0; gap = 0; cur = 0
        for y, on in enumerate(ink_rows):
            if on:
                if cur == 0:
                    seg_start = y0 + y
                cur += 1 + gap if cur > 0 else 1
                gap = 0
            else:
                if cur > 0:
                    gap += 1
                    if gap > 2:
                        segs.append(cur - (gap-1))
                        cur = 0; gap = 0
        if cur > 0:
            segs.append(cur)
        segs = [s for s in segs if s >= 6]   # 過濾雜訊(<6px)
        print(f"  band '{name}' ink-row heights: {segs}")

if __name__ == '__main__':
    base = sys.argv[1] if len(sys.argv) > 1 else 'docs/assessment'
    # band: (name, y0, y1, x0, x1)  —— window 座標(640×480)
    # 標題「火龍之戰」虛擬 y=6 → 視窗 y=6*2+40=52,字級 48px → 約 52..120
    # 選單選項 CJK 內文 PX_BODY=24,落在標題下方
    measure(f"{base}/mode_640x480_menu.ppm", [
        ("title(48px expect)", 48, 110, 16, 400),
        ("body-cjk(24px expect)", 120, 300, 16, 500),
    ])
    measure(f"{base}/mode_640x480_para.ppm", [
        ("ui-levelname(16px expect)", 40, 70, 16, 300),
    ])
