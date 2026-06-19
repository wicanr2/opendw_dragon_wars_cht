# 65 — 產品全面評估(QA + PM 視角):opendw_remake 現況

> 評估日期:2026-06-19。分支:`docs/game-evaluation`。
> 對象:`opendw_remake/`(C++20 / SDL2 重製《火龍之戰》Dragon Wars, Interplay 1989/90)。
> 方法:**唯讀**。全程 docker(`dwsdl` 跑遊戲、`dwimg` 轉圖),`SDL_VIDEODRIVER=dummy` / `SDL_AUDIODRIVER=dummy` headless,逐系統實跑 + 目視 dump。截圖在 `../media/assessment/eval65/`。
> 基準對照:`../../../docs/57_PM_REVIEW.md`(2026-06-16 PM review,當時整體還原度 ~45–50%)。
> 誠實分級沿用既有四級:**bytecode 真值** / **best-fit** / **remake 設計(grounded 手冊)** / **受阻**。

---

## 1. 執行摘要(BLUF)

**一句話定位**:opendw_remake 現在是一套「**從建角玩到結局、RPG 成長與經濟循環已接通、四種平台主題可切、全繁中且支援三語、戰鬥有特殊攻擊與施法的高保真可玩重製**」——它已從上次 review 的「空心 demo」跨進「**麻雀雖小、五臟俱全的可玩 CRPG**」。

**整體評分(玩家可感體驗加權):**

| 構面 | 上次 PM review(2026-06-16) | 本次(2026-06-19) | 推導 |
|---|:---:|:---:|---|
| **可玩性** | ~40%(可玩內容) | **~68%** | 成長/經濟/招募三大空洞已補齊(見 §3),CRPG 核心循環「探索→戰鬥→升級→買裝→招募」全環接通;扣分在中後期內容深度與少量未觸發機制 |
| **完整度(系統覆蓋)** | ~45–50% | **~72%** | 12 大系統中 9 個已 ✅/🟡 可玩,音訊從 0% 變有真實 PCM,四主題 + 三語 + 熱鍵 + 結局過場全到位 |
| **保真度(對原版真值)** | ~75%(引擎) | **~76%(引擎層維持)** | 引擎/渲染/VM/戰鬥公式/存讀檔仍 bytecode 真值;新增的成長/經濟/招募/音訊事件對映多為 remake 設計(grounded 手冊),不謊稱原版真值 |

**整體還原度:約 68–72%**(上次 ~45–50%)。上次 PM review §5 推薦的最高槓桿 A(補 RPG 成長迴圈)與 B(經濟+招募)**都已落地**,並額外做了 4 主題、音訊、世界地圖、結局過場、全域熱鍵、戰鬥深度、怪物程序化動畫。

**給決策者的一句話**:現在交出去,是一個**可以真的坐下來玩一段、會升級會買裝會招人、四種復古畫風隨按隨換、全繁中**的重製版;殘留缺口集中在「中後期劇情內容量」與「少數非戰鬥技能檢定/門互動的原版真值無法逆出」,屬深水區而非門面。可玩性已過「值得對外體驗」的門檻。

---

## 2. 逐系統 PASS / 問題表

全部測項 **exit code 0、無 crash、無 abort/assert**;繁中渲染全程正常(24px CJK 銳利)。`ctest` 在容器內乾淨重建後 **34/34 全綠**(上次 review 為 22 項)。

圖例:**✅ PASS** / **🟡 PASS 但有觀察** / **⚠️ 問題**。截圖路徑相對本檔(`../media/assessment/eval65/`)。

### 2.1 核心可玩鏈

| 環節 | 指令 | 結果 | 截圖 | 觀察 |
|---|---|:---:|---|---|
| 建角 | `--newgame` / `--char-sheet 0` | ✅ | `newgame.png` `char_sheet.png` | S_CREATE 流程(命名→配點→性別→多員)`verify_chargen` 綠 |
| 探索(第一人稱) | `--map 1 --fp` | ✅ | `fp_map1.png` `fp_dos.png` | 透視走廊逐像素對拍 opendw(`render_sweep` 全 40 關 154 case byte-for-byte) |
| 踩格事件 | `--map 1 --at 13 20` | ✅ | `trap_event.png` | 半透明訊息框 + 繁中事件文字 + 自動分頁 |
| 戰鬥 | `--encounter 5 --combat-seed 1` | ✅ | `encounter5.png` `combat_dos.png` | 怪物圖 @(16,8) golden 佈局、命中/傷害公式 bytecode 真值 |
| 終戰 | `--fight-namtar` | ✅ | `fight_namtar.png` | exit 0,無 crash(Namtar 屬性 = remake 平衡,誠實標示) |
| 結局 | `--ending --ending-idx 0/1/4` | ✅ | `ending0.png` `ending1.png` `ending4.png` | 五結局過場,雙語呈現(原文 + 繁中字幕),藝術圖完整 |

