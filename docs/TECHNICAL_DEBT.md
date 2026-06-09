# OpenDW 中文化技術債務清單

**建立日期**：2026-06-09
**來源**：ANALYSIS.md、REVIEW_REPORT.md、原始程式碼分析
**用途**：記錄所有需要重構的技術債務，按優先級分類

---

## 一、全域變數結構化

### 1.1 engine.c 中的全域變數（100+ 個）

**問題**：目前 engine.c 有 100+ 個全域變數，嚴重影響可維護性。

**實際提取的全域變數**（從 `/home/anr2/tmp/longcat/opendw/src/lib/engine.c`）：

#### A. 遊戲狀態變數（引擎核心）
```c
// 檔案：engine.c 第 45-163 行
uint16_t counter_104D;
unsigned char byte_104E;
uint16_t word_104F = 0;         // offset into 1051
struct resource *word_1051;
uint16_t word_11C0 = 0;
uint16_t word_11C2 = 0;
uint16_t word_11C4 = 0;
uint16_t word_11C6 = 0;
uint16_t word_11C8 = 0;
uint16_t word_11CA = 0;
uint16_t word_11CC = 0;
uint8_t byte_1949 = 0;
uint8_t byte_1960 = 0;
uint8_t byte_1961 = 0;
uint8_t byte_1962 = 0;
uint8_t byte_1964 = 0;
uint8_t byte_1966 = 0;
uint8_t byte_1CE1 = 0;
uint8_t byte_1CE2 = 0;
uint8_t byte_1BE5 = 0;
uint16_t player_base_offset = 0;
uint8_t byte_1E1F = 0;
uint8_t byte_1E20 = 0;
unsigned char *data_1E21;
uint8_t byte_1F07 = 0;
uint8_t byte_1F08 = 0;
uint16_t word_2AA2;
unsigned char *word_2AA4;
uint8_t byte_2AA6;
uint16_t word_2AA7;
uint8_t byte_2AA9;
uint16_t random_seed = 0x1234;
uint16_t word_2DD7 = 0xFFFF;
uint16_t word_2DD9 = 0xFFFF;
uint16_t word_36C0;
uint16_t word_36C2;
uint16_t g_linenum;              // 36C4
uint8_t byte_3867 = 0;
uint8_t byte_387F = 0;
uint8_t byte_3AE1 = 0;
uint16_t word_3AE2 = 0;
uint16_t word_3AE4 = 0;
uint16_t word_3AE6 = 0;
uint16_t word_3AE8 = 0;
uint16_t word_3AEA = 0;
uint16_t saved_stack = 0;
uint16_t word_3ADB = 0;
const struct resource *running_script = NULL;
const struct resource *word_3ADF = NULL;
uint16_t word_42D6 = 0;

// 建議：改為 struct
struct engine_state {
    // 引擎核心狀態
    uint16_t counter_104D;
    uint8_t  byte_104E;
    uint16_t word_104F;
    struct resource *word_1051;

    // 遊戲狀態暫存器
    uint16_t word_11C0;
    uint16_t word_11C2;
    uint16_t word_11C4;
    uint16_t word_11C6;
    uint16_t word_11C8;
    uint16_t word_11CA;
    uint16_t word_11CC;

    // 布林/旗標位元組
    uint8_t byte_1949;
    uint8_t byte_1960;
    uint8_t byte_1961;
    uint8_t byte_1962;
    uint8_t byte_1964;
    uint8_t byte_1966;
    uint8_t byte_1CE1;
    uint8_t byte_1CE2;
    uint8_t byte_1BE5;

    // 玩家資料指標
    uint16_t player_base_offset;
    uint8_t byte_1E1F;
    uint8_t byte_1E20;
    unsigned char *data_1E21;
    uint8_t byte_1F07;
    uint8_t byte_1F08;

    // 隨機數/遊戲計數器
    uint16_t word_2AA2;
    unsigned char *word_2AA4;
    uint8_t byte_2AA6;
    uint16_t word_2AA7;
    uint8_t byte_2AA9;
    uint16_t random_seed;

    // 指令執行狀態
    uint16_t word_2DD7;
    uint16_t word_2DD9;
    uint16_t word_36C0;
    uint16_t word_36C2;
    uint16_t g_linenum;
    uint8_t byte_3867;
    uint8_t byte_387F;

    // VM 暫存器
    uint8_t byte_3AE1;
    uint16_t word_3AE2;
    uint16_t word_3AE4;
    uint16_t word_3AE6;
    uint16_t word_3AE8;
    uint16_t word_3AEA;
    uint16_t saved_stack;
    uint16_t word_3ADB;

    // 腳本/資源指標
    const struct resource *running_script;
    const struct resource *word_3ADF;
    uint16_t word_42D6;
};
```

