# 中文顯示實作計畫

> **日期**：2026-06-09
> **依據**：`REVIEW_REPORT.md`、`NEXT_PLAN.md`、`PLAN.md`
> **目標**：建立可逐步執行的中文顯示實作路徑

---

## 現況盤點

### 已完成
- ✅ 52 個 unnamed 函式更名
- ✅ 143 個 opcode 命名
- ✅ 3,926 條文字萃取（DATA1 sections 0x00-0x16）
- ✅ 中文手冊 43/44 頁掃描
- ✅ 腳本文字映射

### 待完成
- ❌ CJK 字型渲染層（24×24）
- ❌ SDL2 顯示升級
- ❌ 文字替換系統
- ❌ 3,870 條文字翻譯（98.6%）
- ❌ 中文手冊 OCR 校對

---

## 實作路徑總覽

```
Phase 0: Pixel Scaling + 24×24 渲染層
    ↓
Phase 1: Text Rendering API
    ↓
Phase 2: Script 文字替換
    ↓
Phase 3: SDL2 優化
    ↓
Phase 4: UI 佈局調整
```

---

## Phase 0: Pixel Scaling + 24×24 渲染層

### 目標
- 建立 CJK 字型載入層
- 修改 SDL2 視窗為 640×480（2x scale）
- 維持 320×200 framebuffer

### 新增檔案
- `src/lib/cjk_font.h` — CJK 字型載入介面
- `src/lib/cjk_font.c` — CJK 字型載入實作

### 修改檔案
- `src/fe/vga_sdl.c` — 設定 640×480 視窗 + `SDL_RenderSetScale(2.0, 2.0)`
- `src/lib/vga.c` — 維持 320×200 framebuffer

### 字型選擇
| 字型 | 授權 | 備註 |
|------|------|------|
| 文泉驛點陣 24×24 | GPL | 推薦，繁體中文完整 |
| 全字庫 24×24 | CNS 標準 | 備選 |
| Noto Sans CJK | OFL | 向量字，需點陣化 |

### `cjk_font.h` 介面
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
const uint8_t *cjk_get_glyph(uint16_t big5_code); // 74 bytes (24×24)
int cjk_char_width(uint16_t code);
int is_cjk_char(uint8_t first_byte);
#endif
```

### 驗證方式
- 顯示「龍之戰」標題
- 顯示中英混合字串
- 確認 24×24 glyph blit 正確

### 預估工時
- 2-3 天

---

## Phase 1: Text Rendering API

### 目標
- 新增 `ui_draw_cjk_char()` API
- 支援中英混合字串顯示
- 處理 CJK 字元對齊（8-pixel 邊界）

### 新增 API（`ui.h`）
```c
void ui_draw_cjk_char(int x, int y, uint16_t big5_code, uint8_t color);
void ui_draw_mixed_string(int x, int y, const uint8_t *bytes, int len, uint8_t color);
void ui_set_cjk_colors(uint8_t fg, uint8_t bg);
int ui_string_pixel_width(const uint8_t *bytes, int len);
```

### 關鍵函式：`draw_mixed_character()`
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

### 驗證方式
- 顯示中英文混排「Hello 你好」
- 測量字串像素寬度
- 確認 CJK 字元不破壞 ASCII 排列

### 預估工時
- 2-3 天

---

## Phase 2: Script 文字替換

### 目標
- 修改 `extract_string()` 支援 Big5
- 建立 `patch_strings.cpp` 工具
- 將翻譯後的 Big5 字串 patch 回 DATA1

### 修改 `extract_string()` 以支援 Big5
```c
// 在解壓時偵測 CJK 字元
// 若為 Big5 字元，直接輸出 2-byte 編碼
// 若為 ASCII，使用現有 5-bit 解壓
```

### 新增工具：`src/tools/patch_strings.cpp`
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

### 驗證方式
- 執行 patch 後執行遊戲
- 確認中文正確顯示
- 確認不破壞遊戲邏輯

### 預估工時
- 3-4 天

---

## Phase 3: SDL2 優化

### 目標
- 視窗標題改為「OpenDW - 龍之戰中文版」
- 使用 `SDL_UpdateTexture` 提升渲染效率
- 命令列參數支援

### 命令列參數
```bash
./sdldragon --font path/to/font.fnt --lang zh-TW --scale 2
```

### 驗證方式
- 視窗標題正確
- 命令列參數生效
- 渲染效能提升

### 預估工時
- 1-2 天

---

## Phase 4: UI 佈局調整

### 目標
- 調整 `rect_dimensions[]` 右側欄
- 調整 `ui_header_draw()` header 字元數
- 擴大 `ui_string.bytes[]` 緩衝區

### 調整項目
- `rect_dimensions[]` 右側欄：w=39 → w=13（24×24 cell 座標）
- `ui_header_draw()` header 字元數：20 → 13
- `ui_string.bytes[40]` 緩衝區：可能需要擴大

### 驗證方式
- 顯示中文選單不截斷
- 顯示中文對話框不重疊

### 預估工時
- 1-2 天

---

## 翻譯工作流程

### 翻譯檔案結構
```
docs/translations/
  menu.po         # 主選單文字
  dialogue.po     # NPC 對話
  items.po        # 物品名稱
  spells.po       # 法術名稱
  monsters.po     # 怪物名稱
  ui.po           # UI 文字
  manual.po       # Read Paragraph 段落
  glossary.md     # 翻譯術語一致性
