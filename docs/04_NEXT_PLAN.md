# OpenDW 龍之戰中文化 — 下一階段計畫

> **文件版本**：v1.0
> **日期**：2026-06-09
> **專案路徑**：`/home/anr2/tmp/longcat/opendw_dragon_wars_cht/`
> **目標**：為下一階段中文化工作提供可逐步執行的詳細計畫

---

## 當前盤點

### 已完成項目
| 項目 | 狀態 |
|------|------|
| 52 個 unnamed `sub_XXX` 函式重新命名 | ✅ 完成 |
| 143 個 `op_XX` opcode 命名與分類 | ✅ 完成 |
| 3,926 個 text strings 從 DATA1（sections 0x00–0x0F）萃取 | ✅ 完成 |
| 100+ 翻譯條目（`TRANSLATION.md`） | ✅ 完成 |
| 英文手冊 PDF（48 頁掃描影像） | ✅ 取得 |
| 遊戲截圖（`/home/anr2/tmp/longcat/org_dialogue/`） | ✅ 取得 |
| `dragon.asm` 反組譯對照表 | ✅ 完成 |

### 關鍵限制
| 限制 | 影響 | 目前狀態 |
|------|------|----------|
| 中文手冊 `珍066-火龍之戰.rar`（17MB, 46 檔案）僅能部分解壓縮 | 7z 對 RAR4 支援不完整，僅解出 3 個 JPG | 需安裝 `unrar` |
| 英文 PDF 為純掃描影像（非文字層） | `pdftotext` 無法萃取 | 需 OCR 或手動輸入 |
| DATA1 完整 section map 尚未建立 | 尚不清楚 sections 0x10–0x16 的內容 | 需重新分析 |

---

## 階段一：中文手冊內容萃取（Read Paragraph）

### 1.1 背景

「Read Paragraph」是 Dragon Wars 的重要遊戲機制。玩家在遊戲特定場景可查閱劇情段落，內容來自**遊戲說明書**而非 DATA1。中文版的「說明書補完計劃」掃描了完整臺灣版手冊（44 頁掃描影像 + Thumbs.db + 一個 .txt 檔案）。

### 1.2 RAR 解壓縮方案

#### 問題
`7z`（23.01）僅能解出 `000a.jpg`、`000b.jpg`、`001.jpg` 與 Thumbs.db，其餘 40+ JPG 解出 0 bytes（RAR4 壓縮法不支援）。

#### 方案 A：安裝 `unrar`（推薦）
```bash
# 需要 root 權限
sudo apt-get install -y unrar
mkdir -p /tmp/chinese_manual
cd /tmp/chinese_manual
unrar x /home/anr2/tmp/longcat/opendw_dragon_wars_cht/docs/珍066-火龍之戰.rar
```

#### 方案 B：使用 Docker + unrar（無 root 權限）
```bash
docker run --rm \
  -v /home/anr2/tmp/longcat/opendw_dragon_wars_cht/docs:/docs:ro \
  -v /tmp/chinese_manual:/out \
  alpine sh -c "apk add --no-cache unrar && unrar x /docs/珍066-火龍之戰.rar /out/"
```

#### 方案 C：Python `rarfile` 套件
```bash
pip install rarfile   # rarfile 4.0+ 支援 RAR4
python3 -c "
import rarfile
rf = rarfile.RarFile('docs/珍066-火龍之戰.rar')
rf.extractall('/tmp/chinese_manual')
"
```

#### 方案 D：手動搬移
若上述皆失敗，從 Windows 機器手動解壓縮後傳回開發機。

### 1.3 萃取後處理

#### Step 1：建立頁面索引
```bash
ls /tmp/chinese_manual/珍066-火龍之戰/*.jpg | sort \
  > docs/manual_page_list.txt
```

#### Step 2：OCR（可選）
掃描檔為 200 DPI，可嘗試 Tesseract：

```bash
# 安裝（需要 root）
sudo apt-get install -y tesseract-ocr tesseract-ocr-chi-tra

# 批次 OCR
for f in /tmp/chinese_manual/珍066-火龍之戰/2F3_SCAN1238_*.jpg; do
  echo "=== $f ==="
  tesseract "$f" stdout -l chi_tra+eng 2>/dev/null
done > docs/manual_ocr_raw.txt
```

**注意**：Tesseract 對繁體中文的辨識品質在 200 DPI 下通常可接受，但建議人工校對。

