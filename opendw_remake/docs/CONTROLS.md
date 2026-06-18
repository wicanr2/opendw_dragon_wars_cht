# OpenDW Remake — 操作規範(與原版說明書一致)

> 來源:臺灣中文版《火龍之戰》操作手冊(轉寫見 `../../docs/33_MANUAL_TRANSCRIPTION.md`)。
> remake 的輸入處理**以本表為準**,確保操作與原版說明書一致。

## 開機 title splash

啟動先顯示**火龍之戰 dragon art 標題畫面**(原版 res29;含金色「Dragon Wars」立繪 + 在地化標題「火龍之戰」+ 閃爍「按任意鍵」提示),對齊原版「dragon art 標題畫面 → 按鍵 → 主選單」流程。

| 鍵 | 動作 |
|---|---|
| 任意鍵(方向 / Enter / Space / Esc / 字母) | 進入主選單 |
| `F4` | 切語系(splash 不離開,就地重排在地化標題) |
| `Q` / 關窗 | 離開遊戲 |

> headless / 自動化:`--no-splash` 直接進主選單(既有測試流不受影響);`--title` 強制顯示 splash。`--menu <tsv>` / `--newgame` / `--load` 等指定入口時自動略過 splash。title art 來源 theme-aware(預設 DOS res29;未來各平台版本各自 title)。

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

