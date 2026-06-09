# OpenDW 原始碼地圖 — 龍戰（Dragon Wars）中文化參考

> **用途**：提供 opendw 重製版各檔案的架構、資料結構、字串／對話位置、以及函式關係的完整導覽。
> **日期**：2026-06-09
> **對應原始程式**：Devin Smith 的 opendw（DOS COM 反組譯重寫）
> **原始遊戲**：Interplay Dragon Wars (1989), by Rebecca Ann Heineman

---

## 1. 專案架構總覽

```
opendw/
├── src/lib/    # 核心邏輯層（虛擬 CPU、資源載入、玩家、UI、資料表）
├── src/fe/     # 前端層（SDL2/DOS/Xlib 顯示驅動，main entry）
├── doc/        # 原始文件
└── dos/        # DOS 版相關檔案
```

**核心理念**：opendw 是一個「可逐位元組相容」的 Dragon Wars COM 可執行檔重製。
幾乎所有資料都是從 `dragon.com`、`data1`、`data2` 三個二進位檔案載入，
程式本身以 16-bit x86 組合語言的虛擬 CPU 來執行遊戲腳本（Script bytecode）。

---

## 2. 檔案明細

### 2.1 `src/lib/` — 核心邏輯層

#### `src/lib/engine.c`（6637 行） — 主引擎（虛擬 CPU + opcode 解碼）
**目的**：實現 Dragon Wars 的虛擬 CPU 與所有 opcode，是整個重製版的心臟。

**關鍵資料結構**：
```c
struct virtual_cpu {
  uint16_t ax, bx, cx, dx, di, si;
  uint8_t  stack[STACK_SIZE]; // STACK_SIZE = 32
  uint8_t  sp;
  uint8_t  cf, zf, sf;
  bool     key_wait_inited;
  unsigned char *pc, *base_pc;
};
extern struct virtual_cpu cpu;

struct bit_extractor {
  uint8_t num_bits, upper_case, bit_buffer;
  const unsigned char *data;
  uint16_t offset;
};
```

**重要全域變數**：
- `word_3AE2` (0x3AE2) — 通用 16-bit 暫存器
- `word_3AE4` (0x3AE4) — 迴圈計數器／臨時值
- `word_3AE6` (0x3AE6) — CPU flags 映像 + player 相關旗標
- `word_3AE8` (0x3AE8) — running_script resource index
- `word_3AEA` (0x3AEA) — word_3ADF resource index
- `word_104F` (0x104F) — script offset
- `word_1051` (0x1051) — 當前載入的 script resource
- `data_2AAA[32]` (0x2AAA) — 鍵盤／事件緩衝區
- `escape_string_table` (0x2A68) — ESC 對話選項字串表

**關鍵 opcodes**：
- `op_05` — 載入 game_state[bx]
- `op_08` — 寫入 game_state[bx]
- `op_0B` — 從 game_state 載入 word
- `op_5D` (`get_character_data`) — 讀取玩家屬性
- `op_5E` (`set_character_data`) — 寫入玩家屬性
- `op_8C` (`prompt_no_yes`) — 顯示 Yes/No 對話框
- `op_8D` (`read_string_input`) — 讀取玩家輸入字串
- `op_8A` — 觸發隨機戰鬥
- `set_msg` (op_78) — 從 script 提取字串

**內嵌字串常值**：
- `str_chained` (0x1BD1) → `chained`
- `str_poisoned` (0x1BD6) → `poisoned`
- `str_stunned` (0x1BDC) → `stunned`
- `str_dead` (0x1BE1) → `dead`
- `data_1BAA` (0x1BAA) → `is `
- `str_table_status[4]` (0x1BC9) — 狀態字串表

**與其它模組的呼叫關係**：
- `resource_load()` / `resource_get_by_index()` → `resource.c`
- `get_player_data()` / `get_player_data_byte()` → `player.c`
- `set_game_state()` / `get_game_state()` → `state.c`
- `ui_draw_string()`, `ui_header_*` → `ui.c`
- `com_extract()` → `resource.c`