#### Step 3：人工建檔
建立 `docs/manual_paragraphs.md`：
```markdown
# 手冊「讀取段落」內容

## 掃描頁 001
[繁體中文內容]

## 掃描頁 002
[繁體中文內容]
...
```

### 1.4 關鍵段落清單（依遊戲流程）

| 遊戲場景 | 內容類型 | 優先度 |
|----------|---------|--------|
| 開場 / 序章 | 故事背景、火龍傳說 | P0 |
| 城市 / 酒保對話 | 傳聞、任務線索 | P0 |
| 商店 / 道具店 | 物品描述 | P1 |
| 法術說明 | 法術效果描述 | P1 |
| 各章節標題 | 章節目錄 | P0 |
| 結局段落 | 勝利 / 失敗結局 | P2 |
| 操作指南 | 遊戲說明 | P1 |

### 1.5 驗證
- 將萃取段落與遊戲截圖交叉比對
- 確認每個「Read Paragraph」場景都有對應內容
- 標記缺漏段落待補

---

## 階段二：DATA1 完整文字萃取

### 2.1 目標
- 擴充 `ALL_TEXT_FROM_DATA1.txt`（目前 3,926 條，sections 0x00–0x0F）
- 處理 sections 0x10–0x16（物品、法術、怪物資料）
- 分類為：對話 / 物品名 / 法術名 / 怪物名 / UI 文字

### 2.2 DATA1 Section 結構

根據 `doc/resources.md` 與 `resource.c`：

| Section | 類型 | 內容 |
|---------|------|------|
| `0x00` | SCRIPT | 遊戲邏輯 + 文字（**已處理**） |
| `0x01–0x06` | MAP/SCENE | 地圖 / 場景資料 |
| `0x07` | CHARACTER_DATA | 角色樣板資料 |
| `0x08–0x0F` | TEXT | 對話 / 描述文字（**已處理**） |
| **`0x10–0x12`** | **ITEM_DATA** | **物品資料（待處理）** |
| **`0x13–0x14`** | **SPELL_DATA** | **法術資料（待處理）** |
| **`0x15–0x16`** | **MONSTER_DATA** | **怪物資料（待處理）** |
| `0x18–0x1D` | TITLE0–TITLE3 | 標題畫面 |
| `>0x17` | COMPRESSED | LZSS 壓縮資料 |

### 2.3 萃取步驟

#### Step 1：建立 section map
```bash
# 用 resextract 工具遍歷所有 section
cd /home/anr2/tmp/longcat/opendw_dragon_wars_cht
for i in 10 11 12 13 14 15 16; do
  idx=$(printf "%d" $i)
  ./build/src/tools/resextract -i $idx -d data1 -o /tmp/section_${i}.bin 2>&1
done
```

#### Step 2：分析 binary 結構
```bash
# 查看 hex
xxd /tmp/section_10.bin | head -50
hexdump -C /tmp/section_10.bin | head -50
```

#### Step 3：新增 `section_dump.cpp`
```cpp
// src/tools/section_dump.cpp — 簡易 section 傾印工具
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../lib/resource.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <data1> <section> [output.txt]\n", argv[0]);
        return 1;
    }
    // 載入指定 section
    // 逐 byte 輸出 hex + ASCII
    // 標注可能的文字區域
    return 0;
}
```

#### Step 4：萃取物品 / 法術 / 怪物名稱

**物品名稱**（推測 section 0x10–0x12）：
```
struct item_record {
    uint16_t item_id;
    char name[20];      // null-terminated ASCII
    uint8_t type;       // 武器 / 防具 / 消耗品
    uint8_t stats[8];   // 屬性數值
};
```

**法術名稱**（推測 section 0x13–0x14）：
```
struct spell_record {
    uint16_t spell_id;
    char name[20];
    uint8_t mana_cost;
    uint8_t level;
};
```

**怪物名稱**（推測 section 0x15–0x16）：
```
struct monster_record {
    uint16_t monster_id;
    char name[20];
    uint16_t hp;
    uint8_t attack;
    uint8_t defense;
};
```

### 2.4 萃取後整合

#### 新增輸出檔案
```
docs/
  ALL_TEXT_FROM_DATA1.txt       # 既有（3,926 條）
  DATA1_SECTION_MAP.md          # 新增：section-by-section 索引
  ITEM_NAMES.txt                # 新增：物品名稱列表
  SPELL_NAMES.txt               # 新增：法術名稱列表
  MONSTER_NAMES.txt             # 新增：怪物名稱列表
  NPC_DIALOGUE.txt              # 新增：NPC 對話（分類後）
```

