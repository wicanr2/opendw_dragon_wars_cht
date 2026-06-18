#!/usr/bin/env python3
"""X68000 Dragon Wars .PIX extractor (chunky 4bpp, headerless).

The Sharp X68000 (Starcraft/Hudson) Dragon Wars stores its UNCOMPRESSED art as
.PIX files: a flat stream of 4-bit pixels, 2 px/byte, high nibble first, indexed
into a 16-colour CLUT. Geometry was recovered by byte-autocorrelation of the row
stride (see docs/61):

  ICON.PIX  16640 B  stride 16 B  -> 32 px wide   (UI / viewport icons)
  MON.PIX  810000 B  stride 24 B  -> 48 px wide   (monster sprites, ~48x48 cells)
  PIC.PIX  518400 B  stride 48 B  -> 96 px wide   (scene / NPC portraits)

The original X68000 16-colour palette is NOT yet recovered (would need to trace
the GVRAM CLUT load in DRAGON.X). We render with the DOS EGA-16 palette as a
faithful-geometry placeholder; shapes are fully recognisable, only hues differ.

Usage:
  x68k_pix_extract.py <file.PIX> <width> <out.png> [--scale N] [--skip-bytes N]
"""
import sys
from PIL import Image

# DOS EGA 16-colour placeholder palette (matches tools_build/scene_render.py).
EGA = [(0,0,0),(0,0,0xAA),(0,0xAA,0),(0,0xAA,0xAA),(0xAA,0,0),(0xAA,0,0xAA),
       (0xAA,0x55,0),(0xAA,0xAA,0xAA),(0x55,0x55,0x55),(0x55,0x55,0xFF),
       (0x55,0xFF,0x55),(0x55,0xFF,0xFF),(0xFF,0x55,0x55),(0xFF,0x55,0xFF),
       (0xFF,0xFF,0x55),(0xFF,0xFF,0xFF)]


def render(raw, width, palette=EGA, hi_first=True):
    h = (len(raw) * 2) // width
    img = Image.new("RGB", (width, h))
    px = img.load()
    i = 0
    for y in range(h):
        for x in range(0, width, 2):
            b = raw[i] if i < len(raw) else 0
            i += 1
            a, lo = (b >> 4) & 0xF, b & 0xF
            if not hi_first:
                a, lo = lo, a
            px[x, y] = palette[a]
            if x + 1 < width:
                px[x + 1, y] = palette[lo]
    return img


def main():
    f, width, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    scale = 1
    skip = 0
    args = sys.argv[4:]
    for i, a in enumerate(args):
        if a == "--scale":
            scale = int(args[i + 1])
        if a == "--skip-bytes":
            skip = int(args[i + 1])
    raw = open(f, "rb").read()[skip:]
    img = render(raw, width)
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    img.save(out)
    print(f"wrote {out} {img.width}x{img.height} (src {len(raw)}B, width {width})")


if __name__ == "__main__":
    main()
