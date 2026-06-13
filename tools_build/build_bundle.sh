#!/bin/bash
# 一鍵把 DATA1/DATA2 萃成 remake 自包含 asset bundle(scripts/strings/sprites + manifest)。
# 執行後 remake 只需此 bundle、不依賴原始磁碟檔(見 docs/adr/0001)。
# 用法: bash build_bundle.sh <bundle_out_dir>   (需 /tmp/dwbuild/{data1,data2,dragon.com} 與工具)
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"; OUT=${1:-$REPO/opendw_remake/assets/bundle}
OUT=$(realpath -m "$OUT")   # docker -v 需絕對路徑(相對路徑會被當 named volume 而出錯)
W=/tmp/dwbuild
[ -f "$W/data1" ] || { echo "需 $W/data1(見 SKILL 重建資料)"; exit 1; }
[ -f "$W/data2" ] || echo "警告:無 $W/data2,DATA2-only 資源(部分 sprite)將缺漏"
[ -x "$W/out/resextract" ] || bash "$REPO/tools_build/build_opendw_tools.sh" /home/anr2/tmp/longcat/opendw "$W/out"
mkdir -p "$OUT/scripts" "$OUT/strings" "$OUT/sprites"

# 1) script bytecode + 內嵌字串表(script-bearing sections)
#    含全 40 關事件 script 經 op_58 跨資源 call 載入的 tag 聯集(由 remake
#    extract_eventscripts 掃描得出:0 1 3 5 8 9 10 11 17 19),讓 app run_event
#    的 BundleProvider 不依賴 DATA1 也能跑 op_58。6 = 既有選單用 section。
SECTIONS="0 1 3 5 6 8 9 10 11 17 19"
docker run --rm -v "$REPO/opendw_remake":/app -v "$W":/o -w /app dwtools bash -c \
  'g++ -O1 -w -std=c++20 -Isrc src/resource/archive.cpp src/resource/decompress.cpp src/resource/text_codec.cpp tools/extract/extract_strings.cpp -o /o/extract_strings'
for id in $SECTIONS; do
  docker run --rm -v "$W":/work -w /work dwtools ./out/resextract -i $id -o /work/_s$id.bin >/dev/null 2>&1
  cp "$W/_s$id.bin" "$OUT/scripts/$id.bin"
  docker run --rm -v "$W":/work -v "$OUT":/b -w /work dwtools ./extract_strings /work $id /b/strings/$id.tsv >/dev/null 2>&1
done

# 2) 怪物 sprite(named set)
for spec in "168:wolf" "196:spider" "200:innocent_man" "210:pikeman" "222:fanatic" "152:guard"; do
  id=${spec%%:*}; nm=${spec#*:}
  docker run --rm -v "$W":/work -w /work dwtools ./out/sprite_dump $id /work/_sp$id.ppm >/dev/null 2>&1
  python3 "$REPO/tools_build/extract_sprite.py" "$W/_sp$id.ppm" $id $nm "$OUT" >/dev/null
done

# 3) 頂層 manifest
python3 - "$OUT" "$SECTIONS" <<'PY'
import sys,os,json
out=sys.argv[1]; secs=sys.argv[2].split()
m={'format':'opendw-bundle/1','source':'Dragon Wars (Interplay 1989) DATA1/DATA2',
   'scripts':[f'scripts/{s}.bin' for s in secs],
   'strings':[f'strings/{s}.tsv' for s in secs],
   'sprites':'sprites/manifest.json',
   'note':'remake 執行期讀此 bundle,不需原始磁碟檔。對話改 strings/*.tsv,sprite 改 sprites/*.png(轉回 .spr)。'}
json.dump(m,open(f'{out}/manifest.json','w'),ensure_ascii=False,indent=2)
print('bundle:',sorted(os.listdir(out)))
PY
echo "✅ bundle 建好: $OUT"