#### 物品名稱格式
```
[ITEM_001] Sword         劍
[ITEM_002] Leather Armor 皮甲
[ITEM_003] Healing Potion 治療藥水
```

#### 法術名稱格式
```
[SPELL_001] Lesser Heal   次級治療
[SPELL_002] Mage Light    法師之光
[SPELL_003] Fire Storm    火焰風暴
```

#### 怪物名稱格式
```
[MON_001] Rat             老鼠
[MON_002] Goblin          哥布林
[MON_003] Dragon          火龍
```

### 2.5 驗證方式
- 將萃取出的物品 / 法術 / 怪物名稱與遊戲截圖交叉比對
- 確認數量合理（原版約 50–100 物品、20–30 法術、30–50 怪物）
- 檢查是否有亂碼（表示萃取邏輯有誤）

---

## 階段三：遊戲腳本分析

### 3.1 目標
- 理解 `*.scr` 檔案格式（`/home/anr2/tmp/longcat/opendw/script/`）
- 辨識哪些文字在遊戲中被顯示、哪些是 script 控制碼
- 交叉比對 script 中的文字與 DATA1 萃取的文字

### 3.2 現況
- 有 13 個 `.scr` 檔案 + `init.cpp` + `encounter.scr`
- 這些是 Devin Smith 的**反組譯組合語言註解格式**，非原始二進位 script
- 格式範例（`script01.scr`）：
  ```
  0x0000: word_3AE2 = 0xff
  0x0002: load_resource res: 0x03, offset: 0x0000
  0x0006: jnc 0x001f
  0x0009: refresh_viewport
  ```

### 3.3 分析步驟

#### Step 1：建立 opcode 字典
整理 `docs/OPCODE_REFERENCE.md`：
```
| Opcode | 名稱 | 參數 | 說明 |
|--------|------|------|------|
| 0x00   | set_word_mode | - | 切換 16-bit 模式 |
| 0x01   | set_byte_mode | - | 切換 8-bit 模式 |
| 0x7A   | extract_string | 1B | 從資源萃取字串 |
| 0x95   | ui_draw_string | - | 在螢幕繪製字串 |
```

#### Step 2：辨識文字顯示相關 opcodes
- `op_7A`（`extract_string`）— 從資源萃取字串
- `op_95`（`ui_draw_string`）— 繪製字串
- `op_96`（`draw_padded_string`）— 繪製填充字串
- `op_7D`（`write_character_name`）— 顯示角色名字

#### Step 3：交叉比對
1. 對每個 `.scr` 檔案，找出所有 `extract_string` 呼叫的資源索引
2. 將索引對應到 DATA1 中的 section/offset
3. 確認該位置的文字與 `ALL_TEXT_FROM_DATA1.txt` 中的記錄一致

### 3.4 新增工具：`script_lint.cpp`
```cpp
// src/tools/script_lint.cpp — 掃描 .scr 檔案
// 找出所有文字顯示相關 opcode，輸出資源索引清單
#include <cstdio>
#include <cstring>

int main(int argc, char *argv[]) {
    // 解析 .scr 檔案
    // 找出 op_7A, op_95, op_96 呼叫
    // 輸出資源索引清單
    return 0;
}
```

### 3.5 驗證方式
- 確認所有 `extract_string` 的資源索引都能在 `ALL_TEXT_FROM_DATA1.txt` 中找到
- 若有缺失，補齊該 section 的萃取
- 確認 script 中的跳躍位址與 DATA1 中的 section 結構一致

---

## 階段四：完整驗證計畫

### 4.1 文字完整性驗證

#### 方法 A：動態驗證（執行遊戲並記錄）
```bash
# 在遊戲中添加日誌，記錄所有萃取的文字
./build/src/fe/sdldragon --log-text /tmp/text_log.txt

# 與萃取結果比對
diff /tmp/text_log.txt docs/ALL_TEXT_FROM_DATA1.txt
```

#### 方法 B：靜態分析
- 用 `resextract` 遍歷所有 section
- 對每個 section 執行 `extract_string()` 嘗試
- 記錄成功 / 失敗的 offset

#### 方法 C：交叉比對
- 將 `ALL_TEXT_FROM_DATA1.txt` 與 `TRANSLATION.md` 交叉比對
- 確認每條翻譯都有對應的英文原文
- 確認每條英文原文都有對應的翻譯

### 4.2 「Read Paragraph」文字驗證