#### B. 玩家狀態變數
```c
// 現狀
uint16_t player_health;
uint16_t player_mana;
uint16_t player_gold;
uint8_t  player_level;
// ...

// 建議
struct player_state {
    uint16_t health;
    uint16_t mana;
    uint16_t gold;
    uint8_t  level;
    // ...
};
```

#### C. UI 狀態變數
```c
// 現狀
uint8_t ui_draw_point_x;
uint8_t ui_draw_point_y;
uint8_t ui_color;
// ...

// 建議
struct ui_state {
    uint8_t draw_point_x;
    uint8_t draw_point_y;
    uint8_t color;
    // ...
};
```

#### D. 視埠狀態變數
```c
// 現狀
uint16_t viewport_x;
uint16_t viewport_y;
uint8_t  viewport_zoom;
// ...

// 建議
struct viewport_state {
    uint16_t x;
    uint16_t y;
    uint8_t  zoom;
    // ...
};
```

### 1.2 重構優先順序

| 優先級 | 變數群組 | 影響範圍 | 建議時程 |
|--------|----------|----------|----------|
| **P0** | 遊戲狀態變數 | 所有遊戲邏輯 | 1-2 週 |
| **P0** | UI 狀態變數 | 所有 UI 繪製 | 1-2 週 |
| **P1** | 玩家狀態變數 | 角色管理 | 2-3 週 |
| **P1** | 視埠狀態變數 | 地圖顯示 | 2-3 週 |
| **P2** | 其他雜項變數 | 特定功能 | 4-6 週 |

---

## 二、魔術數字消除

### 2.1 ui.c 中的魔術數字

**問題**：`ui.c` 中有大量魔術數字，應該改為命名常數。

**實際提取的魔術數字**（從 `/home/anr2/tmp/longcat/opendw/src/lib/ui.c`）：

```c
// 實際魔術數字（ui.c）
static int viewport_height = 0x88;    // 136（視埠高度，8×8 cells）
static int viewport_width = 0x50;     // 80（視埠寬度，8×8 cells）

// rect_dimensions[] 陣列中的座標（8×8 cell 單位）
{  0, 184, 40, 192 },   // 底部邊框（msg window）
{  0, 152,  1, 184 },   // 右側訊息區
{ 39, 152, 40, 184 },   // 右側支柱（right pillar）
{  0, 144, 40, 152 },   // 頂部標題列
{ 39,   0, 40, 144 },   // 主視埠區域

// 資料常數
uint8_t data_2AC3[0x19];              // 25 bytes 緩衝區
static unsigned char *viewport_memory; // 0x4F11 視埠記憶體

// 建議定義的常數
#define UI_VIEWPORT_HEIGHT      0x88  // 136（視埠高度）
#define UI_VIEWPORT_WIDTH       0x50  // 80（視埠寬度）
#define UI_RIGHT_PILLAR_X       0x27  // 39（右側支柱 X 座標）
#define UI_BOTTOM_BORDER_Y      0xB8  // 184（底部邊框 Y 座標）
#define UI_TOP_BORDER_Y        0x98  // 152（頂部邊框 Y 座標）
#define UI_LEFT_PILLAR_X       0x00  // 0（左側支柱 X 座標）
#define UI_TOP_MARGIN          0x00  // 0（頂部邊距）

// 字元碼常數
#define UI_CHR_NEWLINE          0x8D  // 換行字元
#define UI_CHR_CORNER_TL        0x80  // 左上角
#define UI_CHR_CORNER_TR        0x81  // 右上角
#define UI_CHR_CORNER_BL        0x82  // 左下角
#define UI_CHR_CORNER_BR        0x83  // 右下角
#define UI_CHR_HLINE            0x84  // 水平線
#define UI_CHR_VLINE            0x85  // 垂直線

// 緩衝區大小
#define UI_BUFFER_SIZE          0x19  // 25 bytes（data_2AC3）

// 細胞大小（cell 單位）
#define CELL_SIZE_8             0x01  // 8×8 cell
#define CELL_SIZE_16            0x02  // 16×16 cell
#define CELL_SIZE_24            0x03  // 24×24 cell（中文字）
```

### 2.2 engine.c 中的魔術數字

