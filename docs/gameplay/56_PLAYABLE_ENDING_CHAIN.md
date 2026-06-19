# 56 — 可通關結局鏈:打贏終戰 Namtar → 結局序列(remake 收官)

> 日期:2026-06-16
> 對象:`opendw_remake/`(C++20/SDL2 重製《火龍之戰》Dragon Wars, Interplay 1989)
> 目標:讓玩家能**實際打贏終戰 Namtar 並看到結局**——「remake 可通關」的收官。
> 策略:**不**靠 res3 全戰鬥閉環(卡遊戲層 context,docs/reverse-engineering/42 §14;非 opcode 缺失),
>   改接 remake 自己已可玩的 `combat_loop`。接續 docs/gameplay/55(主線 gate + 結局事件文字)。

---

## 0. 結論摘要

| 項目 | 狀態 |
|---|---|
| 終戰 Namtar 遭遇 → 戰鬥 | **可玩**:area27 tile 0x18/0x19 op_8A encounter → `begin_namtar` → S_COMBAT(隊伍 vs Namtar Boss,走 `combat_loop`,單次攻擊 = bytecode 真值公式)。 |
| 打贏 Namtar | **確定性可勝**:預設隊伍(第 0 名持受祝福的自由之劍)多 seed 皆 Victory、隊伍存活、Namtar HP=0(`verify_ending` E1,7 seed)。 |
| 勝利 → 結局序列 | **S_ENDING**:Victory → `enter_ending`,播 area27 結局敘事 + 結局段落(131/132/135/137/138)+ remake 勝利訊息 + 全劇終,繁中、可捲動翻頁,結束回標題選單(互動)/退出(headless)。 |
| 端到端驗證 | headless `--fight-namtar --combat-rounds N`(打贏)→ `--ending`(截圖);`verify_ending` ctest(可勝 + 確定性);截圖 4 張。 |
| 誠實標示 | 戰鬥公式 = bytecode 真值;**Namtar Boss 屬性 + 自由之劍祝福加成 = remake 平衡設計**(原版 op_8A 怪物 id 無乾淨 res31 record);**結局序列 = remake 組合**(非原版單一 script)。 |
| 回歸 | ctest **22/22**(原 21 + `verify_ending`);diff_trace 逐指令一致;verify_scene_golden 6/6;render_sweep 154。**未改 VM source**。 |

---

## 1. Namtar 遭遇怎麼接 combat_loop(怪物 id 來源)

### 1.1 終戰 = area27 tile 0x18/0x19 op_8A combat encounter

- `probe_encounter_id assets/bundle 27`(新工具:跑事件格 + `headless_encounter`,記 op_8A
  設的怪物 id)實測:
  - **area27 tile 0x18 / 0x19** → op_8A,monster_id = **0x03**,steps ≈ 2505。
  - 龍谷(area32)tile 0x0A/0x0B/0x10/0x12 → 亦 **0x03**。
  - 對照 area18 tile 0x19 → **0x2F**(直接 operand,31 steps,未載 res3)。
- **關鍵發現(誠實)**:area27 終戰格的 monster_id `0x03` **不是乾淨的 res31 record 索引**——
  它是「戰鬥設定腳本 res3(經 op_58 載入)走到 ~2505 步時 `word_3AE2` 的值」,是
  **戰鬥設定流程的產物**(多個不同遭遇格都得到同一個 0x03)。
  → **Namtar 沒有可逐欄對齊的 res31 21B 屬性記錄**(monsters.bin 的 25 筆是 area-1 系早期怪,
  無 Namtar)。攻略 §5.20 + 段落 131/132/135 只給「極強最終 Boss」的定性描述,無數值。

### 1.2 接法:begin_namtar → combat_loop

- `src/main.cpp` `begin_namtar` lambda:隊伍(`Party::load_default` 或讀檔)vs 單一 Namtar Boss
  (`game::make_namtar()`),走既有 `game::CombatLoop`(4 人 vs 怪群、回合制、勝負、XP)。
  - 隊伍**第 0 名**套「自由之劍受祝福」(`game::make_blessed_hero`):攻略「Irkalla 0x80 +
    永恆之神 0x10 祝福自由之劍 → 一擊削 100 HP;永恆之神 +3 全屬性」→ AV/DV +3、傷害骰
    升級(4d12+20)。`--no-bless` 可關閉。
  - 怪物 sprite 借胡姆巴巴立繪(無 Namtar 專屬 sprite;誠實:占位)。
- **觸發點**:
  - **互動**:S_GAME 移動迴圈踩到 area27 tile 0x18/0x19 → `begin_namtar()`(取代 run_event 的
    op_8A halt)。
  - **headless**:`--fight-namtar` 旗標 → `begin_namtar()`(end-to-end 驗證 + 截圖)。
