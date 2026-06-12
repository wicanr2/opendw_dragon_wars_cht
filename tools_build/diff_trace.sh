#!/bin/bash
# R1 差異測試:remake VM trace vs opendw oracle trace,逐行 diff。
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"; OUT=/tmp/dwbuild/trace; mkdir -p "$OUT"
bash "$REPO/tools_build/build_trace_oracle.sh" /home/anr2/tmp/longcat/opendw "$OUT"
docker run --rm -v "$REPO/opendw_remake":/app -v "$OUT":/out -w /app dwtools bash -c \
  'g++ -O1 -w -std=c++20 -Isrc src/vm/interpreter.cpp tools/verify/trace_remake.cpp -o /out/trace_remake 2>/dev/null && echo "OK trace_remake" || echo FAIL'
docker run --rm -v "$OUT":/t dwtools /t/trace_opendw 2>/dev/null | grep -E '^[0-9a-f]{4} op=' > "$OUT/opendw.txt"
docker run --rm -v "$OUT":/t dwtools /t/trace_remake 2>/dev/null > "$OUT/remake.txt"
if diff "$OUT/opendw.txt" "$OUT/remake.txt" >/dev/null; then
  echo "✅ 差異測試通過 — remake VM == opendw oracle(逐指令一致)"; exit 0
else echo "❌ 分歧:"; diff "$OUT/opendw.txt" "$OUT/remake.txt"; exit 1; fi
