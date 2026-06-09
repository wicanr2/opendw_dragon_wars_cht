# OpenDW Dragon Wars 中文化 Skill

## 概述

本 Skill 記錄了將 OpenDW（Dragon Wars 開源重製版）中文化的完整經驗與工作流程。
所有成果已推送至：https://github.com/wicanr2/opendw_dragon_wars_cht

**專案路徑**：`/home/anr2/tmp/longcat/opendw_dragon_wars_cht/`

**目前進度**（2026-06-09）：
- 52 個 unnamed 函式已更名
- 143 個 opcodes 已命名
- 3,926 個文字串從 DATA1 提取
- 100+ 翻譯條目完成（覆蓋率約 2.5%）
- 反組譯分析完成
- 中文化方案設計完成
- **實作尚未開始**（SDL2、CJK 渲染）

**整體評分**：6.5/10（規劃良好，實作才剛開始）

---

## 工作流程

### 1. 反組譯還原 (Decompilation Restoration)

#### 目標
將 opendw 中所有 `sub_XXX` 和未命名的 `op_XX` 函式還原為有意義的名稱。

#### 方法
1. **對照 dragon.asm**：`/opendw/dos/dragon.asm` 是位元組級反組譯，包含每個函式的原始組合語言位址
2. **分析函式行為**：根據暫存器操作、呼叫的系統中斷、資料結構偏移量來判斷函式功能
3. **交叉比對**：將 C 程式碼與組合語言逐一對照，確認功能吻合
4. **命名規則**：使用動詞+名詞格式，如 `decode_viewport_data`、`draw_player_status_panel`

#### 關鍵檔案
- `src/lib/engine.c`：虛擬 CPU + 115 個 opcode handler
- `src/lib/ui.c`：UI 繪製（字元、矩形、視埠）
- `src/lib/resource.c`：資源載入（DATA1/DATA2/dragon.com）
- `dos/dragon.asm`：原始 DOS 反組譯參考

#### 還原成果
- 52 個 `sub_XXX` 函式全部重新命名
- 143 個 `op_XX` opcode 全部標記（已實作/未實作/未使用）
- 每個函式加入中英文雙語描述

### 2. 資料檔案分析

#### 檔案結構
| 檔案 | 大小 | 用途 |
|------|------|------|
| `DRAGON.COM` | 55,217 bytes | 主程式（DOS COM 格式） |
| `DATA1` | 296,439 bytes | 遊戲資源（script、圖片、字型） |
| `DATA2` | 352,430 bytes | 地圖/戰鬥/音效資源 |
| `DWTRAN.COM` | 4,044 bytes | 角色轉移工具（Bard's Tale I/II） |

#### 資源格式
- **Header**：768 bytes，256 個 16-bit 值（每個 section 的大小）
- **Section 類型**：
  - `0x00` = SCRIPT（遊戲邏輯 + 文字）
  - `0x07` = CHARACTER_DATA（角色資料）
  - `0x18-0x1D` = TITLE0-TITLE3（標題畫面）
  - `>0x17` = 壓縮資料（LZSS）

#### 文字編碼
- 使用 5-bit 壓縮字母表（見 `compress.c` 的 `alphabet[]`）
- 每個字元用 5 bits 編碼，特殊字元用 6 bits
- 大寫字母使用 `0x1E` 標記
- 文字由 `extract_string()` 函式從 script 中提取

### 3. 中文化規劃

#### 顯示升級
- **方案 B（推薦）**：Pixel Scaling
  - 維持 320×200 framebuffer（所有遊戲邏輯不改）
  - 中文字使用 11×11 點陣
  - SDL2 用 `SDL_RenderSetScale(2.0, 2.0)` 放大到 640×400
  - 每個中文字 11 pixels 寬（放大後 22 pixels）

#### 字型需求
- **推薦尺寸**：22×22 繁體中文點陣
- **來源**：文泉驛點陣字型（開源）
- **格式**：外部 `.fnt` 檔案，每個 glyph = 2 bytes code + 24 rows × 3 bytes

