#!/usr/bin/env python3
"""X68000 Dragon Wars .PKH 解壓器。

逆向自 DRAGON.X 解碼核心 0x3207c (file 0x3407c):
    decode(out, in, in_len):
      0x445d6 = out_ptr (寫 16-bit word/pixel)
      0x445da = in_ptr
      0x445de = in_ptr + in_len
      loop:
        ctrl = *in_ptr++           # control byte
        count = ctrl & 0x7F        # 低 7 bit 長度, 存 0x445d2
        if ctrl & 0x80:  run()     # 0x32ccc: 讀1 byte, 重複 count 次 → word
        else:            literal()  # 0x32cfe: 讀 byte, 拆 hi/lo nibble → word 各一
      until in_ptr >= in_end

literal (0x32cfe) 精確語意:
   count = 0x445d2 (byte)
   loop:
     b = *in++
     *out++ = (b >> 4)        # hi nibble → word
     count--; if 0: break
     *out++ = (b & 0x0F)      # lo nibble → word  (原碼 movew %d1, d1=整 byte; 但作為 4bpp index 取低 nibble)
     count--; if !=0 continue
run (0x32ccc):
   count = 0x445d2
   b = *in++
   repeat count: *out++ = (word) b
"""
import sys


def decode(data, lo_mask=0x0F):
    out = []          # list of 16-bit pixel values
    i = 0
    n = len(data)
    while i < n:
        ctrl = data[i]; i += 1
        count = ctrl & 0x7F
        if count == 0:
            # count==0：原碼 subqb 後立即 cmpb #0 → 不寫；防呆跳過
            continue
        if ctrl & 0x80:
            # run：讀 1 byte，重複 count 次
            if i >= n:
                break
            b = data[i]; i += 1
            out.extend([b] * count)
        else:
            # literal：每 byte 拆兩 nibble
            c = count
            while c > 0:
                if i >= n:
                    break
                b = data[i]; i += 1
                out.append(b >> 4)
                c -= 1
                if c == 0:
                    break
                out.append(b & lo_mask)
                c -= 1
    return out


def main():
    src = sys.argv[1]
    dst = sys.argv[2]
    data = open(src, "rb").read()
    px = decode(data)
    # 寫成 byte/pixel（4bpp index, 0..15）方便檢視
    with open(dst, "wb") as f:
        f.write(bytes(p & 0xFF for p in px))
    print(f"{src}: in={len(data)} out_pixels={len(px)} -> {dst}")
    # 統計輸出尺寸推測
    import collections
    c = collections.Counter(p & 0x0F for p in px)
    print("nibble histogram:", dict(sorted(c.items())))
    for w in (256, 512, 320, 384, 768):
        if px and len(px) % w == 0:
            print(f"  {w} x {len(px)//w}")


if __name__ == "__main__":
    main()
