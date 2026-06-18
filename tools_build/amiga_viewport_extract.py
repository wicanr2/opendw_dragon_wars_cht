#!/usr/bin/env python3
"""Amiga data3 viewport 元件(地牢牆面 / 天空 / 地面…)→ remake .spr 平面圖塊。

Amiga data3 res 110–135 為第一人稱 viewport 元件,與 DOS bundle/components/<tag>.bin 對應
(110=Castle wall、111=Sky、112=Road、116=Water…)。解壓後結構(本工具逆向確認):

  [BE word offset 表]  N 個遞增 BE-u16,各指向一個子圖塊(對應 DOS bin 內 pointer table 的
                       sprite_offset slot;index = (DOS sprite_offset-4)/2)。
  各子圖塊 @off:       word[1]=高度 H、word[2]=每 plane 每列 bytes bpr → 寬 W=bpr*8;
                       影像自 off 起 4-bitplane plane-sequential MSB-first(同 sprite,惟無
                       自帶 palette → 共用 viewport 全域盤)。

輸出:每子圖塊一個 <tag>_<blockidx>.spr(DWSP indexed + 內嵌 16 色 stone palette)。
blockidx = offset 表索引,供引擎以 (DOS sprite_offset-4)/2 對映選圖塊。

用法: amiga_viewport_extract.py <res.bin> <tag> <out_dir>
"""
import sys, os, struct
from PIL import Image

# viewport 全域 16 色 stone palette(逆向 + 目視調校;Amiga 原盤未從程式碼抽出,此為相容色)。
STONE = [(20,20,28),(235,235,235),(150,140,130),(95,88,80),(110,80,55),(70,55,40),
         (175,150,110),(200,195,185),(50,48,55),(90,100,140),(110,160,110),(140,200,200),
         (60,55,50),(120,110,100),(210,190,140),(230,225,215)]


def read_offsets(d):
    offs = []
    i = 0
    while i < 64:
        w = struct.unpack_from('>H', d, i * 2)[0]
        if offs and (w <= offs[-1] or w >= len(d)):
            break
        if w >= len(d) or (w == 0):
            break
        offs.append(w)
        i += 1
    return offs


def decode(d, off):
    H = struct.unpack_from('>H', d, off + 2)[0]
    bpr = struct.unpack_from('>H', d, off + 4)[0]
    W = bpr * 8
    if not (0 < W <= 256 and 0 < H <= 256):
        return None
    psz = bpr * H
    idx = [0] * (W * H)
    for y in range(H):
        for x in range(W):
            byi = x // 8
            bit = 7 - (x % 8)
            v = 0
            for p in range(4):
                o = off + p * psz + y * bpr + byi
                if o < len(d):
                    v |= ((d[o] >> bit) & 1) << p
            idx[y * W + x] = v & 15
    return W, H, idx


def main():
    if len(sys.argv) < 4:
        sys.stderr.write("usage: amiga_viewport_extract.py <res.bin> <tag> <out_dir>\n")
        sys.exit(2)
    src, tag, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    d = open(src, 'rb').read()
    offs = read_offsets(d)
    os.makedirs(out_dir, exist_ok=True)
    n = 0
    for bi, off in enumerate(offs):
        r = decode(d, off)
        if r is None:
            continue
        W, H, idx = r
        spr = struct.pack('<4sHHB', b'DWSP', W, H, 16)
        spr += b''.join(struct.pack('BBB', *c) for c in STONE)
        spr += bytes(idx)
        open(f'{out_dir}/{tag}_{bi}.spr', 'wb').write(spr)
        img = Image.new('RGB', (W, H))
        px = img.load()
        for y in range(H):
            for x in range(W):
                px[x, y] = STONE[idx[y * W + x]]
        img.save(f'{out_dir}/{tag}_{bi}.png')
        n += 1
    print(f"tag {tag}: {len(offs)} offsets -> {n} blocks")


if __name__ == '__main__':
    main()
