# OpenDW 原始碼地圖 — 龍戰（Dragon Wars）中文化參考

> **用途**：提供 opendw 重製版 (`/home/anr2/tmp/longcat/opendw/src/`) 各檔案的架構、資料結構、字串／對話位置、以及函式關係的完整導覽。撰寫目標為中文化工程（cht 分支）的前置閱讀文件。
>
> **日期**：2026-06-09
> **對應原始程式**：Devin Smith 的 [opendw](https://github.com/dsmith/opendw)（DOS COM 反組譯重寫）
> **原始遊戲**：Interplay Dragon Wars (1989), by Rebecca Ann Heineman

---

## 1. 專案架構總覽

```
opendw/
├── src/
│   ├── lib/         # 核心邏輯層（虛擬 CPU、資源載入、玩家、UI、資料表）
│   ├── fe/          # 前端層（SDL2/DOS/Xlib 顯示驅動，main entry）
│   ├── tests/       # 測試
│   └── tools/       # 工具
├── doc/             # 原始文件
├── dos/             # DOS 版相關檔案
└── img/             # 影像素材
```

**核心理念**：opendw 是一個「可逐位元組相容」的 Dragon Wars COM 可執行檔重製。幾乎所有資料都是從 `dragon.com`、`data1`、`data2` 三個二進位檔案載入，程式本身以 16-bit x86 組合語言的虛擬 CPU 來執行遊戲腳本（Script bytecode）。

---

## 2. 檔案明細

### 2.1 `src/lib/` — 核心邏輯層

#### `src/lib/engine.c`（6637 行） — 主引擎（虛擬 CPU + opcode 解碼）
**目的**：實現 Dragon Wars 的虛擬 CPU 與所有 opcode，是整個重製版的心臟。

**關鍵資料結構**：
```c
// 虛擬 CPU（16-bit 暫存器 + stack + flags）
struct virtual_cpu {
  uint16_t ax, bx, cx, dx, di, si;
  uint8_t  stack[STACK_SIZE]; // STACK_SIZE = 32
  uint8_t  sp;
  uint8_t  cf, zf, sf;       // carry / zero / sign flags
  bool     key_wait_inited;
  unsigned char *pc, *base_pc;
};
extern struct virtual_cpu cpu;

// Bit extractor（解壓縮用）
struct bit_extractor {
  uint8_t num_bits, upper_case, bit_buffer;
  const unsigned char *data;
  uint16_t offset;
};
```

**重要全域變數**：
- `word_3AE2` (0x3AE2) — 通用 16-bit 暫存器，常傳遞 opcode 結果
- `word_3AE4` (0x3AE4) — 迴圈計數器／臨時值
- `word_3AE6` (0x3AE6) — CPU flags 映像 + player 相關旗標
- `word_3AE8` (0x3AE8) — running_script resource index
- `word_3AEA` (0x3AEA) — word_3ADF resource index
- `word_104F` (0x104F) — script offset (word_1051 內)
- `word_1051` (0x1051) — 當前載入的 script resource
- `data_2AAA[32]` (0x2AAA) — 鍵盤／事件緩衝區
- `escape_string_table` (0x2A68) — ESC 對話選項字串表

**Opcode 呼叫表**：
- `targets[]` 陣列（583–840 行）— 共 256 個 entries（很多 NULL），每個 entry = `{ func, src_offset_string }`

**關鍵 opcodes**：
- `op_05` — 載入 game_state[bx]
- `op_08` — 寫入 game_state[bx]
- `op_0B` — 從 game_state 載入 word
- `op_5D` (`get_character_data`) — 讀取玩家屬性
- `op_5E` (`set_character_data`) — 寫入玩家屬性
- `op_8C` (`prompt_no_yes`) — 顯示 Yes/No 對話框
- `op_8D` (`read_string_input`) — 讀取玩家輸入字串（角色命名）
- `op_8A` — 觸發隨機戰鬥
- `op_77` — 繪製 pattern + set_msg
- `set_msg` (op_78) — 從 script 提取字串
- `op_7A` — 從 word_3ADF 提取字串

**內嵌字串常值（直接寫在 .c 裡的 bytes）**：
- `str_chained` (0x1BD1) — `{0x22,0x44,0xA7,0x18,0xA0}` → `chained`
- `str_poisoned` (0x1BD6) — `{0x83,0xD5,0x27,0xB8,0xC5,0x0}` → `poisoned`
- `str_stunned` (0x1BDC) — `{0x94,0xE8,0xE7,0x18,0xA0}` → `stunned`
- `str_dead` (0x1BE1) — `{0x29,0x84,0x50,0x0}` → `dead`
- `data_1BAA` (0x1BAA) — `{0x54,0x82,0x00}` → `is `
- `data_1BC5` (0x1BC5) — `{0x1C,0x1C,0x1C,0x1E}` — 狀態顯示用 pixel width
- `data_1EB9` (0x1EB9) — `{0xC2,0x00}` — 鍵盤事件資料
- `str_table_status[4]` (0x1BC9) — 狀態字串表 `chained/poisoned/stunned/dead`

**與其它模組的呼叫關係**：
- `resource_load()` / `resource_get_by_index()` → `resource.c`
- `get_player_data()` / `get_player_data_byte()` → `player.c`
- `set_game_state()` / `get_game_state()` → `state.c`
- `ui_draw_string()`, `ui_header_*` → `ui.c`
- `extract_string()` → 自身 inline，用 callback `handle_byte_callback`
- `com_extract()` → `resource.c`（從 dragon.com 提取嵌入資料）

---

#### `src/lib/engine.h`（43 行） — 引擎對外介面
**匯出**：
- `reset_game_state()`, `run_engine()`, `release_flagged_resource()`
- `extract_string(src_ptr, offset, func)` — 從資源字串表提取字串
- 多個 `extern` 全域變數供其它模組讀寫 (`data_2AAA`, `word_4C31`, `byte_4F0F`, `byte_4F10`)

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
  unsigned char spell1;  // Low Magic: Elvar's Fire, Fire Light, Mage Light, Lesser Heal, Luck, Charm, Disarm, Mage Fire
  unsigned char spell2;  // Vorn's Guard, Sala's Swift, Reveal Glamor, Mystic Might, Dazzle, Big Chill, Ice Chill, Poog's Vortex
  unsigned char spell3;  // Water/Earth/Air Summon, Sense Traps, Cloak Arcane, Group Heal, Healing, Cowardice
  unsigned char spell4;  // High Magic: Greater Healing, Brambles, Scare, Whirlwind, Insect Plague, Fire Blast, Death Curse, Fire Summon
  unsigned char spell5;  // Druid: Exorcism, Sunstroke, Wood Spirit, Beast Call, Invoke Spirit, Soften Stone, Create Wall, Cure All
  unsigned char spell6;  // (same as spell5 in comments)
  unsigned char spell7;  // Sun Spells: Mithras' Bless, Column of Fire, Battle Power, Holy Aim, Inferno, Fire Storm, Wrath/Rage of Mithras
  unsigned char spell8;  // Radiance, Guidance, Disarm Trap, Major Heal, Heal, Sun Light, Armor of Light, Light Flash
  unsigned char spell9;  // Misc: Poison, Kill Ray, Zak's Speed, Charger, Summon Salamander
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
  // Stats: 0x0C~0x13 (str/dex/int/spi, current + max)
  // Health: 0x14~0x17, Stun: 0x18~0x1B, Power: 0x1C~0x1F
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

**物品型別列舉（程式註解）**：
- `0x00` General Item
- `0x01` Shield, `0x02` Full Shield
- `0x03` Axe, `0x04` Flail, `0x05` Sword, `0x06` Two-handed sword
- `0x07` Mace, `0x08` Bow, `0x09` Crossbow, `0x0A` Gun, `0x0B` Thrown weapon
- `0x0C` Ammunition, `0x0D` Gloves, `0x0E` Mage Gloves, `0x0F` Ammo Clip
- `0x10` Cloth Armor, `0x11` Leather Armor, `0x12` Cuir Bouilli, `0x13` Brigandine
- `0x14` Scale, `0x15` Chain, `0x16` Plate and Chain, `0x17` Full Plate
- `0x18` Helmet, `0x19` Scroll, `0x1A` Pair of Boots, `0x1B+` blank

**導出函式**：
- `get_player_data_base()` → 回傳 `data_C960`
- `get_player_data(player)` → 回傳第 N 個角色指標
- `get_player_data_byte(player, property)` → 讀取屬性
- `player_property_name(prop_idx)` → 屬性名稱字串（"Strength", "Dexterity", "Status", "Gender" 等）

**注意**：`player_property_name()` 回傳的 "Unknown Property" 是 `lib/` 中的一個英文 UI 字串。

---

#### `src/lib/player.h`（35 行） — 玩家 API 宣告

---

#### `src/lib/state.c`（53 行） — 遊戲狀態
**目的**：維護一個 256-byte 的 `game_state` 結構（從 0x3860 開始），供虛擬 CPU 的 opcode 讀寫。

```c
struct game_state {
  unsigned char unknown[256]; // 0x00~0xFF
};
```

**已知偏移**（見 `state.h` 註解）：
- `0x00` 玩家 X
- `0x01` 玩家 Y
- `0x02` 世界編號（如 Purgatory）
- `0x03` 方向 (0=N,1=E,2=S,3=W)
- `0x06` 當前選擇的玩家
- `0x18~0x1F` 玩家 1~7 存在旗標 (0=存在, -1=不存在)
- `0x1F` 隊伍人數
- `0x6A~0x6D` Gold (4 bytes)
- `0x6E~0x71` Experience (4 bytes)
- `0xBE` 方向 (重複?)
- `0xC6~0x??` 新角色名字（輸入中）

**導出**：`set_game_state()`, `get_game_state()`

---

#### `src/lib/resource.c`（366 行） — 資源載入
**目的**：管理 `data1` / `data2` 檔案的載入、快取、解壓縮。

**關鍵資料結構**：
```c
enum resource_section {
  RESOURCE_SCRIPT       = 0x00,  // 遊戲腳本字碼
  RESOURCE_CHARACTER_DATA = 0x07, // 角色初始資料
  RESOURCE_TITLE0       = 0x18,  // 標題畫面資源 0
  RESOURCE_TITLE1       = 0x19,
  RESOURCE_TITLE2       = 0x1A,
  RESOURCE_TITLE3       = 0x1D,  // 標題畫面資源 3（run_title 載入）
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

**128 個 resource slot**（`allocations[128]`），每個有独立的 `tag`（即 `resource_section`）。

**重要函式**：
- `rm_init()` — 初始化資源系統 + 載入 data1 標頭
- `resource_load(section)` — 載入指定 section（cache miss 時自動 decompress）
- `com_extract(offset, size)` — 從 `dragon.com` 提取嵌入資料（用於 font、ESC table 等）
- `resource_write_to_disk()` — 用於 Save game（僅支援未壓縮 sections ≤ 0x17）

**解壓縮**：section > 0x17 的資料會經過 `decompress_data1()` 解壓。

**Game State 與 Save**：
- Save 是透過 `resource_write_to_disk()` 把修改過的 section 寫回 data1。
- 玩家角色資料（section 0x07）是可被寫回的。

---

#### `src/lib/resource.h`（75 行） — 資源 API 宣告

---

#### `src/lib/tables.c`（400 行） — 資料表
**目的**：提供 framebuffer 查表、位元遮罩表、字元表（font）等唯讀資料。

**關鍵資料表**：
- `framebuffer_line_offsets[200]` (0xAEB2, 400 bytes) — 每行 framebuffer 偏移 (y*320)
- `b152_table[256]` (0xB152, 256 bytes) — VGA 位元遮罩表
- `and_table[256]` (0xB252, 256 bytes) — AND 遮罩
- `or_table[256]` (0xB352, 256 bytes) — OR 遮罩
- `and_table_B452[256]` (0xB452, 512 bytes) — 16-bit AND 遮罩
- `or_table_B652[256]` (0xB652, 512 bytes) — 16-bit OR 遮罩
- `ba52_table[256]` (0xBA52, 512 bytes) — 16-bit byte-swap 表
- `unknown_1BC1[]` (0x1BC1, 35 bytes) — 未知資料（用於 status bit 查表）
- `unknown_4456[]` (0x4456, 26 bytes) — 未知（看起來是 13 個 2-byte 值）
- `chr_table` (0xBF52, 1024 bytes) — **8x8 字元點陣表**（從 dragon.com 載入）

**chr_table 格式**：
- 每個字元 8 bytes（8 行 × 8 pixels）
- 字元碼 OR 0x80 表示（如 0xC1 = 'A', 0xE1 = 'a'）
- 大寫 A = `0x30,0x78,0xCC,0xCC,0xFC,0xCC,0xCC,0x00`

**載入**：
- `load_chr_table()` — `chr_table = com_extract(0xBF52, 0x400)`
- `get_chr(chr_num)` — `chr_num & 0x7F`，每個字元 8 bytes

---

#### `src/lib/tables.h`（43 行） — 資料表 API 宣告

---

#### `src/lib/ui.c`（1407 行） — UI 繪製
**目的**：所有 UI 繪製（viewport, header, minimap, 字元繪製, 狀態面板, 方框, 線條）。

**關鍵資料結構**：
```c
struct ui_string_line {
  int len;
  unsigned char bytes[40];
};

struct viewport_data {
  uint16_t xpos;
  int ypos, runlength, numruns;
  int unknown1, unknown2;
  unsigned char *data;
};

struct ui_rect  { uint16_t x, y, w, h; };
struct ui_point { uint16_t x, y; };
struct ui_header { int len; unsigned char data[16]; };
```

**全域**：
- `ui_string` (0x320C) — 當前行繪製緩衝
- `draw_point` (0x32BF) — 當前繪製座標
- `draw_rect` (0x2697) — 當前繪製區域

**唯讀字串**：
- `ui_header_loading` (0x288B) — `{0xCC,0xEF,0xE1,0xE4,0xE9,0xEE,0xE7,0xAE,0xAE,0xAE}` → `Loading...`

**調色盤**：
- `color_data[] = { 0x00, 0xFF, 0xCC, 0xAA, 0x99 };` — 黑, 白, 紅, 綠, 藍

**重要函式**：
- `ui_load()` — 初始化 viewport memory
- `ui_draw()` / `ui_draw_full()` — 主繪製迴圈
- `ui_draw_string()` — 從 `ui_string.bytes` 繪製字串
- `ui_draw_chr_piece(chr)` — 繪製單一字元
- `ui_header_draw()` — 繪製 header（上方狀態列）
- `ui_header_set_byte(byte)` — 設定 header 字元
- `draw_viewport()` — 繪製 3D 視圖
- `show_random_encounter()` — 繪製隨機戰鬥
- `draw_graphic_to_viewport()` — 繪製圖形到 viewport
- `init_viewport_for_map()` — 初始化地圖 viewport

**與 engine.c 的互動**：
- engine.c 呼叫 `ui_draw_string()`, `ui_header_*` 來顯示文字
- engine.c 的 `handle_byte_callback` 會呼叫 `ui_draw_chr_piece()`
- engine.c 的 `draw_player_status_panel()` 在 engine.c 裡（不在 ui.c）

---

#### `src/lib/ui.h`（127 行） — UI API 宣告

---

#### `src/lib/offsets.c`（49 行） — 欄位偏移表
**目的**：建立 0x88 個 16-bit 偏移表，用於 script 定址。

```c
#define NUM_OFFSETS 0x88
uint16_t offsets[NUM_OFFSETS]; // 0xB042

void init_offsets(unsigned short dx); // dx = 0x50
uint16_t get_offset(int pos);
```

---

#### `src/lib/compress.c`（284 行） — 解壓縮
**目的**：實現 Dragon Wars 的 bit-level 解壓縮（用於 data1 高編號 sections）。

**關鍵資料結構**：`struct bit_extractor`
```c
struct bit_extractor {
  uint8_t num_bits;      // 剩餘 bits 數
  uint8_t upper_case;    // 大寫旗標
  uint8_t bit_buffer;    // bit 緩衝
  const unsigned char *data;
  uint16_t offset;
};
```

**函式**：
- `bit_extract(be, n)` — 提取 n bits
- `extract_letter(be)` — 提取一個字母（5-bit 編碼）
- `decompress_data1(input, output, size)` — 解壓 data1 區段

---

#### `src/lib/mouse.c`（162 行） — 滑鼠控制
**目的**：DOS 中斷式滑鼠控制（游標顯示/隱藏, 點擊偵測, 邊界檢查）。

---

#### `src/lib/vga.c`（170 行） — VGA 驅動程式
**目的**：VGA 硬體層（320×200×16 色）的 framebuffer 操作。

---

#### `src/lib/timers.c`（44 行） — 計時器
**目的**：18.2Hz 系統計時器（用於 random seed 更新, 事件排程）。

```c
struct timer_ctx {
  uint8_t  timer0, timer1, timer2;
  uint16_t timer3, timer4;
  uint8_t  timer5; // 較像旗標
};
```

---

#### `src/lib/log.c`（160 行） — 日誌
**目的**：rxi/log.h 的包裝，用於除錯輸出（`log_trace` / `log_debug` 等）。

---

#### `src/lib/utils.c`（105 行） — 工具函式
**目的**：`dump_hex()`, `hexdump()` 等除錯工具。

---

#### `src/lib/bufio.c`（138 行） — I/O 緩衝
**目的**：`buf_rdr`（讀）/ `buf_wri`（寫）結構，供 resource.c 與 compress.c 使用。

---

### 2.2 `src/fe/` — 前端層

#### `src/fe/main.c`（184 行） — 主程式進入點
**目的**：初始化遊戲、顯示標題畫面、載入 UI、進入主迴圈。

**流程**：
1. `check_files()` — 檢查 dragon.com/data1/data2 是否存在
2. `rm_init()` — 初始化資源系統
3. `video_setup()` — 註冊 VGA 驅動（SDL/DOS/Xlib）
4. `setup_memory()` — 初始化 viewport memory
5. `init_offsets(0x50)` — 建立偏移表
6.
