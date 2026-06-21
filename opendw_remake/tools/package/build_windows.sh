#!/usr/bin/env bash
# build_windows — Windows x64 可攜包(mingw-w64 交叉編譯;在 docker/Linux 內完成)。
#   流程:裝 mingw-w64 → 下載 SDL2 / SDL2_ttf mingw devel → CMake toolchain 交叉編譯
#         → 收 opendw-remake.exe + 所有執行期 DLL + 自包含 assets → zip。
#   產物只含引擎 + DLL + assets,**不含原始遊戲檔**。
#   需網路(github releases / libsdl)。用法: tools/package/build_windows.sh [BUILD_DIR] [OUT_DIR]
set -euo pipefail

HERE="$(cd "$(dirname "$0")/../.." && pwd)"   # opendw_remake/
cd "$HERE"
BUILD_DIR="${1:-build_win}"
OUT_DIR="${2:-dist}"
VER="0.1.0"
SDL2_VER="2.30.9"
TTF_VER="2.22.0"
PKG="opendw-remake-${VER}-windows-x64"
STAGE="$BUILD_DIR/$PKG"
DL="${BUILD_DIR}_dl"          # 下載放 BUILD_DIR 之外,清 cmake cache 時不丟
TRIPLET="x86_64-w64-mingw32"
# 切換工具鏈(win32↔posix)時 CMake cache 會卡舊編譯器 → 每次清 cmake 產物(保留 _dl)。
rm -rf "$BUILD_DIR"

# 非 root(CI runner)時用 sudo 跑 apt / 寫 /usr/local;docker(root)則為空。
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO=sudo

echo "== 0) 工具鏈(mingw-w64)=="
if ! command -v ${TRIPLET}-g++ >/dev/null; then
  $SUDO apt-get update >/dev/null 2>&1
  $SUDO apt-get install -y mingw-w64 wget unzip zip ca-certificates file >/dev/null 2>&1
fi
command -v file >/dev/null || { $SUDO apt-get update >/dev/null 2>&1; $SUDO apt-get install -y file >/dev/null 2>&1; }

echo "== 1) 下載 SDL2 / SDL2_ttf mingw devel =="
mkdir -p "$DL"
fetch() { [ -f "$DL/$2" ] || wget -qO "$DL/$2" "$1"; }
fetch "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VER}/SDL2-devel-${SDL2_VER}-mingw.tar.gz" sdl2.tgz
fetch "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${TTF_VER}/SDL2_ttf-devel-${TTF_VER}-mingw.tar.gz" ttf.tgz
rm -rf "$DL/sdl2" "$DL/ttf"; mkdir -p "$DL/sdl2" "$DL/ttf"
tar xzf "$DL/sdl2.tgz" -C "$DL/sdl2" --strip-components=1
tar xzf "$DL/ttf.tgz"  -C "$DL/ttf"  --strip-components=1
# mingw .pc 用硬編碼 prefix=/usr/local/${TRIPLET}(非相對)→ 必須裝到該預期位置才能解析。
SDL_PREFIX="/usr/local/$TRIPLET"
$SUDO mkdir -p "$SDL_PREFIX"
$SUDO cp -r "$DL/sdl2/$TRIPLET/." "$SDL_PREFIX/"
$SUDO cp -r "$DL/ttf/$TRIPLET/."  "$SDL_PREFIX/"
SDL2_ROOT="$SDL_PREFIX"
TTF_ROOT="$SDL_PREFIX"

