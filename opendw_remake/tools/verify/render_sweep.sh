#!/bin/bash
# render_sweep — 全 40 關第一人稱 viewport 像素對拍(byte-for-byte vs opendw golden)。
# 從「已入庫的 golden_sweep fixture」反推每關 casefile(自然排除 opendw 未實作的 wrap 邊界
# case —— 那些沒有 golden),驅動 verify_fp_sweep verifycases。連同 viewport 模板解碼一起跑。
# 這是 remake 最關鍵的正確性保證(進入遊戲的透視牆面與原版逐像素一致),納入 ctest 守護回歸。
#
# 用法: render_sweep.sh <build_dir> <srcdir(含 assets/)>
set -u
BUILD="${1:?需要 build_dir(含 verify_* 執行檔)}"
SRC="${2:?需要 srcdir(含 assets/)}"
SWEEP="$SRC/assets/bundle/maps/golden_sweep"
COMP="$SRC/assets/bundle/components"
MAPS="$SRC/assets/bundle/maps"
VPDIR="$SRC/assets/bundle/viewport"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

fail=0; total_pass=0; total_fail=0

# 1) viewport 模板解碼對拍(vp*.bin → .vpmem)
if [ -d "$VPDIR" ]; then
  if "$BUILD/verify_viewport" "$VPDIR" >/dev/null 2>&1; then
    echo "  ✅ verify_viewport(模板解碼)"
  else
    echo "  ❌ verify_viewport"; fail=1
  fi
fi

# 2) 全 40 關 fp sweep:逐關從 fixture 反推 casefile
echo "== 40 關 viewport 像素對拍 =="
for lvl in $(seq 0 39); do
  # 收集該關所有 fixture → casefile "facing x y"
  cf="$TMP/case_$lvl.txt"; : > "$cf"
  for f in "$SWEEP"/$lvl.f*.vpmem; do
    [ -e "$f" ] || continue
    b="$(basename "$f" .vpmem)"          # 形如 1.f0.10_10
    fac="${b#*.f}"; fac="${fac%%.*}"     # 0
    xy="${b##*.}"; x="${xy%_*}"; y="${xy#*_}"
    echo "$fac $x $y" >> "$cf"
  done
  n=$(wc -l < "$cf")
  [ "$n" -eq 0 ] && continue             # 無 fixture(wrap 關卡整關略過)
  out=$("$BUILD/verify_fp_sweep" verifycases "$MAPS/$lvl.lvl" 0 "$SWEEP" "$COMP" "$cf" 2>&1)
  # 解析結尾統計(verify_fp_sweep 印 "PASS=.. FAIL=.." 或逐行;以退碼為準 + 抓 FAIL 數)
  rc=$?
  nf=$(printf '%s\n' "$out" | grep -ciE 'FAIL|不符|mismatch')
  if [ $rc -eq 0 ] && [ "$nf" -eq 0 ]; then
    total_pass=$((total_pass + n))
  else
    echo "  ❌ 關 $lvl:$(printf '%s\n' "$out" | grep -iE 'FAIL|不符|mismatch' | head -2)"
    total_fail=$((total_fail + nf)); fail=1
  fi
done

echo
echo "viewport 對拍:PASS=$total_pass FAIL=$total_fail"
if [ $fail -eq 0 ]; then echo "PASS: 全 40 關 viewport 像素 byte-for-byte == opendw"; exit 0
else echo "FAIL: 見上"; exit 1; fi
