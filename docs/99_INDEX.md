# 文件索引 (99_INDEX)

> **更新**：2026-06-10
> 本索引反映 `docs/` 現況與每檔狀態。詳細審查見 [`00_DOC_AUDIT.md`](00_DOC_AUDIT.md)。
> (註:本檔原內容為 OpenDW DOS build 說明,文不對題,已重建為真索引。)

狀態圖例：✅ 現行/可信 ｜ ⚠️ 作廢或結論錯誤(已加註) ｜ ✏️ 部分修正(已加註) ｜ 📄 資料檔

---

## 先讀這幾份(現行基準)

| 檔案 | 狀態 | 說明 |
|------|------|------|
| [00_DOC_AUDIT.md](00_DOC_AUDIT.md) | ✅ | 全 docs 審查報告 + 待裁決清單 |
| [../CONTEXT.md](../CONTEXT.md) | ✅ | 術語/譯名標準(repo 根) |
| [07_REVISED_PLAN.md](07_REVISED_PLAN.md) | ✅ | **現行計畫 v2**(取代 04) |
| [ALL_TEXT_FROM_SCRIPTS.txt](ALL_TEXT_FROM_SCRIPTS.txt) | ✅📄 | **乾淨的真實遊戲文字**(op_77/78/7b 解出) |
| [26_MONSTERS_AND_SPRITES.md](26_MONSTERS_AND_SPRITES.md) | ✅ | 怪物名(res31)+ sprite 正解 |

## 規劃與分析 (01–08)

| 檔案 | 狀態 | 說明 |
|------|------|------|
| [01_PLAN.md](01_PLAN.md) | ✏️ | 早期總體計畫(字型尺寸矛盾,以 07 為準) |
| [02_ANALYSIS.md](02_ANALYSIS.md) | ✅ | 反組譯還原 ASM↔C 對照 |
| [03_REVIEW_REPORT.md](03_REVIEW_REPORT.md) | ✏️ | 審查報告(3,926 條=優質成果 之結論已作廢) |
| 04_NEXT_PLAN.md | ⚠️ | 已移至 [_deprecated/04_NEXT_PLAN.md](_deprecated/04_NEXT_PLAN.md);被 07 取代 |
| [05_SDL2_IMPLEMENTATION.md](05_SDL2_IMPLEMENTATION.md) | ✅ | SDL2 取代 DOS 設定/音效 |
| 05_VERIFICATION_REPORT.md | ⚠️ | 已移至 [_deprecated/05_VERIFICATION_REPORT.md](_deprecated/05_VERIFICATION_REPORT.md);結論錯誤 |
| [06_IMPLEMENTATION_PLAN.md](06_IMPLEMENTATION_PLAN.md) | ✏️ | 中文顯示實作路徑(數據以 07 為準) |
| [07_REVISED_PLAN.md](07_REVISED_PLAN.md) | ✅ | **現行計畫** |
| [08_READ_PARAGRAPH_FEATURE.md](08_READ_PARAGRAPH_FEATURE.md) | ✅ | Read Paragraph 內嵌顯示規劃 |

## 翻譯 (10–14)

| 檔案 | 狀態 | 說明 |
|------|------|------|
| [10_TRANSLATION.md](10_TRANSLATION.md) | ✏️ | 主翻譯表(雜訊「未知文字」已刪、譯名已對齊) |
| [11_TRANSLATION_DIALOGUE.md](11_TRANSLATION_DIALOGUE.md) | ✏️ | 對話(Purgatory/Namtar/Humbaba 等已統一) |
| [12_TRANSLATION_ITEMS.md](12_TRANSLATION_ITEMS.md) | ✏️ | 物品(臆測物品已刪,僅留已驗證武器/防具) |
| [13_TRANSLATION_SKILLS.md](13_TRANSLATION_SKILLS.md) | ✏️ | 技能(來源標示已修正) |
| [14_TRANSLATION_MONSTERS.md](14_TRANSLATION_MONSTERS.md) | ✏️ | 怪物(sprite 編號當名字 + 虛構清單,改參考 26) |

## 資料分析與 opcode (20–26)

