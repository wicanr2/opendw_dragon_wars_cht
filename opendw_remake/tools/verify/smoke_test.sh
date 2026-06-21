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
run "物品欄(空)"     --map 1 --char-sheet 1
run "物品欄(樣本)"   --map 1 --char-sheet 1 --inventory
run "遭遇畫面"        --encounter 12 --combat-seed 4660
run "俯視地圖(fog)"  --map 1 --automap 1
run "俯視地圖(探索)" --automap 1 --mm-seed 2
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

echo "== 640×480 視窗模式(letterbox;docs/assessment/47 方案 3)=="
# 視窗確為 640×480、像素層 320×200 ×2 垂直置中(上下各 40px 黑邊 letterbox)。
# 640 模式像素層固定 ×2,不吃 --scale;仍可 --frames 1 限制幀數。
"$BIN" --win640 --frames 1 --scene 29 --dump "$TMP/w640.ppm" >/dev/null 2>&1
if head -c 32 "$TMP/w640.ppm" 2>/dev/null | grep -q "^P6"; then
  dims="$(head -c 32 "$TMP/w640.ppm" | tr '\n' ' ' | awk '{print $2"x"$3}')"
  if [ "$dims" = "640x480" ]; then
    echo "  ✅ [640×480] 視窗尺寸 = $dims"
  else
    echo "  ❌ [640×480] 視窗尺寸 = $dims(預期 640x480)"; fail=1
  fi
  # letterbox 對齊保證:上 40 列 + 下 40 列應全黑(像素層 ×2=400 垂直置中於 480)。
  # 任何非黑像素表示像素層溢出黑邊 / 對齊漂移。
  if python3 - "$TMP/w640.ppm" <<'PY'
import sys
f=open(sys.argv[1],"rb"); assert f.readline().strip()==b"P6"
w,h=map(int,f.readline().split()); f.readline(); d=f.read(w*h*3)
def nonblack(y): return any(b!=0 for b in d[y*w*3:(y+1)*w*3])
bad=[y for y in list(range(40))+list(range(440,480)) if nonblack(y)]
sys.exit(1 if bad else 0)
PY
  then
    echo "  ✅ [640×480] letterbox 上下黑邊純黑(對齊無漂移)"
  else
    echo "  ❌ [640×480] letterbox 黑邊有非黑像素(像素層溢出 / 對齊漂移)"; fail=1
  fi
else
  echo "  ❌ [640×480] 無 dump 輸出"; fail=1
fi

echo "== 全域熱鍵 / 離開確認流程(F1 Help / F8 主題 / F10·ESC 離開確認)=="
# 跑 --keys 注入序列,斷言 stderr 含預期 log(行為驗證,非畫面對拍)。
expect() {  # <說明> <stderr 應含字串> -- <args...>
  local desc="$1"; local needle="$2"; shift 2
  "$BIN" --scale 1 "$@" >/dev/null 2>"$TMP/e"
  if grep -qF "$needle" "$TMP/e"; then echo "  ✅ [$desc]"
  else echo "  ❌ [$desc] stderr 缺 \"$needle\""; sed 's/^/      /' "$TMP/e" | tail -4; fail=1; fi
}
# F8:循環切換主題(目前 1 個 → wrap 回 dos)。
expect "F8 主題循環"      "theme: cycle"                    --no-splash --frames 3 --keys F8
# F1:開 Help 覆蓋層 → ESC 關閉。
expect "F1 Help 開啟"     "help: open (F1)"                 --map 1 --fp --frames 4 --keys F1
expect "Help ESC 關閉"    "help: close"                     --map 1 --fp --frames 4 --keys F1,ESC
# F10:自動存檔 → 離開確認視窗。
expect "F10 自動存檔"     "request-quit (F10): autosaved=1" --map 1 --fp --frames 4 --keys F10
# 確認 Y → 離開;N → 取消回遊戲。
expect "確認 Y 離開"      "confirm-quit: Y → quit"          --map 1 --fp --frames 4 --keys F10,Y
expect "確認 N 取消"      "confirm-quit: N → cancel"        --map 1 --fp --frames 4 --keys F10,N
# 主選單 ESC **不**觸發離開(使用者要求:選單離開只留 F10;ESC 在選單為 no-op)。
"$BIN" --no-splash --frames 4 --keys ESC >/dev/null 2>"$TMP/e"
if grep -qF "request-quit" "$TMP/e"; then
  echo "  ❌ [主選單 ESC 不離開] 不應觸發離開確認"; fail=1
else echo "  ✅ [主選單 ESC 不離開]"; fi
# F10 在主選單 → 離開確認(選單離開鍵)。
expect "主選單 F10 離開"   "request-quit (F10)"              --no-splash --frames 4 --keys F10
# 子畫面 ESC 隔離:menu_mode S_GAME 且角色表開啟(--char-sheet 自動進 S_GAME),
#   ESC 應關角色表、不觸發頂層離開確認(stderr 不應有 request-quit)。
"$BIN" --scale 1 --char-sheet 1 --frames 4 --keys ESC >/dev/null 2>"$TMP/e"
if grep -qF "request-quit" "$TMP/e"; then
  echo "  ❌ [子畫面 ESC 隔離] 不應觸發離開確認"; fail=1
else echo "  ✅ [子畫面 ESC 隔離]"; fi

echo
if [ $fail -eq 0 ]; then echo "PASS: app 整合 smoke 全綠"; exit 0
else echo "FAIL: 見上"; exit 1; fi
