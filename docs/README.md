# OpenDW 火龍之戰中文化 — 文件總表

**Repo**：https://github.com/wicanr2/opendw_dragon_wars_cht
**術語 / 譯名標準**：[../CONTEXT.md](../CONTEXT.md) ｜ **完整狀態索引**：[99_INDEX.md](99_INDEX.md)

本專案把 1989 年《火龍之戰》(Dragon Wars, Interplay) 逆向工程後，用 C++20 / SDL2 乾淨重寫並繁中化。全部文件統一收在這個 `docs/` 樹下，依「做什麼用」分成十類子目錄。下表每個子目錄一段、每檔一句說明；想找哪類資料，直接看對應段落。

狀態圖例：✅ 現行有效 ｜ ✏️ 部分修正（檔內已加註） ｜ ⚠️ 作廢 / 結論已過時（檔內已加註）。

---

## 文件結構一覽

| 子目錄 | 用途 |
|--------|------|
| [`reverse-engineering/`](reverse-engineering/) | 逆向工程史、虛擬 CPU opcode、DATA1/DATA2 資料格式、戰鬥 bytecode、原始碼地圖 |
| [`walkthrough/`](walkthrough/) | 《軟體世界》三期連載攻略全文 + 逐區地圖 |
| [`gameplay/`](gameplay/) | remake 玩法系統真值：結局鏈、門 / 陷阱 / 地形、法術、技能判定、主線可達性 / quest gate |
| [`engine/`](engine/) | 引擎與實作：viewport / 操作 / 重寫就緒度規格 + SDL2 實作計畫、建置、測試 |
| [`reference/`](reference/) | 跨版本參考素材：多版本資產、VGA256 主題、架構對照、X68000、Bard's Tale 血緣 |
| [`assessment/`](assessment/) | PM review、缺口稽核、文件漂移、版面 / 視覺保真度、遊戲評估、技術債 |
| [`translation/`](translation/) | 翻譯對照表（主表 / 對話 / 物品 / 技能 / 怪物 / 草表）+ 主線事件待審 |
| [`manual/`](manual/) | 中英文操作手冊轉寫、Read Paragraph 段落書、原始手冊 PDF / RAR |
| [`media/`](media/) | 截圖、demo、DOS vs remake 對照 GIF、各類視覺素材（含 remake 端） |
| [`adr/`](adr/) | 架構決策紀錄 (Architecture Decision Records) |
| [`_deprecated/`](_deprecated/) | 已作廢 / 被取代的文件，僅供歷史對照 |

---

## reverse-engineering — 逆向工程史 / VM / 資料格式

| 檔案 | 說明 |
|------|------|
| [01_PLAN.md](reverse-engineering/01_PLAN.md) | ✏️ 早期總體規劃（字型尺寸以 engine/07 為準） |
| [02_ANALYSIS.md](reverse-engineering/02_ANALYSIS.md) | ✅ 反組譯還原分析（ASM ↔ C 對應表） |
| [03_REVIEW_REPORT.md](reverse-engineering/03_REVIEW_REPORT.md) | ✏️ 專案審查報告（早期成果統計已作廢） |
| [21_DATA1_RESOURCE_INDEX.md](reverse-engineering/21_DATA1_RESOURCE_INDEX.md) | ✏️ DATA1 資源索引 |
| [22_DATA1_SECTION_DETAILS.md](reverse-engineering/22_DATA1_SECTION_DETAILS.md) | ✏️ DATA1 區段詳細分析 |
| [23_SOURCE_CODE_MAP.md](reverse-engineering/23_SOURCE_CODE_MAP.md) | ✅ 原始碼地圖（opendw 22 檔） |
| [24_SCRIPT_TEXT_MAPPING.md](reverse-engineering/24_SCRIPT_TEXT_MAPPING.md) | ✏️ 遊戲腳本與 DATA1 文字對應 |
| [25_OPCODE_INTERPRETATION.md](reverse-engineering/25_OPCODE_INTERPRETATION.md) | ✅ 虛擬 CPU opcode 判讀（繁中詳細版） |
| [OPCODE_REFERENCE.md](reverse-engineering/OPCODE_REFERENCE.md) | ✅ Opcode 參考（中英雙語，對外發佈版） |
| [26_MONSTERS_AND_SPRITES.md](reverse-engineering/26_MONSTERS_AND_SPRITES.md) | ✅ 怪物名稱 + sprite 抽取（含 DATA2 修正） |
| [40_ORIGINAL_DOCS_SUMMARY.md](reverse-engineering/40_ORIGINAL_DOCS_SUMMARY.md) | ✏️ opendw 原始文件摘要 |
| [42_COMBAT_BYTECODE.md](reverse-engineering/42_COMBAT_BYTECODE.md) | ✅ 戰鬥結算改用原版 bytecode — 調查與 gap 分析 |
| [42_WHY_DATA1_DATA2.md](reverse-engineering/42_WHY_DATA1_DATA2.md) | ✅ 為什麼原版要拆 DATA1 / DATA2（1989 硬體限制） |
| [43_DOS_PLAYTEST.md](reverse-engineering/43_DOS_PLAYTEST.md) | ✅ 原版 DOS 實機戰鬥觀察（命中 / 傷害 / XP 校準） |
| [44_DATA_FORMATS_AND_MECHANICS.md](reverse-engineering/44_DATA_FORMATS_AND_MECHANICS.md) | ✅ 資料格式與戰鬥機制（角色 / 物品 / 技能位元規則） |
| [46_PC98_JA_EXTRACTION.md](reverse-engineering/46_PC98_JA_EXTRACTION.md) | ✅ X68000 / PC-98 日文版文字抽出 |
| [51_WORLDMAP_AREA_SWITCH_RE.md](reverse-engineering/51_WORLDMAP_AREA_SWITCH_RE.md) | ✅ 世界圖 / 地底樞紐「踩格進區」轉移機制逆向 |
| [52_MAINLINE_EVENT_STRINGS.md](reverse-engineering/52_MAINLINE_EVENT_STRINGS.md) | ✅ 主線事件字串萃取（grounded in bytecode） |
| [ALL_TEXT_FROM_SCRIPTS.txt](reverse-engineering/ALL_TEXT_FROM_SCRIPTS.txt) | ✅ 乾淨的真實遊戲文字（disasm 解出） |
| [dragon.asm](reverse-engineering/dragon.asm) | 原始 DOS 反組譯（參考） |