```

### `.po` 格式範例
```po
#: DATA1 section=0x08 offset=0x123
msgid "Healing Potion"
msgstr "治療藥水"
```

### 翻譯優先順序
| 優先度 | 項目 | 翻譯量 |
|--------|------|--------|
| P0 | UI 文字 | ~50 條 |
| P0 | 主選單文字 | ~30 條 |
| P1 | 物品名稱 | ~50–100 條 |
| P1 | 法術名稱 | ~20–30 條 |
| P2 | NPC 對話 | ~500–1000 條 |
| P2 | 怪物名稱 | ~30–50 條 |
| P3 | Read Paragraph 段落 | 視手冊內容 |

### 術語一致性
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

## 里程碑

### Milestone 1：可顯示中文（5-7 天）
- [ ] Phase 0 完成
- [ ] Phase 1 完成
- [ ] 顯示「龍之戰」標題
- [ ] 顯示中英混合字串

### Milestone 2：可執行中文版（7-10 天）
- [ ] Phase 2 完成
- [ ] Phase 3 完成
- [ ] Phase 4 完成
- [ ] 從標題畫面開始玩完整遊戲

### Milestone 3：翻譯完成（10-14 天）
- [ ] 所有 UI 文字翻譯
- [ ] 所有物品/法術/怪物名稱翻譯
- [ ] NPC 對話翻譯
- [ ] Read Paragraph 段落翻譯

### Milestone 4：最終測試與發布（3-5 天）
- [ ] 整合測試通過
- [ ] 遊戲可正常存檔/讀檔
- [ ] 中英切換正常
- [ ] 效能測試通過

---

## 風險與緩解

| 風險 | 影響 | 緩解措施 |
|------|------|----------|
| 中文手冊 RAR 無法完整解壓 | 無法取得 Read Paragraph 內容 | 安裝 `unrar` 或手動傳回 |
| 英文 PDF OCR 品質不佳 | 無法交叉比對 DATA1 文字 | 手動輸入關鍵段落 |
| 24×24 點陣字型缺字 | 部分罕見字無法顯示 | 使用文泉驛點陣（Big5 第一字面完整） |
| DATA1 固定長度欄位過小 | 中文翻譯比英文長，patch 時截斷 | 適度截斷 + 術語保持一致長度 |
| 原始 C 程式碼缺乏註解 | 修改時易引入 bug | 先寫測試再修改 |

---

## 立即可做的項目

### 不需原始 DATA1
1. **CJK 字型載入層**：只要有 `.fnt` 字型檔即可開發
2. **Text Rendering API**：可先用測試程式驗證 24×24 blit 正確性
3. **SDL2 設定系統**：完全不依賴 DATA1（`config.h/c`）
4. **按鍵映射**：`keymap.h/c` 可先設計好介面
5. **中文輸入法**：可用 SDL2 Text Input API 先建框架

---

## 待辦事項總覽

### Phase 0-4 實作
- [ ] 新增 `src/lib/cjk_font.h` / `cjk_font.c`
- [ ] 修改 `fe/vga_sdl.c` 為 640×480 + pixel scaling
- [ ] 修改 `lib/ui.h` / `ui.c` 新增 Text Rendering API
- [ ] 新增 `src/tools/patch_strings.cpp`
- [ ] 建立 `docs/translations/` 目錄與 `.po` 檔案

### 翻譯
- [ ] 完整翻譯 UI 文字
- [ ] 完整翻譯物品/法術/怪物名稱
- [ ] 完整翻譯 NPC 對話
- [ ] 完整翻譯 Read Paragraph 段落

### 文件
- [ ] 更新 `PLAN.md` 修正錯誤資訊
- [ ] 更新 `TRANSLATION.md` 補充缺失翻譯
- [ ] 建立 `docs/translations/glossary.md`

---

*產生日期：2026-06-09*
*工具：Claude Opus 4.7*