#### 驗證清單
```
[ ] 開場劇情段落
[ ] 各城市酒保傳聞
[ ] 道具店物品描述
[ ] 法術說明
[ ] 結局段落
[ ] 章節標題
```

### 4.3 英文手冊交叉比對
將英文 PDF 掃描檔 OCR 後，與 DATA1 中的英文文字交叉比對：
```bash
# 將 PDF 轉為圖片
pdftoppm -png -r 200 docs/Dragon-Wars_Manual_DOS_EN.pdf /tmp/manual_page

# OCR
for f in /tmp/manual_page-*.png; do
  tesseract "$f" stdout -l eng 2>/dev/null
done > /tmp/english_manual_ocr.txt

# 與 DATA1 文字比對
diff /tmp/english_manual_ocr.txt docs/ALL_TEXT_FROM_DATA1.txt
```

### 4.4 自動化驗證腳本

#### `tools/verify_extraction.py`
```python
#!/usr/bin/env python3
"""
驗證 DATA1 文字萃取完整性
1. 載入 DATA1
2. 遍歷所有 section
3. 嘗試萃取文字
4. 與 ALL_TEXT_FROM_DATA1.txt 交叉比對
5. 輸出缺失的文字
"""
import struct
import sys

def load_data1(path):
    with open(path, 'rb') as f:
        header = f.read(768)
        sections = struct.unpack('<256H', header)
        data = f.read()
    return sections, data

def extract_all_strings(sections, data):
    # 實作文字萃取邏輯
    pass

if __name__ == '__main__':
    sections, data = load_data1(sys.argv[1])
    strings = extract_all_strings(sections, data)
    # 與已知列表比對
```

---

## 階段五：中文顯示實作

### 5.1 相依順序（依 PLAN.md 規劃）

```
Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 4
  ↓          ↓          ↓          ↓          ↓
Pixel      Text       Script     SDL2       UI 調整
Scaling    Rendering  文字替換   優化
+24×24
```

### 5.2 Phase 0：Pixel Scaling + 24×24 渲染層

#### 新增檔案
- `src/lib/cjk_font.h` — CJK 字型載入介面
- `src/lib/cjk_font.c` — CJK 字型載入實作

#### 修改檔案
- `src/fe/vga_sdl.c` — 設定 640×480 視窗 + `SDL_RenderSetScale(2.0, 2.0)`
- `src/lib/vga.c` — 維持 320×200 framebuffer

#### 字型選擇
- **推薦**：文泉驛點陣 24×24（GPL 授權，繁體中文完整）
- **備選**：全字庫 24×24（CNS 標準）
- **格式**：`.fnt` 檔案，每個 glyph = 2B code + 24 rows × 3B = 74 bytes

#### `cjk_font.h` 介面
```c
#ifndef CJK_FONT_H
#define CJK_FONT_H
#include <stdint.h>

enum cjk_font_source {
    CJK_FONT_BUILT_IN,   // 內建常用字
    CJK_FONT_EXTERNAL    // 外部 .fnt 檔案
};

int cjk_font_init(enum cjk_font_source src, const char *path);
void cjk_font_free();
const uint8_t *cjk_get_glyph(uint16_t big5_code); // 74 bytes
int cjk_char_width(uint16_t code);
int is_cjk_char(uint8_t first_byte);
#endif
```

### 5.3 Phase 1：Text Rendering API

#### 新增 API（`ui.h`）
```c
void ui_draw_cjk_char(int x, int y, uint16_t big5_code, uint8_t color);
void ui_draw_mixed_string(int x, int y, const uint8_t *bytes, int len, uint8_t color);
void ui_set_cjk_colors(uint8_t fg, uint8_t bg);
int ui_string_pixel_width(const uint8_t *bytes, int len);
```

#### 關鍵函式：`draw_mixed_character()`
```c
static void draw_mixed_character(int x, int y, uint8_t first_byte) {
    if (is_cjk_char(first_byte)) {
        uint16_t code = (first_byte << 8) | next_byte();
        // CJK 字對齊到 8-pixel 邊界（3×8=24）
        ui_draw_cjk_char((x / 3) * 3, (y / 3) * 3, code, COLOR_WHITE);
        draw_point.x += 3; // CJK 佔 3 個 8×8 cell
    } else {
        draw_character(x, y, get_chr(first_byte));
        draw_point.x++;
    }
}
```

### 5.4 Phase 2：Script 文字替換

