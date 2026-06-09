# OpenDW 中文化 PLAN（含 SDL2 整合 + 640×480 升級 + 反組譯還原）

---

## 附錄 A：原始 DOS 光碟映像分析

### A.1 取得的原始檔案

從 `Dragon Wars (1990).zip` 提取 5.25" 磁碟映像（用 `7z x` 解包 FAT12 image）：

| 檔案 | 大小 | 內容 |
|------|------|------|
| `5.25/1.0/DISK01.IMA` | 368,640 bytes | 原始 DOS 安裝碟 1（FAT12） |
| `5.25/1.0/DISK02.IMA` | 368,640 bytes | 原始 DOS 安裝碟 2（FAT12） |
| `3.5/1.1/DISK01.IMA` | 737,280 bytes | 3.5" 更新版 |

`DISK01.IMA` 內容：
```
DRAGON.COM   55,217 bytes  主程式（DOS COM 格式）
DWTRAN.COM    4,044 bytes  翻譯/對話資源？
DATA1       296,439 bytes  遊戲資料（script、圖片、字型）
```

`DISK02.IMA` 內容：
```
DATA2       352,430 bytes  地圖/戰鬥/音效資料
```

### A.2 關鍵發現

1. **`DWTRAN.COM`**（4,044 bytes）— 原始安裝碟上的翻譯資源！可能包含對話腳本的獨立語言包。opendw 目前把 DATA1 視為唯一資源檔，但原始設計可能有分離的翻譯層。中文化時應考慮利用或相容此格式。

2. **`check_config()`**（`0x627-0963`）— 完整的 DOS 設定選單（CGA/EGA/VGA/Tandy/Mouse 設定），opendw 完全跳過。

3. **`PIT/music`**（`0x5C3B-0x5D1D`）— PC speaker 音樂播放，opendw 未實作。

---

## 一、640×480 + 中文渲染方案評估

### 1.1 方案比較

| 方案 | Framebuffer | 視窗大小 | 中文字尺寸 | 修改量 | 推薦 |
|------|-------------|----------|-----------|--------|------|
| **A：原生 640×480** | 640×480 | 640×480 | 16×16 | 大 | ❌ UI 重構多 |
| **B：Pixel Scaling + 24×24** | **320×200** | **640×480** | **24×24** | **中** | **✅ 最佳畫質** |
| **C：Pixel Scaling + 16×16** | **320×200** | **640×400** | **16×16** | **小** | **✅ 推薦（保守）** |

**注意**：所有方案的中文字尺寸已統一為 24×24（方案 B）或 16×16（方案 C），不再使用 11×11 或 22×22 等非標準尺寸。

### 1.2 推薦方案 B：Pixel Scaling + 24×24

**核心概念**：
- 維持原始 320×200 framebuffer（所有遊戲邏輯不改）
- 中文字使用 **24×24 點陣**（每個字 cell = 24×24 pixels）
- framebuffer 可容納 **13×8 個中文字 cell**（320÷24≈13, 200÷24≈8）
- SDL2 用 `SDL_RenderSetScale(2.0, 2.0)` 放大到 640×400
- 視窗 640×480，上下留 40px 黑邊

**24×24 中文字在 320×200 framebuffer 中的佈局**：
- 每列字元數 = 320 ÷ 24 ≈ **13 字元**（右側欄 w=39×8=312px = 13×24=312px ✅ 完美貼合）
- 每行字元數 = 200 ÷ 24 ≈ **8 行**
- 8×8 cell 座標仍然有效，中文字 cell = 24×24 = 3×3 個 8×8 cell

**24×24 點陣格式**：
- 每個 glyph = 2 bytes (Big5 code) + 24 rows × 3 bytes = **74 bytes**
- 每 row 24 bits，存於 3 bytes（24 bits = 3 bytes）
- 總記憶體：5000 字 × 74 bytes = **370 KB**

**重要**：統一使用 24×24 點陣格式，不使用 11×11 或 22×22 等非標準尺寸。所有字型相關描述均已標準化。