## walkthrough — 軟體世界攻略 + 地圖

| 檔案 | 說明 |
|------|------|
| [35_SOFTWORLD_25.md](walkthrough/35_SOFTWORLD_25.md) | ✅ 《軟體世界》第 25 期攻略（一） |
| [36_SOFTWORLD_26.md](walkthrough/36_SOFTWORLD_26.md) | ✅ 《軟體世界》第 26 期攻略（二） |
| [37_SOFTWORLD_27.md](walkthrough/37_SOFTWORLD_27.md) | ✅ 《軟體世界》第 27 期攻略（完） |
| [38_SOFTWORLD_WALKTHROUGH.md](walkthrough/38_SOFTWORLD_WALKTHROUGH.md) | ✅ 三期連載攻略整合導覽 |
| [39_SOFTWORLD_FULLTEXT_AND_MAPS.md](walkthrough/39_SOFTWORLD_FULLTEXT_AND_MAPS.md) | ✅ 逐頁全文轉寫 + 逐區地圖（圖在 `softworld_images/`） |

> 攻略地圖素材：[`walkthrough/softworld_images/`](walkthrough/softworld_images)；過場圖：[`walkthrough/scene_pictures/`](walkthrough/scene_pictures)。

## gameplay — remake 玩法系統真值

| 檔案 | 說明 |
|------|------|
| [54_WORLDMAP_REACHABILITY_AUDIT.md](gameplay/54_WORLDMAP_REACHABILITY_AUDIT.md) | ✅ 世界圖逐地點可達性盤點 |
| [55_MAINLINE_QUEST_GATE_AND_ENDGAME.md](gameplay/55_MAINLINE_QUEST_GATE_AND_ENDGAME.md) | ✅ 主線 quest gate 依賴鏈 + 結局事件狀態 |
| [56_PLAYABLE_ENDING_CHAIN.md](gameplay/56_PLAYABLE_ENDING_CHAIN.md) | ✅ 可通關結局鏈（Boss / 結局＝remake 設計，誠實標示） |
| [57_DOORS_TRAPS_TERRAIN.md](gameplay/57_DOORS_TRAPS_TERRAIN.md) | ✅ 門 / 陷阱 / 地形真值層級 |
| [58_MAGIC_REFERENCE.md](gameplay/58_MAGIC_REFERENCE.md) | ✅ 法術表（數值權威，fraterrisus 攻略 v3.0） |
| [59_SKILL_CHECK_TRIGGERS.md](gameplay/59_SKILL_CHECK_TRIGGERS.md) | ✅ 技能判定觸發點 |

## engine — 引擎規格與實作

