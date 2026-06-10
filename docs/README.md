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

### 資料分析（20-24）
| 檔案 | 說明 |
|------|------|
| [20_ALL_TEXT_FROM_DATA1.txt](20_ALL_TEXT_FROM_DATA1.txt) | DATA1 所有提取文字（3926 條） |
| [21_DATA1_RESOURCE_INDEX.md](21_DATA1_RESOURCE_INDEX.md) | DATA1 資源索引 |
| [22_DATA1_SECTION_DETAILS.md](22_DATA1_SECTION_DETAILS.md) | DATA1 區段詳細分析 |
| [23_SOURCE_CODE_MAP.md](23_SOURCE_CODE_MAP.md) | 原始碼地圖（22 個檔案） |
| [24_SCRIPT_TEXT_MAPPING.md](24_SCRIPT_TEXT_MAPPING.md) | 腳本文字映射 |

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

## 目前進度

| 項目 | 狀態 |
|------|------|
| 反組譯還原 | ✅ 完成（52 個函式） |
| DATA1 文字提取 | ✅ 完成（3926 條） |
| 翻譯對照表 | ✅ 完成（437 條目，覆蓋率 98.4%） |
| 中文手冊提取 | ⚠️ 部分完成（OCR + 索引） |
| 實作 | ❌ 未開始 |
| 測試 | ❌ 未開始 |

---

## 授權

OpenDW 原始碼採用 BSD 授權。  
Dragon Wars 是 Interplay 的商標，原始遊戲檔案僅供個人使用。

## 貢獻者

- Chun-Yu Wang
