#!/usr/bin/env bash
#
# build_run.sh — 編譯 golden_compose.c (docker dwsdl) 並對固定 (level,facing,x,y)
# 組合 dump FOV 取樣 + 牆 nibble golden 檔。
#
# golden 落點: opendw_remake/assets/bundle/maps/golden/<base>.f<facing>.<x>_<y>.golden
# 與 remake verify_fov 的命名慣例一致。
set -euo pipefail

REPO="/home/anr2/tmp/longcat/opendw_dragon_wars_cht"
LONGCAT="/home/anr2/tmp/longcat"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="${LONGCAT}/tmp/viewport_compose_golden"
MAPS="${REPO}/opendw_remake/assets/bundle/maps"
GOLD_DIR="${MAPS}/golden"

LVL="${1:-${MAPS}/1.lvl}"          # area 1 = Purgatory
BASE="$(basename "${LVL}" .lvl)"

mkdir -p "${WORK}" "${GOLD_DIR}"
cp "${SCRIPT_DIR}/golden_compose.c" "${WORK}/golden_compose.c"

echo "== compile (docker dwsdl, gcc) =="
docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${WORK}" dwsdl \
  gcc -O0 -g -std=c11 -Wall -Wextra -o "${WORK}/golden_compose" "${WORK}/golden_compose.c"

# 測試組合 (facing x y) — 與 verify_fov.cpp 內 cases[] 一致。
CASES=(
  "0 10 10"
  "1 10 10"
  "2 15 12"
  "3 8 20"
)

echo "== dump goldens for ${BASE} =="
for c in "${CASES[@]}"; do
  read -r facing x y <<<"${c}"
  out="${GOLD_DIR}/${BASE}.f${facing}.${x}_${y}.golden"
  docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${WORK}" dwsdl \
    "${WORK}/golden_compose" "${LVL}" "${facing}" "${x}" "${y}" "${out}"
  echo "---- f${facing} (${x},${y}) ----"
  cat "${out}"
done
echo "== done =="
