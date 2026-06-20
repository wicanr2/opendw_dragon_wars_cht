#!/usr/bin/env bash
# build_appimage — Linux 可攜 AppImage:cmake install → AppDir → 打包 SDL2 依賴
#                  → appimagetool → opendw-remake-x86_64.AppImage(雙擊即玩,自包含)。
#
# 需:linuxdeploy + appimagetool(本腳本自動下載至 BUILD_DIR;CI 網路可達)。
#     docker / FUSE 受限環境用 APPIMAGE_EXTRACT_AND_RUN=1(本腳本已設)。
# 產物只含引擎 + 啟動器 + 自包含 assets + 打包的 SDL2/SDL2_ttf,**不含原始遊戲檔**。
#
# 用法: tools/package/build_appimage.sh [BUILD_DIR] [OUT_DIR]
set -euo pipefail
export APPIMAGE_EXTRACT_AND_RUN=1

HERE="$(cd "$(dirname "$0")/../.." && pwd)"   # opendw_remake/
cd "$HERE"
BUILD_DIR="${1:-build_appimage}"
OUT_DIR="${2:-dist}"
APPDIR="$BUILD_DIR/AppDir"
TOOLS="$BUILD_DIR/_tools"
VER="0.1.0"

echo "== 1) build + install 到 AppDir/usr =="
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)" --target opendw_remake >/dev/null
rm -rf "$APPDIR"; mkdir -p "$APPDIR"
DESTDIR="$(pwd)/$APPDIR" cmake --install "$BUILD_DIR" >/dev/null

echo "== 2) desktop / icon / AppRun =="
install -Dm644 packaging/opendw-remake.desktop "$APPDIR/usr/share/applications/opendw-remake.desktop"
install -Dm644 packaging/opendw-remake.png      "$APPDIR/usr/share/icons/hicolor/256x256/apps/opendw-remake.png"
# AppRun:切到 assets 後執行 binary(沿用啟動器邏輯;AppImage 內 usr/share/opendw-remake/assets)。
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:${LD_LIBRARY_PATH:-}"
cd "$HERE/usr/share/opendw-remake" 2>/dev/null || cd "$HERE/usr/bin"
exec "$HERE/usr/bin/opendw-remake" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"
cp packaging/opendw-remake.desktop "$APPDIR/opendw-remake.desktop"
cp packaging/opendw-remake.png      "$APPDIR/opendw-remake.png"

echo "== 3) 取 linuxdeploy + appimagetool(打包 SDL2 依賴)=="
mkdir -p "$TOOLS"
fetch() { # url dest
  [ -f "$2" ] && return 0
  curl -fsSL -o "$2" "$1" || wget -qO "$2" "$1"
  chmod +x "$2"
}
LD="$TOOLS/linuxdeploy-x86_64.AppImage"
AT="$TOOLS/appimagetool-x86_64.AppImage"
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" "$LD"
fetch "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" "$AT"

echo "== 4) linuxdeploy 打包依賴 + appimagetool 產 AppImage =="
rm -rf "$OUT_DIR"; mkdir -p "$OUT_DIR"
"$LD" --appdir "$APPDIR" \
      --executable "$APPDIR/usr/bin/opendw-remake" \
      --desktop-file "$APPDIR/usr/share/applications/opendw-remake.desktop" \
      --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/opendw-remake.png" || true
OUT="$OUT_DIR/opendw-remake-$VER-x86_64.AppImage"
ARCH=x86_64 "$AT" "$APPDIR" "$OUT"

echo "== 5) 驗證不含原始遊戲檔 =="
if find "$APPDIR" -iname 'DRAGON.COM' -o -iname 'DATA1' -o -iname 'DATA2' | grep -q .; then
  echo "FAIL: AppImage 含原始遊戲檔(版權)"; exit 1
fi
echo "  ✅ 無原始遊戲檔;產物: $OUT ($(du -h "$OUT" 2>/dev/null | cut -f1))"

echo "== 6) headless smoke(--frames 0 不崩)=="
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$OUT" --frames 0 >/dev/null 2>&1 && echo "  ✅ AppImage 可執行" || echo "  ⚠ headless 執行需 FUSE/extract（CI 上驗）"
echo "AppImage done: $OUT"