**優點**：
- 24×24 中文字可讀性最佳（主流開源點陣字型都有 24×24）
- 所有 sprite/tile 程式碼不改
- 只需改 SDL2 顯示層 + 字型尺寸 + cell 大小
- 遊戲邏輯完全不受影響

**像素放大策略**：
```
┌─────────────────────────────────────────┐
│  SDL2 Window 640×480                    │
│  ┌─────────────────────────────────┐    │
│  │  SDL_RenderSetScale(2.0, 2.0)  │    │
│  │  ┌───────────────────────┐      │    │
│  │  │  Framebuffer 320×200  │      │    │
│  │  │  中文字 24×24          │      │    │
│  │  │  (每個字 3×3 cells)    │      │    │
│  │  └───────────────────────┘      │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

### 1.3 方案 B 實作細節

#### vga_sdl.c 修改
```c
// 維持 framebuffer 320×200 不變
#define VGA_WIDTH 320
#define VGA_HEIGHT 200

// SDL2 視窗
#define WIN_WIDTH 640
#define WIN_HEIGHT 480

// display_start() 中設定 scaling
SDL_RenderSetLogicalSize(renderer, 320, 200);
// 或 SDL_RenderSetScale(renderer, 2.0, 2.0);
```

#### 中文字渲染（24×24）
```c
void ui_draw_cjk_char(int x, int y, uint16_t big5_code, uint8_t color) {
    const uint8_t *glyph = cjk_get_glyph(big5_code); // 24×24 點陣
    uint8_t *fb = vga_memory(); // 320×200 framebuffer
    for (int row = 0; row < 24; row++) {
        uint32_t row_data = (glyph[row*3] << 16) | (glyph[row*3+1] << 8) | glyph[row*3+2];
        uint16_t fb_off = get_line_offset(y + row) + (x << 1);
        for (int col = 0; col < 24; col++) {
            if (row_data & (0x800000 >> col)) {
                fb[fb_off++] = color;
                fb[fb_off++] = color;
            } else {
                fb_off += 2;
            }
        }
    }
}
```

#### 座標系統
```c
// 原始 rect_dimensions 用 8×8 cell 座標
// 中文字 cell = 24×24 pixels = 3×3 個 8×8 cell
// 需要 cell 大小轉換層：
//   中文字：x_pixel = x_cell * 3, y_pixel = y_cell * 3（以 8×8 cell 為單位）
//   ASCII： x_pixel = x_cell * 1, y_pixel = y_cell * 1
```

### 1.4 方案 C（16×16 保守方案）

若 24×24 狹窄問題嚴重，可改用 16×16：
- 每列 = 320 ÷ 16 = **20 字元**
- 每行 = 200 ÷ 16 = **12 行**
- 右側欄 w=39 → 可容納 19 字元（19×16=304px < 312px ✅）
- 16×16 字型更常見（文泉驛、全字庫都有）
- 放大後 = 32×32 pixels，可讀性仍佳

**16×16 點陣格式**：
- 每個 glyph = 2 bytes (Big5 code) + 16 rows × 2 bytes = **34 bytes**
- 每 row 16 bits，存於 2 bytes
- 總記憶體：5000 字 × 34 bytes = **170 KB**

### 1.5 方案 B 風險與緩解

| 風險 | 影響 | 緩解措施 |
|------|------|----------|
| 24×24 cell 與 8×8 cell 混排 | 座標計算複雜 | 引入 `cell_width/cell_height` 變數，根據字元類型動態選擇 |
| 右側欄 13 字元太窄 | 中文顯示空間有限 | 備選方案 C（16×16，20 字元/列） |
| 24×24 字型記憶體 | 5000 字 × 72B = 360KB | 可接受 |

### 1.5 方案 A（備選）細節

若選擇原生 640×480 framebuffer：

| 檔案 | 修改量 | 說明 |
|------|--------|------|
| `fe/vga_sdl.c` | 小 | 視窗大小、surface 建立 |
| `lib/vga.c` | 小 | `VGA_WIDTH × VGA_HEIGHT` 常數 |
| `lib/tables.c` | 中 | `framebuffer_line_offsets[480]` |
| `lib/offsets.c` | 中 | `NUM_OFFSETS` 0x1E0, stride 0x280 |
| `lib/ui.c` | **大** | 所有 UI 座標/寬度常數 |
| `lib/engine.c` | 中 | viewport、minimap |
| `fe/main.c` | 小 | `GAME_WIDTH/GAME_HEIGHT` |

---

## 二、反組譯還原分析

### 2.1 方法論

對照 `dragon.asm`（Devin Smith 的位元組級反組譯，3721 行）與 opendw 的 C 重寫，逐一還原 `sub_XXX` 的真實名稱與語意。

### 2.2 dragon.asm 與 opendw C 碼的對應關係

| dragon.asm 位址 | opendw C 檔案 | 函式 | 備註 |
|---|---|---|---|
| `0x100-0x1B2` | `fe/main.c` | `main()` | 幾乎 1:1 對應 |
| `0x1CF-0x1E7` | `resource.c` | `init_mm()` | 記憶體管理初始化 |
| `0x21D-0x360` | `fe/vga_dos.c` | `init_graphics()` | CGA/EGA/VGA/Tandy 初始化 |
| `0x387-0x3A5` | `fe/main.c` | `run_title()` | 標題畫面 |
| `0x4B1-0x527` | `fe/vga_dos.c` | drawing handlers | CGA/Tandy/EGA/VGA 繪圖 |
| `0x5A5-0x5CF` | `fe/main.c` | `title_adjust()` | 標題 XOR 解碼 |
| `0x627-0x963` | — | `check_config()` | 設定選單（**opendw 未實作**） |
| `0x9E5-0xA31` | `fe/vga_dos.c` | `putchar/set_cursor` | 字元輸出 |
| `0xA5A-0xA93` | `main.c:rm_exit()` | `cleanup()` | 記憶體釋放 |
| `0xCA0-0xE6B` | `ui.c` | `sub_CF8/update_viewport` | 視埠資料解碼 |
| `0x1060-0x120B` | `ui.c` | `draw_viewport` | 視埠繪製 |
| `0x12C0-0x13B5` | `resource.c` | `rm_init/game_mem_alloc` | 資料載入 |
| `0x17DD-0x17F5` | `offsets.c` | `init_offsets` | 偏移表 |
| `0x184B` | `ui.c` | `ui_viewport_reset` | 視埠滾動 |
| `0x19C7-0x1A33` | `engine.c` | minimap 相關 | 小地圖繪製 |
| `0x25E0-0x2739` | `ui.c` | `draw_rectangle/ui_rect_expand/shrink` | UI 矩形 |
| `0x28B0-0x2D0B` | `fe/vga_sdl.c` | `get_key/check_keystroke` | 鍵盤輸入 |
| `0x2EB0-0x3116` | `resource.c` | `resource_load/read_write_DATA1` | 檔案 I/O |
| `0x3578-0x387E` | — | 變數區域 | opendw 用 global array |
| `0x3AA0-0x3B17` | `engine.c` | `run_engine()` | 遊戲主迴圈 |
| `0x4B10-0x4B5C` | `timers.c` | `timer_proc_handler` | 計時器中斷 |
| `0x5C3B-0x5D1D` | — | PIT/music | **opendw 未實作** |
| `0x6748-0x6820` | `ui.c` | `viewport_metadata` | 視埠中繼資料 |

### 2.3 engine.c 中可還原的 unnamed 函式

#### 跳躍類（已全部正確命名）

| opendw 名稱 | ASM 位址 | 說明 |
|---|---|---|
| `op_jnc` | `0x407C` | Jump if Not Carry |
| `op_jc` | `0x4085` | Jump if Carry |
| `op_jmp` | `0x408E` | 無條件跳躍 |
| `op_jz` | `0x4099` | Jump if Zero |
| `op_jnz` | `0x40A3` | Jump if Not Zero |
| `op_js` | `0x40AF` | Jump if Sign |
| `op_jns` | `0x40B8` | Jump if Not Sign |
| `op_loop` | `0x4106` | 迴圈 |
| `op_stc` | `0x4122` | Set Carry |
| `op_clc` | `0x412A` | Clear Carry |
| `op_call` | `0x41C0` | 副程式呼叫 |
| `op_ret` | `0x41E1` | 返回 |
| `op_retf` | `0x41C8` | 遠返回 |
| `op_push` | `0x41FD` | 推入堆疊 |
| `op_nop` | `0x4161` | 無操作 |

#### 可還原名稱的函式

| opendw 名稱 | ASM 位址 | 建議還原名稱 | 說明 |
|---|---|---|---|
| `sub_387` | `0x387` | `run_title_sequence` | 標題畫面完整序列 |
| `sub_3A7` | `0x3A7` | `show_ending_screen` | 結局畫面 |
| `sub_3BE` | `0x3BE` | `play_intro_sequence` | 開場動畫 |
| `sub_435` | `0x435` | `clear_framebuffer_word` | 清除 framebuffer |
| `sub_443` | `0x443` | `copy_intro_frame` | 複製開場畫面 frame |
| `sub_49E` | `0x49E` | `draw_current_picture` | 繪製當前圖片 |
| `sub_5A5` | `0x5A5` | `prepare_title_data` | 準備標題 XOR 解碼 |
| `sub_5ECA` | `0x5ECA` | `init_music_state` | 初始化音樂狀態 |
| `sub_5D1D` | `0x5D1D` | `play_music_note` | 播放音符 |
| `sub_26B8` | `0x26B8` | `draw_ui_panels` | 繪製 UI 面板 |
| `sub_27CC` | `0x27CC` | `draw_right_pillar` | 繪製右側支柱 |
| `sub_2D0B` | `0x2D0B` | `get_key_from_buffer` | 從緩衝區取得按鍵 |
| `sub_2FBC` | `0x2FBC` | `reopen_data1_file` | 重新開啟 DATA1 檔案 |
| `sub_3AA0` | `0x3AA0` | `game_main_loop` | 遊戲主迴圈 |
| `sub_37C8` | `0x37C8` | `init_viewport_data` | 視埠初始化 |
| `sub_59A6` | `0x59A6` | `draw_minimap` | 繪製小地圖 |
| `sub_54D8` | `0x54D8` | `draw_minimap_cell` | 繪製小地圖單格 |
| `sub_587E` | `0x587E` | `exit_game` | 離開遊戲 |
| `sub_5C3B` | `0x5C3B` | `init_pit_timer` | 初始化 PIT 計時器 |
| `sub_5C7B` | `0x5C7B` | `pit_timer_isr` | PIT 計時器中斷服務常式 |
| `sub_5CB6` | `0x5CB6` | `play_music_frame` | 播放音樂幀 |

#### 尚未還原的引擎內部函式

| ASM 位址 | opendw 名稱 | 建議名稱 | 說明 |
|---|---|---|---|
| `0xCA0-0xD77` | `sub_CF8` | `decode_viewport_data` | 視埠資料解碼 |
| `0xD88-0xFB2` | `process_quadrant` 等 | `draw_viewport_*` | 5 種視埠繪圖 handler |
| `0x10A0-0x10F3` | `sub_10A5/10D8/10F3` | `draw_viewport_cga/tandy/ega` | 視埠繪圖 handler |
| `0x1175` | `sub_1175` | `draw_viewport_vga` | VGA 視埠繪製 |
| `0x11A0` | `sub_11A0` | `multiply_words` | 16-bit 乘法 |
| `0x11CE` | `sub_11CE` | `divide_words` | 16-bit 除法 |
| `0x120B` | `sub_120B` | `parse_number_string` | 解析數字字串 |
| `0x12E6` | `sub_12E6` | `allocate_resource` | 配置資源記憶體 |
| `0x1A10` | `sub_1A10` | `draw_minimap_from_resource` | 從資源繪製小地圖 |
| `0x1F54` | `sub_1F54` | `sub_1F54` | 未識別 |
| `0x25E0` | `sub_25E0` | `run_script` | 執行腳本 |
| `0x2752` | `sub_2752` | `ui_check_redraw` | 檢查 UI 重繪 |
| `0x280E` | `sub_280E` | `sub_280E` | 未識別 |
| `0x2EB0` | `sub_2EB0` | `load_resource_section` | 載入資源段 |
| `0x3578` | `sub_3578` | `game_init` | 遊戲初始化 |

### 2.4 opendw 中「反組譯狀態」最嚴重的區段

#### 區段 1：viewport 解碼（`ui.c:1003-1078` `sub_CF8`）
```c
// 現狀：多處 printf + exit(1) + 魔術數字
// 對應 ASM：0xCA0-0xD77
// 建議重構：
void decode_viewport_data(struct viewport_data *vp, unsigned char *data) {
    vp->runlength = data[0];
    vp->numruns = data[1];
    // ... 用 struct 取代 word_1048/counter_104D/zero_104E 等全域變數
}
```

#### 區段 2：viewport 繪製 handler（`ui.c` 中的 `process_quadrant/sub_DEB/sub_E6D/sub_EC5/sub_F3D`）
```c
// 現狀：5 個類似函式，每個 50-100 行，大量重複
// 對應 ASM：0xD88-0xFB2 的 function_ptr_tbl
// 建議重構：
typedef void (*viewport_draw_fn)(struct viewport_data*, unsigned char*);
static viewport_draw_fn draw_handlers[] = {
    draw_viewport_normal,   // 0: D88
    draw_viewport_word,     // 2: DEB
    draw_viewport_neg_x,    // 4: E6D
    draw_viewport_neg_x_alt,// 6: EC5
    draw_viewport_flip,     // 8: F3D
};
```

#### 區段 3：小地圖（`engine.c` 中的 `sub_54D8/sub_59A6/sub_587E`）
```c
// 現狀：空的 stub + 未命名變數
// 對應 ASM：0x54D8-0x5A7F
// 需要完整實作
```

### 2.5 還原優先順序

| 優先級 | 項目 | 理由 |
|--------|------|------|
| P0 | `sub_CF8` → `decode_viewport_data` | viewport 核心，影響所有地圖顯示 |
| P0 | `sub_3AA0` → `game_main_loop` | 遊戲主迴圈，目前 opendw 未正確暴露 |
| P1 | `sub_59A6` → `draw_minimap` | 小地圖完整實作 |
| P1 | `sub_25E0` → `run_script` | 腳本執行器（目前 engine.c 內部） |
| P2 | `sub_3BE` → `play_intro_sequence` | 開場動畫 |
| P2 | `sub_3A7` → `show_ending_screen` | 結局畫面 |
| P3 | PIT/music 完整實作 | PC speaker 音樂 |

---

## 三、中文化架構設計

### 3.1 分層策略

```
┌─────────────────────────────────────────┐
│  Layer 5: SDL2 Window 640×480           │  ← vga_sdl.c (pixel scaling)
├─────────────────────────────────────────┤
│  Layer 4: 320×200 Framebuffer           │  ← vga.c (維持原始)
├─────────────────────────────────────────┤
│  Layer 3: Text Rendering API            │  ← ui_draw_cjk_char (24×24)
├─────────────────────────────────────────┤
│  Layer 2: CJK Font Cache & Metrics      │  ← 24×24 點陣 + 寬度表
├─────────────────────────────────────────┤
│  Layer 1: SDL2 Surface / Texture        │  ← 像素 blit + RenderSetScale
└─────────────────────────────────────────┘
```

### 3.2 編碼選擇：Big5
- 1989 年遊戲原始環境為 DOS，台灣/香港 DOS 使用 Big5
- 每個中文字 2 bytes，可與現有 `unsigned char *` 字串處理相容
- 避免 UTF-8 變長解析，減少 engine.c 修改量
- 可選擇同時支援 UTF-8（現代系統）+ Big5 轉碼層

---

## 四、實作階段

### Phase 0：Pixel Scaling + 24×24 中文渲染（推薦方案 B）

**修改檔案**：`fe/vga_sdl.c`、新增 `src/lib/cjk_font.h`、`src/lib/cjk_font.c`

#### vga_sdl.c 修改重點
```c
// 維持 framebuffer 320×200 不變
#define VGA_WIDTH 320
#define VGA_HEIGHT 200