---

#### `src/lib/engine.h`（43 行） — 引擎對外介面
**匯出**：
- `reset_game_state()`, `run_engine()`, `release_flagged_resource()`
- `extract_string(src_ptr, offset, func)` — 從資源字串表提取字串
- 多個 `extern` 全域變數供其它模組讀寫

---

#### `src/lib/player.c`（316 行） — 玩家角色資料
**目的**：管理 7 個玩家角色的靜態資料區（`data_C960[0xE00]`，每個角色 512 bytes）。

**關鍵資料結構**：
```c
struct skill_info {      // 0x20 ~ 0x3A (27 bytes)
  unsigned char arcane_lore, cave_lore, forest_lore, mountain_lore, town_lore,
                bandage, climb, fistfighting, hide, lockpick, pickpocket,
                swim, tracking, bureaucracy, druid_magic, high_magic, low_magic,
                merchant, sun_magic, axe, flail, mace, sword, two_handed_sword,
                bow, crossbow, thrown_weapons;
};

struct spell_info {       // 0x3C ~ 0x44 (9 bytes, each is bitmask)
  unsigned char spell1;  // Low Magic
  unsigned char spell2;  // Guard/Swift/Might/etc
  unsigned char spell3;  // Summon/Traps/Heal
  unsigned char spell4;  // High Magic
  unsigned char spell5;  // Druid magic
  unsigned char spell6;  // (same as spell5)
  unsigned char spell7;  // Sun Spells
  unsigned char spell8;  // More Sun Spells
  unsigned char spell9;  // Misc spells
};

struct item_info {        // 12 bytes per item
  unsigned char wielded;   // 8=wielded, 0=not
  unsigned char req1;      // bits 2-3: stat (Dex/Int/Spi)
  unsigned char req2;      // bits 1-5: required points (0-31)
  unsigned char unknown1..6;
  unsigned char name[12];  // high-bit encoded
};

struct player_record {     // 512 bytes total
  unsigned char name[12];          // 0x00
  // Stats: 0x0C~0x13, Health: 0x14~0x17, Stun: 0x18~0x1B, Power: 0x1C~0x1F
  struct skill_info skills;        // 0x20~0x3A
  unsigned char advancement_points; // 0x3B
  struct spell_info spells;        // 0x3C~0x44
  unsigned char unknown[8];        // 0x44~0x4B
  unsigned char status;            // 0x4C: bitfield (0=ok,1=dead,2=chained,4=poisoned,8=stunned)
  unsigned char gender;            // 0x4E: 0=male, 1=female
  unsigned short level;            // 0x4F~0x50
  unsigned int xp;                 // 0x51~0x54
  unsigned int gold;               // 0x55~0x58
  unsigned char armor, defense, armor_class; // 0x59~0x5B
  unsigned char padding[143];
  struct item_info inventory_items[12]; // 12 slots
};
```

**物品型別列舉**：
- `0x00` General Item, `0x01` Shield, `0x02` Full Shield
- `0x03` Axe, `0x04` Flail, `0x05` Sword, `0x06` Two-handed sword
- `0x07` Mace, `0x08` Bow, `0x09` Crossbow, `0x0A` Gun, `0x0B` Thrown weapon
- `0x0C` Ammunition, `0x0D` Gloves, `0x0E` Mage Gloves, `0x0F` Ammo Clip
- `0x10` Cloth Armor, `0x11` Leather, `0x12` Cuir Bouilli, `0x13` Brigandine
- `0x14` Scale, `0x15` Chain, `0x16` Plate and Chain, `0x17` Full Plate
- `0x18` Helmet, `0x19` Scroll, `0x1A` Pair of Boots, `0x1B+` blank

**導出函式**：
- `get_player_data_base()`, `get_player_data(player)`, `get_player_data_byte(player, property)`
- `player_property_name(prop_idx)` → "Strength", "Dexterity", "Status", "Gender", "Unknown Property"