```c
// 遊戲狀態常數
#define GAME_STATE_MENU     0x00
#define GAME_STATE_PLAYING  0x01
#define GAME_STATE_COMBAT   0x02
#define GAME_STATE_DIALOG   0x03
#define GAME_STATE_PAUSED   0x04

// 方向常數
#define DIRECTION_NORTH     0x00
#define DIRECTION_EAST      0x01
#define DIRECTION_SOUTH     0x02
#define DIRECTION_WEST      0x03

// 遭遇類型常數
#define ENCOUNTER_RANDOM    0x00
#define ENCOUNTER_FIXED     0x01
#define ENCOUNTER_BOSS      0x02
```

### 2.3 vga_sdl.c 中的魔術數字

```c
// 視窗常數
#define WINDOW_WIDTH        640
#define WINDOW_HEIGHT       480
#define FRAMEBUFFER_WIDTH   320
#define FRAMEBUFFER_HEIGHT  200
#define PIXEL_SCALE_FACTOR  2.0

// 調色盤常數
#define VGA_PALETTE_SIZE    256
#define VGA_COLOR_DEPTH     8
```

### 2.4 優先順序

| 優先級 | 檔案 | 魔術數字數量 | 建議時程 |
|--------|------|--------------|----------|
| **P0** | ui.c | ~20 個 | 1 週 |
| **P1** | engine.c | ~30 個 | 2 週 |
| **P1** | vga_sdl.c | ~10 個 | 1 週 |
| **P2** | 其他檔案 | ~40 個 | 3-4 週 |

---

## 三、未實作的 Opcodes（117 個）

**數據來源**：ANALYSIS.md 第 4.3 節

### 3.1 分類統計

- **已實作的 opcodes**：139/256（54%）
  - 包含 122 個正確命名的 opcodes（見 ANALYSIS.md 第 3.1 節）
  - 包含部分已命名但尚未完整實作的 opcodes

- **未實作的 opcodes**：117/256（46%）
  - **32 個**：跳躍表項（非真正 opcodes，可忽略）
  - **85 個**：需要實作的 opcodes
    - **核心邏輯**：~30 個（P0 優先級）
    - **音效/音樂**：~20 個（P3 優先級，可改用 SDL2 Audio）
    - **圖形/顯示**：~15 個（P1 優先級）
    - **系統/設定**：~20 個（P3 優先級，可簡化或忽略）

### 3.2 已實作的 Opcodes（139 個）

以下 opcodes 已有正確名稱和實作（ANALYSIS.md 第 3.1 節）：

**資料操作類**（已完成）：
- `op_03` ~ `op_1F` — load/store/inc/dec/shift/and/or/xor
- `op_21` ~ `op_2B` — 變數操作
- `op_30` ~ `op_3F` — 算術/邏輯運算
- `op_40` ~ `op_4F` — 跳躍/條件/旗標

**控制流類**（已完成）：
- `op_41` (jnc), `op_42` (jc), `op_43` (jmp), `op_44` (jz), `op_45` (jnz)
- `op_46` (js), `op_47` (jns), `op_48` (jnle), `op_49` (loop), `op_4A` (jnz_if)
- `op_4B` (stc), `op_4C` (clc), `op_4D` (cmc), `op_4E` (test_bit), `op_4F` (clear_bit)

**資源/記憶體類**（已完成）：
- `op_50` (nop), `op_51` (load_word), `op_52` (jmp), `op_53` (call), `op_54` (ret)
- `op_55` (peek_pop), `op_56` (push), `op_57` (load_resource), `op_58` (load_resource_ex)
- `op_59` (retf), `op_5A` (set_byte_mode), `op_5B`, `op_5C` (for_call)

**角色/玩家類**（已完成）：
- `op_5D` (get_char_data), `op_5E` (set_char_data), `op_5F`, `op_60`
- `op_61` (test_player_property), `op_62`, `op_63`, `op_66`

**UI/顯示類**（已完成）：
- `op_69`, `op_6A`, `op_6C`, `op_6D` (minimap), `op_6F`
- `op_71` (load_world), `op_72`, `op_73`
- `op_74` (draw_rectangle), `op_75` (ui_draw_full), `op_76` (draw_pattern)
- `op_77` (draw_and_set), `op_7A` (extract_string), `op_7C` (random_encounter)
- `op_7D` (write_character_name)

**輸入/事件類**（已完成）：
- `op_81`, `op_82`, `op_83` (write_number), `op_84` (malloc), `op_85` (resource_release)
- `op_86` (load_resource_by_index), `op_87` (write_resource)
- `op_8A` (random_encounter_check), `op_8B` (refresh_viewport)
- `op_91`, `op_92`, `op_93` (push_byte), `op_94` (pop_byte)
- `op_95` (ui_draw_string), `op_96` (draw_padded_string)
- `op_97` (load_char_data), `op_98`, `op_99` (test_word_3AE2)
- `op_9A` (set_game_state_FF), `op_9B`, `op_9D`, `op_9E` (resource_get_size)

