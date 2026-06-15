#!/usr/bin/env python3
# ppm_to_png — 純 stdlib(zlib)把 P6 PPM 轉 PNG(無 PIL 依賴),供截圖入庫 / 檢視。
import sys, zlib, struct

def read_ppm(path):
    with open(path,'rb') as f: data=f.read()
    assert data[:2]==b'P6'
    idx=2; toks=[]
    while len(toks)<3:
        while idx<len(data) and data[idx:idx+1].isspace(): idx+=1
        if data[idx:idx+1]==b'#':
            while idx<len(data) and data[idx:idx+1]!=b'\n': idx+=1
            continue
        s=idx
        while idx<len(data) and not data[idx:idx+1].isspace(): idx+=1
        toks.append(int(data[s:idx]))
    w,h,_=toks; idx+=1
    return w,h,data[idx:idx+w*h*3]

def write_png(path,w,h,rgb):
    def chunk(t,d): return struct.pack(">I",len(d))+t+d+struct.pack(">I",zlib.crc32(t+d)&0xffffffff)
    raw=bytearray()
    for y in range(h):
        raw.append(0); raw+=rgb[y*w*3:(y+1)*w*3]
    png=b"\x89PNG\r\n\x1a\n"
    png+=chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))
    png+=chunk(b"IDAT",zlib.compress(bytes(raw),9))
    png+=chunk(b"IEND",b"")
    with open(path,'wb') as f: f.write(png)

if __name__=='__main__':
    for p in sys.argv[1:]:
        w,h,rgb=read_ppm(p); o=p.rsplit('.',1)[0]+'.png'; write_png(o,w,h,rgb)
        print(f"{p} -> {o} ({w}x{h})")
