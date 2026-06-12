#!/bin/bash
# R2 render golden 測試:remake 渲染 res29 標題 vs golden(== opendw 解壓 + 驗證過 title_adjust)。
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"; W=/tmp/dwbuild
[ -f "$W/data1" ] || { echo "需先備好 $W/data1(見 SKILL)"; exit 1; }
[ -x "$W/out/resextract" ] || bash "$REPO/tools_build/build_opendw_tools.sh" /home/anr2/tmp/longcat/opendw "$W/out"
docker run --rm -v "$REPO/opendw_remake":/app -v "$W":/out -w /app dwtools bash -c \
  'g++ -O1 -w -std=c++20 -Isrc src/resource/archive.cpp src/resource/decompress.cpp src/render/picture.cpp tools/verify/render_title.cpp -o /out/render_title' 
docker run --rm -v "$W":/work -w /work dwtools ./render_title /work /work/remake_title.ppm 2>/dev/null
docker run --rm -v "$W":/work -w /work dwtools ./out/resextract -i 29 -o /work/res29.bin >/dev/null 2>&1
python3 - "$W" <<'PY'
import sys; W=sys.argv[1]; buf=bytearray(open(f'{W}/res29.bin','rb').read())
P=[(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),(85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
src=0;dst=0xA0
for _ in range(0x3E30):
    if src+0x9F>=len(buf) or dst+1>=len(buf): break
    ax=buf[src]|(buf[src+1]<<8); src+=2; ax^=buf[src+0x9E]|(buf[src+0x9F]<<8)
    buf[dst]=ax&0xff; buf[dst+1]=(ax>>8)&0xff; dst+=2
f=open(f'{W}/golden_title.ppm','wb'); f.write(b'P6\n320 200\n255\n'); i=0
for y in range(200):
    for x in range(0,320,2):
        b=buf[i] if i<len(buf) else 0; i+=1
        for nib in ((b>>4)&0xF,b&0xF): f.write(bytes(P[nib]))
f.close()
PY
if cmp -s "$W/remake_title.ppm" "$W/golden_title.ppm"; then echo "✅ R2 title golden 通過(remake == golden,byte-for-byte)"; exit 0
else echo "❌ R2 title 不一致"; exit 1; fi