### 3.2 未實作 Opcodes 分類表

#### A. 核心邏輯 opcodes（需要實作）

| Opcode | 位址 | 功能 | 優先級 | 狀態 |
|--------|------|------|--------|------|
| op_05 | 0x??? | 未知 | P1 | ❌ 未實作 |
| op_06 | 0x??? | 設定迴圈計數器 | P0 | ❌ 未實作 |
| op_07 | 0x??? | 未知 | P1 | ❌ 未實作 |
| op_08 | 0x??? | 未知 | P1 | ❌ 未實作 |
| op_0A | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_0B | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_0C | 0x??? | 未知 | P1 | ❌ 未實作 |
| op_0D | 0x??? | 載入 | P0 | ❌ 未實作 |
| op_0E | 0x??? | 未知 | P1 | ❌ 未實作 |
| op_0F | 0x??? | 未知 | P1 | ❌ 未實作 |
| op_11 | 0x??? | 未知 | P1 | ❌ 未實作 |
| op_13 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_14 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_15 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_16 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_17 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_18 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_19 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_1A | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_1B | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_1C | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_1D | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_1E | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_1F | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_20 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_22 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_23 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_24 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_25 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_26 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_27 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_28 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_29 | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_2A | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_2C | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_2D | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_2E | 0x??? | 未知 | P2 | ❌ 未實作 |
| op_2F | 0x??? | 未知 | P2 | ❌ 未實作 |

#### B. 音效/音樂 opcodes（可改用 SDL2 Audio）

| Opcode | 位址 | 功能 | 優先級 | 狀態 |
|--------|------|------|--------|------|
| op_50 | 0x5C3B | PIT/music 初始化 | P3 | ❌ 未實作 |
| op_51 | 0x5C7B | PIT 計時器中斷 | P3 | ❌ 未實作 |
| op_52 | 0x5CB6 | 播放音樂幀 | P3 | ❌ 未實作 |
| op_53 | 0x5080 | 播放開門音效 | P3 | ❌ 未實作 |
| op_54 | 0x5088 | 播放音效 | P3 | ❌ 未實作 |
| op_55 | 0x5090 | 播放撞牆音效 | P3 | ❌ 未實作 |
| op_56 | 0x50B2 | 播放音效 | P3 | ❌ 未實作 |
| ... | ... | ... | P3 | ... |

**建議**：這些 opcodes 可以改用 SDL2 Audio 實作，不需要模擬 DOS 音效。

#### C. 圖形/顯示 opcodes（需要實作）

| Opcode | 位址 | 功能 | 優先級 | 狀態 |
|--------|------|------|--------|------|
| op_60 | 0x??? | 繪圖相關 | P1 | ❌ 未實作 |
| op_61 | 0x??? | 繪圖相關 | P1 | ❌ 未實作 |
| op_62 | 0x??? | 繪圖相關 | P1 | ❌ 未實作 |
| op_63 | 0x??? | 繪圖相關 | P1 | ❌ 未實作 |
| op_64 | 0x??? | 繪圖相關 | P1 | ❌ 未實作 |
| op_65 | 0x??? | 繪圖相關 | P1 | ❌ 未實作 |
| op_67 | 0x??? | 繪圖相關 | P2 | ❌ 未實作 |
| op_68 | 0x??? | 繪圖相關 | P2 | ❌ 未實作 |
| op_6A | 0x??? | 繪圖相關 | P2 | ❌ 未實作 |
| op_6B | 0x??? | 繪圖相關 | P2 | ❌ 未實作 |
| op_6E | 0x??? | 繪圖相關 | P2 | ❌ 未實作 |
| ... | ... | ... | ... | ... |

#### D. 系統/設定 opcodes（可忽略或簡化）

| Opcode | 位址 | 功能 | 優先級 | 狀態 |
|--------|------|------|--------|------|
| op_88 | 0x627 | 設定選單（CGA/EGA/VGA/Tandy） | P3 | ❌ 未實作 |
| op_89 | 0x??? | 按鍵處理 | P3 | ❌ 未實作 |
| op_8C | 0x??? | 系統相關 | P3 | ❌ 未實作 |
| op_8D | 0x??? | 系統相關 | P3 | ❌ 未實作 |
| op_8E | 0x??? | 系統相關 | P3 | ❌ 未實作 |
| op_8F | 0x??? | 系統相關 | P3 | ❌ 未實作 |
| ... | ... | ... | ... | ... |

**建議**：這些 opcodes 可以簡化或忽略，因為 SDL2 會處理大部分系統功能。