#### 音訊取代
- **PC Speaker 音樂** → SDL2 Audio
- **音效** → SDL2 音訊回調
- **取樣率**：44100 Hz，16-bit，單聲道

#### 按鍵映射
| 按鍵 | SDL Keycode | 功能 |
|------|-------------|------|
| ↑ | `SDLK_UP` | 上移 |
| ↓ | `SDLK_DOWN` | 下移 |
| ← | `SDLK_LEFT` | 左移 |
| → | `SDLK_RIGHT` | 右移 |
| Enter | `SDLK_RETURN` | 確認 |
| ESC | `SDLK_ESCAPE` | 取消 |

### 4. 未實作功能

#### 需要 SDL2 取代的功能
1. **DOS 設定選單** (`0x627-0x963`) → `config.h/c` 現代設定系統
2. **PC Speaker 音樂** (`0x5C3B-0x5D1D`) → `audio.h/c` SDL2 Audio
3. **~85 個未實作 opcode** → 分類後實作/標記為 unused

#### 未實作 opcode 分類
| 類別 | 數量 | 處理方式 |
|------|------|----------|
| 圖形模式相關 | ~15 | SDL2 不需要，標記為 unused |
| 音效相關 | ~10 | 改用 SDL2 Audio |
| 檔案 I/O | ~5 | 改用 stdio |
| 記憶體管理 | ~5 | 改用 malloc |
| 遊戲邏輯 | ~30 | 需要實作 |
| 未知功能 | ~20 | 需要進一步調查 |

### 5. 翻譯工作

#### 已知文字
- UI 文字：從 `dragon.com` 提取（已完整列出）
- 狀態文字：從 `ui.c` 提取（chained, poisoned, stunned, dead）
- 對話/物品名稱：在 DATA1 script 中，需要正確解壓

#### 翻譯檔案
- `docs/10_TRANSLATION.md`：已知的中英文對照表（100+ 條目，2.5% 覆蓋率）
- 需要從 DATA1 提取完整的對話和物品名稱

#### 翻譯進度（依類別）
| 類別 | 已翻譯 | 總量 | 覆蓋率 |
|------|--------|------|--------|
| 主選單 | 20 | ~30 | 67% |
| 狀態文字 | 6 | ~10 | 60% |
| 戰鬥文字 | 14 | ~50 | 28% |
| 法術文字 | 16 | ~30 | 53% |
| 物品文字 | 18 | ~100 | 18% |
| 對話文字 | 70 | ~1000 | 7% |
| 技能名稱 | 6 | ~20 | 30% |
| UI 文字 | 18 | ~50 | 36% |
| 怪物名稱 | 0 | ~50 | 0% |
| 關卡名稱 | 0 | ~30 | 0% |

#### 缺失的文字類型（高優先級）
1. **完整對話文字**：Section 0x03 有 859 個文字，大部分未翻譯
2. **物品名稱**：只有 2 個物品名稱（leather armor, plate and chain armor）
3. **怪物名稱**：完全缺失（Wolf, Spider, Pikeman 等）
4. **關卡名稱**：完全缺失（Purgatory, Castle wall, Sky portion 等）
5. **角色名稱**：完全缺失（遊戲有 7 個預設角色）
6. **法術描述**：只有 16 個法術名稱，缺少完整描述
7. **隨機遭遇文字**：戰鬥相關文字只翻譯了部分

#### "Read Paragraph" 文字完全缺失
- 遊戲中的 "Paragraph" 功能是重要劇情顯示
- 文字來自遊戲手冊（`珍066-火龍之戰.rar`），**無法從 DATA1 提取**（DATA1 只有英文）
- 需要從中文手冊掃描檔提取

### 6. 建置與測試

#### Docker 建置
```dockerfile
FROM debian:bullseye-slim
RUN apt-get update && apt-get install -y build-essential cmake libsdl2-dev
WORKDIR /app
COPY . .
RUN mkdir build && cd build && cmake .. && make
VOLUME ["/app/build/src/fe/data"]
CMD ["./src/fe/sdldragon"]
```