// SDL2 視窗
#define WIN_WIDTH 640
#define WIN_HEIGHT 480

// display_start() 中設定 scaling
SDL_RenderSetLogicalSize(renderer, 320, 200);
// 或 SDL_RenderSetScale(renderer, 2.0, 2.0);
```

#### cjk_font.h
```c
#ifndef CJK_FONT_H
#define CJK_FONT_H
#include <stdint.h>

enum cjk_font_source {
    CJK_FONT_BUILT_IN,   // 內建 24×24 點陣
    CJK_FONT_EXTERNAL    // 載入外部 .fnt 檔案
};

int cjk_font_init(enum cjk_font_source src, const char *path);
void cjk_font_free();
const uint8_t *cjk_get_glyph(uint16_t big5_code); // 74 bytes = 2B code + 24 rows × 3B
int cjk_char_width(uint16_t code);
int is_cjk_char(uint8_t first_byte);
#endif
```

#### cjk_font.c 實作
```c
// 24×24 點陣：每個 glyph = 2 bytes code + 24 rows × 3 bytes = 74 bytes
// 每 row 24 bits，存於 3 bytes
void ui_draw_cjk_char(int x, int y, uint16_t big5_code, uint8_t color) {
    const uint8_t *glyph = cjk_get_glyph(big5_code);
    if (!glyph) return;
    uint8_t *fb = vga_memory();
    for (int row = 0; row < 24; row++) {
        uint32_t row_data = (glyph[row*3] << 16) | (glyph[row*3+1] << 8) | glyph[row*3+2];
        uint16_t fb_off = get_line_offset(y + row) + (x << 1);
        for (int col = 0; col < 24; col++) {
            if (row_data & (0x800000 >> col)) {
                fb[fb_off++] = color;
                fb[fb_off++] = color;
            } else {
                fb_off += 2;
            }
        }
    }
}
```

#### 字型資料格式（外部 `.fnt`）
```
Offset 0x00: "CJKF" magic (4 bytes)
Offset 0x04: glyph_count (uint16 LE)
Offset 0x06: per_glyph_size (uint16 LE, = 74 for 24×24)
Offset 0x08: glyph_data[glyph_count * 74]
  Each glyph: [big5_code(2B)] [24 rows × 3B = 72B]