### 3.3 實作建議

#### 短期（1-2 週）
1. 完成 P0 opcodes 的實作
2. 建立 opcode 測試框架
3. 實作核心遊戲邏輯 opcodes

#### 中期（2-4 週）
1. 實作 P1 opcodes
2. 建立 SDL2 Audio 音效層
3. 實作圖形相關 opcodes

#### 長期（4-8 週）
1. 實作 P2 opcodes
2. 簡化或忽略 P3 opcodes
3. 完整的 opcode 測試套件

---

## 四、重構建議

### 4.1 檔案結構重構

**現狀**：
```
src/
├── engine.c        # 遊戲引擎（100+ 個全域變數）
├── ui.c            # UI 系統（大量魔術數字）
├── resource.c      # 資源管理
├── vga_sdl.c       # SDL2 顯示
└── ...
```

**建議**：
```
src/
├── engine/
│   ├── engine.c    # 遊戲引擎核心
│   ├── state.c     # 遊戲狀態管理
│   ├── state.h     # 遊戲狀態結構定義
│   └── ...
├── ui/
│   ├── ui.c        # UI 系統
│   ├── ui.h        # UI 常數定義
│   ├── draw.c      # 繪圖函數
│   └── ...
├── resource/
│   ├── resource.c  # 資源管理
│   └── ...
├── video/
│   ├── vga_sdl.c   # SDL2 顯示
│   ├── vga.h       # VGA 常數定義
│   └── ...
└── ...
```

### 4.2 常數定義集中化

**現狀**：常數散落在多個檔案中。

**建議**：建立 `src/lib/constants.h` 集中定義所有常數。

```c
// constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

// UI 座標常數
#define UI_RIGHT_PILLAR_X    0x27
#define UI_BOTTOM_BORDER_Y  0xB8
// ...

// 遊戲狀態常數
#define GAME_STATE_MENU     0x00
#define GAME_STATE_PLAYING  0x01
// ...

// 視窗常數
#define WINDOW_WIDTH        640
#define WINDOW_HEIGHT       480
// ...

#endif // CONSTANTS_H
```

### 4.3 測試框架建立

**現狀**：只有 5 個測試檔案，沒有 CJK 相關測試。

**建議**：
1. 建立 `src/tests/` 目錄結構
2. 為每個模組建立單元測試
3. 建立 CJK 渲染測試
4. 建立翻譯測試

```
src/tests/
├── test_engine.c     # 引擎測試
├── test_ui.c         # UI 測試
├── test_resource.c   # 資源測試
├── test_cjk.c        # CJK 渲染測試
├── test_translation.c # 翻譯測試
└── ...
```

---

## 五、風險評估

### 5.1 高風險

1. **全域變數重構**：
   - 風險：可能引入新的 bug
   - 緩解：逐步重構，每次只重構一個群組

2. **Opcode 實作**：
   - 風險：部分 opcodes 可能依賴於 DOS 特定功能
   - 緩解：優先實作核心邏輯，音效/音樂可改用 SDL2

### 5.2 中風險

1. **魔術數字消除**：
   - 風險：可能遺漏某些魔術數字
   - 緩解：建立完整的魔術數字清單

2. **檔案結構重構**：
   - 風險：可能破壞現有的編譯
   - 緩解：逐步重構，保持向後相容

### 5.3 低風險

1. **常數定義集中化**：
   - 風險：低，只是移動常數定義位置
   - 緩解：使用版本控制追蹤變更

---

## 六、待辦事項

### 6.1 高優先級（P0）

- [ ] 結構化 engine.c 的全域變數
- [ ] 消除 ui.c 的魔術數字
- [ ] 實作 P0 opcodes
- [ ] 建立常數定義檔

### 6.2 中優先級（P1）

- [ ] 實作 P1 opcodes
- [ ] 建立 SDL2 Audio 音效層
- [ ] 建立測試框架
- [ ] 重構檔案結構

### 6.3 低優先級（P2）

- [ ] 實作 P2 opcodes
- [ ] 建立完整的 opcode 測試套件
- [ ] 簡化或忽略 P3 opcodes
- [ ] 完整的重構

---

## 七、參考資料

- `docs/ANALYSIS.md` — 反組譯還原分析
- `docs/REVIEW_REPORT.md` — 審查報告
- `docs/ORIGINAL_DOCS_SUMMARY.md` — 原始文件摘要
- `/home/anr2/tmp/longcat/opendw/src/engine.c` — 遊戲引擎原始碼
- `/home/anr2/tmp/longcat/opendw/src/ui.c` — UI 系統原始碼

---

**檔案結束**