- **單次攻擊**仍走 `game::resolve_attack`(combat.cpp;命中 1d16+3 門檻 13+AV−def、徒手傷害
  dice+STR/5 = **bytecode 真值**,docs/reverse-engineering/42 §11/§12)。Boss 只提供「一個強力 Combatant」,
  **不重算公式**。

### 1.3 Namtar Boss 屬性(remake 平衡設計,誠實標示)

| 欄位 | 值 | 依據 |
|---|---|---|
| HP(STUN) | 120 | remake 平衡:起始隊伍 STUN 極低(12–16),受祝福勇者(4d12+24)約 3–4 擊可破。 |
| AV / DV | 5 / 6 | 精銳命中 / 高閃避(roll-under 門檻 = 13+AV−def)。 |
| AC | 0 | 不減傷(AC 在命中側,bytecode 真值)。 |
| 傷害 | 1d6+1 | 會傷人但不秒殺滿血隊員,讓祝福勇者撐到打完。 |

> **誠實鐵則**:Boss 屬性 + 祝福加成 = **remake 設計**(平衡至可勝),**非原版逐欄真值**
>   (原版 op_8A monster_id 為設定流程產物,無乾淨 res31 record)。標示見 `combat.hpp`
>   `make_namtar` / `make_blessed_hero` 檔頭與 `verify_ending` 檔頭。

---

## 2. 勝利 → 結局序列怎麼串(用哪些段落/emit)

### 2.1 S_ENDING + enter_ending(main.cpp)

- 新狀態 `S_ENDING`;Namtar 戰鬥 Victory(`enc.is_namtar && enc.victory`)→ `enter_ending()`。
- `enter_ending` 把結局組成單一可捲動文件(沿用 `ParaViewer`,`para_n = -1` 表示結局),
  黑底 + 全螢幕捲動 overlay(↑↓ 捲行、Space/Enter 翻頁、捲到底 Enter/Esc 結束)。
- 結束:互動 → 回標題選單(可再開新局);headless(`--ending`/`--fight-namtar`)→ 退出。

### 2.2 結局文件組成(`build_ending_doc`)——真實素材 + remake 組合

1. **終戰敘事**(area27 真實 emit 鍵,已 bundle/已繁中 events.tsv):
   - Namtar 現身(tile 0x0C「一個聲音自陰影中響起…」)
   - 鐵頭巴克(tile 0x22)、南方納達軍隊(0x24)、單挑整支軍隊(0x26)
2. **remake 勝利結算**:「你高舉受祝福的自由之劍…」「納達——深淵之獸——崩解殞滅。」
3. **結局段落**(手冊段落,已 bundle 真實素材;攻略 §5.20 註明):
   **131 / 132 / 135**(納達現身/對話)、**137 / 138**(Irkalla 與自由之劍重生 / 結局)。
4. **收尾**:屍身送靈魂之泉 → 納達之坑 → 「歐西納終於自由了。盡情享受這最後勝利的甘美
   滋味吧!」(攻略結局語)→ **全 劇 終**(THE END)。

> **誠實標示**:原版「勝利後結局畫面」由戰鬥流程/DRAGON.COM 主控觸發,**不在任何 level
>   event script** 中(掃全 40 關證實,docs/gameplay/55 §3.3),逆不出獨立結局 script。本序列以
>   **已 bundle 的真實素材組合**:敘事鍵 + 段落 = 真實;「組合與串接」= remake 設計。
>   結局文件首段即明示「(remake 組合結局:以已收錄的手冊段落＋第27區決戰敘事組成,
>   非原版單一腳本)」。

---

## 3. 可通關路徑驗證(headless 端到端 + ctest)

### 3.1 `verify_ending`(ctest)

- **E1 終戰可勝(7 seed)**:預設隊伍 + 受祝福自由之劍 vs Namtar Boss,固定 seed 跑
  `combat_loop` → 全部 **Victory + 隊伍存活 + Namtar HP=0**(2–4 回合)。可勝且非僥倖。
- **E2 勝利 → XP**:`xp_award()==80`(DOS 實機扁平制)。
- **E3 確定性**:同 seed 兩次執行 outcome/rounds/hp/alive 完全一致。
- **E4 --no-bless 對照**(INFO,不硬性 assert):記錄未受祝福戰況差異。

### 3.2 端到端(headless app)

