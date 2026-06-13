#!/bin/bash
# smoke_test — app 層整合 smoke:headless 跑遍 main.cpp 各模式,斷言不崩 + dump 確定性。
# lib 級 ctest 不涵蓋 main.cpp 狀態機各入口(S_MENU/S_GAME/S_COMBAT/段落/角色表/存讀檔),
# 此測試守護整合回歸:任一模式入口若崩潰/退碼非 0 即 FAIL,並驗關鍵 dump 兩次執行 byte-stable。
#
# 用法: smoke_test.sh <opendw_remake_binary> <srcdir(含 assets/)>
set -u
BIN="${1:?需要 binary 路徑}"
SRC="${2:?需要 srcdir(含 assets/)}"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
cd "$SRC" || { echo "FAIL: 無法進入 $SRC"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
fail=0
run() {  # <說明> -- <args...>
  local desc="$1"; shift
  "$BIN" --scale 1 --frames 1 "$@" >/dev/null 2>"$TMP/err"
  local rc=$?
  if [ $rc -ne 0 ]; then
    echo "  ❌ [$desc] 退碼 $rc"; sed 's/^/      /' "$TMP/err" | head -3; fail=1
  else
    echo "  ✅ [$desc]"
  fi
}

echo "== 各模式 headless 不崩 =="
run "選單"            --menu assets/i18n/zh-TW/menu.tsv
run "標題場景圖"      --scene 29
run "波卡城地圖 FP"   --map 1 --fp
run "FP 踩事件格"     --map 1 --fp --at 12 6
run "段落檢視器"      --read-para 88
run "段落捲動"        --read-para 88 --para-scroll 1
run "角色表"          --char-sheet 0
run "遭遇畫面"        --encounter 12 --combat-seed 4660
run "日文 FP"         --map 1 --fp --locale ja
run "viewport"        --viewport

echo "== 存讀檔 round-trip =="
"$BIN" --selftest-save >/dev/null 2>"$TMP/err"
if [ $? -eq 0 ]; then echo "  ✅ [存讀檔 selftest]"; else echo "  ❌ [存讀檔 selftest]"; fail=1; fi

echo "== dump 確定性(同輸入兩次 byte-for-byte)=="
det() {  # <說明> -- <args...>
  local desc="$1"; shift
  "$BIN" --scale 1 --frames 1 "$@" --dump "$TMP/a.ppm" >/dev/null 2>&1
  "$BIN" --scale 1 --frames 1 "$@" --dump "$TMP/b.ppm" >/dev/null 2>&1
  if [ -f "$TMP/a.ppm" ] && cmp -s "$TMP/a.ppm" "$TMP/b.ppm"; then
    echo "  ✅ [$desc] 確定性"
  else
    echo "  ❌ [$desc] 非確定性或無輸出"; fail=1
  fi
}
det "標題場景圖"  --scene 29
det "段落 88"     --read-para 88
det "遭遇畫面"    --encounter 12 --combat-seed 4660

echo
if [ $fail -eq 0 ]; then echo "PASS: app 整合 smoke 全綠"; exit 0
else echo "FAIL: 見上"; exit 1; fi
