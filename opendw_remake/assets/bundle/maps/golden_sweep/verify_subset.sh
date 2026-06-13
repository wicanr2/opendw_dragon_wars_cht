#!/usr/bin/env bash
#
# verify_subset.sh — 驗證入庫的 golden_sweep 代表子集 (全 40 關)。
#
# 不重生 golden:直接讀 golden_sweep/*.vpmem 的檔名推出 (facing x y) case,
# 用 remake verify_fp_sweep `verifycases` 對拍。docker dwsdl。
set -euo pipefail

LONGCAT="/home/anr2/tmp/longcat"
REPO="${LONGCAT}/opendw_dragon_wars_cht"
R="${REPO}/opendw_remake"
GOLD="${R}/assets/bundle/maps/golden_sweep"
MAPS="${R}/assets/bundle/maps"
COMP="${R}/assets/bundle/components"
BUILD="${R}/build_verify"

echo "== build verify_fp_sweep (docker dwsdl) =="
docker run --rm -v "${LONGCAT}:${LONGCAT}" -w "${BUILD}" dwsdl bash -c \
  "cmake .. >/dev/null 2>&1 && make verify_fp_sweep >/dev/null 2>&1"

VERIFY="docker run --rm -v ${LONGCAT}:${LONGCAT} -w ${BUILD} dwsdl ${BUILD}/verify_fp_sweep"

# casefile 必須落在 longcat 掛載內,docker 容器才讀得到。
CASEDIR="${LONGCAT}/tmp/golden_sweep_cases"; mkdir -p "${CASEDIR}"

total_pass=0 total_case=0 fail_areas=()
declare -A SUM; for k in quad word neg_x neg_x_alt flip_y; do SUM[$k]=0; done

for area in $(seq 0 39); do
  LVL="${MAPS}/${area}.lvl"
  [ -f "${LVL}" ] || continue
  casefile="${CASEDIR}/${area}.txt"
  ls "${GOLD}/${area}.f"*.vpmem 2>/dev/null \
    | sed -E "s@.*/${area}\.f([0-9]+)\.([0-9]+)_([0-9]+)\.vpmem@\1 \2 \3@" > "${casefile}" || true
  [ -s "${casefile}" ] || { rm -f "${casefile}"; continue; }

  out="$(${VERIFY} verifycases "${LVL}" 0 "${GOLD}" "${COMP}" "${casefile}" || true)"
  rm -f "${casefile}"
  echo "${out}" | grep -E "FAIL" || true
  summary="$(echo "${out}" | grep -E "PASS  \(" || true)"
  echo "${summary}"
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
echo "============ golden_sweep SUBSET TOTAL ============"
echo "PASS ${total_pass}/${total_case}"
echo "decode branches: quad=${SUM[quad]} word=${SUM[word]} neg_x=${SUM[neg_x]} neg_x_alt=${SUM[neg_x_alt]} flip_y=${SUM[flip_y]}"
[ "${#fail_areas[@]}" -gt 0 ] && { echo "FAIL areas: ${fail_areas[*]}"; exit 1; } || echo "FAIL areas: (none)"
