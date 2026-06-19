# 60 — 原版 DOS 與 Remake 視覺差異稽核

本文比對《火龍之戰》(Dragon Wars, Interplay 1989/90) 原版 DOS 與 opendw_remake 的逐畫面視覺呈現,區分「在地化 / 模式設計差異」與「保真缺口」。全程 docker 唯讀稽核,未改 src、未動 main。

稽核日期:2026-06-17。分支:`audit/dos-vs-remake`。

---

## 結論先講

整體視覺保真度高。第一人稱透視、右側隊伍面板、藍磚邊框、綠柱火炬、戰鬥怪物圖、平面地圖網格、角色狀態欄,版面與配色都貼著原版 DOS;標題畫面幾乎逐像素還原(且刻意保留英文 logo)。多數差異是**刻意的在地化(繁中)或現代化輔助**(底部操作提示列、新/續遊戲選單),不是缺口。

真缺口集中在三處:① 主選單語意不同(remake 是「新/續遊戲」二選一,DOS 是「目前隊伍 + 開始遊戲」的隊伍管理選單);② **世界區 (area 0 Dilmun) 的 automap 全圖渲染只畫出一條水平帶**,未鋪滿整張世界網格(同 seed 下一般關卡 automap 正常);③ headless `--char-sheet` 旗標在冷啟動 / 配 `--map` 時都打不開狀態欄(會落到 automap 或主選單),屬旗標 / 測試入口問題,角色狀態欄本身渲染正常(見既有 `screenshots/grow_sheet.png`)。

---

## 名詞說明

- **DOS 原版**:`Dragon Wars (1990).zip` 內 `3.5/1.1/disk1.ima` 解出的 `DRAGON.COM` / `DATA1` / `DATA2`,在 DOSBox 0.74 以 VGA/MCGA 16 色模式執行。原版以 320×200 native 解析度輸出。
- **Remake**:`opendw_remake` 以 headless(`SDL_VIDEODRIVER=dummy`)dump,輸出 960×600(320×200 的 3 倍,同 16:10 比例)。
- **Dilmun**:遊戲世界名;`org_map/dragon-wars-map-dilmun.jpg` 為網路權威世界圖(設計者繪製的全區連通地圖,非遊戲擷圖)。
- **Purgatory(煉獄)**:遊戲開場區(area 1),隊伍被剝奪裝備後丟入此地。

---

## DOS 畫面擷取方法(真擷取)

DOS 截圖**全部為真 DOSBox 擷取**,非替代基準。流程:

1. `7z` 從 `disk1.ima`(FAT12 軟碟映像)解出遊戲檔(注意檔名 `DRAGON.COM` / `DATA1` / `DATA2`,原始檔不入庫)。
2. `dwdos:latest` 容器內 `Xvfb :99`(1024×768x24)起虛擬顯示;DOSBox `output=surface scaler=none aspect=false`,autoexec 掛載 + 跑 `DRAGON.COM`。
3. `xdotool` 注入鍵序(config→`E`(VGA)→Enter→title→`B` 開始→`Esc`×8 清開場敘事→移動 `I`/`J`/`L`→`?` automap→數字鍵叫角色選單→四處走觸發隨機遭遇)。
4. `import -window root -crop 320x200+0+0` 抓 DOSBox surface(實測 native 320×200 落在 root 左上 +0+0,無縮放,像素準確)。

成功取得全部 8 類目標畫面:標題、主選單、開場敘事、第一人稱 viewport、平面地圖、戰鬥遭遇(Innocent Man / Loon / 強盜)、角色狀態選單、角色 General overview 數值表。原始 DOS frame 在稽核暫存區;入庫的是 320×200 小縮圖比對用,標來源 Interplay 1989/90。

> 唯一未真擷取的是 **DOS 世界區(overworld)俯視圖**:從 Purgatory 走到地表需長距離遊玩,scripted 自動化成本過高。世界圖比對改以權威 Dilmun 圖(等同 DOS 遊戲世界的地圖)為 DOS 側基準,並標示此替代。

---

## 逐畫面比對

並排圖見 `dos_compare/sidebyside/`;DOS 縮圖 `dos_compare/dos/`,remake `dos_compare/remake/`。

