#!/usr/bin/env bash
#
# build_run.sh - compile golden_decode.c with the dwsdl docker image (g++)
# and dump golden viewport_memory for vp0 / vp2 / data6820 templates.
#
# Usage: ./build_run.sh
#
# Globals used for the golden dump (these MUST match the remake side when
# diffing): xpos=0 ypos=0 byte_104E=0 word_1053=0x50 fill=0x00
#
set -euo pipefail

# Resolve repo paths (absolute).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="/home/anr2/tmp/longcat/opendw_dragon_wars_cht"
VP_DIR="${REPO_ROOT}/opendw_remake/assets/bundle/viewport"
LONGCAT="/home/anr2/tmp/longcat"   # docker mount (writable)
WORK="${LONGCAT}/tmp/viewport_golden"  # scratch under the mounted path

# Golden globals (keep in sync with remake port).
XPOS=0
YPOS=0
BYTE_104E=0
WORD_1053=0x50
FILL=0x00

mkdir -p "${WORK}"
cp "${SCRIPT_DIR}/golden_decode.c" "${WORK}/golden_decode.c"

echo "== compiling with docker dwsdl (g++) =="
docker run --rm \
  -v "${LONGCAT}:${LONGCAT}" \
  -w "${WORK}" \
  dwsdl \
  g++ -O0 -g -std=c++17 -Wall -o "${WORK}/golden_decode" "${WORK}/golden_decode.c"

echo "== running templates =="
for tmpl in vp0 vp2 data6820; do
  src="${VP_DIR}/${tmpl}.bin"
  out="${VP_DIR}/${tmpl}.bin.vpmem"
  echo "---- ${tmpl} ----"
  docker run --rm \
    -v "${LONGCAT}:${LONGCAT}" \
    -w "${WORK}" \
    dwsdl \
    "${WORK}/golden_decode" "${src}" "${out}" \
    "${XPOS}" "${YPOS}" "${BYTE_104E}" "${WORD_1053}" "${FILL}"
  echo "    -> ${out} ($(stat -c%s "${out}") bytes)"
done

echo "== done =="