#### 修改 `extract_string()` 以支援 Big5
```c
// 在解壓時偵測 CJK 字元
// 若為 Big5 字元，直接輸出 2-byte 編碼
// 若為 ASCII，使用現有 5-bit 解壓
```

#### 新增工具：`src/tools/patch_strings.cpp`
```cpp
// 將翻譯後的 Big5 字串 patch 回 DATA1
// 策略：固定長度欄位（不足補零，過長截斷）
int main(int argc, char *argv[]) {
    // 載入 DATA1
    // 讀取翻譯表
    // 逐條 patch
    // 輸出修改後的 DATA1
}
```

### 5.5 Phase 3：SDL2 優化
- 視窗標題改為「OpenDW - 龍之戰中文版」
- 使用 `SDL_UpdateTexture` 提升渲染效率
- 命令列參數：`--font path/to/font.fnt --lang zh-TW --scale 2`

### 5.6 Phase 4：UI 佈局調整
- `rect_dimensions[]` 右側欄從 w=39 → w=13（24×24 cell 座標）
- `ui_header_draw()` header 字元數從 20 → 13
- `ui_string.bytes[40]` 緩衝區可能需要擴大

### 5.7 可在無原始 DATA1 情況下先做的事

1. **CJK 字型載入層**：只要有 `.fnt` 字型檔即可開發
2. **Text Rendering API**：可先用測試程式驗證 24×24 blit 正確性
3. **SDL2 設定系統**：完全不依賴 DATA1，可獨立開發（`config.h/c`）
4. **按鍵映射**：`keymap.h/c` 可先設計好介面
5. **中文輸入法**：可用 SDL2 Text Input API 先建框架

---

## 階段六：翻譯工作流程

### 6.1 翻譯檔案結構

#### 目錄結構
```
docs/
  translations/
    menu.po         # 主選單文字
    dialogue.po     # NPC 對話
    items.po        # 物品名稱
    spells.po       # 法術名稱
    monsters.po     # 怪物名稱
    ui.po           # UI 文字
    manual.po       # Read Paragraph 段落
    glossary.md     # 翻譯術語一致性
```

#### `.po` 格式範例
```po
#: DATA1 section=0x08 offset=0x123
msgid "Begin a new game"
msgstr "開始新遊戲"

#: DATA1 section=0x10 offset=0x456
msgid "Healing Potion"
msgstr "治療藥水"
```

### 6.2 翻譯優先順序
| 優先度 | 項目 | 翻譯量 |
|--------|------|--------|
| P0 | UI 文字 | ~50 條 |
| P0 | 主選單文字 | ~30 條 |
| P1 | 物品名稱 | ~50–100 條 |
| P1 | 法術名稱 | ~20–30 條 |
| P2 | NPC 對話 | ~500–1000 條 |
| P2 | 怪物名稱 | ~30–50 條 |
| P3 | Read Paragraph 段落 | 視手冊內容 |

### 6.3 術語一致性
建立 `docs/translations/glossary.md`：
```markdown
# 翻譯術語對照表
| 英文 | 中文 | 備註 |
|------|------|------|
| Hit Points | 生命值 | 不可用「血量」 |
| Spell Power | 法力 | 不可用「魔法值」 |
| Encounter | 遭遇戰 | 戰鬥類型 |
| Paragraph | 段落 | Read Paragraph 功能 |
```

---

## 階段七：實作里程碑與時程

### Milestone 1：可顯示中文（預估 5–7 天）

**完成條件**：
- [ ] Phase 0 完成：24×24 中文渲染層
- [ ] Phase 1 完成：Text Rendering API
- [ ] 能在 SDL2 視窗中顯示「龍之戰」標題
- [ ] 能正確顯示中英混合字串

### Milestone 2：可執行中文版（預估 7–10 天）

**完成條件**：
- [ ] Phase 2 完成：Script 文字替換
- [ ] Phase 3 完成：SDL2 優化
- [ ] Phase 4 完成：UI 調整
- [ ] 能從標題畫面開始玩完整遊戲

### Milestone 3：翻譯完成（預估 10–14 天）

**完成條件**：
- [ ] 所有 UI 文字翻譯完成
- [ ] 所有物品 / 法術 / 怪物名稱翻譯完成
- [ ] NPC 對話翻譯完成
- [ ] Read Paragraph 段落翻譯完成

### Milestone 4：最終測試與發布（預估 3–5 天）