| # | 畫面 | 並排圖 | 結果 | 說明 |
|---|---|---|---|---|
| 1 | 標題 Title | `cmp_01_title.png` | **PASS** | 龍頭 + 紅膚戰士 + 隊員 + 「Dragon Wars / Copyright Interplay 89-90」逐像素還原。remake 刻意保留英文 logo(非缺口,設計決策 `fix/title-zh-keep-logo`)。 |
| 2 | 主選單 Main Menu | `cmp_02_mainmenu.png` | **設計差異 + 語意缺口** | DOS:藍磚框內「Current party… 1)Muskels 2)Theb 3)Elendil 4)Cheetah / Begin the game」——是**隊伍管理 + 開始**。Remake:純藍底「火龍之戰 / 您希望.. B)開始新遊戲 C)繼續舊遊戲」——是**新/續二選一**。版面與語意都不同。在地化(繁中)為設計差異;但 remake 主選單未呈現 DOS 的「目前隊伍清單 + 隊伍管理(建/刪/改名/查看)」整合在同一選單的樣貌,屬語意保真缺口(建角 / 角色管理在 remake 已實作,只是不在主選單同屏)。 |
| 3 | 開場敘事 Intro | (DOS `03_intro_text.png`) | **PASS(對應)** | DOS 開場「Stripped of all possessions… into the slums of purgatory…」白框 + 後方 viewport。remake 對應為事件訊息檢視器(深藍底白邊、繁中、分頁),已在既有 showcase 驗證。 |
| 4 | 第一人稱 Viewport | `cmp_04_fp.png` | **PASS** | 版面一致:左上區名銀幕、右側 Dragon Wars logo + 隊伍面板(四人 + 血條)、藍磚邊框、綠柱火炬、透視走廊(石牆 / 藍磚牆)、紅紋地板天花。remake 額外底部操作提示列(`I:fwd J/L:turn V:stats…`)= 現代化輔助。注意 DOS 截圖在 Purgatory 起點(開放天空),remake 截圖在 map1 走廊內,位置不同非渲染差異。viewport pipeline 已對 opendw byte-for-byte(`render_sweep` 全 40 關)。 |
| 5 | 平面地圖 Automap | `cmp_05_automap.png` | **PASS(關卡)/ 缺口(世界區)** | DOS `?` → 俯視網格 + 玩家圖標 + 「ESC to exit」。remake `--automap`(配 `--mm-seed 0` 全圖)Purgatory 正常鋪滿網格。**但 area 0(Dilmun 世界區)全 seed 下只渲染一條水平帶**(見下節),為真缺口。fog-of-war 行為(只畫走過格)兩邊一致且 remake 已 byte-for-byte 對拍(`verify_automap_l1`)。 |
| 6 | 戰鬥遭遇 Combat | `cmp_06_combat.png` | **PASS** | DOS:怪物圖(Innocent Man / Loon)+「Will the party: Fight / Quickly fight / Run / Advance ahead」+「Read paragraph N」。remake:怪物圖「強盜」+「6 隻 強盜(存活 6)」+ 繁中戰鬥列(F:戰鬥 R:逃跑 C:施法 / M:強力 D:卸武 A:前進 Q:快速 E:閃避)。版面(怪物置左、隊伍面板置右、指令置下)與精靈風格一致。文字繁中為設計差異。 |
| 7 | 角色狀態 Character | `cmp_07_charsheet.png` | **PASS** | DOS:「Muskels's status」→ General overview:Str/Dex/Int/Spr、Attack/Defense/Level/AC、Health/Stun/Power/Exp、Carried items/Gold。remake(`grow_sheet.png`):繁中「角色 1/4 Muskels / 力量 21/21 / 敏捷 20/20 / 智力 / 精神 / 生命 / 暈眩值 / 法力 / 等級 / 成長點數 / 金幣 / 狀態 / 性別」。數值對齊(力量 21、敏捷 20 與 DOS Str:21 Dex:20 一致)。在地化為設計差異。 |

備註 7:DOS 角色狀態選單(General overview / Abilities / Low~Misc magic 多分頁)remake 以單一彙整欄呈現,屬版面整併設計差異,非缺漏。

---

## 大地圖特別檢查(area 0 Dilmun 三方對照)

三方圖:`dos_compare/sidebyside/cmp_08_worldmap_3way.png`(權威 Dilmun 圖 / remake 俯視 `--map 0` / remake 世界 automap `wm_world.png`)。

- **權威 Dilmun 圖**:設計者繪製的全區連通地圖,標出 Kings Isle、Rustic、Eastern Isles、Quag、Forlorn、Isle of the Sun 等大區與 Byzanople / Salvation / Phoebus / Lansk / Purgatory / Dragon Valley / Necropolis 等地點。這是**跨區的世界拓樸**,不是單一遊戲畫面。
- **remake 俯視 `--map 0`**:把 area 0 當成**單一關卡的 tile 網格**渲染(密集彩格 + 玩家標記 `>`),banner「Dilmun」。同理 Dragon Valley(`wm_dragonvalley.png`)也是單區網格。remake 沒有、原版也沒有「一屏顯示整張世界連通圖」的畫面——權威圖是場外攻略物。
- **remake 世界 automap(`wm_world.png`)**:全 seed 下能鋪滿一張大網格(水域 + 陸塊 + 地點色塊),結構上對應 area 0 的內部 tile 佈局。

**結論**:remake 對「單一區 tile 網格」的渲染與 opendw oracle byte-for-byte 一致(automap / viewport sweep 皆過),結構正確。跨區世界拓樸(權威圖層級)是由區與區之間的出入口連通實現(`verify_areaswitch` 8/8),非單畫面渲染,故三方無法逐像素疊合,只能在「區內結構正確 + 區間連通正確」兩個層級分別驗證——兩者 remake 都有 ctest 支撐。

