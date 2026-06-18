#!/usr/bin/env python3
"""Amiga Dragon Wars full-screen picture extractor (title.pic / endgame).

The Amiga (Interplay 1990) port uses the SAME Huffman codec as the DOS version
(see src/resource/decompress.cpp). A .pic file decompresses to:

    [32-byte palette: 16 big-endian words, 0x0RGB][bitplane data]

The bitplane data is 320x200, 4 bitplanes, stored SEQUENTIALLY (plane0 whole
image, then plane1, ...), NOT Amiga-interleaved. The 16-colour palette is
Amiga 12-bit RGB (each nibble * 17 -> 8-bit).

Confirmed against title.pic: decompressed 35996 B = 32-byte palette + 32000-byte
4-plane 320x200 image (+ a few trailing bytes). Palette word[0]=black,
word[1]=white(0xfff), word[2]=gold(0xf59) -> the golden dragon.

Usage:
  amiga_pic_extract.py <raw_decompressed.bin> <out.png> [--scale N]
    (input must already be Huffman-decompressed; use the project's decompress()
     or tools_build/scratch decompressor first.)
"""
import sys
from PIL import Image

W, H, PLANES = 320, 200, 4
PAL_OFF, DATA_OFF = 0, 32


def read_palette(raw, off=PAL_OFF, ncol=16):
    pal = []
    for i in range(ncol):
        w = (raw[off + i * 2] << 8) | raw[off + i * 2 + 1]
        r, g, b = (w >> 8) & 0xF, (w >> 4) & 0xF, w & 0xF
        pal.append((r * 17, g * 17, b * 17))
    return pal


def render(raw, w=W, h=H, planes=PLANES, data_off=DATA_OFF, pal=None):
    if pal is None:
        pal = read_palette(raw)
    bpr = w // 8
    plane_size = bpr * h
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            bi = y * bpr + x // 8
            bit = 7 - (x % 8)
            val = 0
            for p in range(planes):
                o = data_off + p * plane_size + bi
                if o < len(raw):
                    val |= ((raw[o] >> bit) & 1) << p
            px[x, y] = pal[val]
    return img


def main():
    raw = open(sys.argv[1], "rb").read()
    out = sys.argv[2]
    scale = 2
    if "--scale" in sys.argv:
        scale = int(sys.argv[sys.argv.index("--scale") + 1])
    img = render(raw)
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    img.save(out)
    print(f"wrote {out} {img.width}x{img.height}")


if __name__ == "__main__":
    main()
