#!/bin/bash
# 自包含的「render == 原版」對拍(全 6 張故事場景圖 res 24-29):
#   remake 的 --scene N(decode_fullscreen,title_adjust 去交錯)輸出
#   vs 獨立 Python 重算的 golden(同樣對 bundle 內 N.pic 做 title_adjust)。
# 只需:已入庫的 assets/bundle/scenes/{24..29}.pic + docker image dwsdl + python3。
# 不需原始 DATA1/DATA2(.pic 先前已對拍 opendw byte-for-byte)。
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
RM="$REPO/opendw_remake"
SCENES="24 25 26 27 28 29"

for n in $SCENES; do
  [ -f "$RM/assets/bundle/scenes/$n.pic" ] || { echo "缺 $RM/assets/bundle/scenes/$n.pic"; exit 1; }
done

# 1) remake 渲染(app --scene N --dump);一次 build,逐張 dump。
docker run --rm -v "$RM":/w -w /w dwsdl bash -c "
  cmake -S . -B build_golden -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
  cmake --build build_golden --target opendw_remake -j2 >/dev/null 2>&1
  for n in $SCENES; do
    SDL_VIDEODRIVER=dummy ./build_golden/opendw_remake --scene \$n --scale 1 --frames 1 --dump /w/_scene\$n.ppm >/dev/null 2>&1
  done
  rm -rf build_golden"

# 2) 獨立 Python golden(title_adjust 垂直 XOR delta 去交錯)+ 比對
fail=0
for n in $SCENES; do
  python3 - "$RM/assets/bundle/scenes/$n.pic" "$RM/_golden$n.ppm" <<'PY'
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
  if cmp -s "$RM/_scene$n.ppm" "$RM/_golden$n.ppm"; then
    echo "  ✅ scene $n: render == golden"
  else
    echo "  ❌ scene $n: render != golden"; fail=1
  fi
  rm -f "$RM/_scene$n.ppm" "$RM/_golden$n.ppm"
done

if [ $fail -eq 0 ]; then
  echo "✅ 全 6 張故事場景圖 render == golden(byte-for-byte;片頭/場景圖渲染與原版一致)"; exit 0
else
  echo "❌ 有場景不符"; exit 1
fi
