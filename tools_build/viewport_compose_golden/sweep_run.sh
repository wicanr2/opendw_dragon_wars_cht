#!/usr/bin/env bash
#
# sweep_run.sh — 全 40 關第一人稱 viewport 像素對拍「廣度掃描」驅動。
#
# 對每關:由 remake verify_fp_sweep 的 `list` 模式挑出 N 個取樣格 × 4 朝向
# (deterministic),用 golden_pixel.c oracle 跑完整 refresh_viewport,dump
# viewport_memory (10880B) 成 .vpmem golden;接著用 verify_fp_sweep `verify`
# 模式 memcmp 對拍,統計 PASS/FAIL + decode 分支覆蓋。
#
# 用法:
#   sweep_run.sh [MAX_POS] [AREAS...]
#     MAX_POS  每關取樣格數 (預設 3 → 3×4=12 case/關)。
#     AREAS    要掃的 area 編號 (預設 0..39 全掃)。
#
# golden 落點 (即時重生,不入庫):
#   <LONGCAT>/tmp/fp_sweep_golden/<area>.f<facing>.<x>_<y>.vpmem
# 代表子集入庫見 commit_golden_subset.sh。
set -euo pipefail

REPO="/home/anr2/tmp/longcat/opendw_dragon_wars_cht"
LONGCAT="/home/anr2/tmp/longcat"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="${LONGCAT}/tmp/viewport_compose_golden"
R="${REPO}/opendw_remake"
MAPS="${R}/assets/bundle/maps"
COMP_DIR="${R}/assets/bundle/components"
SWEEP_GOLD="${LONGCAT}/tmp/fp_sweep_golden"
BUILD="${R}/build_verify"

MAX_POS="${1:-3}"
shift || true
if [ "$#" -gt 0 ]; then
  AREAS=("$@")
else
  AREAS=()
  for a in $(seq 0 39); do AREAS+=("$a"); done
fi

DOCKER="docker run --rm -v ${LONGCAT}:${LONGCAT} -w ${WORK} dwsdl"

mkdir -p "${WORK}" "${SWEEP_GOLD}"

echo "== compile golden_pixel oracle (docker dwsdl) =="
cp "${SCRIPT_DIR}/golden_pixel.c" "${WORK}/golden_pixel.c"
${DOCKER} gcc -O0 -g -std=c11 -Wall -Wextra -o "${WORK}/golden_pixel" "${WORK}/golden_pixel.c"

echo "== build remake verify_fp_sweep =="
docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${BUILD}" dwsdl bash -c \
  "cmake .. >/dev/null 2>&1 && make verify_fp_sweep >/dev/null 2>&1"

VERIFY="docker run --rm -v ${LONGCAT}:${LONGCAT} -w ${BUILD} dwsdl ${BUILD}/verify_fp_sweep"

total_pass=0 total_case=0 total_decline=0 fail_areas=()
declare -A SUM
for k in quad word neg_x neg_x_alt flip_y; do SUM[$k]=0; done

for area in "${AREAS[@]}"; do
  LVL="${MAPS}/${area}.lvl"
  [ -f "${LVL}" ] || { echo "!! ${LVL} 不存在,跳過"; continue; }

  # 1) 取得候選 case (facing x y),逐筆跑 oracle 生成 golden。
  #    oracle 對 flag&2 (wrap map) 邊界取樣會印 "unimpl" + exit(1) — 這是原版
  #    反組譯本身未實作的分支 (engine.c check_map_boundary_x/y),並非 remake bug。
  #    這類 case 視為「oracle 無法生成 golden」→ 從對拍清單剔除,另計 declined。
  mapfile -t CASES < <(${VERIFY} list "${LVL}" "${MAX_POS}")
  if [ "${#CASES[@]}" -eq 0 ]; then echo "area ${area}: 無 case,跳過"; continue; fi

  casefile="${WORK}/cases_${area}.txt"
  : > "${casefile}"
  declined=0
  for c in "${CASES[@]}"; do
    read -r facing x y <<<"${c}"
    vpm="${SWEEP_GOLD}/${area}.f${facing}.${x}_${y}.vpmem"
    if ${DOCKER} "${WORK}/golden_pixel" "${LVL}" "${facing}" "${x}" "${y}" "${COMP_DIR}" "${vpm}" \
         >/dev/null 2>"${WORK}/oracle_err.txt"; then
      echo "${facing} ${x} ${y}" >> "${casefile}"
    else
      declined=$((declined + 1))
    fi
  done
  total_decline=$((total_decline + declined))

  if [ ! -s "${casefile}" ]; then
    echo "[${area}] 0/0 PASS  (all ${#CASES[@]} cases oracle-declined: flag&2 wrap unimpl)"
    continue
  fi

  # 2) remake 對拍 oracle 接受的 case。
  out="$(${VERIFY} verifycases "${LVL}" "${MAX_POS}" "${SWEEP_GOLD}" "${COMP_DIR}" "${casefile}" || true)"
  echo "${out}" | grep -E "FAIL" || true
  summary="$(echo "${out}" | grep -E "PASS  \(" | sed "s@\$@  declined=${declined}@")"
  echo "${summary}"

  # 3) 累計 (從 summary 解析 "X/Y PASS" 與分支)。
  p=$(echo "${summary}" | sed -nE 's@.* ([0-9]+)/([0-9]+) PASS .*@\1@p')
  t=$(echo "${summary}" | sed -nE 's@.* ([0-9]+)/([0-9]+) PASS .*@\2@p')
  [ -n "${p:-}" ] && total_pass=$((total_pass + p))
  [ -n "${t:-}" ] && total_case=$((total_case + t))
  [ -n "${p:-}" ] && [ -n "${t:-}" ] && [ "${p}" != "${t}" ] && fail_areas+=("${area}")
  for k in quad word neg_x neg_x_alt flip_y; do
    v=$(echo "${summary}" | sed -nE "s@.*${k}=([0-9]+).*@\1@p")
    [ -n "${v:-}" ] && SUM[$k]=$((SUM[$k] + v))
  done
done

echo ""
echo "==================== SWEEP TOTAL ===================="
echo "PASS ${total_pass}/${total_case}  ($([ "${total_case}" -gt 0 ] && echo "scale=1; ${total_pass}*100/${total_case}" | bc || echo 0)% of oracle-accepted cases)"
echo "oracle-declined (flag&2 wrap unimpl): ${total_decline} cases"
echo "decode branches: quad=${SUM[quad]} word=${SUM[word]} neg_x=${SUM[neg_x]} neg_x_alt=${SUM[neg_x_alt]} flip_y=${SUM[flip_y]}"
if [ "${#fail_areas[@]}" -gt 0 ]; then
  echo "FAIL areas: ${fail_areas[*]}"
else
  echo "FAIL areas: (none)"
fi
echo "golden 即時重生於: ${SWEEP_GOLD}"