| 檔案 | 說明 |
|------|------|
| [VIEWPORT.md](engine/VIEWPORT.md) | ✅ 第一人稱 viewport 渲染規格 |
| [VIEWPORT_COMPOSE.md](engine/VIEWPORT_COMPOSE.md) | ✅ viewport 合成（FOV / 對拍結論） |
| [CONTROLS.md](engine/CONTROLS.md) | ✅ 操作鍵表（對齊臺灣中文版手冊）+ headless 測試旗標 |
| [REWRITE_READINESS.md](engine/REWRITE_READINESS.md) | ✏️ 重寫就緒度里程碑快照 |
| [05_SDL2_IMPLEMENTATION.md](engine/05_SDL2_IMPLEMENTATION.md) | ✅ SDL2 實作計畫 |
| [06_IMPLEMENTATION_PLAN.md](engine/06_IMPLEMENTATION_PLAN.md) | ✏️ 中文顯示實作計畫 |
| [07_REVISED_PLAN.md](engine/07_REVISED_PLAN.md) | ✅ 現行計畫 v2 |
| [08_READ_PARAGRAPH_FEATURE.md](engine/08_READ_PARAGRAPH_FEATURE.md) | ✅ Read Paragraph 內嵌顯示規劃 |
| [50_BUILD.md](engine/50_BUILD.md) | ✅ 建置指南 |
| [51_TEST_PLAN.md](engine/51_TEST_PLAN.md) | ✅ 測試計畫 |

## reference — 跨版本參考素材

| 檔案 | 說明 |
|------|------|
| [61_MULTIVERSION_ASSETS.md](reference/61_MULTIVERSION_ASSETS.md) | ✅ 多版本（DOS / Amiga / X68000）資產對照 |
| [62_VGA256_THEME.md](reference/62_VGA256_THEME.md) | ✅ VGA256 主題渲染 |
| [63_OPENDW_VS_REMAKE_ARCH.md](reference/63_OPENDW_VS_REMAKE_ARCH.md) | ✅ opendw（C）vs remake（C++）架構對照 |
| [64_X68K_PKH_LESSONS.md](reference/64_X68K_PKH_LESSONS.md) | ✅ X68000 PKH 美術解碼經驗 |
| [66_BARDS_TALE_LINEAGE.md](reference/66_BARDS_TALE_LINEAGE.md) | ✅ Bard's Tale 血緣脈絡考證 |

## assessment — PM / 稽核 / 評估

| 檔案 | 說明 |
|------|------|
| [00_DOC_AUDIT.md](assessment/00_DOC_AUDIT.md) | ✅ docs 審查報告 + 待裁決清單 |
| [41_TECHNICAL_DEBT.md](assessment/41_TECHNICAL_DEBT.md) | ✅ 技術債清單 |
| [47_REMAKE_ASSESSMENT.md](assessment/47_REMAKE_ASSESSMENT.md) | ✅ remake 重製評估（分項評分 + 可玩性） |
| [48_COMPLETABILITY_ROADMAP.md](assessment/48_COMPLETABILITY_ROADMAP.md) | ✅ 可通關性分析 + roadmap |
| [49_GAP_AUDIT.md](assessment/49_GAP_AUDIT.md) | ✅ 系統性「遺漏 / 未實作」稽核 |
| [57_PM_REVIEW.md](assessment/57_PM_REVIEW.md) | ✅ 產品狀態 review（PM 視角） |
| [58_DOC_AUDIT_AND_DRIFT.md](assessment/58_DOC_AUDIT_AND_DRIFT.md) | ✅ 文件盤點與漂移稽核 |
| [59_LAYOUT_FIDELITY.md](assessment/59_LAYOUT_FIDELITY.md) | ✅ 版面忠實度比對（remake vs 原版） |
| [60_DOS_VS_REMAKE_VISUAL.md](assessment/60_DOS_VS_REMAKE_VISUAL.md) | ✅ DOS vs remake 逐畫面保真分析（圖在 `dos_compare/`） |
| [65_GAME_EVALUATION.md](assessment/65_GAME_EVALUATION.md) | ✅ 遊戲評估（多版本對照，圖在 `media/remake/assessment/`） |
| [60_SKILL.md](assessment/60_SKILL.md) | ✅ 專案 skill 文件（完整經驗記錄） |
| [61_SHOP_AND_RECRUIT.md](assessment/61_SHOP_AND_RECRUIT.md) | ✅ 商店買賣 + 酒館招募實作 |

> 稽核用圖：[`assessment/dos_compare/`](assessment/dos_compare)（DOS / remake / sidebyside）、[`assessment/pm_scale_screens/`](assessment/pm_scale_screens)（縮放比較）。

## translation — 翻譯對照表

| 檔案 | 說明 |
|------|------|
| [10_TRANSLATION.md](translation/10_TRANSLATION.md) | ✏️ 主翻譯表（譯名已對齊 CONTEXT.md） |
| [11_TRANSLATION_DIALOGUE.md](translation/11_TRANSLATION_DIALOGUE.md) | ✏️ 對話翻譯 |
| [12_TRANSLATION_ITEMS.md](translation/12_TRANSLATION_ITEMS.md) | ✏️ 物品翻譯 |
| [13_TRANSLATION_SKILLS.md](translation/13_TRANSLATION_SKILLS.md) | ✏️ 技能翻譯 |
| [14_TRANSLATION_MONSTERS.md](translation/14_TRANSLATION_MONSTERS.md) | ✏️ 怪物翻譯（參考 reverse-engineering/26） |
| [15_TRANSLATION_DRAFT.md](translation/15_TRANSLATION_DRAFT.md) | ✏️ 繁體中文翻譯草表 |
| [53_EVENTS_TRANSLATION_REVIEW.md](translation/53_EVENTS_TRANSLATION_REVIEW.md) | ✅ 主線事件繁中待審清單（語言權威裁決用） |

