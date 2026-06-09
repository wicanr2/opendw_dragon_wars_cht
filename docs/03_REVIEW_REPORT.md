# OpenDW Dragon Wars 中文化專案審查報告

**審查日期**：2026-06-09  
**審查範圍**：`/home/anr2/tmp/longcat/opendw_dragon_wars_cht/`  
**對照文件**：`/home/anr2/tmp/longcat/opendw/doc/` 原始文件與 opendw 原始程式碼

---

## 一、整體評估

### 1.1 完成度摘要

| 項目 | 狀態 | 完成度 |
|------|------|--------|
| 文件結構與規劃 | ✅ 完成 | 90% |
| 反組譯文件（ANALYSIS.md） | ✅ 完成 | 85% |
| 翻譯文件（TRANSLATION.md） | ⚠️ 部分完成 | 60% |
| 文件交叉比對 | ⚠️ 不足 | 40% |
| 實作（SDL2/CJK） | ❌ 未開始 | 0% |
| 測試 | ❌ 未開始 | 0% |

### 1.2 關鍵指標

- **52 個 unnamed 函式** 已全部更名 ✅
- **143 個 opcodes** 已命名（256 個中有 113 個仍標示為未實作）
- **3926 個文字串** 從 DATA1 提取（優質成果）
- **100+ 翻譯條目** 在 TRANSLATION.md（但只佔總文字串的 2.5%）
- **0 個 CJK 渲染相關檔案** 已實作
- **0 個 SDL2 升級相關檔案** 已實作

---

## 二、已完成項目（✅ 良好的成果）

### 2.1 文件品質

**README.md**：
- ✅ 清晰的目錄結構
- ✅ 完整的文件清單
- ✅ 快速開始指南
- ✅ 清晰的授權資訊

**PLAN.md**：
- ✅ 完整的中文化分層架構（5 層）
- ✅ 清晰的實作路徑（Phase 0-4）
- ✅ 合理的方案評估（方案 B 推薦）
- ✅ 工作時數估計（9.5-14.5 天）
- ✅ 風險評估（字型版權、data1 格式、效能）

**ANALYSIS.md**：
- ✅ 完整的 ASM ↔ C 對應表
- ✅ 清晰的還原優先順序（P0-P3）
- ✅ 全域變數結構化建議
- ✅ 魔術數字改善建議

**SKILL.md**：
- ✅ 完整的工作流程記錄
- ✅ 5-bit 編碼解壓演算法
- ✅ 翻譯工作方法論
- ✅ 經驗教訓（文字提取問題、反組譯技巧）

### 2.2 文字提取成果

**ALL_TEXT_FROM_DATA1.txt**：
- ✅ 3926 個文字串提取（141KB）
- ✅ 16 個 Section 完整掃描（0x00-0x16）
- ✅ 包含 45 個英文詞首大寫的文字（如 "Armor of Light"）
- ✅ 包含技能名稱（Mountain Lore 等）
- ✅ 包含戰鬥/對話/物品/法術文字

### 2.3 反組譯還原

**已完成**：
- ✅ 52 個 `sub_XXX` 函式全部更名
- ✅ 143 個 opcodes 已命名
- ✅ 每個函式加入中英文雙語描述
- ✅ 建立完整的 ASM ↔ C 對應表

---

## 三、缺失項目（⚠️ 需要處理）

### 3.1 翻譯完整性問題

#### 3.1.1 翻譯覆蓋率不足

**現狀**：
- TRANSLATION.md 只有 **~100 個翻譯條目**
- 實際遊戲有 **3926 個文字串** 可提取
- 覆蓋率僅 **2.5%**

**缺失的文字類型**：
1. **完整對話文字**：遊戲有 859 個 Section 0x03 文字（dialogue），大部分未翻譯
2. **物品名稱**：只有 2 個物品名稱（leather armor, plate and chain armor）
3. **怪物名稱**：完全缺失（Wolf, Spider, Pikeman 等）
4. **關卡名稱**：完全缺失（Purgatory, Castle wall, Sky portion 等）
5. **角色名稱**：完全缺失（遊戲有 7 個預設角色）
6. **法術描述**：只有 16 個法術名稱，缺少完整描述
7. **隨機遭遇文字**：戰鬥相關文字只翻譯了部分

