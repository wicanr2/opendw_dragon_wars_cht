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

COMP_DIR="${MAPS%/maps}/components"   # assets/bundle/components(step3 元件 bin)

mkdir -p "${WORK}" "${GOLD_DIR}"
cp "${SCRIPT_DIR}/golden_compose.c" "${WORK}/golden_compose.c"
cp "${SCRIPT_DIR}/golden_select.c"  "${WORK}/golden_select.c"
cp "${SCRIPT_DIR}/golden_pixel.c"   "${WORK}/golden_pixel.c"

echo "== compile (docker dwsdl, gcc) =="
docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${WORK}" dwsdl \
  gcc -O0 -g -std=c11 -Wall -Wextra -o "${WORK}/golden_compose" "${WORK}/golden_compose.c"
docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${WORK}" dwsdl \
  gcc -O0 -g -std=c11 -Wall -Wextra -o "${WORK}/golden_select" "${WORK}/golden_select.c"
docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${WORK}" dwsdl \
  gcc -O0 -g -std=c11 -Wall -Wextra -o "${WORK}/golden_pixel" "${WORK}/golden_pixel.c"

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
  # step1: FOV 取樣 + 牆 nibble (5A56)。
  out="${GOLD_DIR}/${BASE}.f${facing}.${x}_${y}.golden"
  docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${WORK}" dwsdl \
    "${WORK}/golden_compose" "${LVL}" "${facing}" "${x}" "${y}" "${out}"
  echo "---- f${facing} (${x},${y}) [compose] ----"
  cat "${out}"
  # step2: 元件選擇 + 繪製指令序列 (.sel.golden)。
  sel="${GOLD_DIR}/${BASE}.f${facing}.${x}_${y}.sel.golden"
  docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${WORK}" dwsdl \
    "${WORK}/golden_select" "${LVL}" "${facing}" "${x}" "${y}" "${sel}"
  echo "---- f${facing} (${x},${y}) [select] ----"
  cat "${sel}"
  # step3: 完整 refresh_viewport → viewport_memory 10880B (.vpmem)。
  #   需先有 bundle/components/<tag>.bin (用 remake extract_components 抽,見下方註解)。
  vpm="${GOLD_DIR}/${BASE}.f${facing}.${x}_${y}.vpmem"
  if [ -d "${COMP_DIR}" ]; then
    docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${WORK}" dwsdl \
      "${WORK}/golden_pixel" "${LVL}" "${facing}" "${x}" "${y}" "${COMP_DIR}" "${vpm}"
    echo "---- f${facing} (${x},${y}) [pixel] wrote $(basename "${vpm}") ----"
  else
    echo "!! ${COMP_DIR} 不存在,跳過 step3 (.vpmem)。先跑:"
    echo "   build_sdl/extract_components <data_dir> ${COMP_DIR} 1"
  fi
done
echo "== done =="
echo "step3 對拍: build_sdl/verify_fp ${LVL} ${GOLD_DIR} ${COMP_DIR}"