#### 測試計畫
1. 設定系統測試：載入/儲存、視窗大小、全螢幕
2. 音訊系統測試：播放音符、音樂、音效
3. 按鍵映射測試：所有方向鍵、確認/取消
4. 遊戲邏輯測試：未實作 opcode 不崩潰

## 經驗教訓

### 文字提取問題
- DATA1 的文字使用 5-bit 壓縮，直接搜索找不到
- 需要正確實作 `bit_extract()` 和 `extract_letter()` 才能解壓
- script section 包含 opcodes 和文字混合，需要跳過 opcodes 才能找到文字

### 反組譯技巧
- 從 `int 10h`（視訊中斷）和 `int 21h`（DOS 中斷）可以判斷功能
- 從暫存器操作（如 `mov ax, 0x13` 設定 VGA 模式）可以推斷用途
- 從資料結構偏移量（如 `0xC960` = player data base）可以識別資料類型

### 常見陷阱
- `sub_XXX` 名稱在 engine.c 中重複出現（前向宣告 vs 實作）
- 有些 `op_XX` 實際上是跳躍表項，不是真正的 opcode
- `com_extract()` 從 dragon.com 提取，`resource_load()` 從 DATA1 提取

### 7. 資源索引

#### DATA1 Section 結構
| Section | 文字數 | 主要用途 | 萃取狀態 |
|---------|--------|----------|----------|
| 0x00 | 241 | 初始遊戲腳本（主選單、角色建立） | ✅ 完成 |
| 0x01 | 4 | 角色資料初始化 | ✅ 完成 |
| 0x02 | 20 | 遊戲狀態文字 | ✅ 完成 |
| 0x03 | 859 | 對話文字（最大量） | ✅ 完成 |
| 0x04 | 10 | UI 文字 | ✅ 完成 |
| 0x05 | 62 | 物品/技能相關 | ✅ 完成 |
| 0x06 | 481 | 戰鬥/遭遇文字 | ✅ 完成 |
| 0x07 | 34 | 角色資料（Character data） | ✅ 完成 |
| 0x08 | 134 | 法術相關 | ✅ 完成 |
| 0x09 | 112 | 地圖/關卡相關 | ✅ 完成 |
| 0x0A | 181 | NPC 對話 | ✅ 完成 |
| 0x0B | 137 | 商店/交易 | ✅ 完成 |
| 0x0C | 151 | 戰鬥訊息 | ✅ 完成 |
| 0x0D | 228 | 物品描述 | ✅ 完成 |
| 0x0E | 59 | 技能描述 | ✅ 完成 |
| 0x0F | 156 | 隨機事件 | ✅ 完成 |
| 0x11 | 71 | 任務/目標 | ✅ 完成 |
| 0x12 | 215 | 劇情文字 | ✅ 完成 |
| 0x13 | 640 | 對話選項 | ✅ 完成 |
| 0x14 | 86 | 戰鬥選項 | ✅ 完成 |
| 0x15 | 15 | 技能名稱（Mountain Lore 等） | ✅ 完成 |
| 0x16 | 30 | 其他 | ✅ 完成 |

**關鍵觀察**：
- Section 0x03（859 個文字）是最大量的對話文字，應優先處理
- Section 0x13（640 個文字）是對話選項，也很重要
- Section 0x15（15 個文字）包含技能名稱，只提取了 5 個
- Section 0x10、0x11 有部分文字（71+30=101 個）

#### 外部資源索引
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

#### 怪物資源索引
| Resource | 怪物名稱 |
|----------|----------|
| 168 | Wolf（野狗？） |
| 196 | Spider / Rock Spiders |
| 200 | Innocent Man |
| 210 | Pikeman |
| 222 | Fanatic / Loon |

### 8. 文字提取 (Text Extraction)

#### 目標
從 DATA1 資源檔中提取遊戲文字（對話、物品名稱、選單）

