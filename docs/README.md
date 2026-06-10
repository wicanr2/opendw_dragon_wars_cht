# OpenDW Dragon Wars 中文化專案 - 文件索引

**專案路徑**：`/home/anr2/tmp/longcat/opendw_dragon_wars_cht/`  
**Repo URL**：https://github.com/wicanr2/opendw_dragon_wars_cht

---

## 目錄

### 規劃與分析（01-06）
| 檔案 | 說明 |
|------|------|
| [01_PLAN.md](01_PLAN.md) | 中文化規劃（含 SDL2 整合 + 640×480 升級） |
| [02_ANALYSIS.md](02_ANALYSIS.md) | 反組譯還原分析（ASM ↔ C 對應表） |
| [03_REVIEW_REPORT.md](03_REVIEW_REPORT.md) | 專案審查報告（優缺點分析） |
| [04_NEXT_PLAN.md](04_NEXT_PLAN.md) | 下一階段計畫（7 個階段） |
| [05_SDL2_IMPLEMENTATION.md](05_SDL2_IMPLEMENTATION.md) | SDL2 實作計畫 |
| [05_VERIFICATION_REPORT.md](05_VERIFICATION_REPORT.md) | 文字萃取驗證報告 |
| [06_IMPLEMENTATION_PLAN.md](06_IMPLEMENTATION_PLAN.md) | 中文顯示實作計畫 |

### 翻譯文件（10-14）
| 檔案 | 說明 |
|------|------|
| [10_TRANSLATION.md](10_TRANSLATION.md) | 主翻譯對照表（100+ 條目） |
| [11_TRANSLATION_DIALOGUE.md](11_TRANSLATION_DIALOGUE.md) | 對話文字翻譯 |
| [12_TRANSLATION_ITEMS.md](12_TRANSLATION_ITEMS.md) | 物品名稱翻譯 |
| [13_TRANSLATION_SKILLS.md](13_TRANSLATION_SKILLS.md) | 技能名稱翻譯 |
| [14_TRANSLATION_MONSTERS.md](14_TRANSLATION_MONSTERS.md) | 怪物名稱翻譯 |

### 資料分析（20-26）
| 檔案 | 說明 |
|------|------|
| [07_REVISED_PLAN.md](07_REVISED_PLAN.md) | **修正版計畫 v2**（萃取方法修正，現行計畫） |
| [20_ALL_TEXT_FROM_DATA1.txt](20_ALL_TEXT_FROM_DATA1.txt) | ⚠️ 作廢：暴力萃取產生的雜訊（見 07） |
| [ALL_TEXT_FROM_SCRIPTS.txt](ALL_TEXT_FROM_SCRIPTS.txt) | ✅ 乾淨的真實遊戲文字（disasm 解出） |
| [21_DATA1_RESOURCE_INDEX.md](21_DATA1_RESOURCE_INDEX.md) | DATA1 資源索引 |
| [22_DATA1_SECTION_DETAILS.md](22_DATA1_SECTION_DETAILS.md) | DATA1 區段詳細分析 |
| [23_SOURCE_CODE_MAP.md](23_SOURCE_CODE_MAP.md) | 原始碼地圖（22 個檔案） |
| [24_SCRIPT_TEXT_MAPPING.md](24_SCRIPT_TEXT_MAPPING.md) | 腳本文字映射 |
| [25_OPCODE_INTERPRETATION.md](25_OPCODE_INTERPRETATION.md) | 虛擬 CPU opcode 判讀（繁中詳細版） |
| [OPCODE_REFERENCE.md](OPCODE_REFERENCE.md) | **Opcode 參考（中英雙語，對外發佈版）** |
| [26_MONSTERS_AND_SPRITES.md](26_MONSTERS_AND_SPRITES.md) | 怪物名稱 + sprite 抽取（含 DATA2 修正） |

### 手冊與資產
| 檔案 | 說明 |
|------|------|
| [32_EN_MANUAL_TEXT.md](32_EN_MANUAL_TEXT.md) | 英文手冊 OCR（48 頁） |
| [monster_sprites/](monster_sprites/) | 怪物 sprite PNG（59 張，含總覽圖） |
| [scene_pictures/](scene_pictures/) | 6 張全螢幕過場圖（片頭/結局，res 24–29） |