```

**內建字型**：將常用約 5000 個中文字（Big5 第一字面）的 24×24 點陣編譯為 C 陣列，約 360KB。

---

### Phase 1：Text Rendering API（新增層）

**修改檔案**：`src/lib/ui.h`、`src/lib/ui.c`

在 `ui.h` 新增：
```c
void ui_draw_cjk_char(int x, int y, uint16_t big5_code, uint8_t color);
void ui_draw_mixed_string(int x, int y, const uint8_t *bytes, int len, uint8_t color);
void ui_set_cjk_colors(uint8_t fg, uint8_t bg);
int ui_string_pixel_width(const uint8_t *bytes, int len);
```

在 `ui.c` 新增 `draw_mixed_character()`：
```c
static void draw_mixed_character(int x, int y, uint8_t first_byte) {
    if (is_cjk_char(first_byte)) {
        uint16_t code = (first_byte << 8) | next_byte();
        // 對齊到 8-pixel 邊界（3×8=24）
        ui_draw_cjk_char((x / 3) * 3, (y / 3) * 3, code, COLOR_WHITE);
        draw_point.x += 3; // CJK 佔 3 個 8×8 cell
    } else {
        draw_character(x, y, get_chr(first_byte));
        draw_point.x++;
    }
}
```

---

### Phase 2：Script 文字抽取與翻譯

**新增工具**：`src/tools/extract_strings.cpp`

從 `data1` resource 中萃取所有可顯示字串：
1. 遍歷所有 `RESOURCE_SCRIPT` (0x00) 類型 resource
2. 解析 `extract_string` 呼叫（opcode 0x7A）的字串參數
3. 輸出為 JSON：
```json
{"resource_index": 5, "offset": 0x123, "original": "Loading...", "translated": "載入中…"}
```

**翻譯流程**：
```
data1 resources → extract_strings → .po 檔 → 人工翻譯 → patch_resources → 修改後 data1
```

**新增工具**：`src/tools/patch_strings.cpp`
- 將翻譯後的 Big5 字串 patch 回 `data1`
- 策略：**固定長度欄位**（每個字串配置固定 byte 數，不足補零）

---

### Phase 3：SDL2 整合優化

**修改檔案**：`src/fe/vga_sdl.c`

#### 視窗標題
```c
SDL_CreateWindow("OpenDW - 龍之戰中文版", ...)
```

#### 渲染最佳化
```c
// 在 display_start() 中建立 persistent texture
static SDL_Texture *texture = NULL;
// display_update 中直接更新 texture
void display_update(void) {
    SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}