---

#### `src/lib/player.h`（35 行） — 玩家 API 宣告

#### `src/lib/state.c`（53 行） — 遊戲狀態
**目的**：維護 256-byte `game_state` 結構（從 0x3860 開始）。

**已知偏移**：
- `0x00` 玩家 X, `0x01` 玩家 Y, `0x02` 世界編號, `0x03` 方向
- `0x06` 當前選擇的玩家
- `0x18~0x1F` 玩家 1~7 存在旗標 (0=存在, -1=不存在)
- `0x1F` 隊伍人數
- `0x6A~0x6D` Gold, `0x6E~0x71` Experience
- `0xC6~0x??` 新角色名字（輸入中）

#### `src/lib/resource.c`（366 行） — 資源載入
**目的**：管理 `data1` / `data2` 檔案的載入、快取、解壓縮。

**關鍵資料結構**：
```c
enum resource_section {
  RESOURCE_SCRIPT       = 0x00,  // 遊戲腳本字碼
  RESOURCE_CHARACTER_DATA = 0x07, // 角色初始資料
  RESOURCE_TITLE0       = 0x18,  // 標題畫面資源
  RESOURCE_TITLE3       = 0x1D,
  RESOURCE_UNKNOWN      = 0x47,
  RESOURCE_LAST         = 0x106,
  RESOURCE_MAX
};

struct resource {
  unsigned char *bytes;
  size_t len;
  int usage_type; // 0=未使用, 1=動態配置, 2=flagged, 0xFF=靜態
  int tag;
  int index;
};
```

**128 個 resource slot**（`allocations[128]`）。

**重要函式**：
- `rm_init()` — 初始化資源系統
- `resource_load(section)` — 載入指定 section
- `com_extract(offset, size)` — 從 `dragon.com` 提取嵌入資料
- `resource_write_to_disk()` — 用於 Save game

#### `src/lib/resource.h`（75 行） — 資源 API 宣告

#### `src/lib/tables.c`（400 行） — 資料表
**目的**：framebuffer 查表、位元遮罩表、字元表（font）。

**關鍵資料表**：
- `framebuffer_line_offsets[200]` (0xAEB2, 400 bytes) — 每行 framebuffer 偏移
- `b152_table[256]` (0xB152) — VGA 位元遮罩表
- `and_table[256]` (0xB252) — AND 遮罩
- `or_table[256]` (0xB352) — OR 遮罩
- `and_table_B452[256]` (0xB452) — 16-bit AND 遮罩
- `or_table_B652[256]` (0xB652) — 16-bit OR 遮罩
- `ba52_table[256]` (0xBA52) — 16-bit byte-swap 表
- `unknown_1BC1[]` (0x1BC1, 35 bytes) — 狀態 bit 查表
- `unknown_4456[]` (0x4456, 26 bytes) — 未知（13 個 2-byte 值）
- `chr_table` (0xBF52, 1024 bytes) — **8x8 字元點陣表**

**chr_table 格式**：每個字元 8 bytes（8×8 pixels），字元碼 OR 0x80。

#### `src/lib/tables.h`（43 行） — 資料表 API 宣告

#### `src/lib/ui.c`（1407 行） — UI 繪製
**目的**：viewport, header, minimap, 字元繪製, 狀態面板, 方框, 線條。

**關鍵資料結構**：
```c
struct ui_string_line { int len; unsigned char bytes[40]; };
struct viewport_data { uint16_t xpos; int ypos, runlength, numruns; unsigned char *data; };
struct ui_rect  { uint16_t x, y, w, h; };
struct ui_point { uint16_t x, y; };
struct ui_header { int len; unsigned char data[16]; };
```

**全域**：`ui_string` (0x320C), `draw_point` (0x32BF), `draw_rect` (0x2697)

**唯讀字串**：`ui_header_loading` (0x288B) → `Loading...`