**真缺口**:`--map 0 --automap 0 --mm-seed 0` 的世界區 automap 只畫出**一條水平帶**(`05_automap0_full.png`),沒鋪滿整張世界網格;而 `--map 0`(viewport 俯視)同一區卻能畫出完整密格(`08_world_topdown.png`)。同 seed、同區、兩條渲染路徑結果不一致 → automap 在 area 0(大型 wraparound overworld)的覆蓋計算有 bug 嫌疑,值得追。

---

## 整體保真度評估

| 面向 | 評估 |
|---|---|
| 標題 / 美術 | 逐像素級還原,精靈與色盤忠實。 |
| 第一人稱透視 | 版面 / 邊框 / 隊伍面板 / 牆面解碼 byte-for-byte 對 oracle。高保真。 |
| 戰鬥畫面 | 怪物圖位置、隊伍面板、指令列佈局一致;文字繁中。高保真。 |
| 平面地圖 | 關卡級 byte-for-byte;fog-of-war 行為一致。世界區全圖渲染有缺。 |
| 角色狀態 | 數值對齊,繁中版面整併。高保真。 |
| 選單語意 | 主選單從「隊伍管理」改為「新/續遊戲」,語意保真度較低(功能另有入口)。 |

設計差異(非缺口):繁中在地化、640×480 模式、底部操作提示列、新/續遊戲選單、英文 logo 保留、角色狀態多分頁整併為單欄。

---

## 真缺口清單(已修;`fix/dos-audit-gaps`)

1. **[中] 世界區 automap 全圖渲染殘缺** — **已修**。
   根因:remake `Minimap::render` 與 golden 產生器都只跑 `draw_minimap` 的**第一趟**(`byte_1964 == 0`),只組出最頂一帶 9×N 格。原版 `draw_minimap`(engine.c:3204)是 **8 趟**捲動疊圖:每趟 `byte_1964 = 0..7` 經 `calc_minimap_position`(`bl = 3 - byte_1964 + byte_1960`)讀**不同 map row 帶**,再由 `set_viewport_size(byte_1964)` 用 `data_1997 / data_19A7 / data_19B7` 表把該帶 blit 到螢幕對應列。缺後 7 趟 → 只剩一帶(area 0 / area 1 同症,非 area 0 專屬;area 1「看似鋪滿」是因 golden 也只驗第一趟,自洽但不完整)。
   修法:新增 `Minimap::render_full` / `render_full_with_seen`,跑完整 8 趟並依 `data_1997/19A7/19B7` 把各帶疊進 `mem` 的對應螢幕列;`draw_automap` 改呼叫 full 版本。`render()` 單趟保留供既有 golden 對拍(leaf-level viewport_memory)。headless dump(`--map 0/1 --automap 0/1 --mm-seed 0`)目視確認兩區皆鋪滿整個視窗(玩家標記 / 牆 / 水域 / 樹叢可見)。
   回歸鎖:補 `verify_automap_l0`(area 0 Dilmun,47×32 wraparound),3 case byte-for-byte;ctest 32/32。
2. **[低-中] 主選單語意不對齊** — **已修**。`draw_menu` 在標題下、選項上疊出「目前隊伍...」+ 編號隊伍清單(`1) Muskels … 4) Cheetah`,昏倒成員標 `(昏倒)`),貼近 DOS「Current party… + Begin the game」整合樣貌;三語(繁中 / EN / 日)在地化,`Current party...` / `unconscious` 走 i18n。headless dump 確認版面。建角 / 刪 / 改名 / 查看仍由既有入口提供(本項聚焦選單同屏呈現隊伍)。
3. **[低 / 測試入口] `--char-sheet` headless 旗標失效** — **已修**。根因:`--char-sheet` 配 `--automap` 時,automap 區塊(main.cpp ~1145)無條件把 `state` 設為 `S_MAP`,壓過 `--char-sheet` 在 S_GAME 的消費點(~2761)→ 落到 automap。修法:automap 區塊偵測到 `char_sheet >= 1` 時 `enter_map` 後 state 留在 `S_GAME`,讓屬性表優先(冷啟動 / 配 `--map` 原本已正常,此修補上 automap 互斥案)。三案(冷啟動 / `--map` / `--automap`)皆可靠開狀態欄。`docs/engine/CONTROLS.md` 補列 `--automap` / `--mm-seed` / `--newgame` / `--scene`。

---

## 附:檔案

- 報告:本檔 `docs/audit/60_DOS_VS_REMAKE_VISUAL.md`
- 並排比對圖:`docs/audit/dos_compare/sidebyside/cmp_*.png`
- DOS 基準縮圖(真擷取,來源 Interplay 1989/90):`docs/audit/dos_compare/dos/*.png`
- Remake 對應圖:`docs/audit/dos_compare/remake/*.png`
- 世界圖三方:`docs/audit/dos_compare/sidebyside/cmp_08_worldmap_3way.png`

擷取與比對全程於 `dwdos` / `dwsdl` / `dwimg` docker 容器執行,未污染系統環境;原始遊戲檔(`DRAGON.COM` / `DATA1` / `DATA2`)未入庫。
