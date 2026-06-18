#!/usr/bin/env python3
"""Render an X68000 .PIX as a wrapped contact sheet.

A .PIX decodes (via x68k_pix_extract) to a tall single-column strip of fixed-width
cells. For a viewable deliverable we slice that strip into fixed-height bands and
lay the bands out left-to-right in `cols` columns, so the whole sheet fits a
reasonable aspect ratio.

Usage:
  x68k_contact_sheet.py <file.PIX> <cell_w> <band_h> <cols> <out.png> [--skip N]
"""
import sys
from PIL import Image
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from x68k_pix_extract import render  # noqa: E402


def main():
    f, cell_w, band_h, cols, out = (
        sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), sys.argv[5])
    skip = 0
    if "--skip" in sys.argv:
        skip = int(sys.argv[sys.argv.index("--skip") + 1])
    raw = open(f, "rb").read()[skip:]
    strip = render(raw, cell_w)
    n_bands = (strip.height + band_h - 1) // band_h
    rows = (n_bands + cols - 1) // cols
    gap = 4
    sheet = Image.new("RGB", (cols * (cell_w + gap) - gap, rows * (band_h + gap) - gap),
                       (40, 40, 40))
    for b in range(n_bands):
        band = strip.crop((0, b * band_h, cell_w, min((b + 1) * band_h, strip.height)))
        cx = (b % cols) * (cell_w + gap)
        cy = (b // cols) * (band_h + gap)
        sheet.paste(band, (cx, cy))
    sheet = sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST)
    sheet.save(out)
    print(f"wrote {out} {sheet.width}x{sheet.height} ({n_bands} bands of {cell_w}x{band_h})")


if __name__ == "__main__":
    main()