**調色盤**：`color_data[] = { 0x00, 0xFF, 0xCC, 0xAA, 0x99 };` — 黑, 白, 紅, 綠, 藍

**重要函式**：`ui_load()`, `ui_draw()`, `ui_draw_string()`, `ui_draw_chr_piece()`, `ui_header_draw()`, `draw_viewport()`, `show_random_encounter()`

#### `src/lib/ui.h`（127 行） — UI API 宣告

#### `src/lib/offsets.c`（49 行） — 欄位偏移表
**目的**：建立 0x88 個 16-bit 偏移表。

```c
#define NUM_OFFSETS 0x88
uint16_t offsets[NUM_OFFSETS]; // 0xB042
void init_offsets(unsigned short dx); // dx = 0x50
```

#### `src/lib/compress.c`（284 行） — 解壓縮
**目的**：Dragon Wars 的 bit-level 解壓縮。

#### `src/lib/mouse.c`（162 行） — 滑鼠控制

#### `src/lib/vga.c`（170 行） — VGA 驅動程式

#### `src/lib/timers.c`（44 行） — 計時器（18.2Hz）

#### `src/lib/log.c`（160 行） — 日誌

#### `src/lib/utils.c`（105 行） — 工具函式

#### `src/lib/bufio.c`（138 行） — I/O 緩衝

### 2.2 `src/fe/` — 前端層

#### `src/fe/main.c`（184 行） — 主程式進入點
**流程**：`check_files()` → `rm_init()` → `video_setup()` → `setup_memory()` → `init_offsets(0x50)` → `load_chr_table()` → `vga_initialize(320,200)` → `run_title()` → `ui_load()` → `run_engine()`

#### `src/fe/vga_sdl.c`（346 行） — SDL2 顯示驅動
**關鍵資料**：`sdl_palette[]`（16 色）, `normal/shifted/ctrl/alt_scancodes[]`（鍵盤映射）

#### `src/fe/vga_dos.c`（71 行）, `vga_null.c`（20 行）, `vga_xlib.c`（230 行） — 其它後端

---

## 3. 字串／對話位置指南

### 3.1 遊戲文字儲存方式

**主要文字儲存**：`data1` 檔案的各個 section

- **Section 0x00（RESOURCE_SCRIPT）**：遊戲腳本字碼，包含所有內嵌對話文字（約 90% 遊戲文字）
- **Section 0x03**：主對話字串（859 個字串）
- **Section 0x13**：對話選項字串（640 個字串）
- **Section 0x06**：物品資料
- **Section 0x07**：角色資料

### 3.2 原始碼中的硬編碼字串

**engine.c**：chained, poisoned, stunned, dead, "is "

**ui.c**："Loading..."

**player.c**："Unknown Property"

### 3.3 High-Bit 編碼

所有文字使用 high-bit 編碼：每個 byte 與 0x80 做 OR。
- 'A' (0x41) 存為 0xC1
- 'a' (0x61) 存為 0xE1

---

## 4. 資料結構對照表

### 4.1 玩家記錄（512 bytes）

| 偏移 | 大小 | 欄位 |
|------|------|------|
| 0x00 | 12 | name（high-bit 編碼） |
| 0x0C | 1 | strength |
| 0x0D | 1 | max_strength |
| 0x0E | 1 | dexterity |
| 0x0F | 1 | max_dexterity |
| 0x10 | 1 | intelligence |
| 0x11 | 1 | max_intelligence |
| 0x12 | 1 | spirit |
| 0x13 | 1 | max_spirit |
| 0x14 | 2 | health |
| 0x16 | 2 | max_health |
| 0x18 | 2 | stun |
| 0x1A | 2 | max_stun |
| 0x1C | 2 | power |
| 0x1E | 2 | max_power |
| 0x20 | 27 | skills（skill_info） |
| 0x3B | 1 | advancement_points |
| 0x3C | 9 | spells（spell_info） |
| 0x44 | 8 | unknown |
| 0x4C | 1 | status（bitfield） |
| 0x4D | 1 | unknown |
| 0x4E | 1 | gender |
| 0x4F | 2 | level |
| 0x51 | 4 | xp |
| 0x55 | 4 | gold |
| 0x59 | 1 | armor |
| 0x5A | 1 | defense |
| 0x5B | 1 | armor_class |
| 0x5D | 143 | padding |
| 0xEC | 144 | inventory[12] |