| 檔案 | 狀態 | 說明 |
|------|------|------|
| 20_ALL_TEXT_FROM_DATA1.txt | ⚠️📄 | 已移至 [_deprecated/20_ALL_TEXT_FROM_DATA1.txt](_deprecated/20_ALL_TEXT_FROM_DATA1.txt);**雜訊**,改用 ALL_TEXT_FROM_SCRIPTS.txt |
| [21_DATA1_RESOURCE_INDEX.md](21_DATA1_RESOURCE_INDEX.md) | ✏️ | 資源索引(「文字數」欄不可信) |
| [22_DATA1_SECTION_DETAILS.md](22_DATA1_SECTION_DETAILS.md) | ✏️ | section 分析(0x08–0x16「上百條 text」為假象) |
| [23_SOURCE_CODE_MAP.md](23_SOURCE_CODE_MAP.md) | ✅ | 原始碼地圖 |
| [24_SCRIPT_TEXT_MAPPING.md](24_SCRIPT_TEXT_MAPPING.md) | ✏️ | 腳本↔資源映射(對照文字源已修正) |
| [25_OPCODE_INTERPRETATION.md](25_OPCODE_INTERPRETATION.md) | ✅ | opcode 判讀(繁中詳版) |
| [OPCODE_REFERENCE.md](OPCODE_REFERENCE.md) | ✅ | opcode 參考(中英對外版) |
| [26_MONSTERS_AND_SPRITES.md](26_MONSTERS_AND_SPRITES.md) | ✅ | 怪物名 + sprite 正解 |

## 手冊與攻略 (30–37)

| 檔案 | 狀態 | 說明 |
|------|------|------|
| 30_CHINESE_MANUAL_INDEX.md | ⚠️ | 已移至 [_deprecated/30_CHINESE_MANUAL_INDEX.md](_deprecated/30_CHINESE_MANUAL_INDEX.md);被 33/34 取代 |
| 31_CHINESE_MANUAL_TEXT.md | ⚠️ | 已移至 [_deprecated/31_CHINESE_MANUAL_TEXT.md](_deprecated/31_CHINESE_MANUAL_TEXT.md);被 33/34 取代 |
| [32_EN_MANUAL_TEXT.md](32_EN_MANUAL_TEXT.md) | ✅ | 英文手冊 OCR(粗糙待校,英文段落唯一來源) |
| [33_MANUAL_TRANSCRIPTION.md](33_MANUAL_TRANSCRIPTION.md) | ✅ | **中文手冊視覺精確轉寫**(取代 30/31) |
| [34_READ_PARAGRAPHS.md](34_READ_PARAGRAPHS.md) | ✅ | **Read Paragraph 段落精確轉寫** |
| [35_SOFTWORLD_25.md](35_SOFTWORLD_25.md) | ✅ | 《軟體世界》25 期攻略轉寫 |
| [36_SOFTWORLD_26.md](36_SOFTWORLD_26.md) | ✅ | 《軟體世界》26 期攻略轉寫 |
| [37_SOFTWORLD_27.md](37_SOFTWORLD_27.md) | ✅ | 《軟體世界》27 期攻略轉寫 |
| [38_SOFTWORLD_WALKTHROUGH.md](38_SOFTWORLD_WALKTHROUGH.md) | ✅ | **《軟體世界》三期攻略 · 圖文整合版**(21 掃描頁 + 逐地點事件表 + 訊息↔手冊段落擴充) |

## 參考 / 建置 / 測試 (40–60)

| 檔案 | 狀態 | 說明 |
|------|------|------|
| [40_ORIGINAL_DOCS_SUMMARY.md](40_ORIGINAL_DOCS_SUMMARY.md) | ✏️ | opendw 原始文件摘要(怪物表 sprite 編號已加註) |
| [41_TECHNICAL_DEBT.md](41_TECHNICAL_DEBT.md) | ✅ | 技術債清單 |
| [50_BUILD.md](50_BUILD.md) | ✅ | 建置指南 |
| [51_TEST_PLAN.md](51_TEST_PLAN.md) | ✅ | 測試計畫 |
| [60_SKILL.md](60_SKILL.md) | ✏️ | Skill 經驗記錄(萃取數據已加註) |

## 資產 / 二進位

| 項目 | 說明 |
|------|------|
| [dragon.asm](dragon.asm) | 原始 DOS 反組譯 ✅ |
| [Dragon-Wars_Manual_DOS_EN.pdf](Dragon-Wars_Manual_DOS_EN.pdf) | 英文手冊掃描(48 頁) |
| 珍066-火龍之戰.rar | 中文手冊(RAR4) |
| monster_sprites/ | 怪物 sprite PNG |
| scene_pictures/ | 過場圖 |
| chinese_manual_images/ / en_manual_images/ | 手冊掃描圖 |
| [_deprecated/](_deprecated/) | ⚠️ 作廢歸檔目錄(04、05_VERIFICATION、20、30、31、verify_extraction.py),僅供歷史對照 |