## manual — 操作手冊 / 段落書

| 檔案 | 說明 |
|------|------|
| [32_EN_MANUAL_TEXT.md](manual/32_EN_MANUAL_TEXT.md) | ✅ 英文手冊文字（視覺轉寫，48 頁） |
| [33_MANUAL_TRANSCRIPTION.md](manual/33_MANUAL_TRANSCRIPTION.md) | ✅ 中文手冊視覺精確轉寫 |
| [34_READ_PARAGRAPHS.md](manual/34_READ_PARAGRAPHS.md) | ✅ Read Paragraph 段落書精確轉寫 |
| [Dragon-Wars_Manual_DOS_EN.pdf](manual/Dragon-Wars_Manual_DOS_EN.pdf) | 英文手冊（48 頁掃描） |
| [珍066-火龍之戰.rar](manual/珍066-火龍之戰.rar) | 中文手冊（RAR 壓縮） |

> 手冊圖：[`manual/chinese_manual_images/`](manual/chinese_manual_images)、[`manual/en_manual_images/`](manual/en_manual_images)。

## media — 截圖 / demo / 視覺素材

DOS 原版 vs remake 的動態比對 GIF（[README](media/README.md)）與各類截圖。`media/remake/` 收 remake 端產出的截圖、showcase、各語言事件畫面、結局鏈、商店招募等。其餘子目錄為逆向期擷取的角色建立、戰鬥、CJK、UI、版面比較、DOS playtest、怪物 sprite 等素材。

## adr — 架構決策紀錄

| 檔案 | 說明 |
|------|------|
| [0001-asset-bundle-and-resource-provider.md](adr/0001-asset-bundle-and-resource-provider.md) | ✅ Asset bundle 與 resource provider |
| [0002-two-layer-cjk-rendering.md](adr/0002-two-layer-cjk-rendering.md) | ✅ 雙層 CJK 渲染 |

## _deprecated — 作廢歸檔

舊計畫、結論已過時的萃取報告與中文手冊初稿等，僅供歷史對照（見 [README](_deprecated/README.md)）。

---

## 閱讀順序（現行）

1. **基準**：[assessment/00_DOC_AUDIT.md](assessment/00_DOC_AUDIT.md) → [../CONTEXT.md](../CONTEXT.md) → [engine/07_REVISED_PLAN.md](engine/07_REVISED_PLAN.md)
2. **逆向 / VM**：[reverse-engineering/02_ANALYSIS.md](reverse-engineering/02_ANALYSIS.md) → [25_OPCODE_INTERPRETATION.md](reverse-engineering/25_OPCODE_INTERPRETATION.md) / [OPCODE_REFERENCE.md](reverse-engineering/OPCODE_REFERENCE.md) → [44_DATA_FORMATS_AND_MECHANICS.md](reverse-engineering/44_DATA_FORMATS_AND_MECHANICS.md)
3. **翻譯 / 手冊**：[translation/10_TRANSLATION.md](translation/10_TRANSLATION.md) → [manual/33_MANUAL_TRANSCRIPTION.md](manual/33_MANUAL_TRANSCRIPTION.md) → [34_READ_PARAGRAPHS.md](manual/34_READ_PARAGRAPHS.md)
4. **引擎 / 實作**：[engine/06_IMPLEMENTATION_PLAN.md](engine/06_IMPLEMENTATION_PLAN.md) → [05_SDL2_IMPLEMENTATION.md](engine/05_SDL2_IMPLEMENTATION.md) → [VIEWPORT.md](engine/VIEWPORT.md) → [50_BUILD.md](engine/50_BUILD.md)
5. **玩法真值 / 評估**：[gameplay/56_PLAYABLE_ENDING_CHAIN.md](gameplay/56_PLAYABLE_ENDING_CHAIN.md) → [assessment/57_PM_REVIEW.md](assessment/57_PM_REVIEW.md) → [60_DOS_VS_REMAKE_VISUAL.md](assessment/60_DOS_VS_REMAKE_VISUAL.md)

## 授權

OpenDW 原始碼採用 BSD 授權。Dragon Wars 是 Interplay 的商標，原始遊戲檔案僅供個人使用。

## 貢獻者

- Chun-Yu Wang
