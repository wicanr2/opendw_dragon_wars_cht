#!/usr/bin/env python3
# 極簡 PPM(P6)→ PNG 轉換(僅標準庫 zlib/struct;環境無 PIL/imagemagick 時備用)。
import sys, zlib, struct

def ppm_to_png(src, dst):
    with open(src, 'rb') as f:
        data = f.read()
    assert data[:2] == b'P6', 'only P6 PPM'
    # 解析 header:P6 W H MAXVAL,以空白分隔
    idx = 2
    fields = []
    while len(fields) < 3:
        while idx < len(data) and data[idx] in b' \t\r\n':
            idx += 1
        if idx < len(data) and data[idx:idx+1] == b'#':
            while idx < len(data) and data[idx] not in b'\r\n':
                idx += 1
            continue
        start = idx
        while idx < len(data) and data[idx] not in b' \t\r\n':
            idx += 1
        fields.append(int(data[start:idx]))
    idx += 1  # 跳過 maxval 後的單一空白
    w, h, _ = fields
    pix = data[idx:idx + w * h * 3]
    # 組 PNG:每列前綴 filter byte 0
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += pix[y * w * 3:(y + 1) * w * 3]
    comp = zlib.compress(bytes(raw), 9)

    def chunk(typ, payload):
        c = struct.pack('>I', len(payload)) + typ + payload
        c += struct.pack('>I', zlib.crc32(typ + payload) & 0xffffffff)
        return c

    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', comp)
    png += chunk(b'IEND', b'')
    with open(dst, 'wb') as f:
        f.write(png)

if __name__ == '__main__':
    ppm_to_png(sys.argv[1], sys.argv[2])
    print('wrote', sys.argv[2])