echo "== 2) CMake toolchain(mingw)=="
# mingw posix thread model(win32 model 無 std::mutex/std::thread);winpthread 後段靜態鏈。
CC_POSIX="${TRIPLET}-gcc-posix"; CXX_POSIX="${TRIPLET}-g++-posix"
command -v "$CXX_POSIX" >/dev/null || { CC_POSIX="${TRIPLET}-gcc"; CXX_POSIX="${TRIPLET}-g++"; }
mkdir -p "$BUILD_DIR"
cat > "$BUILD_DIR/mingw.cmake" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER   ${CC_POSIX})
set(CMAKE_CXX_COMPILER ${CXX_POSIX})
set(CMAKE_RC_COMPILER  ${TRIPLET}-windres)
set(CMAKE_FIND_ROOT_PATH /usr/${TRIPLET} ${SDL2_ROOT} ${TTF_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

echo "== 3) configure + build(交叉編譯)=="
# SDL2_ttf 走 pkg-config:指向 mingw 的 .pc(pkg_check_modules 在 configure 期跑 → 須先設)。
export PKG_CONFIG_PATH="$TTF_ROOT/lib/pkgconfig:$SDL2_ROOT/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$TTF_ROOT/lib/pkgconfig:$SDL2_ROOT/lib/pkgconfig"
# 靜態鏈 gcc/stdc++/winpthread → 不必另帶 mingw runtime DLL。
cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$BUILD_DIR/mingw.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDL2_DIR="$SDL2_ROOT/lib/cmake/SDL2" \
  -DCMAKE_PREFIX_PATH="$SDL2_ROOT;$TTF_ROOT" \
  -DCMAKE_EXE_LINKER_FLAGS="-L$SDL_PREFIX/lib -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic" \
  >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)" --target opendw_remake >/dev/null
EXE="$BUILD_DIR/opendw_remake.exe"
[ -f "$EXE" ] || EXE="$(find "$BUILD_DIR" -maxdepth 2 -name 'opendw_remake*.exe' | head -1)"
[ -f "$EXE" ] || { echo "FAIL: 找不到 opendw_remake.exe"; exit 1; }

echo "== 4) 收 staging(exe + DLL + assets)=="
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp "$EXE" "$STAGE/opendw-remake.exe"
# 執行期 DLL:SDL2 + SDL2_ttf bin/ 下全部(含 freetype/zlib 等相依)。
find "$SDL2_ROOT/bin" "$TTF_ROOT/bin" -maxdepth 1 -name '*.dll' -exec cp {} "$STAGE/" \;
# mingw posix runtime:exe 動態相依 libwinpthread-1.dll(posix thread model),須一併打包。
for d in $(x86_64-w64-mingw32-g++-posix -print-search-dirs 2>/dev/null | sed -n 's/^libraries: =\?//p' | tr ':' ' ') \
         /usr/lib/gcc/$TRIPLET/*/ /usr/$TRIPLET/lib/ /usr/lib/$TRIPLET/; do
  [ -f "$d/libwinpthread-1.dll" ] && { cp "$d/libwinpthread-1.dll" "$STAGE/"; break; }
done
[ -f "$STAGE/libwinpthread-1.dll" ] || { \
  WP="$(find /usr -name 'libwinpthread-1.dll' 2>/dev/null | grep -i "$TRIPLET" | head -1)"; \
  [ -n "$WP" ] && cp "$WP" "$STAGE/"; }
cp -r assets "$STAGE/assets"
cp README.md "$STAGE/README.md" 2>/dev/null || true
cat > "$STAGE/PLAY.txt" <<'TXT'
火龍之戰 Dragon Wars — 繁體中文重製 (Windows x64)
直接執行 opendw-remake.exe。預設繁體中文,遊戲中 F4 切 繁中 / EN / 日。
本包自包含 assets,不需原始遊戲檔。字型:若系統無 CJK 字型,可設環境變數 DWR_FONT 指向 .ttf。
TXT

echo "== 5) 驗證(PE32+ + DLL + 無原始遊戲檔)=="
file "$STAGE/opendw-remake.exe" | grep -q "PE32+" && echo "  ✅ PE32+ x86-64 可執行檔" || { echo "FAIL: 非 PE32+"; exit 1; }
echo "  DLL: $(cd "$STAGE" && ls *.dll | tr '\n' ' ')"
# 相依完整性:exe import 的每個非系統 DLL 都須在包內(否則 Windows/wine 起不來)。
MISS=""
for dll in $(x86_64-w64-mingw32-objdump -x "$STAGE/opendw-remake.exe" 2>/dev/null \
             | sed -n 's/.*DLL Name: //p' | grep -ivE "^(KERNEL32|msvcrt|USER32|GDI32|ADVAPI32|SHELL32|ole32|WINMM|IMM32|VERSION|SETUPAPI|OLEAUT32|ws2_32|RPCRT4|gdi32|user32)\.dll$"); do
  [ -f "$STAGE/$dll" ] || MISS="$MISS $dll"
done
[ -z "$MISS" ] && echo "  ✅ exe 相依的非系統 DLL 全在包內" || { echo "FAIL: 缺 DLL:$MISS"; exit 1; }
if find "$STAGE" -iname 'DRAGON.COM' -o -iname 'DATA1' -o -iname 'DATA2' | grep -q .; then
  echo "FAIL: 含原始遊戲檔"; exit 1; fi
echo "  ✅ 無原始遊戲檔"

echo "== 6) wine smoke(WIN_SMOKE=1 才跑;裝 wine+xvfb 實機驗證)=="
# smoke 為 bonus 驗證(結構已在 step 5 驗過,該層才是 gate)。整段 best-effort:
#   wine/xvfb 在某些環境(套件名變動、無顯示)裝不起或跑不動,絕不因此讓打包失敗。
if [ "${WIN_SMOKE:-0}" = "1" ]; then
  ( set +e
    $SUDO dpkg --add-architecture i386 >/dev/null 2>&1
    $SUDO apt-get update >/dev/null 2>&1
    # ubuntu 24.04 套件名為 wine(含 64-bit);舊版 wine64。兩者都試。
    $SUDO apt-get install -y wine xvfb >/dev/null 2>&1 || $SUDO apt-get install -y wine64 xvfb >/dev/null 2>&1
    WBIN="$(command -v wine64 || command -v wine)"
    if [ -n "$WBIN" ]; then
      export WINEPREFIX=/tmp/dwr_wine WINEDEBUG=-all
      "$WBIN" wineboot --init >/dev/null 2>&1; sleep 2
      ( cd "$STAGE" && xvfb-run -a "$WBIN" opendw-remake.exe --frames 0 --dump win_smoke.ppm >/tmp/winsmoke.log 2>&1 )
      if [ -f "$STAGE/win_smoke.ppm" ] && grep -q "quest grants:" /tmp/winsmoke.log; then
        echo "  ✅ wine+xvfb headless 跑通(i18n/段落/quest 載入 + 畫面 dump)"; rm -f "$STAGE/win_smoke.ppm"
      else
        echo "  ⚠ wine smoke 未產出 dump(exe 結構與相依已驗;見 /tmp/winsmoke.log)"
      fi
    else
      echo "  ⚠ wine 裝不起來,略過(結構已驗)"
    fi
  ) || echo "  ⚠ wine smoke 區段出錯,略過(best-effort;結構已驗)"
else
  echo "  (WIN_SMOKE!=1,略過 wine 執行;PE32+/DLL 相依/無原始檔已驗。手動:WIN_SMOKE=1 重跑)"
fi

echo "== 7) zip =="
mkdir -p "$OUT_DIR"
( cd "$BUILD_DIR" && zip -qr "$(cd "$HERE/$OUT_DIR" && pwd)/$PKG.zip" "$PKG" )
echo "PASS: Windows 包產出 $OUT_DIR/$PKG.zip ($(du -h "$OUT_DIR/$PKG.zip" | cut -f1))"