**核心鏈 PASS**:建角→探索→事件→戰鬥→終戰→結局全段可走、零 crash、繁中正常。

### 2.2 四主題(F8 / `--theme`)

| 主題 | title | fp 牆面 | combat | 完整度評 |
|---|---|---|---|---|
| **DOS** | ✅ 綠龍 `title_dos.png` | ✅ EGA 16 色走廊 `fp_dos.png` | ✅ `combat_dos.png` | **完整**(預設、逐像素對拍基準) |
| **Amiga** | ✅ **金龍**(原生 title.pic,紫灰底)`title_amiga.png` | ✅ **原生石牆**(棕/teal 磚 + 原生盤)`fp_amiga.png` | ✅ `combat_amiga.png` | **接近完整**(標題/牆面/UI 框皆原生差異化) |
| **X68000** | 🟡 **回退 DOS 綠龍** `title_x68000.png` | ✅ 走廊渲染 `fp_x68000.png` | ✅ `combat_x68000.png` | **partial**(TITLE.PKH 受阻,標題回退 DOS;誠實標示見 §6) |
| **VGA-256** | — | ✅ **256 色增強**(漸層浮雕、柔邊)`fp_vga.png` | — | **完整增強 pass**(演算法增強,非手繪,誠實標示) |

四主題切換 PASS,DOS↔Amiga 差異化明顯(綠龍 vs 金龍、EGA 平塗 vs 原生石牆);VGA 對 DOS 同幾何做出可辨識的漸層立體感;X68000 標題如預期回退 DOS。F8 即時循環 + toast 提示(`theme_f8.png`)。

### 2.3 音訊(上次 0%,本次重大變化)

| 項目 | 結果 | 證據 |
|---|:---:|---|
| 音訊子系統存在 | ✅ | `src/audio/sound.{hpp,cpp}`;SDL2 audio + PC speaker 風格方波合成 |
| 真實 PCM 音效 | ✅ | `assets/bundle/audio/` 三檔:`amiga_data5.wav`(3.33s)/`amiga_data6.wav`(1.42s)/`x68k_dwsnd.wav`(2.00s),8-bit signed PCM **觀測真值** |
| 事件音(op_90)不 crash | ✅ | `--encounter 5 --combat-rounds 3` 走戰鬥音路徑,EXIT=0 無 crash |
| dummy 退化安全 | ✅ | 無音訊裝置時退「靜音模式」,不影響流程 |

**音訊從「無子系統」→「有 SDL2 合成 + 真實 PCM 取樣」**。方波頻率由 DOS `func_5060` dx/bx 反組譯 grounded(door/wall/effect_88);事件→樣本對映為 remake 設計(誠實標示,缺檔自動退回方波)。未納入:Amiga 68k 音樂播放器、DOS PC speaker 音符序列(受阻,見 §6)。

### 2.4 介面 / 操作

| 項目 | 指令 | 結果 | 截圖 | 觀察 |
|---|---|:---:|---|---|
| Title splash | `--title` | ✅ | `title_dos.png` | dragon art + 繁中「火龍之戰」+「按任意鍵繼續」 |
| 半透明對話框 | `--at` 事件格 | ✅ | `trap_event.png` `event_ja.png` | 深藍 dither 3/4 覆蓋 + 雙線藍框,文字層恆銳利 |
| 世界地圖(area 0) | `--map 0` | 🟡 | `worldmap.png` | Dilmun 俯視網格 + 彩色地點點 + `>` 玩家標記;**但見 §5 bug:area 0 automap 只鋪一條水平帶,未填滿世界網格** |
| automap(`?`) | `--automap 1 --mm-seed 0` | ✅ | `automap1.png` | 一般關卡 automap 正常鋪滿 |
| F1 Help | `--keys F1` | ✅ | `help_f1.png` | 半透明框列全鍵位,i18n 三語,「Esc:關閉」 |
| F8 主題循環 | `--keys F8` | ✅ | `theme_f8.png` | 即時重繪 + toast |
| F10 離開確認 | `--keys F10` | ✅ | `quit_f10.png` | 自動存檔 + yes/no 確認視窗 |