#### 關鍵發現
1. **文字編碼**：遊戲使用 5-bit 壓縮字母表（見 `compress.c` 的 `alphabet[]`）
2. **資料位置**：文字在 DATA1 的 Section 0x00（SCRIPT）中
3. **解壓方式**：使用 `extract_string()` 函式從 script 中提取
4. **字母表對應**：
   - `0xa0` = 空格
   - `0xe1-0xfa` = 小寫字母 a-z 和數字 0-9
   - `0xb0-0xb9` = 數字 0-9（備用）
   - `0x41-0x5a` = 大寫字母 A-Z
   - `0x8d` = 換行符
   - 其他 = 標點符號（`!`, `'`, `:`, `/`, `(`, `)`, `]`, `-`, `+`, `*`, `,`, `.`）

#### 解壓演算法
```python
class BitExtractor:
    def __init__(self, data, byte_offset=0):
        self.data = data
        self.byte_offset = byte_offset
        self.num_bits = 0
        self.bit_buffer = 0
        self.upper_case = 0
    
    def extract(self, n):
        """從資料流中提取 n 個位元"""
        al = 0
        for i in range(n):
            if self.num_bits == 0:
                self.bit_buffer = self.data[self.byte_offset]
                self.num_bits = 8
                self.byte_offset += 1
            tmp = self.bit_buffer
            self.bit_buffer = (self.bit_buffer << 1) & 0xFF
            self.num_bits -= 1
            carry = 1 if tmp > self.bit_buffer else 0
            al = (al << 1) | carry
        return al
    
    def extract_letter(self):
        """提取一個字母（5-bit 編碼）"""
        while True:
            ret = self.extract(5)
            if ret == 0:  # 字串結束
                return 0
            if ret == 0x1E:  # 大寫標記
                self.upper_case = (self.upper_case >> 1) | 0x80
                continue
            if ret > 0x1E:  # 擴展字元
                ret = self.extract(6)
                ret += 0x1E
            return alphabet[ret - 1]
```

#### 提取成果
- 24+ 個選單文字已提取並翻譯
- 狀態文字（chained, poisoned, stunned, dead）已提取
- UI 文字（載入、存檔、錯誤訊息）已提取
- 物品名稱和對話文字需要進一步提取

#### 注意事項
- bit_extractor 的 `carry` 計算是關鍵：`carry = 1 if tmp > (tmp << 1) else 0`
- 文字可能包含控制碼（如快捷鍵標記）
- 每個選單項目的第一個字通常是快捷鍵（如 'B' = Begin, 'C' = Continue）

### 9. 技術細節補充

#### 中文化方案：Pixel Scaling + 24×24
- 維持 320×200 framebuffer（所有遊戲邏輯不改）
- 中文字使用 24×24 點陣
- SDL2 用 `SDL_RenderSetScale(2.0, 2.0)` 放大到 640×400
- 視窗 640×480，上下留 40px 黑邊
- 每列可容納 13 個中文字（320÷24≈13）
- 每行可容納 8 個中文字（200÷24≈8）

#### 字型需求
- **推薦**：24×24 繁體中文點陣（文泉驛點陣、全字庫）
- **格式**：外部 `.fnt` 檔案，每個 glyph = 2 bytes code + 24 rows × 3 bytes = 74 bytes
- **內建**：可將常用 5000 個中文字編譯為 C 陣列，約 360KB

#### 編碼選擇：Big5
- 1989 年遊戲原始環境為 DOS，台灣/香港 DOS 使用 Big5
- 每個中文字 2 bytes，可與現有 `unsigned char *` 字串處理相容
- 避免 UTF-8 變長解析，減少 engine.c 修改量

#### 翻譯策略
- **方法 A**：Patch DATA1（固定長度欄位，不足補零，過長截斷）
- **方法 B**：執行時動態替換（hook `extract_string()`）
- **推薦**：方法 B 較靈活，但方法 A 較簡單

### 10. 經驗教訓總結

