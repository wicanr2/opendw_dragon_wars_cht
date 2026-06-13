#!/bin/bash
# 自包含的「render == 原版」對拍:
#   remake 的 --scene 29(decode_fullscreen,title_adjust 去交錯)輸出
#   vs 獨立 Python 重算的 golden(同樣對 bundle 內 29.pic 做 title_adjust)。
# 只需:已入庫的 assets/bundle/scenes/29.pic + docker image dwsdl + python3。
# 不需原始 DATA1/DATA2(.pic 先前已對拍 opendw byte-for-byte)。
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
RM="$REPO/opendw_remake"
PIC="$RM/assets/bundle/scenes/29.pic"
[ -f "$PIC" ] || { echo "缺 $PIC"; exit 1; }

# 1) remake 渲染(app --scene 29 --dump)
docker run --rm -v "$RM":/w -w /w dwsdl bash -c '
  cmake -S . -B build_golden -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
  cmake --build build_golden --target opendw_remake -j2 >/dev/null 2>&1
  SDL_VIDEODRIVER=dummy ./build_golden/opendw_remake --scene 29 --frames 1 --dump /w/_scene29.ppm >/dev/null 2>&1
  rm -rf build_golden'

# 2) 獨立 Python golden(title_adjust 垂直 XOR delta 去交錯)
python3 - "$PIC" "$RM/_golden29.ppm" <<'PY'
import sys
buf=bytearray(open(sys.argv[1],'rb').read())
P=[(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
   (85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
src=0;dst=0xA0
for _ in range(0x3E30):
    if src+0x9F>=len(buf) or dst+1>=len(buf): break
    ax=buf[src]|(buf[src+1]<<8); src+=2; ax^=buf[src+0x9E]|(buf[src+0x9F]<<8)
    buf[dst]=ax&0xff; buf[dst+1]=(ax>>8)&0xff; dst+=2
f=open(sys.argv[2],'wb'); f.write(b'P6\n320 200\n255\n'); i=0
for y in range(200):
    for x in range(0,320,2):
        b=buf[i] if i<len(buf) else 0; i+=1
        for nib in ((b>>4)&0xF,b&0xF): f.write(bytes(P[nib]))
f.close()
PY

# 3) 比對
if cmp -s "$RM/_scene29.ppm" "$RM/_golden29.ppm"; then
  echo "✅ render == golden(byte-for-byte;片頭/場景圖渲染與原版一致)"
  rm -f "$RM/_scene29.ppm" "$RM/_golden29.ppm"; exit 0
else
  echo "❌ render != golden"; exit 1
fi