#### 3.1.2 "Read Paragraph" 文字完全缺失

**關鍵問題**：TRANSLATION.md 第 216 行提到：
> "這部分文字主要從遊戲手冊（`珍066-火龍之戰.rar`）中提取"

但實際上：
- ❌ 沒有從 PDF 手冊提取任何文字
- ❌ 沒有從 RAR 壓縮檔提取中文手冊
- ❌ TRANSLATION.md 中沒有 "Read Paragraph" 相關翻譯
- ❌ 遊戲中的 "Paragraph" 功能是重要劇情顯示，無法從 DATA1 提取（因 DATA1 只有英文）

**需要處理**：
1. 解壓縮 `珍066-火龍之戰.rar`
2. 掃描中文手冊內容
3. 建立「讀取段落」功能的中文化方案
4. 可能需要實作雙語切換（英文原版 + 中文翻譯）

#### 3.1.3 物品/技能名稱翻譯不完整

**物品名稱缺失範例**（從截圖和文件應可提取）：
- 武器：Sword, Bow, Axe, Mace, Staff 等
- 防具：Shield, Helmet, Gloves, Boots 等
- 消耗品：Potion, Scroll, Key 等
- 特殊物品：Armor of Light（已提取但未翻譯）

**技能名稱只提取了 5 個**：
- Arcane Lore ✅
- Cave Lore ✅
- Forest Lore ✅
- Mountain Lore ✅
- Town Lore ✅
- Fistfighting ✅
- **其他技能完全缺失**（如 Sword, Bow, Shield 等戰鬥技能）

### 3.2 文件與原始文件脫節

#### 3.2.1 缺少關鍵原始文件資訊

**script.md**（遊戲腳本格式）：
- ❌ 未在 PLAN/ANALYSIS 中提及已知的 script 位置（Section 0 offset 1103, 237, 246, 1137）
- ❌ 未利用已知的 script 結構（Section 71, 22, 3, 71 的 encounter scripts）
- ❌ 未利用 `keypress.txt` 的按鍵處理資訊（按鍵綁定映射）

**resources.md**（資源格式）：
- ❌ 未建立 DATA1 資源索引表（Resource 0 = script, 7 = character data, 29 = title screen 等）
- ❌ 未在翻譯工具中利用資源類型資訊
- ❌ 未提及 Resource 7 的結構（3584 bytes player records + 256 bytes gamestate + 0x700 bytes D760）

**monsters.txt**（怪物資料）：
- ❌ 未建立怪物名稱表（Wolf, Spider, Pikeman, Fanatic 等）
- ❌ 未利用資源索引（168, 196, 200, 210, 222）

**levels.md**（關卡格式）：
- ❌ 未建立關卡名稱表（Purgatory = 0x71）
- ❌ 未利用關卡資源索引（71, 110, 111, 112, 116 等）

#### 3.2.2 虛假或過時的資訊

**PLAN.md 中的錯誤資訊**：
- ❌ **第 59 行**：「11×11 點陣」→ 實際應為 **24×24** 或 **16×16**
- ❌ **第 62 行**：「22×22 繁體中文點陣」→ 與前面 11×11 矛盾
- ❌ **第 68 行**：「每個 glyph = 2 bytes code + 24 rows × 3 bytes」→ 這是 24×24 的格式，與 11×11 或 22×22 不符

**TRANSLATION.md 中的結構問題**：
- ❌ **第 228 行**：「物品名稱可能儲存於 DATA1 的 Section 0x07（CHARACTER_DATA）」→ Section 0x07 是角色資料，不是物品名稱
- ❌ **第 239 行**：「技能名稱（從 DATA1 Section 0x15 提取）」→ Section 0x15 只有 15 個文字，但技能名稱應該在別處

### 3.3 技術債務

#### 3.3.1 全域變數結構化未執行

**ANALYSIS.md 第 209 行**建議：
```c
struct engine_state {
    uint16_t counter_104D;
    uint8_t  byte_104E;
    // ...
};
```

但 **engine.c 仍有 100+ 個全域變數**，未結構化：
- `counter_104D`, `byte_104E`, `word_104F`, `word_1051`, `word_11C0`, `word_11C2`, ...
- 嚴重影響可維護性