- **選單**:`B`(開始)/ `C`(繼續:有存檔則讀檔進遊戲)快捷字母 + ↑↓/Enter 輔助、`Esc`/`Q` 離開 —— 已實作。**建角流程已實作**(`B` → S_CREATE:命名 → 屬性配點 → 性別 → 多員,ctest `verify_chargen`)。
- **角色管理 `D` 刪除 / `R` 改名**:在角色屬性表(`V` → 選定角色)內,`D` 進刪除確認(`Y`/`N`)、`R` 進改名輸入(TTF;Enter 確認 / Esc 取消)。改名同步 raw[0..11] 高位元終止編碼;刪除後夾住游標 / 空隊則關閉。**真值層級:remake 設計(grounded 手冊 147 B/C/D/R)**;手冊「先按人物號碼再 D」對映為「屬性表內已選定角色 → 按 D/R」。ctest `verify_party_ops`。
- **`O` 重排隊伍**:S_GAME 按 `O` 開重排子畫面;↑↓ 移游標、Enter/Space 抓起當前成員(再 ↑↓ 與相鄰成員對調 = Party::move)、Enter 放下、Esc 離開。重排影響戰鬥站位(第 0 名 = 主角 / 施法者)與右側面板顯示順序。**真值層級:remake 設計(grounded 手冊 / CONTROLS「O 重排隊伍」)**。ctest `verify_party_ops`。
- **物品轉移 / 丟棄**(物品欄 `V` → `E`):`D` 丟棄(從背包移除該格)、`T` 轉移給其他隊員(選目標 → 搬整 23B 到對方第一個空格,含裝備位元 / 名)。目標背包滿則失敗提示。**真值層級:remake 設計(grounded 手冊「Item」段 Discard / Transfer)**。ctest `verify_party_ops`(含 512B round-trip 存檔相容)。
- **移動**:`I`/↑ 前進、`J`/← 左轉、`L`/→ 右轉、`Esc` 返回 —— 已實作(真實關卡 .lvl)。
- **開門 / 破密門(`K`)**:面向前方格 → 關閉的門開啟、鎖門做 Lockpick 檢定、牆中密門粉碎、石牆障礙提示需 Soften Stone;門/密門/石牆未開時擋路(像牆),陷阱格可走但踩中觸發傷害。狀態 per-area 保存(存檔 v3)。**真值層級:remake 設計(opendw 主遊戲 K handler 未反編;見 `57_DOORS_TRAPS_TERRAIN.md`)**;tile 型 0x30..0x34 為機制保留約定,真實 .lvl 目前未含 → 不影響既有關卡。機制以 ctest `verify_terrain` 驗證。
- **戰鬥外施法(`C`)/ 陷阱**:`C` 開探索施法選單(隊伍第 0 名 castable);Soften Stone(0x22)軟化前方石牆、Disarm Trap(0x36)解除陷阱、Sense Traps(0x14)標記陷阱可見。陷阱踩中扣血(remake 1d8)。headless `--terrain-cast <id>`。**真值層級:remake 設計**。
- **第一人稱 viewport**:`--fp` → S_GAME 透視牆面 viewport(對拍 opendw;ctest `verify_fp_l1` 4/4、`render_sweep` 全 40 關 154 case byte-for-byte)。
- **地圖區域切換**:踩到出入口/階梯事件 → 換 area + 入口位置(對齊 opendw poll 重載;ctest `verify_areaswitch` 8/8)。wrap 邊界關卡(opendw 自身未實作)跳過。
- **事件文字(訊息檢視器)**:踩事件格(tile>1,對拍 op_71)→ 跑該關事件 script(VM op_58 跨資源 call,自包含)→ i18n 在地化 → 畫面下半**半透明訊息框**(深藍底 dither 半透明,底下 viewport / 地圖隱約透出 + 白外框 + 亮藍內框雙線優雅邊;文字層 24px CJK 恆銳利,自動換行 + 依框高分頁約 7 行)。翻頁 `Space`/`Enter`/`↓`/`I`,末頁再按關閉;`Esc` 直接關;多頁左下 `▼ 頁碼/總頁數`。檢視期間暫停移動,`F4` 切語系就地重排。半透明採 3/4 覆蓋 dither(可讀性優先;theme-aware,見 `src/render/ui_theme.hpp`)。
- **Read Paragraph 段落捲動檢視器(ParaViewer)**:Read Paragraph N → 近全螢幕 overlay 顯示**完整**繁中譯文(不截斷、不切字)。標題「段落 N」(i18n);捲動 `↑↓` 逐行、`PgUp`/`PgDn`/`Space`/`Enter`/`I`/`K` 逐頁、`Esc` 關閉;底部 `▲▼ 行範圍/總行數`。長段落跨頁可完整閱讀。
- **角色屬性表(CharSheet)**:`V` 或 `1`-`4` → 顯示選定角色完整屬性(力量/敏捷/智力/精神/生命/暈眩值/法力/等級/金幣/狀態/性別),`↑↓`/`1`-`4` 切換、`Esc` 關;i18n 三語、`F4` 即時重排。
- **存檔 / 讀檔**:`S` 存檔(訊息提示);選單 `C` 或 `--load` 讀檔還原 area/位置/朝向/game_state/隊伍。round-trip byte-for-byte(ctest `verify_save`)。
- **遭遇 / 戰鬥畫面(S_COMBAT)**:`--encounter <id>` → 怪物圖(@ (16,8) 對齊 opendw 佈局,golden byte-for-byte)+ 隊伍面板 + `F`戰鬥 / `R`逃跑 / `C`施法。**命中 / 徒手 / 武器傷害骰公式 = bytecode 真值**(從 res3 + DRAGON.COM 反組譯,詳見 `42_COMBAT_BYTECODE.md`);怪物 21B → AV/DV/STR 亦 bytecode 真值。**仍受阻**:res3 全戰鬥閉環(op_89 動作指派卡點)未真值化 → combat_loop 為 remake 設計(同真值公式,非 res3 閉環);怪物 HP/AC 暫定。
- **特殊攻擊 + 閃避(S_COMBAT,群戰)**:手冊 §戰鬥列 4 種特殊攻擊 + Dodge,對應熱鍵 `M`強力一擊(Mighty Blow)/ `D`卸武裝(Disarm enemy)/ `A`前進(Advance)/ `Q`快速戰鬥(Quickly fight)/ `E`閃避(Dodge enemies)。**真值層級:remake 設計 grounded 手冊**——opendw C 未實作戰鬥結算(`player.c` spell_info 的 `0x40 Disarm` 是法術 bitmask,非近戰特殊攻擊),res3 戰鬥 bytecode 全閉環受阻,**特殊攻擊的命中/傷害/卸武裝/DV 修正係數無法由 bytecode 逆出**。底層單次攻擊仍走 `resolve_attack`(命中/傷害公式 = bytecode 真值),特殊攻擊只在其上套修正,不重寫公式:
  - 強力一擊:命中門檻 −`kMightyBlowAvPenalty`(較難命中),命中時傷害 +`kMightyBlowDmgBonus`。
  - 卸武裝:命中則打掉敵人武器(傷害骰回退徒手 1d4),本次不造成身體傷害。
  - 前進:remake 無實體 range → 以 AV +`kAdvanceAvBonus` 反映「逼近、站位更好」。
  - 快速戰鬥:結算等同一般攻擊(「快速」= UI 跳過逐一動畫)。
  - 閃避:本回合自身 DV +`kDodgeDvBonus`(被命中門檻下降),下回合輪到前清除。
  - headless `--combat-special <mighty|disarm|advance|quick|dodge>`(配 `--encounter`);ctest `verify_combat_special`。
