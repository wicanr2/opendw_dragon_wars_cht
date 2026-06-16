#!/usr/bin/env bash
# build_package — Linux 可攜發佈包:cmake build → install 到 staging → cpack tarball
#                 → 解開 → headless 執行驗證(--frames 0 不崩、退碼 0)。
#
# 在 docker dwsdl 內可完整跑(實際產包 + 解開 + headless 驗證);產物 .tar.gz 只含
# 引擎 binary + 啟動器 + 自包含 assets(bundle/fonts/i18n)+ README,不含原始遊戲檔。
#
# 用法: tools/package/build_package.sh [BUILD_DIR] [OUT_DIR]
#   BUILD_DIR 預設 build_pkg;OUT_DIR 預設 dist/。
set -euo pipefail

HERE="$(cd "$(dirname "$0")/../.." && pwd)"   # opendw_remake/
cd "$HERE"

BUILD_DIR="${1:-build_pkg}"
OUT_DIR="${2:-dist}"
STAGE="$BUILD_DIR/_stage"

echo "== 1) configure + build =="
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)" --target opendw_remake >/dev/null

echo "== 2) cpack 產 tarball =="
rm -rf "$OUT_DIR"; mkdir -p "$OUT_DIR"
( cd "$BUILD_DIR" && cpack -G TGZ -B "$(cd "$HERE/$OUT_DIR" && pwd)" >/dev/null )
TARBALL="$(ls -1 "$OUT_DIR"/opendw-remake-*.tar.gz | head -1)"
[ -n "$TARBALL" ] || { echo "FAIL: 找不到 tarball"; exit 1; }
echo "  tarball: $TARBALL ($(du -h "$TARBALL" | cut -f1))"

echo "== 3) 驗證不含原始遊戲檔 =="
if tar tzf "$TARBALL" | grep -iE 'DRAGON\.COM|DATA1|DATA2' ; then
  echo "FAIL: 發佈包含原始遊戲檔(版權)"; exit 1
fi
echo "  ✅ 無 DRAGON.COM/DATA1/DATA2"

echo "== 4) 解開到 staging =="
rm -rf "$STAGE"; mkdir -p "$STAGE"
tar xzf "$TARBALL" -C "$STAGE"
ROOT="$(ls -d "$STAGE"/opendw-remake-* | head -1)"
echo "  解開根目錄: $ROOT"
echo "  內容:"; ( cd "$ROOT" && find . -maxdepth 3 -type d | sort | sed 's/^/    /' )

echo "== 5) headless 執行驗證(--frames 0,經啟動器 wrapper)=="
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy DWR_MUTE=1
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
"$ROOT/bin/opendw-remake.sh" --frames 0 --scene 29 --dump "$TMP/pkg.ppm" >/dev/null 2>"$TMP/err" || {
  echo "FAIL: 啟動器執行退碼非 0"; sed 's/^/    /' "$TMP/err"; exit 1; }
if head -c 16 "$TMP/pkg.ppm" 2>/dev/null | grep -q '^P6'; then
  dims="$(head -c 32 "$TMP/pkg.ppm" | tr '\n' ' ' | awk '{print $2"x"$3}')"
  echo "  ✅ 啟動器 headless 跑通,dump = $dims"
else
  echo "FAIL: 無 dump 輸出"; sed 's/^/    /' "$TMP/err"; exit 1
fi

echo "== 6) 640×480 模式也驗一次 =="
"$ROOT/bin/opendw-remake.sh" --win640 --frames 0 --scene 29 --dump "$TMP/pkg640.ppm" >/dev/null 2>&1
dims640="$(head -c 32 "$TMP/pkg640.ppm" | tr '\n' ' ' | awk '{print $2"x"$3}')"
[ "$dims640" = "640x480" ] && echo "  ✅ 640×480 = $dims640" || { echo "FAIL: 640 模式尺寸 $dims640"; exit 1; }

echo
echo "PASS: 發佈包產出 + 解開 + headless 驗證全綠"
echo "      $TARBALL"