### 4.2 Resource Section 標籤

| 標籤 | 名稱 | 內容 |
|------|------|------|
| 0x00 | RESOURCE_SCRIPT | 遊戲腳本字碼 |
| 0x07 | RESOURCE_CHARACTER_DATA | 角色初始資料 |
| 0x18-0x1A, 0x1D | RESOURCE_TITLE0-3 | 標題畫面圖形 |
| 0x47 | RESOURCE_UNKNOWN | 未知 |
| 0x106 | RESOURCE_LAST | 最後一個 section |

---

## 5. 函式關係圖

```
main.c
  +-- rm_init() [resource.c]
  +-- setup_memory() [resource.c]
  +-- init_offsets() [offsets.c]
  +-- load_chr_table() [tables.c]
  +-- vga_initialize() [vga.c]
  +-- ui_load() [ui.c]
  +-- run_engine() [engine.c]
       +-- run_script()
       +-- targets[opcode]()
            +-- get_character_data/set_character_data [engine.c -> player.c]
            +-- set_game_state/get_game_state [engine.c -> state.c]
            +-- resource_load [engine.c -> resource.c]
            +-- extract_string [engine.c]
            +-- ui_draw_string/ui_header_* [engine.c -> ui.c]
            +-- com_extract [engine.c -> resource.c -> dragon.com]
            +-- decompress_data1 [compress.c]
```

---

## 6. 中文化策略

### 6.1 需要翻譯的內容

1. **data1 section 0x00 的腳本文字** — 所有對話、敘事、UI 文字
2. **原始碼中的硬編碼字串** — 狀態字串、"Loading..."、"Unknown Property"
3. **物品名稱** — data1 section 0x06
4. **怪物名稱** — data1 各 section
5. **法術／技能名稱** — data1 各 section

### 6.2 技術考量

1. **High-bit 編碼**：翻譯時必須保留此編碼
2. **固定長度緩衝區**：玩家名稱 12 bytes、物品名稱 12 bytes
3. **字元表**：chr_table 只有拉丁字母，中文需要新的字型資料
4. **文字壓縮**：Section > 0x17 被壓縮，修改後必須可重新壓縮
5. **Save game 相容性**：Section 0x07 會被儲存／載入

### 6.3 需要修改的檔案

1. **data1** — 主要文字內容
2. **src/lib/engine.c** — 硬編碼狀態字串
3. **src/lib/ui.c** — "Loading..." 字串
4. **src/lib/player.c** — "Unknown Property" 字串
5. **src/lib/tables.c** — 中文字元表
6. **src/fe/vga_sdl.c** — 中文輸入法支援

---

## 7. 參考：原始 COM 位址

| 位址 | 內容 |
|------|------|
| 0x1BD1-0x1BE1 | 狀態字串（chained, poisoned, stunned, dead） |
| 0x1BAA | "is " 字串 |
| 0x1BC1 | 未知狀態表（35 bytes） |
| 0x1BC5 | 狀態顯示像素寬度 |
| 0x2A68 | ESC 對話選項字串表（58 bytes，在 dragon.com） |
| 0xAEB2 | Framebuffer 行偏移（400 bytes） |
| 0xBF52 | 字元表（1024 bytes，128 字元 × 8 bytes） |
| 0xC960 | 玩家資料基底（7 角色 × 512 bytes = 0xE00） |
| 0x1E21 | 未知資料（0xEF bytes） |
| 0x3860 | 遊戲狀態（256 bytes） |

---

*Generated by analyzing opendw source code for Chinese localization project*