#### 3.3.2 魔術數字未消除

**ANALYSIS.md 第 237 行**建議：
```c
#define UI_RIGHT_PILLAR  0x27
#define UI_BOTTOM_BORDER 0xB8
```

但 **ui.c 中仍有大量魔術數字**：
- `0x27`, `0xB8`, `0x8D`, `0x80` 等
- 影響可讀性

#### 3.3.3 未實作的 opcodes（113 個）

**ANALYSIS.md 第 253 行**提到：
- 32 個是跳躍表項（非真正 opcodes）
- 85 個需要實作
- 但 **SDL2_IMPLEMENTATION.md** 只列出了約 15 個需要調查的 opcodes

**缺失的 opcode 分類**：
- ❌ 未建立完整的 opcode 分類表
- ❌ 未標示哪些是「圖形模式相關（可忽略）」
- ❌ 未標示哪些是「音效相關（改用 SDL2 Audio）」
- ❌ 未標示哪些是「需要實作」

### 3.4 建置與測試

#### 3.4.1 無法建置

**CMakeLists.txt**：
- 只有項目定義，沒有完整的建置規則
- 缺少 SDL2 find_package
- 缺少資料檔案複製規則

**Dockerfile**（在 SKILL.md 和 SDL2_IMPLEMENTATION.md 中）：
- 只有範例，沒有整合到專案
- 沒有中文字型建置步驟
- 沒有測試步驟

#### 3.4.2 無測試

