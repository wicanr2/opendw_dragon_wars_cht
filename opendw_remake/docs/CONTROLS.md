# OpenDW Remake — 操作規範(與原版說明書一致)

> 來源:臺灣中文版《火龍之戰》操作手冊(轉寫見 `../../docs/33_MANUAL_TRANSCRIPTION.md`)。
> remake 的輸入處理**以本表為準**,確保操作與原版說明書一致。

## 選單 / 角色

| 鍵 | 動作 | 出處 |
|---|---|---|
| `B` | 開始新遊戲(Begin;會摧毀已存檔)/ 以預設四人開始 | 手冊 147/149 |
| `C` | 繼續舊遊戲(Continue 已存檔)/ 創造人物(隊員 ≤3 時) | 手冊 149/153 |
| `D` | 刪除人物(先按人物號碼,再 `D`) | 手冊 147 |
| `R` | 改人物名字 | 手冊 147 |
| `V` | 查看人物特質 | 手冊 147 |
| `Esc` | 看完中央訊息後繼續 / 離開子畫面 | 手冊 139/167 |

> 選單採**快捷字母**(highlighted letter),非方向鍵選取。remake 額外提供 ↑↓ 移動游標 + Enter 作為現代輔助,但快捷字母為主、與手冊一致。

## 移動(遊戲景觀畫面)

| 鍵 | 動作 | 出處 |
|---|---|---|
| `I` / ↑ | 往前 | 手冊 184 |
| `J` / ← | 左轉 | 手冊 184 |
| `L` / → | 右轉 | 手冊 184 |
| `K` / ↑ | 打開關閉的門、粉碎牆中密門 | 手冊 176/184 |
| 方向鍵 | 同上(手冊明示鍵盤可用四方向鍵) | 手冊 175 |

## 控制指令(遊戲中)

| 鍵 | 動作 |
|---|---|
| `C` | 施法 |
| `U` | 使用物品或技能 |
| `X` | 進入人物屬性分配畫面 |
| `O` | 重排隊伍順序 |
| `D` | 遣散隊員 |
| `?` | 顯示平面地圖 |
| `S` | 儲存遊戲 |
| `Q` | 離開遊戲 |
| `Ctrl`+`S` | 聲音開／關 |

## remake 實作狀態(2026-06-14)

- **選單**:`B`(開始,預設四人)/ `C`(繼續:有存檔則讀檔進遊戲)快捷字母 + ↑↓/Enter 輔助、`Esc`/`Q` 離開 —— 已實作。`D`/`R`(刪除/改名)、建角流程未實作。
- **移動**:`I`/↑ 前進、`J`/← 左轉、`L`/→ 右轉、`K` 開門(stub)、`Esc` 返回 —— 已實作(真實關卡 .lvl)。
- **第一人稱 viewport**:`--fp` → S_GAME 透視牆面 viewport(對拍 opendw;ctest `verify_fp_l1` 4/4、`render_sweep` 全 40 關 154 case byte-for-byte)。
- **地圖區域切換**:踩到出入口/階梯事件 → 換 area + 入口位置(對齊 opendw poll 重載;ctest `verify_areaswitch` 8/8)。wrap 邊界關卡(opendw 自身未實作)跳過。
- **事件文字(訊息檢視器)**:踩事件格(tile>1,對拍 op_71)→ 跑該關事件 script(VM op_58 跨資源 call,自包含)→ i18n 在地化 → 畫面下半訊息框(深藍底+白邊,文字層 24px CJK,自動換行 + 依框高分頁約 7 行)。翻頁 `Space`/`Enter`/`↓`/`I`,末頁再按關閉;`Esc` 直接關;多頁左下 `▼ 頁碼/總頁數`。檢視期間暫停移動,`F4` 切語系就地重排。
- **Read Paragraph 段落捲動檢視器(ParaViewer)**:Read Paragraph N → 近全螢幕 overlay 顯示**完整**繁中譯文(不截斷、不切字)。標題「段落 N」(i18n);捲動 `↑↓` 逐行、`PgUp`/`PgDn`/`Space`/`Enter`/`I`/`K` 逐頁、`Esc` 關閉;底部 `▲▼ 行範圍/總行數`。長段落跨頁可完整閱讀。
- **角色屬性表(CharSheet)**:`V` 或 `1`-`4` → 顯示選定角色完整屬性(力量/敏捷/智力/精神/生命/暈眩值/法力/等級/金幣/狀態/性別),`↑↓`/`1`-`4` 切換、`Esc` 關;i18n 三語、`F4` 即時重排。
- **存檔 / 讀檔**:`S` 存檔(訊息提示);選單 `C` 或 `--load` 讀檔還原 area/位置/朝向/game_state/隊伍。round-trip byte-for-byte(ctest `verify_save`)。
- **遭遇 / 戰鬥畫面(S_COMBAT)**:`--encounter <id>` → 怪物圖(@ (16,8) 對齊 opendw 佈局,golden byte-for-byte)+ 隊伍面板 + `F`戰鬥 / `R`逃跑。**戰鬥結算為乾淨室 placeholder**(opendw C 本身未實作結算,詳見 `42_COMBAT_BYTECODE.md`),非原版真值。
- **多語**:`F4` 循環 繁中 / EN / 日;`--locale <id>`。
- **未實作指令**(U/X/O/?、施法、建角、Ctrl+S 聲音):待對應系統接入。

## 測試 / headless 旗標

| 旗標 | 用途 |
|---|---|
| `--map <area>` | 直接載入某區關卡(0..39),進 S_GAME |
| `--fp` | S_GAME 用第一人稱 viewport(取代俯視彩格) |
| `--at <x> <y>` | 把玩家放到指定格;若為事件格立刻跑事件腳本(headless 驗證) |
| `--frames N` | 跑 N 幀後結束(`0` = 只 dump 不開窗) |
| `--dump <PPM>` | 把當前 framebuffer 輸出成 PPM(轉 PNG 用 dwimg) |
| `--scale N` | 視窗整數倍率(dump 對拍用 `--scale 1`) |
| `--locale <id>` | 切語系(預設 zh-TW;i18n 取 `assets/i18n/<id>/`) |
| `--read-para <N>` | 直接開段落 N 進 ParaViewer(需配 `--map`) |
| `--para-scroll <P>` | ParaViewer dump 前先下捲 P 頁(驗證長段落跨頁) |
| `--msg-page <N>` | 事件訊息檢視器先翻到第 N 頁(0-based)再 dump |
| `--char-sheet <N>` | 直接開角色 N 屬性表 |
| `--encounter <id>` | 進遭遇畫面(怪物表 index);配 `--combat-seed`/`--combat-rounds` |
| `--load <path>` / `--save-path <path>` | 讀檔 / 指定存檔路徑 |
| `--selftest-save` | headless 存讀檔 round-trip 自測 |

範例(自包含,不需 DATA1;SDL dummy driver):

```
# 事件短訊息(1 頁,踩事件格)
SDL_VIDEODRIVER=dummy opendw_remake --map 1 --at 13 20 --frames 0 --dump /tmp/ev.ppm
# 長段落分頁:第 1 頁(含 ▼)
SDL_VIDEODRIVER=dummy opendw_remake --map 1 --read-para 88 --msg-page 0 --frames 0 --dump /tmp/p1.ppm
# 長段落分頁:第 2 頁(翻一頁後)
SDL_VIDEODRIVER=dummy opendw_remake --map 1 --read-para 88 --msg-page 1 --frames 0 --dump /tmp/p2.ppm
```
