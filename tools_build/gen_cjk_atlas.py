#!/usr/bin/env python3
"""產生 24×24 1-bit 中文點陣 atlas(供 remake render/cjk_font 載入,免 freetype 相依)。
格式: magic "CJK1"(4) + uint32 count + count×{ uint32 codepoint(LE) + 72 bytes glyph }
glyph: 24 列 × 3 byte,每列 MSB-first(x=0 → byte0 bit7)。
字型: 文泉驛 zenhei(GPL)。用法: gen_cjk_atlas.py <out.atlas> <chars...>
"""
import sys, struct
from PIL import Image, ImageDraw, ImageFont
FONT='/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc'
def glyph(ch, font):
    im=Image.new('L',(24,24),0); d=ImageDraw.Draw(im)
    bb=d.textbbox((0,0),ch,font=font); w=bb[2]-bb[0]; h=bb[3]-bb[1]
    d.text(((24-w)//2-bb[0],(24-h)//2-bb[1]),ch,font=font,fill=255)
    out=bytearray()
    for y in range(24):
        for bx in range(3):
            b=0
            for bit in range(8):
                x=bx*8+bit
                if im.getpixel((x,y))>=128: b|=(0x80>>bit)
            out.append(b)
    return bytes(out)
def main():
    out=sys.argv[1]; chars=''.join(sys.argv[2:])
    seen=[]; 
    for c in chars:
        if c not in seen and not c.isspace(): seen.append(c)
    font=ImageFont.truetype(FONT,22)
    blob=struct.pack('<4sI',b'CJK1',len(seen))
    for c in seen:
        blob+=struct.pack('<I',ord(c))+glyph(c,font)
    open(out,'wb').write(blob)
    print(f"atlas: {len(seen)} glyphs -> {out} ({len(blob)} bytes)")
if __name__=='__main__': main()