**完成條件**：
- [ ] 整合測試通過
- [ ] 遊戲可正常存檔 / 讀檔
- [ ] 中英顯示切換正常
- [ ] 效能測試通過

---

## 風險與緩解

| 風險 | 影響 | 緩解措施 |
|------|------|----------|
| 中文手冊 RAR 無法完整解壓縮 | 無法取得 Read Paragraph 內容 | 安裝 `unrar` 或 Docker 方案 |
| 英文 PDF OCR 品質不佳 | 無法交叉比對 DATA1 文字 | 手動輸入關鍵段落 |
| Tesseract 繁體中文辨識率低 | 中文 OCR 結果需大量校對 | 以人工建檔為主、OCR 為輔 |
| 24×24 點陣字型缺字 | 部分罕見字無法顯示 | 使用文泉驛點陣（Big5 第一字面完整） |
| DATA1 固定長度欄位過小 | 中文翻譯比英文長，patch 時截斷 | 適度截斷 + 術語保持一致長度 |
| 原始 C 程式碼缺乏註解 | 修改時易引入 bug | 先寫測試再修改，逐步驗證 |

---

## 附錄 A：相關檔案路徑

### 開發機路徑
| 路徑 | 內容 |
|------|------|
| `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/` | 中文化專案根目錄 |
| `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/docs/` | 計畫 / 分析文件 |
| `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/docs/PLAN.md` | 上層實作計畫 |
| `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/docs/TRANSLATION.md` | 目前翻譯表 |
| `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/docs/ALL_TEXT_FROM_DATA1.txt` | DATA1 萃取文字（3,926 條） |
| `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/src/tools/` | 工具程式原始碼 |
| `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/src/fe/` | SDL2 frontend |
| `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/src/lib/` | 核心引擎 |

### 原始 OpenDW 路徑
| 路徑 | 內容 |
|------|------|
| `/home/anr2/tmp/longcat/opendw/` | OpenDW 原始專案 |
| `/home/anr2/tmp/longcat/opendw/doc/` | 文件 |
| `/home/anr2/tmp/longcat/opendw/script/` | 反組譯腳本 |
| `/home/anr2/tmp/longcat/opendw/dos/dragon.asm` | 反組譯參考 |

### 外部資料
| 路徑 | 內容 |
|------|------|
| `/home/anr2/tmp/longcat/org_dialogue/` | 遊戲截圖 |
| `/tmp/chinese_manual/珍066-火龍之戰/` | 中文手冊掃描檔（解壓縮後） |

---

## 附錄 B：工具建置指令

```bash
# 建置專案
cd /home/anr2/tmp/longcat/opendw_dragon_wars_cht
mkdir -p build && cd build
cmake .. && make

# 執行 resextract
./src/tools/resextract -i 0x10 -d /path/to/data1 -o /tmp/section_10.bin

# 執行 disasm
./src/tools/disasm /path/to/dragon.com
```

---

## 附錄 C：待辦事項總覽

- [ ] 安裝 `unrar` 並完整解壓縮中文手冊
- [ ] 建立 `docs/manual_page_list.txt`
- [ ] 對中文手冊掃描檔 OCR 或人工建檔 → `docs/manual_paragraphs.md`
- [ ] 分析 DATA1 sections 0x10–0x16 結構
- [ ] 新增 `src/tools/section_dump.cpp`
- [ ] 萃取物品 / 法術 / 怪物名稱
- [ ] 建立 `docs/DATA1_SECTION_MAP.md`
- [ ] 交叉比對 script 與 DATA1 文字
- [ ] 新增 `src/tools/script_lint.cpp`
- [ ] 新增 `src/lib/cjk_font.h` / `cjk_font.c`
- [ ] 修改 `fe/vga_sdl.c` 為 640×480 + pixel scaling
- [ ] 修改 `lib/ui.h` / `ui.c` 新增 Text Rendering API
- [ ] 新增 `src/tools/patch_strings.cpp`
- [ ] 建立 `docs/translations/` 目錄與 `.po` 檔案
- [ ] 實作 `src/lib/config.h` / `config.c`（SDL2 設定系統）
- [ ] 實作 `src/lib/keymap.h` / `keymap.c`（按鍵映射）
- [ ] 完整翻譯 UI 文字
- [ ] 完整翻譯物品 / 法術 / 怪物名稱
- [ ] 完整翻譯 NPC 對話
- [ ] 完整翻譯 Read Paragraph 段落
- [ ] 整合測試

---

*文件結束 — OpenDW 龍之戰中文化工作計畫 v1.0*