```

#### 啟動參數
```bash
./sdldragon --font path/to/font.fnt --lang zh-TW --scale 2
```

---

### Phase 4：UI 佈局調整

**修改檔案**：`src/lib/ui.c`

1. **`rect_dimensions[]`**：右側欄 w=39 → w=13（24×24 cell 座標）
2. **`ui_header_draw()`**：header 字元數從 20 → 13
3. **`ui_string.bytes[40]`**：可能需要擴大緩衝區

```c
int ui_string_pixel_width(const uint8_t *bytes, int len) {
    int w = 0;
    for (int i = 0; i < len; i++) {
        if (is_cjk_char(bytes[i])) { w += 24; i++; }
        else { w += 8; }
    }
    return w;
}
```

---

## 五、SDL2 建置環境（Docker）

### Dockerfile
```dockerfile
FROM debian:bullseye-slim
RUN apt-get update && apt-get install -y build-essential cmake libsdl2-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN mkdir build && cd build && cmake .. && make
VOLUME ["/app/build/src/fe/data"]
CMD ["./src/fe/sdldragon"]
```

---

## 六、工作量估計

| 階段 | 項目 | 預估工時 |
|------|------|----------|
| Phase 0 | Pixel Scaling + 24×24 framebuffer | 1 天 |
| Phase 1 | Text Rendering API | 1-2 天 |
| Phase 2 | 字串萃取 + patch 工具 | 2 天 |
| Phase 3 | SDL2 優化 | 0.5 天 |
| Phase 4 | UI 佈局調整 | 1-2 天 |
| 翻譯 | 遊戲文字翻譯（約 2000 字串） | 3-5 天 |
| 測試 | 整合測試 | 1-2 天 |
| **合計** | | **9.5-14.5 天** |

---

## 七、風險與緩解

| 風險 | 影響 | 緩解措施 |
|------|------|----------|
| 字型版權 | 24×24 點陣字型可能有版權 | 使用開源字型（如文泉驛點陣）或自行繪製 |
| data1 格式限制 | 翻譯後字串長度不同 | 固定長度欄位 + 截斷/補零 |
| 效能問題 | CJK 字元 blit 較慢 | 使用 SDL2 texture 快取 + dirty rect |
| 原文編碼非 Big5 | 可能為 CP950 變體 | 實作 Big5 ↔ CP950 轉換層 |
| 部分 UI 無法顯示中文 | 空間不足 | 提供「英文模式」切換選項 |

---

## 八、下一步

1. **確認翻譯範圍**：哪些文字需要翻譯？（UI、對話、物品名稱、戰鬥訊息）
2. **準備字型**：選擇開源 24×24 中文點陣字型
3. **實作 Phase 0**：先完成 Pixel Scaling + 24×24 顯示
4. **建立翻譯流程**：`extract_strings` → `.po` → `patch_strings`