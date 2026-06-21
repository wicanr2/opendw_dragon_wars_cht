#!/usr/bin/env bash
# game_tester — 老遊戲 remake「正常玩家路徑」實機驗證(非 debug hook、非 --frames 0 空 dump)。
#
#   為什麼:headless CI(--frames 0 dump)會 PASS,但「正常開遊戲玩」可能整個壞掉
#   (真實案例:預設俯視非第一人稱、AppImage 唯讀 cwd 存檔失敗、視窗 logical-size 偏移)。
#   本工具走玩家真的會走的路徑,產出**截圖藝廊**(供目視)+ **機械檢查**(存檔/讀檔/不崩)。
#
#   產物:<OUT>/NN_<scene>.png(截圖)+ report.txt(機械 PASS/FAIL)。
#   目視判讀由人或 agent 看 PNG(是否第一人稱、有無偏移、文字正確、怪物出現…)。
#
#   用法: tools/package/../verify/game_tester.sh [OUT_DIR]   (預設 dist_gametest)
set -uo pipefail
HERE="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$HERE"      # opendw_remake/
OUT="${1:-dist_gametest}"; rm -rf "$OUT"; mkdir -p "$OUT"
BIN=build/opendw_remake
BUNDLE=assets/bundle
PARTY="Aria:1:14,16,10,10/Borin:0:18,9,9,12"
SAVE="$OUT/_save"; rm -rf "$SAVE"; mkdir -p "$SAVE"
REPORT="$OUT/report.txt"; : > "$REPORT"
run(){ DWR_SAVE_DIR="$(pwd)/$SAVE" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$BIN" --bundle "$BUNDLE" --mute "$@" 2>>"$OUT/_run.log"; }
shot(){ python3 -c "from PIL import Image;Image.open('$1').save('$2')" 2>/dev/null && rm -f "$1"; }
pass(){ echo "PASS  $1" | tee -a "$REPORT"; }
fail(){ echo "FAIL  $1" | tee -a "$REPORT"; }
note(){ echo "  ··  $1" | tee -a "$REPORT"; }
[ -x "$BIN" ] || { echo "需先 build opendw_remake"; exit 1; }

echo "== game tester:正常玩家路徑 ==" | tee -a "$REPORT"

# 1) 主選單 / title splash
run --title --frames 3 --dump "$OUT/01_title.ppm" >/dev/null; shot "$OUT/01_title.ppm" "$OUT/01_title.png"
[ -f "$OUT/01_title.png" ] && pass "01 title 畫面有產出(目視:火龍 art)" || fail "01 title 無畫面"

# 2) 建角(配點畫面)
run --newgame-screen "$PARTY" --frames 3 --dump "$OUT/02_chargen.ppm" >/dev/null; shot "$OUT/02_chargen.ppm" "$OUT/02_chargen.png"
[ -f "$OUT/02_chargen.png" ] && pass "02 建角配點畫面有產出(目視:屬性配點)" || fail "02 建角無畫面"

# 3) ★ 進遊戲預設視圖 = 第一人稱 3D(不是俯視彩格)
run --newgame-demo "$PARTY" --frames 2 --dump "$OUT/03_ingame.ppm" >/dev/null; shot "$OUT/03_ingame.ppm" "$OUT/03_ingame.png"
[ -f "$OUT/03_ingame.png" ] && pass "03 進遊戲畫面有產出 ★目視必看:應為第一人稱 3D 走廊,非俯視彩格" || fail "03 進遊戲無畫面"

# 4) 移動(前進/轉向)後視角應改變(★ --dump-frame 抓按鍵之後的幀;plain --dump 會抓到移動前)。
run --newgame-demo "$PARTY" --keys "I,I,I,I,L,I" --frames 12 --dump-frame 9 --dump "$OUT/04_moved.ppm" >/dev/null; shot "$OUT/04_moved.ppm" "$OUT/04_moved.png"
if [ -f "$OUT/04_moved.png" ]; then
  if cmp -s "$OUT/03_ingame.png" "$OUT/04_moved.png"; then fail "04 移動後畫面與起點相同 → 移動可能卡住(需查)"; else pass "04 移動後視角改變(目視:走廊/牆面不同於起點)"; fi
else fail "04 移動後無畫面"; fi

# 5) ★ 存檔(S 鍵)→ 檔案落地(可寫目錄)
rm -f "$SAVE/slot0.sav"
run --newgame-demo "$PARTY" --keys "S" --frames 6 >/dev/null
if [ -s "$SAVE/slot0.sav" ]; then pass "05 S 存檔 → 檔案落地($(wc -c <"$SAVE/slot0.sav") bytes)"; else fail "05 S 存檔無檔案(存檔壞!)"; fi

# 6) ★ 讀檔還原(--load)→ 隊伍/位置回來
LOADLOG=$(run --load "$SAVE/slot0.sav" --frames 3 --dump "$OUT/06_loaded.ppm" 2>&1; grep -h "load applied" "$OUT/_run.log" | tail -1)
shot "$OUT/06_loaded.ppm" "$OUT/06_loaded.png"
if grep -q "load applied" "$OUT/_run.log"; then pass "06 讀檔還原($(grep -h 'load applied' "$OUT/_run.log" | tail -1 | sed 's/.*load applied: //'))"; else fail "06 讀檔未還原"; fi

# 7) 遭遇 / 戰鬥畫面(怪物出現)
run --encounter 3 --combat-seed 1 --frames 3 --dump "$OUT/07_combat.ppm" >/dev/null; shot "$OUT/07_combat.ppm" "$OUT/07_combat.png"
[ -f "$OUT/07_combat.png" ] && pass "07 戰鬥畫面有產出(目視:怪物立繪 + 戰報)" || fail "07 戰鬥無畫面"

# 8) 角色屬性表 / 物品欄(V)
run --newgame-demo "$PARTY" --char-sheet 1 --inventory --frames 3 --dump "$OUT/08_sheet.ppm" >/dev/null; shot "$OUT/08_sheet.ppm" "$OUT/08_sheet.png"
[ -f "$OUT/08_sheet.png" ] && pass "08 角色表/物品欄有產出(目視:屬性 + 背包)" || fail "08 角色表無畫面"

# 9) 結局序列
run --ending --frames 3 --dump "$OUT/09_ending.ppm" >/dev/null; shot "$OUT/09_ending.ppm" "$OUT/09_ending.png"
[ -f "$OUT/09_ending.png" ] && pass "09 結局畫面有產出(目視:結局過場 + 繁中敘事)" || fail "09 結局無畫面"

echo "" | tee -a "$REPORT"
P=$(grep -c '^PASS' "$REPORT"); F=$(grep -c '^FAIL' "$REPORT")
echo "== 機械檢查 $P PASS / $F FAIL ==" | tee -a "$REPORT"
echo "★ 截圖目視判讀(尤其 03 第一人稱、04 視角、07 怪物):$OUT/*.png" | tee -a "$REPORT"
[ "$F" -eq 0 ]