**src/tests/**：
- 只有 5 個測試檔案
- 沒有 CJK 相關測試
- 沒有翻譯相關測試
- 沒有 SDL2 顯示相關測試

---

## 四、需要修正的問題（❌ 高優先級）

### 4.1 翻譯文件結構重整

**問題**：TRANSLATION.md 結構混亂，混合了多種來源的文字

**建議**：
1. 將翻譯文件按來源分類：
   - `TRANSLATION_MENU.md`：選單文字
   - `TRANSLATION_DIALOGUE.md`：對話文字
   - `TRANSLATION_ITEMS.md`：物品名稱
   - `TRANSLATION_SPELLS.md`：法術名稱
   - `TRANSLATION_SKILLS.md`：技能名稱
   - `TRANSLATION_MONSTERS.md`：怪物名稱
   - `TRANSLATION_LEVELS.md`：關卡名稱
   - `TRANSLATION_PARAGRAPHS.md`：讀取段落（從手冊提取）

2. 每個分類使用統一格式：
```markdown
## 物品名稱

| ID | 英文 | 中文 | 來源資源 | 狀態 |
|----|------|------|----------|------|
| ITEM_SWORD | Sword | 長劍 | DATA1 Section 0x03 | ✅ 已翻譯 |
| ITEM_BOW | Bow | 弓 | DATA1 Section 0x03 | ✅ 已翻譯 |
```

### 4.2 物品/技能/怪物名稱完整提取

**需要處理**：
1. 利用 `resources.md` 的資源索引：
   - Resource 31 = 怪物字串資料
   - Resource 71 = Purgatory 關卡（包含對話）
   - Resource 29 = Title screen（包含物品圖片）

2. 利用 `monsters.txt` 的怪物資源索引：
   - Resource 168 = Wolf
   - Resource 196 = Spider / Rock Spiders
   - Resource 200 = Innocent Man
   - Resource 210 = Pikeman
   - Resource 222 = Fanatic / Loon

3. 利用 `levels.md` 的關卡資源索引：
   - Resource 71 = Purgatory
   - Resource 110 = Castle wall
   - Resource 111 = Sky portion
   - Resource 112 = Red clay road portion
   - Resource 116 = Water puddle

### 4.3 中文手冊提取

**需要處理**：
1. 解壓縮 `珍066-火龍之戰.rar`
2. 掃描中文手冊內容
3. 建立「讀取段落」的中文化方案：
   - 若手冊有英文版對應，可直接替换
   - 若只有中文版，需要實作「顯示中文段落」功能
4. 可能需要實作「雙語切換」選項

### 4.4 文件格式修正

**問題**：PLAN.md 和 SKILL.md 中的格式描述錯誤

**需要修正**：
1. 統一中文字尺寸描述：
   - 推薦 24×24（PLAN.md 方案 B）
   - 備選 16×16（PLAN.md 方案 C）
   - 刪除 11×11 和 22×22 的錯誤描述

2. 統一 glyph 格式描述：
   - 24×24 = 74 bytes（2B code + 24 rows × 3B）
   - 16×16 = 34 bytes（2B code + 16 rows × 2B）

### 4.5 資源索引建立

**需要建立**：
1. `DATA1_RESOURCE_INDEX.md`：
   - 完整的 256 個 section 類型對照表
   - 每個 section 的大小、壓縮狀態、用途
   - 與 `resources.md` 的對應關係

2. `DATA2_RESOURCE_INDEX.md`（若 DATA2 有文字）：
   - 地圖/戰鬥/音效資源索引

---

## 五、建議的下一步（按優先級排序）

### 5.1 高優先級（P0）- 1-2 週

#### 5.1.1 修正文件錯誤
- [ ] 修正 PLAN.md 中的尺寸描述錯誤
- [ ] 修正 TRANSLATION.md 中的資源索引錯誤
- [ ] 統一所有文件中的格式描述

#### 5.1.2 建立完整資源索引
- [ ] 分析 DATA1 的 256 個 section
- [ ] 建立 `DATA1_RESOURCE_INDEX.md`
- [ ] 對應每個 section 的用途和內容類型

#### 5.1.3 提取物品/技能/怪物名稱
- [ ] 利用 `monsters.txt` 提取怪物名稱
- [ ] 利用 `resources.md` 提取物品名稱
- [ ] 利用 `levels.md` 提取關卡名稱
- [ ] 補充 TRANSLATION.md 的翻譯條目

### 5.2 中優先級（P1）- 2-4 週

#### 5.2.1 中文手冊處理
- [ ] 解壓縮 `珍066-火龍之戰.rar`
- [ ] 掃描中文手冊內容
- [ ] 建立「讀取段落」的中文化方案

#### 5.2.2 對話文字完整提取
- [ ] 分析 Section 0x03（859 個文字）的完整對話
- [ ] 分析其他 section 的對話文字
- [ ] 建立完整的對話翻譯表

#### 5.2.3 翻譯文件結構重整
- [ ] 按來源分類翻譯文件
- [ ] 建立統一的翻譯格式
- [ ] 建立翻譯進度追蹤表

### 5.3 低優先級（P2）- 4-8 週

#### 5.3.1 CJK 渲染實作
- [ ] 建立 `src/lib/cjk_font.h` 和 `src/lib/cjk_font.c`
- [ ] 實作 24×24 中文點陣顯示
- [ ] 修改 `fe/vga_sdl.c` 支援 pixel scaling
- [ ] 實作 Unicode/Big5 編碼轉換

#### 5.3.2 SDL2 功能實作
- [ ] 實作 `src/lib/config.h` 和 `src/lib/config.c`
- [ ] 實作 `src/lib/audio.h` 和 `src/lib/audio.c`
- [ ] 實作按鍵映射 `src/lib/keymap.h` 和 `src/lib/keymap.c`

#### 5.3.3 重構與測試
- [ ] 結構化 engine.c 的全域變數
- [ ] 消除 ui.c 的魔術數字
- [ ] 實作完整的測試套件
- [ ] 建立 Docker 建置環境

---

## 六、風險評估

### 6.1 高風險項目

1. **中文手冊版權**：
   - `珍066-火龍之戰.rar` 可能是版權內容
   - 建議：確認版權狀態，或使用 OCR + 重新翻譯

2. **字型版權**：
   - 24×24 中文點陣字型可能有版權
   - 建議：使用開源字型（如文泉驛點陣、全字庫）

3. **DATA1 格式限制**：
   - 翻譯後字串長度不同，可能無法直接 patch
   - 建議：實作「固定長度欄位 + 截斷/補零」策略

### 6.2 中風險項目

1. **113 個未實作 opcodes**：
   - 部分可能是遊戲核心功能
   - 建議：優先實作與文字顯示相關的 opcodes

2. **SDL2 顯示升級**：
   - 24×24 cell 與 8×8 cell 混排複雜
   - 建議：先實作 16×16 保守方案

### 6.3 低風險項目

1. **反組譯還原**：
   - 52 個 unnamed 函式已全部更名
   - 143 個 opcodes 已命名
   - 風險低，但需要驗證正確性

2. **文字提取**：
   - 3926 個文字串已提取
   - 風險低，但需要驗證完整性

---

## 七、結論

### 7.1 優點

1. ✅ **文件品質高**：PLAN.md、ANALYSIS.md、SKILL.md 結構清晰，內容完整
2. ✅ **文字提取成果豐碩**：3926 個文字串，16 個 Section 完整掃描
3. ✅ **反組譯還原完成**：52 個 unnamed 函式更名，143 個 opcodes 命名
4. ✅ **中文化方案合理**：分層架構、Pixel Scaling、Big5 編碼
5. ✅ **技術路線正確**：SDL2 取代 DOS、24×24 中文點陣

### 7.2 缺點

1. ❌ **翻譯覆蓋率不足**：只有 2.5% 的文字已翻譯
2. ❌ **關鍵文字缺失**：物品/技能/怪物/關卡名稱大部分未提取
3. ❌ **中文手冊未處理**：Read Paragraph 功能完全沒有中文化
4. ❌ **文件與原始文件脫節**：未充分利用 opendw 原始文件
5. ❌ **實作尚未開始**：0 個 CJK 或 SDL2 相關檔案已實作
6. ❌ **文件格式錯誤**：PLAN.md 和 TRANSLATION.md 有尺寸描述矛盾

### 7.3 整體評分

| 項目 | 評分 | 說明 |
|------|------|------|
| 文件品質 | 8/10 | 結構清晰，但有一些錯誤 |
| 翻譯進度 | 3/10 | 只有 2.5% 覆蓋率 |
| 技術規劃 | 8/10 | 方案合理，但實作尚未開始 |
| 可行性 | 7/10 | 技術路線正確，但工程量巨大 |
| **總評** | **6.5/10** | **規劃良好，但實作才剛開始** |

### 7.4 建議

1. **立即修正**：修正 PLAN.md 和 TRANSLATION.md 中的錯誤
2. **短期目標**（1-2 週）：建立完整資源索引，提取物品/技能/怪物名稱
3. **中期目標**（2-4 週）：處理中文手冊，建立「讀取段落」中文化方案
4. **長期目標**（4-8 週）：實作 CJK 渲染、SDL2 升級、完整翻譯

---

## 八、附錄

### 8.1 關鍵數據

- **DATA1 大小**：296,439 bytes
- **DATA2 大小**：352,430 bytes
- **DRAGON.COM 大小**：55,217 bytes
- **DWTRAN.COM 大小**：4,044 bytes（角色轉移工具）
- **提取的文字串**：3926 個
- **已翻譯的文字串**：~100 個（2.5%）
- **未實作的 opcodes**：113 個
- **已命名的函式**：52 個
- **已命名的 opcodes**：143 個

### 8.2 文件清單

**本專案文件**：
- `docs/README.md`（503 bytes）
- `docs/PLAN.md`（21,611 bytes）
- `docs/ANALYSIS.md`（13,899 bytes）
- `docs/TRANSLATION.md`（15,385 bytes）
- `docs/SKILL.md`（8,417 bytes）
- `docs/SDL2_IMPLEMENTATION.md`（11,459 bytes）
- `docs/ALL_TEXT_FROM_DATA1.txt`（141,230 bytes）
- `docs/dragon.asm`（71,852 bytes）
- `docs/Dragon-Wars_Manual_DOS_EN.pdf`（7.5 MB）
- `珍066-火龍之戰.rar`（17.7 MB）

**原始 opendw 文件**：
- `doc/script.md`（2,247 bytes）
- `doc/resources.md`（1,981 bytes）
- `doc/viewport.md`（564 bytes）
- `doc/monsters.txt`（250 bytes）
- `doc/levels.md`（800 bytes）
- `doc/keypress.txt`（722 bytes）
- `doc/style.md`（503 bytes）
- `doc/interrupts.md`（811 bytes）
- `doc/references.md`（166 bytes）

### 8.3 資源索引（從 resources.md）

| Resource | 大小 | 壓縮 | 用途 |
|----------|------|------|------|
| 0 | 1148 | 否 | 初始遊戲腳本 |
| 7 | 5632 | 否 | 角色資料 |
| 29 | 未知 | 是 | 標題畫面 |
| 31 | 2177 | 是 | 怪物字串資料 |
| 71 | 5846 | 是 | Purgatory 關卡 |
| 110 | 11860 | 是 | Castle wall（視埠） |
| 111 | 7050 | 是 | Sky portion（視埠） |
| 112 | 5054 | 是 | Red clay road portion（視埠） |
| 116 | 936 | 是 | Water puddle（視埠） |
| 261 | 12452 | 是 | Scream（PCM 音訊） |

### 8.4 怪物資源索引（從 monsters.txt）

| Resource | 怪物名稱 |
|----------|----------|
| 168 | Wolf（野狗？） |
| 196 | Spider / Rock Spiders |
| 200 | Innocent Man |
| 210 | Pikeman |
| 222 | Fanatic / Loon |

### 8.5 DATA1 Section 類型（從 ALL_TEXT_FROM_DATA1

| Section | 文字數 | 主要用途 |
|---------|--------|----------|
| 0x00 | 241 | 初始遊戲腳本（主選單、角色建立） |
| 0x01 | 4 | 角色資料初始化 |
| 0x02 | 20 | 遊戲狀態文字 |
| 0x03 | 859 | 對話文字（最大量） |
| 0x04 | 10 | UI 文字 |
| 0x05 | 62 | 物品/技能相關 |
| 0x06 | 481 | 戰鬥/遭遇文字 |
| 0x07 | 34 | 角色資料（Character data） |
| 0x08 | 134 | 法術相關 |
| 0x09 | 112 | 地圖/關卡相關 |
| 0x0A | 181 | NPC 對話 |
| 0x0B | 137 | 商店/交易 |
| 0x0C | 151 | 戰鬥訊息 |
| 0x0D | 228 | 物品描述 |
| 0x0E | 59 | 技能描述 |
| 0x0F | 156 | 隨機事件 |
| 0x11 | 71 | 任務/目標 |
| 0x12 | 215 | 劇情文字 |
| 0x13 | 640 | 對話選項 |
| 0x14 | 86 | 戰鬥選項 |
| 0x15 | 15 | 技能名稱（Mountain Lore 等） |
| 0x16 | 30 | 其他 |

**觀察**：
- Section 0x03（859 個文字）是最大量的對話文字，應優先處理
- Section 0x13（640 個文字）是對話選項，也很重要
- Section 0x06（481 個文字）是戰鬥/遭遇文字
- Section 0x00（241 個文字）包含主選單，大部分已翻譯
- Section 0x15（15 個文字）包含技能名稱，只提取了 5 個

**建議**：
1. 優先翻譯 Section 0x03（對話）和 Section 0x13（選項）
2. 利用 Section 類型資訊，建立分類翻譯表
3. 每個 Section 建立獨立的翻譯檔案

---

## 九、總結

本專案在**規劃階段**表現優異，文件品質高、技術路線正確。但在**實作階段**才剛剛開始，翻譯覆蓋率嚴重不足（僅 2.5%），且關鍵的遊戲資料（物品、技能、怪物、關卡名稱）大部分未提取。

**核心問題**：
1. 翻譯進度嚴重落後
2. 中文手冊未處理（Read Paragraph 功能）
3. 實作尚未開始（SDL2、CJK 渲染）
4. 文件有一些錯誤（尺寸描述矛盾）

**建議的行動**：
1. **立即**：修正文件錯誤，建立完整資源索引
2. **短期**（1-2 週）：提取物品/技能/怪物/關卡名稱，處理中文手冊
3. **中期**（2-4 週）：完成對話文字翻譯，建立翻譯工具鏈
4. **長期**（4-8 週）：實作 CJK 渲染、SDL2 升級、整合測試

**最終評估**：本專案有良好的基礎，但需要大量的實作工作才能完成中文化。建議按優先級逐步執行，先完成翻譯工作，再進行實作。

---

**報告結束**