操作層 PASS。F1/F8/F10 + ESC 確認流程完整,符合「ESC 只 cancel、F10 才離開、離開前自動存檔 + 確認」鐵則。底部恆駐操作提示列(`I:fwd J/L:turn V:stats P:shop T:tavern S:save Esc`)。

### 2.5 戰鬥深度

| 項目 | 指令 | 結果 | 證據 |
|---|---|:---:|---|
| 特殊攻擊(強力一擊) | `--combat-special mighty` | ✅ | `combat_mighty.png`,`verify_combat_special` 綠 |
| 閃避(Dodge) | `--combat-special dodge` | ✅ | `combat_dodge.png` |
| 戰鬥內施法 | `--encounter --cast` | ✅ | `cast_combat.png`,61 法術可施 |
| 命令列繁中化 | — | ✅ | combat 截圖底部:「F:戰鬥 R:逃跑 C:施法」+「M:強力 D:卸武 A:前進 Q:快速 E:閃避」 |
| 怪物程序化動畫 | (git #166) | ✅ | idle 呼吸 + 受擊閃白(單格程序化動畫,已 commit) |

戰鬥深度 PASS。上次 review「4 種特殊攻擊 + 閃避全缺」已補齊(remake 設計 grounded 手冊,底層仍走 bytecode 真值 `resolve_attack`)。

### 2.6 RPG 系統

| 項目 | 指令 | 結果 | 截圖 / 證據 |
|---|---|:---:|---|
| 商店買賣 | `--shop` | ✅ | `shop.png`:「購買-金幣」+ 11 件商品繁中名 + 售價 + 買/賣 |
| 酒館招募 | `--recruit` | ✅ | `recruit.png`:「酒館(4/7)」烏瑞克/路易/瓦拉/哈利法克斯 + STR/DEX/INT |
| 升級觸發 | (戰鬥後) | ✅ | `../screenshots/grow_levelup.png`:「勝利！每名隊員 +80 XP」→「Muskels 升到了 2 級!」 |
| X 配點 / 成長 UI | `verify` ctest | ✅ | `progression.cpp` `check_level_up`(XP 門檻消耗 + 多級 + AP/STR/HP 成長)+ `../screenshots/grow_alloc.png` |
| 技能檢定 | source | ✅(機制) | `progression.cpp` `skill_check(skill,difficulty,rng)` 已實作 |
| 存讀檔 | `--selftest-save` | ✅ | 「save->load->save byte-for-byte: yes / PASS」 |

**RPG 系統 PASS——這是相對上次 review 最大的躍進**:升級觸發、配點、技能檢定、商店、招募、使用物品/裝備全部從 ⛔ 變 ✅。`grow_levelup.png` 直接證明「打贏→+80XP→升級」的核心正回饋迴圈已閉環。

### 2.7 在地化

| 項目 | 指令 | 結果 | 截圖 |
|---|---|:---:|---|
| 繁中覆蓋 | 預設 | ✅ | 全 menu/chars/combat/items/spells/shop 繁中;i18n 載入 587 條 |
| F4 三語 | `--locale ja/en` | ✅ | `fp_ja.png` `fp_en.png` `event_ja.png` |
| 日文 events | `--locale ja` 事件格 | ✅ | `event_ja.png`:開場敘事完整日文(212/283),lang 指示 `[日]` |

在地化 PASS。繁中 283 全覆蓋、日文 212/283。**觀察(見 §5)**:少數標籤未進 i18n —— area 名「Purgatory」恆顯英文(所有截圖左上)、商店首項「Dragon Stone」、招募屬性標籤「STR/DEX/INT」、隊員名(Muskels/Theb…)為英文。

### 2.8 保真 vs 原版(對照 docs/audit/60)

引擎層保真維持高水準:第一人稱 viewport 逐像素對拍、VM opcode 逐指令、戰鬥三大公式 bytecode 真值、存讀檔 byte-for-byte、怪物圖 golden 佈局。doc 60 稽核結論「整體視覺保真度高,真缺口集中三處」與本次目視一致(主選單語意、area 0 automap 水平帶、headless char-sheet 旗標入口)。

---

## 3. 與上次 PM review(docs/57)逐項對比

上次 review 列 §2 逐系統表;本次重新評估(✅=已關閉 / 🟡=部分 / ⛔=仍缺):

| 系統 | docs/57 還原度 | 本次 | 變化關鍵 |
|---|:---:|:---:|---|
| 探索 / 世界 | 🟡 70% | 🟡 **75%** | 開門/破密門/陷阱已實作(remake 設計);area 0 automap bug 仍在 |
| 戰鬥 | 🟡 55% | 🟡 **65%** | 4 特殊攻擊 + 閃避 + 施法補齊;怪物程序化動畫;res3 閉環仍受阻 |
| 角色成長(升級) | ⛔ **10%** | ✅ **70%** | **最大躍進**:升級觸發 + AP/屬性成長 + 配點全到位 |
| 技能 | ⛔ 15% | 🟡 **45%** | skill_check 機制已實作;非戰鬥技能觸發點仍受 walking-engine 未反編所阻 |
| 物品 / 經濟 | ⛔ 20% | ✅ **70%** | 商店買賣 + 使用物品 + 裝備穿脫 + 丟棄/轉移全到位 |
| NPC 招募 | ⛔ **0%** | ✅ **75%** | 酒館招募 4 名角(譯名完整 + 隊伍上下限) |
| 劇情 / Quest | 🟡 40% | 🟡 **45%** | gate 機制 + 序盤繁中 + 結局可觸發;物品 gate set 來源仍受阻 |
| 法術 | 🟡 60% | 🟡 **65%** | 戰鬥外施法(地形法術)補上;召喚/工具類仍部分 TODO |
| 存讀檔 | ✅ 90% | ✅ **90%** | 維持 byte-for-byte |
| 音訊 | ⛔ **0%** | 🟡 **40%** | **從無到有**:SDL2 合成 + 真實 PCM;音樂仍受阻 |
| UI / 在地化 | 🟡(超原版) | 🟡 **(超原版,更完整)** | 三語 + 四主題 + 全域熱鍵 + F1 help;少量標籤未 i18n |
| (新)平台主題 | — | ✅ **新增** | DOS/Amiga/X68000/VGA-256 四主題 F8 切換 |

**重算 §2.1 量化(玩家可感體驗加權):**

| 體驗構面 | 原版佔比 | docs/57 達成 | 本次達成 | 本次加權 |
|---|:---:|:---:|:---:|:---:|
| 探索迷宮 + 自動地圖 | 25% | 70% | 75% | 18.8% |
| 戰鬥(指令 + 數值手感) | 20% | 55% | 65% | 13.0% |
| 角色養成(升級/技能/配點) | 20% | ~12% | 65% | 13.0% |
| 物品經濟(買賣/用/換) | 15% | 20% | 70% | 10.5% |
| 劇情 / quest 驅動 | 15% | 40% | 45% | 6.8% |
| 招募 / 隊伍構築 | 5% | 0% | 75% | 3.8% |
| **合計** | 100% | **~40%** | | **~66%** |

加上四主題/音訊/熱鍵屬「超原版加值」不計入原版佔比,故**整體可玩內容還原度約 66–68%**(docs/57 §5 預期「A+B 後可拉到 65–70%」,**已達標**)。

---

## 4. 亮點(相對上次)

1. **CRPG 核心正回饋迴圈閉環**:`grow_levelup.png` 證明「戰鬥勝利 → +80 XP → 升級(屬性成長 + AP)」串通,上次 review 點名的「最傷一刀」已補。
2. **四平台主題可切換**:DOS 綠龍 / Amiga 金龍 + 原生石牆 / X68000(回退) / VGA-256 增強,F8 即時循環。Amiga 為真正抽出的原生美術,非濾鏡。
3. **音訊從零到有真實 PCM**:三段原版觀測真值 PCM + grounded 方波合成。
4. **全域熱鍵 + 離開保護**:F1 help / F8 主題 / F10 離開(自動存檔 + 確認)/ ESC 確認,符合互動 app 離開鐵則。
5. **結局過場有藝術圖 + 雙語呈現**:五結局可觸發,呈現完整不是純文字。

---

## 5. 發現的 bug / 粗糙處(就算小也列)

| # | 嚴重度 | 現象 | 證據 | 性質 | 狀態 |
|:--:|:---:|---|---|---|---|
| B1 | 中 | **area 0(Dilmun 世界區)俯視只鋪一條水平帶** | `worldmap.png` vs `automap1.png` | 釐清後實為 `--map 0`(S_GAME 俯視 `draw_game`)畫原始 tile 格,被誤標為 automap;`?`/`--automap 0` 早已走 `WorldMap::render`(#154)| **✅ 已修**:area 0 俯視探索(`draw_game`)亦改走 `WorldMap::render`,`?`/automap/`--map 0` 三路徑共用美化世界圖,不再一條帶 |
| B2 | 低 | **area 名恆顯英文「Purgatory」** | 全 fp/event 截圖 | 在地化漏網(area 名表未接 i18n) | **✅ 已修**:新增 `area_name_tr()`,40 關 area 名走 `WorldMap::place_name_zh`(補齊 18/19/22/27/33/34/35/36/38);「Purgatory→波卡城」「Phoeban Dungeon→菲巴斯地下城」等 |
| B3 | 低 | **隊員名(Muskels/Theb/Elendil/Cheetah)英文** | 所有 fp 截圖右側面板 | 原版預設角色名,存於 512B player_record 的 7-bit 高位元終止字串 | **待審/保留**:該欄位編碼無法容 CJK,改譯會破存讀檔 byte-for-byte golden;玩家可自由改名;依 CONTEXT.md「proper names 暫保留原文」 |
| B4 | 低 | **商店首項「Dragon Stone」英文** | `shop.png` | 單品項漏譯(劇情關鍵道具) | **✅ 已修**:shop.tsv 加 `Dragon Stone→龍寶石`(經既有 `tr.tr(name_key)` 路徑) |
| B5 | 低 | **招募屬性標籤「STR/DEX/INT」英文** | `recruit.png` | 縮寫標籤硬編 | **✅ 已修**:改走 `recruit_str/dex/int`→力量/敏捷/智力(CONTEXT.md 屬性節) |
| B6 | 極低 | headless `--char-sheet` 配 `--map` 讓位給 automap | 本次重現 | 已知(doc 60 缺口③);測試入口問題 | 未處理(非本次範圍) |
| B7 | 極低 | ALSA 無音效卡時 stderr 警告(已退靜音) | 啟動 log | 環境噪音 | 未處理(環境噪音) |

無任何 crash / abort / 非零 exit。**B1/B2/B4/B5 已修**(見上「狀態」欄);B3 待審(存檔格式限制);B6/B7 為測試入口 / 環境噪音。

### 5.1 中後期在地化補譯(本次)

新增 i18n-aware 偵測模式至 `mainline_events`(載 zh-TW 全表,逐 emit 段落判 `tr(s)==s` 且含 ASCII → 標 `[EN!]`),全 40 area 掃描:**可達英文回退 74 條 → 1 條**(僅剩 `"egin a new game` 主選單緩衝雜訊,非真實事件)。補譯 73 條中後期探索/對白(瑪根地底世界/尼塞山腹/菲巴斯地下城/拜占儂地下城/蘭斯克渡輪/礦坑/雕像等),譯名依 CONTEXT.md(Lansk蘭斯克/Phoebus菲巴斯/Quag奎格/Necropolis死城);`Rustic→拉斯提克` 為音譯待審。`ctest` 34/34 全綠。

---

## 6. 誠實受阻清單(維持上次,部分演進)

| 項目 | 狀態 | 出處 |
|---|---|---|
| **X68000 標題(TITLE.PKH)** | **暫時放棄**(四輪 + 全程式模擬仍對角斜紋;PKH codec/GVRAM blit/呼叫鏈已逆出,頂部 ~190px 收斂、藝術區 layout 對不上)→ X68000 主題標題回退 DOS res29 | `reference/64_X68K_PKH_LESSONS.md` |
| **怪物逐回合 HP vs oracle** | 受阻(res3 全戰鬥閉環 op_89 動作指派卡點,無法 byte-diff);combat_loop 為 remake 設計(同真值公式) | docs/57 R2、`42_COMBAT_BYTECODE.md` |
| **怪物屬性(21B blob)** | AV/DV/STR = bytecode 真值;完整 HP/AC 暫定 | docs/57 R3 |
| **武器 STR bonus** | best-fit(self-modifying-code 矛盾未解,定論 bonus=0) | docs/57 R4 |
| **Phoebus(area 6)/ area 33** | 資料層隔離分量進不去;連通 38/40 靠 remake 慣例 | docs/57 R5 |
| **物品 gate set 來源** | 逆不出(quest「取得道具→設旗標」端) | docs/57 R6 |
| **Amiga 透視 / 音樂、X68000 3D/END** | Amiga 牆面已逆出可用;Amiga 68k 音樂播放器、X68000 .PKH 3D/END 受阻 | `reference/61_MULTIVERSION_ASSETS.md` |
| **非戰鬥技能檢定觸發點** | skill_check 機制已實作,但「在哪個格觸發哪個檢定」受 walking-engine 未反編所阻 | docs/57、`gameplay/59` |
| **音訊精確波形 / 原版音樂** | PCM 取樣為觀測真值;事件對映 = remake 設計;PC speaker 音符序列 / Amiga 68k 音樂未還原 | `assets/bundle/audio/README.md` |

受阻項多為「深水區真值」(原版 oracle 或架構所阻),已誠實記錄;不影響可玩性,只影響「100% bytecode 真值」的宣稱上限。

---

## 7. 下一步建議(PM 排序:玩家價值 × 工時 × 風險)

上次 review 的 A/B 已完成,本次重排。工時量級:S<1 週 / M 1–3 週 / L 3–6 週(單人估)。

| 優先 | 選項 | 玩家價值 | 工時 | 風險 | 理由 |
|:---:|---|:---:|:---:|:---:|---|
| ⭐ 1 | **中後期內容 / 在地化完整化**(事件繁中 ~序盤→全主線、quest gate 串成「持有 X 解鎖 Y」體系) | **高** | **L** | 低(純內容工) | 系統已齊,現在的天花板是「玩到中段沒新內容/撞未譯」;這是把 66% 拉向 80% 的主路徑 |
| 2 | **B1 area 0 世界圖修復 + B2–B5 在地化漏網**(area 名/隊員名/Dragon Stone/STR-DEX-INT 進 i18n) | 中(門面) | **S** | 低 | 幾乎零風險、立即提升完成度觀感;世界圖是玩家最早看到的大畫面之一 |
| 3 | **技能檢定觸發點落地**(把已實作的 skill_check 接到探索格事件:開鎖/包紮/Lore) | 中–高 | **M** | 中(觸發點需反編 walking-engine 或 grounded 手冊) | 讓 27 個技能從「死數字」變「能用的能力」,補 §3 技能 45%→更高 |
| 4 | **打包發佈**(Linux/Win 打包、640×480 收尾、README 漂移清理) | 中(對外曝光) | **S–M** | 低 | 系統與可玩性已過門檻,值得讓外部體驗 |
| 5 | **怪物屬性逆向 + res3 全戰鬥閉環真值化** | 中 | **L–XL** | **高**(受 oracle 阻) | 忠實度潔癖項;玩家可感價值低,留最後 |
| — | X68000 標題 | 低 | — | — | ROI 過低,維持回退(doc 64 結論);除非取得實機 GVRAM dump |

**明確推薦**:**先做 1(內容)+ 2(在地化漏網 + 世界圖)**。理由:系統面已從上次的「空心」補成「五臟俱全」,**現在的瓶頸從『系統缺』變成『內容量與打磨』**。1 是把可玩性推向 80% 的唯一主路徑;2 幾乎零成本修掉所有「玩家一眼看到的英文/缺口」,對「全繁中」宣稱與完成度觀感立即見效。3 接技能讓深度再加一層。5(戰鬥真值)仍留最後——風險高、玩家無感。

---

## 附:本報告實據

- 評估截圖(34 張):`../media/assessment/eval65/`(本次 headless dump)+ `../media/screenshots/grow_levelup.png`/`grow_alloc.png`(升級/配點 UI 既有證據)。
- 基準:`../../../docs/57_PM_REVIEW.md`(2026-06-16,~45–50%)。
- 視覺稽核:`60_DOS_VS_REMAKE_VISUAL.md`(三處真缺口)。
- 多版本素材 / 主題:`../reference/61_MULTIVERSION_ASSETS.md`、`../reference/62_VGA256_THEME.md`、`../reference/64_X68K_PKH_LESSONS.md`。
- 音訊:`assets/bundle/audio/README.md`、`src/audio/sound.{hpp,cpp}`。
- 成長系統:`src/game/progression.{hpp,cpp}`(`check_level_up` / `skill_check` / `xp_threshold_for_next`)。
- 操作 / headless 旗標:`../engine/CONTROLS.md`。
- 驗證:容器內乾淨重建 `ctest` **34/34 全綠**;所有測項 exit 0、無 crash。

> **誠實聲明**:百分比為玩家可感體驗加權估值(非逐 byte 量測),依逐系統實跑 + 目視 dump 的 PASS / 觀察狀態加權推得,供產品決策用;工程真值請以對應 ctest 與 docs/42–55 的逐項對拍為準。