#### 已解決的問題
1. **5-bit 文字提取**：正確實作 `bit_extract()` 和 `extract_letter()` 才能解壓
2. **script section 混合**：script 包含 opcodes 和文字混合，需要跳過 opcodes 才能找到文字
3. **反組譯對應**：從 `int 10h`（視訊中斷）和 `int 21h`（DOS 中斷）可以判斷功能
4. **命名衝突**：`sub_XXX` 名稱在 engine.c 中有重複出現（前向宣告 vs 實作）

#### 待處理的問題（從審查報告）
1. **翻譯覆蓋率不足**：只有 2.5% 的文字已翻譯
2. **關鍵文字缺失**：物品/技能/怪物/關卡名稱大部分未提取
3. **中文手冊未處理**：Read Paragraph 功能完全沒有中文化
4. **文件與原始文件脫節**：未充分利用 opendw 原始文件
5. **文件格式錯誤**：PLAN.md 和 TRANSLATION.md 有尺寸描述矛盾
6. **技術債務**：engine.c 有 100+ 個全域變數未結構化

#### 關鍵限制
1. **中文手冊 RAR 無法完整解壓縮**：`7z` 僅能解出 3 個 JPG（RAR4 不支援），需安裝 `unrar`
2. **英文 PDF 為純掃描影像**：`pdftotext` 無法萃取，需 OCR 或手動輸入
3. **DATA1 完整 section map 尚未建立**：尚不清楚 sections 0x10–0x16 的內容

### 11. 待辦事項（按優先級）

#### P0（高優先級）- 1-2 週
- [ ] 修正 PLAN.md 中的尺寸描述錯誤（11×11 → 24×24）
- [ ] 修正 TRANSLATION.md 中的資源索引錯誤
- [ ] 建立完整資源索引（DATA1_RESOURCE_INDEX.md）
- [ ] 安裝 `unrar` 並完整解壓縮中文手冊
- [ ] 從中文手冊提取 "Read Paragraph" 內容

#### P1（中優先級）- 2-4 週
- [ ] 分析 DATA1 sections 0x10–0x16 結構
- [ ] 萃取物品/法術/怪物名稱
- [ ] 優先翻譯 Section 0x03（對話）和 Section 0x13（選項）
- [ ] 建立分類翻譯表（menu.po, dialogue.po, items.po 等）

#### P2（低優先級）- 4-8 週
- [ ] 實作 CJK 渲染（src/lib/cjk_font.h, cjk_font.c）
- [ ] 實作 SDL2 設定系統（config.h/c）
- [ ] 實作 SDL2 音訊系統（audio.h/c）
- [ ] 實作按鍵映射（keymap.h/c）
- [ ] 結構化 engine.c 的全域變數
- [ ] 消除 ui.c 的魔術數字

### 12. 參考資源

#### 專案資源
- GitHub Repo：https://github.com/wicanr2/opendw_dragon_wars_cht
- OpenDW 原始：https://github.com/devinSmith/opendw
- Dragon Wars：https://www.gog.com/game/dragon_wars

#### 關鍵文件
| 路徑 | 內容 |
|------|------|
| `docs/99_INDEX.md` | 專案說明 |
| `docs/01_PLAN.md` | 中文化實作計畫 |
| `docs/02_ANALYSIS.md` | 反組譯分析 |
| `docs/10_TRANSLATION.md` | 翻譯對照表 |
| `docs/SKILL.md` | 本 Skill 文件 |
| `docs/REVIEW_REPORT.md` | 審查報告 |
| `docs/NEXT_PLAN.md` | 下一階段計畫 |
| `docs/ALL_TEXT_FROM_DATA1.txt` | DATA1 萃取文字（3,926 條） |
| `docs/dragon.asm` | 反組譯參考 |

#### 技術參考
- 5-bit 編碼：`src/lib/compress.c` 的 `alphabet[]` 和 `extract_letter()`
- 資源載入：`src/lib/resource.c` 的 `resource_load()`
- 文字萃取：`src/lib/compress.c` 的 `extract_string()`
- UI 繪製：`src/lib/ui.c` 的 `draw_character()`