### 手冊與參考（30-41）
| 檔案 | 說明 |
|------|------|
| [30_CHINESE_MANUAL_INDEX.md](30_CHINESE_MANUAL_INDEX.md) | 中文手冊索引 |
| [31_CHINESE_MANUAL_TEXT.md](31_CHINESE_MANUAL_TEXT.md) | 中文手冊文字 |
| [40_ORIGINAL_DOCS_SUMMARY.md](40_ORIGINAL_DOCS_SUMMARY.md) | 原始文件摘要 |
| [41_TECHNICAL_DEBT.md](41_TECHNICAL_DEBT.md) | 技術債清單 |

### 建置與測試（50-60）
| 檔案 | 說明 |
|------|------|
| [50_BUILD.md](50_BUILD.md) | 建置指南 |
| [51_TEST_PLAN.md](51_TEST_PLAN.md) | 測試計畫 |
| [60_SKILL.md](60_SKILL.md) | Skill 文件（完整經驗記錄） |

### 二進位檔案
| 檔案 | 說明 |
|------|------|
| [dragon.asm](dragon.asm) | 原始 DOS 反組譯（參考） |
| [Dragon-Wars_Manual_DOS_EN.pdf](Dragon-Wars_Manual_DOS_EN.pdf) | 英文手冊（48 頁掃描） |
| [珍066-火龍之戰.rar](珍066-火龍之戰.rar) | 中文手冊（RAR4 壓縮） |

---

## 快速開始

### 閱讀順序
1. **規劃**：[01_PLAN.md](01_PLAN.md) → [02_ANALYSIS.md](02_ANALYSIS.md)
2. **翻譯**：[10_TRANSLATION.md](10_TRANSLATION.md) → [11-14_*.md](11_TRANSLATION_DIALOGUE.md)
3. **資料**：[20_ALL_TEXT_FROM_DATA1.txt](20_ALL_TEXT_FROM_DATA1.txt) → [21-24_*.md](21_DATA1_RESOURCE_INDEX.md)
4. **實作**：[05_SDL2_IMPLEMENTATION.md](05_SDL2_IMPLEMENTATION.md) → [50_BUILD.md](50_BUILD.md)

### 檔案命名規則
- `01-06`：規劃與分析
- `10-14`：翻譯文件
- `20-24`：資料分析
- `30-41`：手冊與參考
- `50-60`：建置與測試
- `99`：索引

---

## Opcode 判讀摘要 / Opcode Interpretation Summary

Dragon Wars 遊戲邏輯跑在一個 256-opcode 的 script 虛擬 CPU（`engine.c` 的 `targets[]`）。判讀結果：

| 項目 | 數值 |
|------|------|
| 總 opcode | 256（0x00–0xFF） |
| 已實作 | 139 |
| 未實作（NULL handler） | 117 |
| **真正有效的未實作 opcode** | 約 **22 個**（集中在 0x02–0x9F） |
| 文字/UI 類未實作（中文化高優先） | **4 個**：`0x79`、`0x7E`、`0x7F`、`0x8F` |
| 原始碼殘留、非真 opcode | 0xA0–0xFF 的 ~95 個（ASM 位址呈 x86 機器碼特徵） |

**關鍵結論**：先前「113 個未實作 opcode 是大工程」被高估。真正要處理的只有 ~22 個，其中與字串顯示直接相關的只有 4 個（`set_msg` 變體 0x79、角色名/格式化 0x7E/0x7F、`read_string` 變體 0x8F）。詳見 [25_OPCODE_INTERPRETATION.md](25_OPCODE_INTERPRETATION.md)（繁中）與 [OPCODE_REFERENCE.md](OPCODE_REFERENCE.md)（中英雙語對外版）。

---

## 目前進度

| 項目 | 狀態 |
|------|------|
| 反組譯還原 | ✅ 完成（52 個函式 / 139 opcode） |
| 遊戲文字萃取 | ⚠️ 修正中：3926 條為雜訊，改以 disasm 跟 bytecode 萃取（見 07） |
| 怪物名稱（res31） | ✅ 22 個（見 26） |
| 怪物 sprite 抽取 | ✅ 工具完成（修正 DATA2 讀取 bug，59 張） |
| 全螢幕過場圖 | ✅ 6 張（res 24–29） |
| 手冊 OCR | ✅ 中文 44 頁 + 英文 48 頁；Read Paragraph 段落已定位 |
| opcode 判讀 | ✅ 完成（25 / OPCODE_REFERENCE） |
| CJK 渲染 / SDL2 實作 | ❌ 未開始 |

---

## 授權

OpenDW 原始碼採用 BSD 授權。  
Dragon Wars 是 Interplay 的商標，原始遊戲檔案僅供個人使用。

## 貢獻者

- Chun-Yu Wang