- **多語**:`F4` 循環 繁中 / EN / 日;`--locale <id>`。
- **已接入指令**:`C` 探索施法、`K` 開門/破密門、`X` 配點(角色表內)、`U` 使用物品(角色表物品欄內 `V`→`E`→選格→`U`)、`E` 裝備穿脫、`P` 商店、`T` 招募、`O` 重排隊伍、`D`/`R` 刪除/改名(角色表內)、物品 `D` 丟棄 / `T` 轉移(物品欄內)。
- **未實作指令**(`Ctrl+S` 聲音):待音訊子系統接入(目前 op_90 忠實 no-op)。

## 測試 / headless 旗標

| 旗標 | 用途 |
|---|---|
| `--map <area>` | 直接載入某區關卡(0..39),進 S_GAME |
| `--automap <area>` | headless 直接進第 N 區俯視平面地圖(`?` 鍵功能);配 `--mm-seed` 控制 fog;同時帶 `--char-sheet` 時讓位給屬性表 |
| `--mm-seed <N>` | 探索 / minimap fog seeding;`0` = 全圖揭露(automap dump 對拍用) |
| `--newgame` | 啟動即進建角畫面(互動建角流程 S_CREATE) |
| `--title` | 強制顯示開機 title splash(火龍之戰 dragon art) |
| `--no-splash` | 略過 title splash,直接進主選單(headless / 自動化) |
| `--scene <name>` | 直接渲染指定場景圖(scene 模式,不進選單 / 遊戲) |
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
| `--combat-special <type>` | 隊伍第 0 名用特殊攻擊一回合(`mighty`/`disarm`/`advance`/`quick`/`dodge`);驗證命中/傷害/卸武裝/DV 修正 |
| `--load <path>` / `--save-path <path>` | 讀檔 / 指定存檔路徑 |
| `--selftest-save` | headless 存讀檔 round-trip 自測 |
| `--terrain-cast <id>` | S_GAME 首幀對前方/當前格施放地形法術一次(驗證 Soften Stone/Disarm Trap/Sense Traps) |

範例(自包含,不需 DATA1;SDL dummy driver):

```
# 事件短訊息(1 頁,踩事件格)
SDL_VIDEODRIVER=dummy opendw_remake --map 1 --at 13 20 --frames 0 --dump /tmp/ev.ppm
# 長段落分頁:第 1 頁(含 ▼)
SDL_VIDEODRIVER=dummy opendw_remake --map 1 --read-para 88 --msg-page 0 --frames 0 --dump /tmp/p1.ppm
# 長段落分頁:第 2 頁(翻一頁後)
SDL_VIDEODRIVER=dummy opendw_remake --map 1 --read-para 88 --msg-page 1 --frames 0 --dump /tmp/p2.ppm
```
