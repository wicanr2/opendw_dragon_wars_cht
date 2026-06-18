#!/usr/bin/env python3
"""Amiga Dragon Wars 怪物 sprite → remake .spr(indexed + 自帶 Amiga palette)。

格式(本工具逆向確認,見 docs/61):Amiga data4 怪物資源解壓後 =
  [32B palette: 16 個 BE word 0x0RGB] [8-word sprite header @ off32] [4-bitplane data]
  - palette: 每 word nibble ×17 還原 8-bit RGB(同 title.pic)。
  - header word[1] = 高度(px);word[2] = 每 plane 每列 bytes(bpr)→ 寬 = bpr*8。
  - 影像資料自 offset 32 起,**plane-sequential**(plane0 整張、plane1 整張…),
    MSB-first,4 planes → 16 色。資料與 header 共用前 16 bytes(header 即 plane0 前幾列)。

輸出 .spr:"DWSP" + u16 w + u16 h + u8 16 + 16*(r,g,b) + w*h*index(每像素 0..15)。
索引即 Amiga 自帶 palette 索引;引擎 Amiga 戰鬥路徑套此 palette 後直接呈現 Amiga 色。

用法: amiga_sprite_extract.py <decompressed_res.bin> <name> <out_dir> [--crop-w N]
  decompressed bin 由 tools_build/amiga_res_extract(C++)解壓產生。
  --auto-crop:自動以「整列同色 solid bar」分隔多動畫格,取最大一格(主格)。
  --crop-x N / --crop-w N:手動指定裁切起點 / 寬(覆蓋 auto-crop)。
"""
import sys, os, struct
from PIL import Image


def amiga_palette(d):
    pal = []
    for i in range(16):
        w = struct.unpack_from('>H', d, i * 2)[0]
        pal.append(((w >> 8 & 0xF) * 17, (w >> 4 & 0xF) * 17, (w & 0xF) * 17))
    return pal


def decode_planar(d):
    """回傳 (W, H, idx_list)。idx_list = H*W 個 0..15 palette 索引。"""
    H = struct.unpack_from('>H', d, 32 + 1 * 2)[0]   # word[1]
    bpr = struct.unpack_from('>H', d, 32 + 2 * 2)[0]  # word[2]
    W = bpr * 8
    planes = 4
    doff = 32
    plane_size = bpr * H
    idx = [0] * (W * H)
    for y in range(H):
        for x in range(W):
            byi = x // 8
            bit = 7 - (x % 8)
            v = 0
            for p in range(planes):
                o = doff + p * plane_size + y * bpr + byi
                if o < len(d):
                    v |= ((d[o] >> bit) & 1) << p
            idx[y * W + x] = v & 15
    return W, H, idx


def auto_crop(W, H, idx):
    """多動畫格資源以「整列同一索引(solid bar)」分隔 → 取最大一格。回傳 (x0, w)。

    每隻怪物常把 2–3 個動畫格水平並排,格間插一條全高純色(分隔條,整欄同色)。
    取最長的「非 solid」連續欄段為主格(去掉相鄰格 / 邊緣 wrap)。
    """
    solid = [len({idx[y * W + x] for y in range(H)}) == 1 for x in range(W)]
    best_x0, best_w = 0, 0
    x = 0
    while x < W:
        if solid[x]:
            x += 1
            continue
        x0 = x
        while x < W and not solid[x]:
            x += 1
        if (x - x0) > best_w:
            best_x0, best_w = x0, x - x0
    if best_w == 0:
        return 0, W
    return best_x0, best_w


def main():
    if len(sys.argv) < 4:
        sys.stderr.write("usage: amiga_sprite_extract.py <res.bin> <name> <out_dir> [--crop-w N]\n")
        sys.exit(2)
    src, name, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
    crop_w = crop_x = None
    if '--crop-w' in sys.argv:
        crop_w = int(sys.argv[sys.argv.index('--crop-w') + 1])
    if '--crop-x' in sys.argv:
        crop_x = int(sys.argv[sys.argv.index('--crop-x') + 1])
    d = open(src, 'rb').read()
    pal = amiga_palette(d)
    W, H, idx = decode_planar(d)
    if '--auto-crop' in sys.argv:
        ax, aw = auto_crop(W, H, idx)
        crop_x = ax if crop_x is None else crop_x
        crop_w = aw if crop_w is None else crop_w
        sys.stderr.write(f"  auto-crop: x0={ax} w={aw}\n")
    if crop_x or (crop_w and crop_w < W):
        x0 = crop_x or 0
        cw = crop_w or (W - x0)
        new = [0] * (cw * H)
        for y in range(H):
            for x in range(cw):
                sx = x0 + x
                if sx < W:
                    new[y * cw + x] = idx[y * W + sx]
        W, idx = cw, new
    os.makedirs(out_dir, exist_ok=True)
    base = name
    # .spr：indexed + 自帶 16 色 Amiga palette。
    spr = struct.pack('<4sHHB', b'DWSP', W, H, 16)
    spr += b''.join(struct.pack('BBB', *c) for c in pal)
    spr += bytes(idx)
    open(f'{out_dir}/{base}.spr', 'wb').write(spr)
    # .png(目視/編輯用):RGB 直出。
    img = Image.new('RGB', (W, H))
    px = img.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = pal[idx[y * W + x]]
    img.save(f'{out_dir}/{base}.png')
    print(f"{name}: {W}x{H} -> {base}.spr ({len(spr)}B) + .png")


if __name__ == '__main__':
    main()
