# 58 — 文件盤點與漂移稽核 (Doc Audit & Drift)

> **盤點日期**:2026-06-16
> **角色**:專案負責人(唯讀盤點)
> **性質**:**唯讀**。本報告只新增本檔,**不搬檔、不改文件、不改 src、不動 git**。搬移與修正由使用者審後執行。
> **基準**:對照「目前實作」= `opendw_remake/src/` + `tools/verify/` ctest + 最新 docs(47–56)+ 最新 git 狀態(PR #100–#123)。
> **與 00 的關係**:`docs/assessment/00_DOC_AUDIT.md`(2026-06-10)是**早期審查**,只涵蓋資產/翻譯/手冊軸,**早於** remake 戰鬥真值化、連通 38/40、可通關鏈(47–56)。本檔接續 00,聚焦「**與目前實作的 drift**」。
> **注意**:`docs/assessment/57_PM_REVIEW.md` 由另一 agent 撰寫,本盤點不重複其結論、不修改該檔。

---

## 0. 目前實作真值(本盤點的對照基準)

逐項以 src / ctest / git 驗證,作為判定 drift 的事實依據:

| 項目 | 真值 | 出處 |
|------|------|------|
| VM opcode(remake 實作) | **119** | `src/vm/interpreter.cpp` 的 `kImpl` 表 `t[0xNN]=` 賦值共 **119** 筆;根 README L33「~119 opcode」 |
| opendw(C oracle)opcode 命名/實作 | 139 命名 / 117 NULL(原版口徑) | `docs/reverse-engineering/OPCODE_REFERENCE.md` L57–64 |
| ctest 數 | **22/22** | `opendw_remake/CMakeLists.txt` `add_test` 共 22;根 README L39「22/22」 |
| 連通性 | **38/40 area** | 根 README L19/L35;`docs/gameplay/54` Update 2 |
| 第一人稱 viewport | **已 port 並接入 S_GAME** | `src/main.cpp` L285/333/356 `--fp`→S_GAME;`render_sweep` 全 40 關 154 case |
| 戰鬥單次攻擊公式 | **bytecode 真值**(命中 `roll≤13+AV−(DV+AC)`、徒手傷害 `骰+floor(STR/5)`) | `src/game/combat.cpp` L164–166;`src/game/combat_loop.cpp`;`verify_combat_script` 對拍 res3 |
| 戰鬥 Boss/怪物數值 | **remake 設計**(誠實標示) | `src/game/combat.hpp` L116–117/L163/L189(Namtar Boss、怪物 21-byte 對映、weapon op_68 = 暫定) |
| 終戰→結局 | **可玩**(`--fight-namtar`→勝利→`S_ENDING`) | `src/main.cpp` L293/343/412/1168;`verify_ending` |
| 主線繁中事件 | 200+ 鍵 + 147 段落 + 結局 | 根 README L36;`docs/gameplay/55`/`docs/gameplay/56` |

> 關鍵釐清:**「119(remake 實作)」與「139/117(opendw 原版口徑)」是兩套不同計數**,不是矛盾。OPCODE_REFERENCE 講 opendw oracle 的 `targets[]`;remake 講自己實作了幾個。報告引用時要標清楚口徑,避免被當成 drift。

---

## 1. 逐文件盤點表

圖例:✅ 現行有效 ｜ 🟡 部分過時(需更新) ｜ ⛔ 已被取代(建議 `_deprecated/`)｜ 📌 歷史紀錄(保留並標明)

### 1.1 根目錄 / 跨專案

| 檔案 | 狀態 | drift(文件 vs 實作) | 建議動作 |
|------|------|----------------------|----------|
| **`README.md`(根)** | 🟡 **最嚴重 drift** | 上半(L17–39)已更新(38/40、22/22、~119 opcode、bytecode 真值);**下半(L41–276)整段過時且與上半自相矛盾**:L49「ALL_TEXT_FROM_DATA1.txt(3926 條)」當成果、L66「engine.c…115 個 opcode」、L124–129「DATA1 文字提取(3926 條)」「640×480 未做」、L162–169「~85 個未實作 opcode」、L269–276「已命名 op 143 / 未實作 ~85」、L104–157 **建置/執行/中文化狀態整段重複貼兩次**、L119「./src/fe/sdldragon」(已非主產物,主產物是 `opendw_remake/build/opendw_remake`) | **更新**:刪除 L41 以後重複/過時段;統一為 remake 口徑(119 opcode、22/22、bytecode 真值、38/40);3926 條改述為「已知雜訊,見 `_deprecated/`」 |
| **`CONTEXT.md`** | ✅ 現行有效 | 術語/譯名表,與 `src/i18n` 用詞一致;area/換場機制節(L95–102)與 `src/world` 實作對齊 | 保留 |
| **`SKILL.md`(根)** | 🟡 部分過時 | L44「進度(2026-06-10,8 個 PR 已合併)…R1 batch 1 = **15/256 opcode**」— 現為 119;「R0 ✅、R1 batch 1」階段描述停在 6/10 | **更新**進度節至 2026-06-16(119 opcode、22 ctest、可通關);或標為歷史快照 |
| **`skills/opendw-chinese-localization.md`** | 🟡 部分過時 / 📌 | 內容停在 2026-06-09(L14「6.5/10 規劃良好,實作才剛開始」、L21「實作尚未開始」、翻譯覆蓋率 2.5%/怪名 0%)。與 `docs/assessment/60_SKILL.md` 高度重疊 | **標歷史快照**;或合併到 `docs/assessment/60_SKILL.md` 後擇一保留(見 §3) |

### 1.2 規劃與分析(00–08)

| 檔案 | 狀態 | drift | 建議動作 |
|------|------|-------|----------|
| `00_DOC_AUDIT.md` | 📌 歷史紀錄 | 2026-06-10 的早期審查,涵蓋面僅到資產/翻譯/手冊;**不含** remake 戰鬥/連通/可通關。本身正確,但已非「最新審查」 | 保留;頂部加一行「最新審查見 58」(由使用者執行) |
| `01_PLAN.md` | 📌 歷史紀錄 | 早期計畫;字型 11×11 vs 22×22/24×24 矛盾(03 已指出);頂部已註記以 07 為準 | 保留(歷史) |
| `02_ANALYSIS.md` | ✅ 現行有效 | ASM↔C 反組譯對照,可信。內含 opendw 口徑 opcode 數(143 命名),非 remake 口徑——非 drift,但易混淆 | 保留;可加一行口徑說明 |
| `03_REVIEW_REPORT.md` | 📌 歷史紀錄 | 頂部已有 3926 條撤回 banner;但 L32 仍留「3926 個文字串(優質成果)」與 banner 自相矛盾 | 保留;可在 L32 就地加註(非必要) |
| `05_SDL2_IMPLEMENTATION.md` | ✅ 現行有效 | SDL2 取代 DOS 設定/音效之規劃,獨立有效 | 保留 |
| `06_IMPLEMENTATION_PLAN.md` | 📌 歷史紀錄 | 頂部已註記引用舊 3926 數據(雜訊),指向 07;CJK 顯示路徑已被 remake 實際做法(雙層渲染 ADR 0002)取代 | 保留;指向 ADR 0002 + remake |
| `07_REVISED_PLAN.md` | 🟡 部分過時 | 2026-06-10 列為「現行計畫 v2」,文字萃取部分正確;但其 roadmap 的 Phase 3(戰鬥/CJK)**現已大量完成**(combat_loop、雙層 CJK、--win640) | **更新**完成狀態,或標「Phase 3 多數已落地,見 47–56」 |
| `08_READ_PARAGRAPH_FEATURE.md` | ✅ 現行有效 | 已含 2026-06-13/14 實作完成更新註;與 `src/resource/paragraphs.cpp` 對齊 | 保留 |

### 1.3 翻譯(10–15)

| 檔案 | 狀態 | drift | 建議動作 |
|------|------|-------|----------|
| `10_TRANSLATION.md` | ✅ 現行有效 | D1/D2 已清理雜訊 + 對齊 CONTEXT | 保留 |
| `11_TRANSLATION_DIALOGUE.md` | ✅ 現行有效 | D2 已統一譯名 | 保留 |
| `12_TRANSLATION_ITEMS.md` | ✅ 現行有效 | D4 已刪臆測物品 | 保留 |
| `13_TRANSLATION_SKILLS.md` | ✅ 現行有效 | 技能名來源已註記 | 保留 |
| `14_TRANSLATION_MONSTERS.md` | ✅ 現行有效 | 已指向 26 | 保留 |
| `15_TRANSLATION_DRAFT.md` | ✅ 現行有效 | 草稿,被 SKILL/CONTEXT 引用;`docs/README.md` 未列入索引 | 保留;**補進 README 索引** |

### 1.4 資料分析 / opcode(20–26 + OPCODE_REFERENCE)

| 檔案 | 狀態 | drift | 建議動作 |
|------|------|-------|----------|
| `_deprecated/20_ALL_TEXT_FROM_DATA1.txt` | ⛔ 已棄置(已在 `_deprecated/`) | 暴力萃取雜訊;已正確歸檔 | 維持現況 |
| `ALL_TEXT_FROM_SCRIPTS.txt` | ✅ 現行有效 | 乾淨真實文字 | 保留 |
| `21_DATA1_RESOURCE_INDEX.md` | 📌 歷史紀錄 | 頂部已註記文字數不可信 | 保留 |
| `22_DATA1_SECTION_DETAILS.md` | 📌 歷史紀錄 | 頂部已註記 0x08–0x16 文字數為假象 | 保留 |
| `23_SOURCE_CODE_MAP.md` | ✅ 現行有效 | opendw 原始碼地圖(22 檔)| 保留 |
| `24_SCRIPT_TEXT_MAPPING.md` | 📌 歷史紀錄 | 引用舊資料源,已註記 | 保留 |
| `25_OPCODE_INTERPRETATION.md` | ✅ 現行有效(口徑須標) | opendw 口徑 139/117/4;非 remake 119——非 drift,但建議加「remake 已實作 119」交叉註 | 保留;加 remake 口徑交叉註 |
| `OPCODE_REFERENCE.md` | ✅ 現行有效(口徑須標) | 同上;對外雙語版 | 保留;加 remake 口徑交叉註 |
| `26_MONSTERS_AND_SPRITES.md` | ✅ 現行有效 | 怪物名 res31 正解;但「怪名 0 個中文」已被 remake 反證(日文 23/23 + 繁中已補,根 README L36) | 保留;可註「remake 已補日文 23/23 + 繁中」 |

### 1.5 手冊與攻略(30–40)

| 檔案 | 狀態 | drift | 建議動作 |
|------|------|-------|----------|
| `_deprecated/30`、`_deprecated/31` | ⛔ 已棄置(已歸檔) | 被 33/34 取代 | 維持現況 |
| `32_EN_MANUAL_TEXT.md` | ✅ 現行有效 | 英文手冊 OCR | 保留 |
| `33_MANUAL_TRANSCRIPTION.md` | ✅ 現行有效 | 中文手冊精確轉寫 | 保留 |
| `34_READ_PARAGRAPHS.md` | ✅ 現行有效 | 段落精確轉寫;與 `paragraphs.tsv` 對齊 | 保留 |
| `35/36/37_SOFTWORLD_*.md` | ✅ 現行有效 | 軟世攻略 25/26/27 | 保留 |
| `38_SOFTWORLD_WALKTHROUGH.md` | ✅ 現行有效 | 攻略真值,被 48/54 連通盤點引用;**README 未列** | 保留;**補 README 索引** |
| `39_SOFTWORLD_FULLTEXT_AND_MAPS.md` | ✅ 現行有效 | 同上;**README 未列** | 保留;**補 README 索引** |
| `40_ORIGINAL_DOCS_SUMMARY.md` | 📌 歷史紀錄 | 怪物 sprite 編號已註記 | 保留 |

### 1.6 技術 / 戰鬥 / 資料格式(41–46)

| 檔案 | 狀態 | drift | 建議動作 |
|------|------|-------|----------|
| `41_TECHNICAL_DEBT.md` | 🟡 部分過時 | 列 opendw 口徑「117 未實作 / 139 命名」(對 opendw 仍成立);但作為 remake 待辦時口徑不一致 | 保留;標「opendw 口徑;remake 已實作 119」 |
| `42_COMBAT_BYTECODE.md` | ✅ 現行有效(核心真值來源) | 多輪更新誠實;**L55–57「戰鬥結算仍是乾淨室 placeholder」針對的是舊 `combat.cpp` 切片**,而**可玩路徑 `combat_loop` 已用 bytecode 真值公式**——文中已分層標示,但易被讀者誤讀為「整個戰鬥還是 placeholder」 | 保留;建議加一行導讀「單次攻擊公式=真值;Boss/怪物數值=設計;見 56」 |
| `42_WHY_DATA1_DATA2.md` | ✅ 現行有效 | 設計推理 | 保留;**注意與 42_COMBAT 同號**(見 §4) |
| `43_DOS_PLAYTEST.md` | ✅ 現行有效 | DOS 實機交叉驗證;小樣本傷害推斷已被 42 bytecode 修正(文中已註) | 保留 |
| `44_DATA_FORMATS_AND_MECHANICS.md` | ✅ 現行有效 | 資料格式參考;AC 行為「待釐清」誠實標示 | 保留 |
| `46_PC98_JA_EXTRACTION.md` | ✅ 現行有效 | PC98 日文萃取;與 remake 日文怪名/事件對應 | 保留 |

> **45 號缺號**:docs 編號 44 後跳到 46,無 45。非錯誤,但索引可註明。

### 1.7 最新評估 / 連通 / 結局(47–56)

| 檔案 | 狀態 | drift | 建議動作 |
|------|------|-------|----------|
| `47_REMAKE_ASSESSMENT.md` | 🟡 部分過時 | **可玩性 62/100(L22)已過時**——評估時點(2026-06-15)在連通 38/40 + 可通關鏈 + 結局之前;ctest「19/19(L31)」現為 22/22;opcode「~117/256(L59)」現為 119 | **更新分數與數字**,或頂部加「此為 2026-06-15 快照,後續 48–56 已推進(連通 38/40、可通關、22/22)」 |
| `48_COMPLETABILITY_ROADMAP.md` | 🟡 部分過時 | roadmap 多數已完成:P0 連通(已 38/40)、P1 quest gate(`docs/gameplay/55` 已通)、P3 結局(`docs/gameplay/56` 已可玩);「<10% 連通」「只 1 條真跨區」已被後續推翻 | **更新**完成狀態,或標「roadmap 已大幅落地,見 54/55/56」 |
| `49_GAP_AUDIT.md` | 🟡 部分過時 | 多面向標 🟡/⛔ 未實作(裝備/use item/商店/NPC 招募/結局觸發);其中**結局觸發已實作**(`docs/gameplay/56`、`S_ENDING`);ctest「20/20(L223)」現 22/22;連通敘述停在中段 | **更新**結局/連通/ctest 欄;其餘未實作項仍成立 |
| `50_BUILD.md` | ✅ 現行有效 | 建置指南,無過時數字 | 保留 |
| `51_TEST_PLAN.md` | ✅ 現行有效 | 測試策略;**與 `51_WORLDMAP_AREA_SWITCH_RE.md` 同號**(見 §4) | 保留;**改號去衝突** |
| `51_WORLDMAP_AREA_SWITCH_RE.md` | 🟡 部分過時 | 連通「27/40(L30)」是該輪結論,**現為 38/40**(54 已推進);機制逆向本身正確 | **更新**連通數字,或標「27/40 為當輪;最終 38/40 見 54」;**改號**去衝突 |
| `52_MAINLINE_EVENT_STRINGS.md` | ✅ 現行有效 | 已含 op_79/op_5B 已實作的更新註 | 保留 |
| `53_EVENTS_TRANSLATION_REVIEW.md` | ✅ 現行有效 | 翻譯待審清單 | 保留 |
| `54_WORLDMAP_REACHABILITY_AUDIT.md` | ✅ 現行有效(連通真值來源) | Update 2 = 38/40 的最終結論;誠實標示 Phoebus(6)逆不出 | 保留 |
| `55_MAINLINE_QUEST_GATE_AND_ENDGAME.md` | ✅ 現行有效 | quest gate 系統 + 結局可觸發;ctest 21/21(該輪) | 保留;ctest 現 22/22(56 後) |
| `docs/gameplay/56_PLAYABLE_ENDING_CHAIN.md` | ✅ 現行有效(最新真值) | 可通關結局鏈 + 22/22;誠實標示 Boss/結局=remake 設計 | 保留 |

> **57 號**:`docs/assessment/57_PM_REVIEW.md` 由另一 agent 撰寫,本盤點不評。

### 1.8 索引 / Skill / 二進位

| 檔案 | 狀態 | drift | 建議動作 |
|------|------|-------|----------|
| `docs/README.md` | 🟡 部分過時 | **未列 38/39/15/47–56**(只到 60_SKILL);「目前進度」表(L136–147)停在 2026-06-10(「CJK 渲染/SDL2 ❌ 未開始」「文字萃取修正中」)——與 remake 可通關現況嚴重不符 | **重整索引**:補 38/39/15/46–56;更新進度表;見 §5 |
| `99_INDEX.md` | 🟡 部分過時 | 2026-06-10 重建版;同樣未含 47–56 與 remake 進度 | **更新**或與 README 合併(見 §5) |
| `60_SKILL.md` | 📌 歷史紀錄 | 2026-06-09 經驗記錄;3926 條已加修正註;與根 `SKILL.md`、`skills/opendw-chinese-localization.md` 三方重疊 | 保留;**三方收斂**(§3) |
| `docs/reverse-engineering/dragon.asm`、`*.pdf`、`*.rar` | ✅ 現行有效 | 二進位參考 | 保留 |

### 1.9 opendw_remake/ 內部文件

| 檔案 | 狀態 | drift | 建議動作 |
|------|------|-------|----------|
| `opendw_remake/README.md` | 🟡 部分過時 | 「現況(R0 進行中)(L22)」「R1 進行中…~117/256 opcode(L34/L38)」——**現為 119 + 可通關**;與根 README 上半(可通關)不一致 | **更新**:R 階段現況、opcode 119、可通關 |
| `opendw_remake/ARCHITECTURE.md` | ✅ 現行有效 | 架構藍圖 + R0–R6 階段表;設計仍成立,階段「現況」欄可能落後 | 保留;階段表現況欄可更新 |
| `docs/engine/REWRITE_READINESS.md` | 🟡 部分過時 | 2026-06-12 里程碑;「~117/256(L25)」現 119;R1「進行中」現已遠超 | **更新**或標里程碑快照 |
| `docs/engine/VIEWPORT.md` | 🟡 部分過時 | **L47「俯視圖為占位」、L51「第一人稱 viewport 尚未 port」已過時**——`--fp` 已接入 S_GAME 且 `render_sweep` 全 40 關 PASS(`src/main.cpp` L333/356) | **更新**:標 viewport 已 port + golden PASS |
| `docs/engine/VIEWPORT_COMPOSE.md` | ✅ 現行有效 | step 1/2 對拍結論已完成 | 保留 |
| `docs/engine/CONTROLS.md` | 🟡 部分過時 | 2026-06-14;**L53「戰鬥結算為乾淨室 placeholder」已過時**(combat_loop 已用 bytecode 真值公式);未列 `--fight-namtar`/`--ending`/建角等新指令 | **更新**:戰鬥真值說明 + 補新 flag |
| `docs/adr/0001`、`0002` | ✅ 現行有效 | Asset bundle、雙層 CJK 渲染 ADR;與實作對齊 | 保留 |
| `opendw_remake/assets/**/README.md`、`automap_demo/`、`party_demo/` 等 | ✅ 現行有效 | 資產/demo 說明 | 保留 |

---

## 2. 主要 drift 清單(Top N,依嚴重度)

1. **根 `README.md` 下半(L41–276)整段過時且自相矛盾** — 同檔上半已寫「可通關/38/40/22/22/~119/bytecode 真值」,下半仍是「3926 條成果/115~143 opcode/~85 未實作/640×480 未做/src/fe/sdldragon」,且建置/狀態節**重複貼兩次**。**最高優先**。
2. **「戰鬥結算 = 乾淨室 placeholder」的舊說法散落多處**(`CONTROLS.md` L53、`42_COMBAT` L55–57 的字面、舊 `combat.hpp` 註)。實際:**可玩路徑 `combat_loop` 的單次攻擊公式已是 bytecode 真值**(命中 `13+AV−(DV+AC)`、徒手傷害 `骰+floor(STR/5)`,`verify_combat_script` 對拍 res3);僅 **Boss/怪物數值/weapon op_68 = remake 設計**(已誠實標)。需把「整個戰鬥是 placeholder」澄清為分層真值。
3. **opcode 數字三套並存且未標口徑**:remake 實作 **119**(`interpreter.cpp`);opendw oracle **139 命名/117 NULL**(OPCODE_REFERENCE/25/41);歷史快照 **15/256**(SKILL)、**~117/256**(REWRITE_READINESS/47/49)、根 README 舊段 **115/143/~85**。非全是 drift(口徑不同),但**未標口徑 → 讀者誤判**。建議全專案統一寫「remake 119 / opendw 139·117」。
4. **連通數字多版本**:`51_WORLDMAP…` 27/40、`54` 中途 33/40、最終 **38/40**(54 Update 2 + 根 README)。早期文件的 27/33 未標「當輪結論」。
5. **可玩性 62/100(`47` L22)已過時** — 評估在可通關鏈之前;同檔 ctest 19、opcode ~117 一併過時。
6. **第一人稱 viewport「尚未 port」(`VIEWPORT.md` L47/51)已過時** — `--fp` 已接 S_GAME,40 關 154 case byte-for-byte PASS。
7. **roadmap 已完成卻仍列待辦**:`48`(P0 連通/P1 quest/P3 結局)、`49`(結局觸發未實作)、`07`(Phase 3 戰鬥/CJK)——多數已落地(54/55/56),文件未回填完成狀態。
8. **docs/README.md + 99_INDEX 進度表停在 2026-06-10**(「CJK/SDL2 未開始」「文字萃取修正中」),與 remake 可通關現況不符;且**未索引 38/39/15/46–56**。
9. **3926 條雜訊仍以「成果」出現在根 README L49/L124**(其他文件已加撤回註,根 README 漏網)。

---

## 3. 建議 `_deprecated/` 清單

> 本次盤點**未發現需新移入 `_deprecated/` 的全檔報廢檔**——多數舊文件具歷史價值(📌),靠頂部註記指向新來源即可,不必歸檔。已在 `_deprecated/` 的 6 項(04、05_VERIFICATION、20、30、31、verify_extraction.py)維持現況。

**唯一邊界案例(供使用者裁量,非強制)**:

| 候選 | 理由 | 取代者 | 建議 |
|------|------|--------|------|
| `skills/opendw-chinese-localization.md` | 與 `docs/assessment/60_SKILL.md` + 根 `SKILL.md` 三方重疊,內容停在 2026-06-09「實作尚未開始」 | `docs/assessment/60_SKILL.md`(較完整)+ 根 `SKILL.md`(較新) | **三選一收斂**:擇一為正本,另兩者標歷史或併入;不必進 `_deprecated/`(skill 檔有觸發用途) |

**結論**:`_deprecated/` 清單**本輪無新增建議**。drift 文件用「更新/加註」處理,不靠歸檔。

---

## 4. 建議更新清單(依優先序)

| 優先 | 檔案 | 動作摘要 |
|------|------|----------|
| P0 | 根 `README.md` | 刪 L41 後重複/過時段;統一 remake 口徑(119/22-22/38-40/bytecode 真值/可通關);移除 `src/fe/sdldragon` 主產物敘述;3926 改述為雜訊 |
| P0 | `docs/README.md` + `99_INDEX.md` | 補索引 38/39/15/46–56;更新「目前進度」表至可通關現況;二者擇一為正本(見 §5) |
| P1 | `opendw_remake/README.md`、`REWRITE_READINESS.md` | R 階段現況 + opcode 119 + 可通關 |
| P1 | `docs/engine/VIEWPORT.md`、`CONTROLS.md` | viewport 已 port;戰鬥 bytecode 真值;補 `--fight-namtar`/`--ending`/建角 flag |
| P1 | `47`、`48`、`49` | 加「2026-06-15 快照」帽 + 回填完成狀態(62/100、19 ctest、~117、roadmap、結局觸發已實作) |
| P2 | `51_WORLDMAP…`、`07` | 連通 27→38/40 標「當輪 vs 最終」;07 Phase 3 已落地 |
| P2 | `42_COMBAT` | 加導讀:單次公式=真值 / Boss·怪物=設計(分層) |
| P2 | `25`/`OPCODE_REFERENCE`/`41` | 加「remake 已實作 119」口徑交叉註 |
| P3 | 根 `SKILL.md`、`skills/opendw-chinese-localization.md`、`60_SKILL.md` | 三方收斂 + 進度更新 |
| P3 | `00_DOC_AUDIT.md` | 頂部加「最新審查見 58」 |

---

## 5. 文件結構健康度與索引重整

### 5.1 結構健康度評語

- **整體**:文件量大但**誠實度高**——絕大多數舊結論都有就地撤回註或指向新來源(尤其翻譯/3926 軸,`00` 已處理乾淨)。主要風險不是「謊報」,而是**「快照未回填」**:remake 在 2026-06-13~16 大幅推進(連通、戰鬥真值、結局),而 6/10–6/15 的評估/roadmap/README 進度表沒同步,造成讀者(尤其根 README)看到自相矛盾。
- **最弱環**:根 `README.md`(對外門面卻自相矛盾)、`docs/README.md`/`99_INDEX` 進度表(過時且漏索引)。
- **口徑不統一**是系統性問題:opcode(119 vs 139/117 vs 15/~117)、連通(27/33/38)、ctest(8/19/20/21/22)在不同時點文件並存,未標時間戳/口徑。

### 5.2 編號衝突與缺號(結構問題)

| 問題 | 細節 | 建議 |
|------|------|------|
| **42 號雙占** | `42_COMBAT_BYTECODE.md` 與 `42_WHY_DATA1_DATA2.md` 同號 | 其一改號(如 WHY 移至 45 空號) |
| **51 號雙占** | `51_TEST_PLAN.md` 與 `51_WORLDMAP_AREA_SWITCH_RE.md` 同號 | 其一改號 |
| **45 缺號** | 44 後跳 46 | 可用 45 收容上列改號檔 |

### 5.3 `docs/README.md` 索引是否需重整

**需要**。具體:
1. **補漏**:38、39、15、46、47、48、49、51(兩支)、52、53、54、55、56、57(指向)、本檔 58、原 `opendw_remake/docs/*`(已併入根 `docs/` 各子目錄)、ADR 0001/0002。
2. **更新「目前進度」表**(L136–147):現況應為「可從頭玩到結局、連通 38/40、22/22 ctest、119 opcode、戰鬥 bytecode 真值、雙層 CJK」。
3. **與 `99_INDEX.md` 去重**:兩者角色重疊(都是索引);建議 **README.md 為唯一入口**,99_INDEX 標「已併入 README」或刪。
4. **加口徑說明區**:一句話講清「119(remake)vs 139/117(opendw oracle)」「38/40 連通」「戰鬥真值分層」,避免後續再被當 drift。

---

## 6. 彙總

- **盤點總數**:約 60 份文件(docs/ 數字檔 + 命名檔 + remake/docs + 根層 + ADR)。
- **狀態分布**(粗估):✅ 現行有效 **~33**｜🟡 部分過時需更新 **~16**｜📌 歷史紀錄保留 **~9**｜⛔ 建議新歸檔 **0**(已在 `_deprecated/` 的 6 項維持)。
- **建議 `_deprecated/` 新增**:**無**(邊界案例僅 skill 三方收斂,不歸檔)。
- **建議更新**:P0 兩項(根 README、docs 索引)、P1 五項、P2/P3 餘項(見 §4)。
- **Top drift**:根 README 自相矛盾、戰鬥 placeholder 舊說、opcode/連通/ctest 口徑未標、62/100 與 viewport「未 port」過時、roadmap 未回填、進度表停 6/10。
- **健康度**:誠實度高、結構偏散;主病灶為「快照未回填 + 口徑不統一 + 索引漏列 + 編號衝突」。對外門面(根 README)優先修。

> **唯讀聲明**:本報告只新增 `docs/assessment/58_DOC_AUDIT_AND_DRIFT.md`。所有「建議動作」均待使用者審核後再執行;本輪未搬任何檔、未改任何文件或 src、未動 git。