```bash
# 打贏 Namtar → 進結局序列(SDL_VIDEODRIVER=dummy)
./opendw_remake --fight-namtar --combat-rounds 30 --frames 3
#   → begin_namtar party=4 blessed=1 namtar_hp=120
#   → combat(group): outcome=1(Victory) xp=80
#   → ENTER ENDING (lines=70, page_count=5)

# 直接看結局序列(demo / 截圖;不打 Namtar)
./opendw_remake --ending --para-scroll N --frames 1 --dump out.ppm
```

### 3.3 截圖(`docs/media/screenshots/endgame/`)

| 檔 | 內容 |
|---|---|
| `namtar_combat.png` | 終戰畫面:Namtar 立繪 + 隊伍面板 + F:戰鬥/R:逃跑/C:施法 + 繁中戰報 |
| `ending_page0.png` | 結局首頁:標題「火龍之戰・結局」+ remake 組合說明 + 終戰敘事 + 勝利結算 |
| `ending_page2.png` | 結局中段:結局段落(131/132/135…) |
| `ending_page4.png` | 結局末頁:段落 137/138 + 「歐西納終於自由了…」+ 全 劇 終 |

---

## 4. 誠實標示總表(真值 vs remake 組合 vs 暫定)

| 項目 | 狀態 |
|---|---|
| 單次攻擊命中/傷害公式 | **bytecode 真值**(resolve_attack;docs/reverse-engineering/42 §11/§12) |
| RNG(op_4D) | bytecode 移植,對拍 oracle |
| XP(清怪 +80) | DOS 實機真值(docs/reverse-engineering/43) |
| 行動順序 / 目標選擇 | remake 設計(combat_loop;SDA 定性) |
| **Namtar Boss 21B 屬性** | **remake 平衡設計(暫定)** —— op_8A monster_id 無乾淨 res31 record,誠實標示 |
| **自由之劍祝福加成** | **remake 設計** —— flags[85] op_5F/61 已實作,但「祝福→具體戰鬥加成數值」原版未逆出 |
| **結局序列** | **remake 組合** —— bundled 段落(真實)+ area27 敘事(真實)+ 組合串接(設計);非原版單一 script |
| 結局段落 131/132/135/137/138 | 真實素材(手冊段落,已 bundle) |
| area27 結局敘事 emit | 真實素材(level script emit,已繁中) |

---

## 5. 改了哪些檔

- **`src/game/combat.hpp` / `combat.cpp`**:新增 `make_namtar()`(終戰 Boss)、
  `make_blessed_hero()`(受祝福自由之劍持有者);檔頭誠實標示(屬性暫定 / remake 設計)。
- **`src/main.cpp`**:新增 `S_ENDING` 狀態、`--fight-namtar` / `--ending` / `--no-bless` 旗標、
  `EncounterState.is_namtar`、`begin_namtar` / `build_ending_doc` / `enter_ending` lambda;
  area27 tile 0x18/0x19 移動觸發 Namtar;Victory → 結局;S_ENDING 輸入/渲染;draw_para_overlay
  結局標題;結局 `--para-scroll` headless。
- **`assets/i18n/zh-TW/events.tsv`**:+8 條(勝利訊息 / 結局收尾 / 標題 / 組合說明)。
- **`assets/fonts/cjk24.atlas`**:重生(2017→2026 glyph,補新譯文字形)。
- **`tools/verify/verify_ending.cpp`**(ctest)+ `probe_encounter_id.cpp` / `probe_namtar_balance.cpp`
  (grounding 觀測);`CMakeLists.txt` 註冊。
- **`docs/media/screenshots/endgame/`**:+4 張截圖(namtar_combat / ending_page0/2/4)。
- **`docs/gameplay/56`**:本檔。**未改 opendw;未改 VM source(interpreter/vm_state);DRAGON.COM/圖檔未入庫。**

## 6. 卡點與限制(精確,不臆造)

1. **Namtar 戰鬥用 remake combat_loop,非 res3 全戰鬥閉環**:res3 卡 per-character 動作指派
   狀態機 + 無獨立戰鬥 oracle(docs/reverse-engineering/42 §14),本任務**不重攻**(誠實記錄)。combat_loop 的
   單次攻擊公式是 bytecode 真值,但「Boss 屬性 / 行動編排 / 祝福加成」是 remake 設計。
2. **結局為 remake 組合**:原版勝利結局畫面逆不出獨立 script(docs/gameplay/55 §3.3),以 bundled
   素材組合,首段明示。
3. **in-game 觸發未加 headless 移動注入驗證**:互動移動踩格 → begin_namtar 已 wired(與
   `--fight-namtar` 同一 `begin_namtar` 路徑),但 headless 端到端走 `--fight-namtar`
   (`--press` 僅作用於選單,未對 in-game 移動逐步注入;屬另案)。
