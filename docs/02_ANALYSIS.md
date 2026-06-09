# OpenDW 反組譯還原分析報告

## 一、原始檔案分析

### 1.1 Dragon Wars (1990).zip 內容

從 zip 提取的 5.25" 磁碟映像（FAT12）：

| 檔案 | 大小 | 用途 |
|------|------|------|
| `DRAGON.COM` | 55,217 bytes | 主程式（DOS COM 格式，16-bit real mode） |
| `DWTRAN.COM` | 4,044 bytes | **角色轉移工具**（Bard's Tale I/II 存檔轉移） |
| `DATA1` | 296,439 bytes | 遊戲資源（script、圖片、字型、壓縮資料） |
| `DATA2` | 352,430 bytes | 地圖/戰鬥/音效資源 |

### 1.2 DWTRAN.COM 分析

這是 **Dragon Wars 的角色轉移工具**，可將 Bard's Tale I/II 的角色轉入 Dragon Wars：

```
"Dragon Wars Utility Program"
"Copyright 1989, 1990 Interplay"
"Transfer characters"
"1) Bard's Tale I : Tales of The Unknown"
"2) Bard's Tale II : The Destiny Knight"
"Press [ESC] to return to MS-DOS"
"Note: Transferring characters will wipe out any existing saved game!!"
```

**中文化影響**：此工具與主遊戲中文化無關，但表明 Interplay 有獨立的公用程式架構。

### 1.3 DRAGON.COM 字串

遊戲內嵌字串（從 binary 提取）：
- `"Created on an Apple ][ GS. Apple ][ Forever!"` — 開發者簽名
- `"Dragon Wars Configure Menu V1.1"` — 設定選單
- `"A. CGA RGB monitor"` ~ `"E. VGA/MCGA 16 color"` — 顯示模式選項
- `"1. Mouse On"` / `"2. Mouse Off"` — 滑鼠選項
- `"Saving game state."` / `"Game state saved."` / `"Drive error."` / `"Write protected."` — 存檔訊息
- `"Fatal error : Out of memory.$"` — 記憶體錯誤

---

## 二、dragon.asm 與 opendw C 碼對應

### 2.1 完整對應表

| ASM 位址 | opendw C 函式 | 檔案 | 狀態 |
|---|---|---|---|
| `0x100-0x1B2` | `main()` | `fe/main.c` | ✅ 完成 |
| `0x1CF-0x1E7` | `init_mm()` | `resource.c` | ✅ 完成 |
| `0x21D-0x360` | `init_graphics()` | `fe/vga_dos.c` | ✅ 完成 |
| `0x387-0x3A5` | `run_title()` | `fe/main.c` | ✅ 完成 |
| `0x4B1-0x527` | drawing handlers | `fe/vga_dos.c` | ✅ 完成 |
| `0x5A5-0x5CF` | `title_adjust()` | `fe/main.c` | ✅ 完成 |
| `0x627-0x963` | `check_config()` | — | ❌ **未實作** |
| `0x9E5-0xA31` | `putchar/set_cursor` | `fe/vga_dos.c` | ✅ 完成 |
| `0xA5A-0xA93` | `cleanup()` | `main.c` | ✅ 完成 |
| `0xCA0-0xE6B` | `sub_CF8/update_viewport` | `ui.c` | ✅ 完成 |
| `0x1060-0x120B` | `draw_viewport` | `ui.c` | ✅ 完成 |
| `0x12C0-0x13B5` | `rm_init/game_mem_alloc` | `resource.c` | ✅ 完成 |
| `0x17DD-0x17F5` | `init_offsets` | `offsets.c` | ✅ 完成 |
| `0x184B` | `ui_viewport_reset` | `ui.c` | ✅ 完成 |
| `0x19C7-0x1A33` | minimap 相關 | `engine.c` | ✅ 完成 |
| `0x25E0-0x2739` | `draw_rectangle/ui_rect_*` | `ui.c` | ✅ 完成 |
| `0x28B0-0x2D0B` | `get_key/check_keystroke` | `fe/vga_sdl.c` | ✅ 完成 |
| `0x2EB0-0x3116` | `resource_load/read_write_DATA1` | `resource.c` | ✅ 完成 |
| `0x3578-0x387E` | 變數區域 | — | ✅ 完成 |
| `0x3AA0-0x3B17` | `run_engine()` | `engine.c` | ✅ 完成 |
| `0x4B10-0x4B5C` | `timer_proc_handler` | `timers.c` | ✅ 完成 |
| `0x5C3B-0x5D1D` | PIT/music | — | ❌ **未實作** |
| `0x6748-0x6820` | `viewport_metadata` | `ui.c` | ✅ 完成 |

---

## 三、engine.c 中可還原的 unnamed 函式

### 3.1 已正確命名的 opcodes（122/256）

以下 opcodes 已有正確名稱和實作：

**資料操作類**：
- `op_03` ~ `op_1F` — 幾乎全部完成（load/store/inc/dec/shift/and/or/xor）
- `op_21` ~ `op_2B` — 變數操作
- `op_30` ~ `op_3F` — 算術/邏輯運算
- `op_40` ~ `op_4F` — 跳躍/條件/旗標

**控制流類**：
- `op_41` (jnc), `op_42` (jc), `op_43` (jmp), `op_44` (jz), `op_45` (jnz)
- `op_46` (js), `op_47` (jns), `op_48` (jnle), `op_49` (loop), `op_4A` (jnz_if)
- `op_4B` (stc), `op_4C` (clc), `op_4D` (cmc), `op_4E` (test_bit), `op_4F` (clear_bit)

**資源/記憶體類**：
- `op_50` (nop), `op_51` (load_word), `op_52` (jmp), `op_53` (call), `op_54` (ret)
- `op_55` (peek_pop), `op_56` (push), `op_57` (load_resource), `op_58` (load_resource_ex)
- `op_59` (retf), `op_5A` (set_byte_mode), `op_5B`, `op_5C` (for_call)

**角色/玩家類**：
- `op_5D` (get_char_data), `op_5E` (set_char_data), `op_5F`, `op_60`
- `op_61` (test_player_property), `op_62`, `op_63`, `op_66`

**UI/顯示類**：
- `op_69`, `op_6A`, `op_6C`, `op_6D` (minimap), `op_6F`
- `op_71` (load_world), `op_72`, `op_73`
- `op_74` (draw_rectangle), `op_75` (ui_draw_full), `op_76` (draw_pattern)
- `op_77` (draw_and_set), `op_7A` (extract_string), `op_7C` (random_encounter)
- `op_7D` (write_character_name)

**輸入/事件類**：
- `op_81`, `op_82`, `op_83` (write_number), `op_84` (malloc), `op_85` (resource_release)
- `op_86` (load_resource_by_index), `op_87` (write_resource)
- `op_8A` (random_encounter_check), `op_8B` (refresh_viewport)
- `op_91`, `op_92`, `op_93` (push_byte), `op_94` (pop_byte)
- `op_95` (ui_draw_string), `op_96` (draw_padded_string)
- `op_97` (load_char_data), `op_98`, `op_99` (test_word_3AE2)
- `op_9A` (set_game_state_FF), `op_9B`, `op_9D`, `op_9E` (resource_get_size)

### 3.2 尚未還原的 unnamed 函式

| opendw 名稱 | ASM 位址 | 建議名稱 | 說明 |
|---|---|---|---|
| `sub_27CC` | `0x27CC` | `draw_right_pillar` | 繪製右側支柱 |
| `sub_4EF4` | `0x4EF4` | `draw_minimap_cell` | 繪製小地圖單格 |
| `sub_DEB` | `0xDEB` | `draw_viewport_word` | 視埠繪製（word 模式） |
| `sub_EC5` | `0xEC5` | `draw_viewport_neg_x_alt` | 視埠繪製（負 X 變體） |
| `sub_E6D` | `0xE6D` | `draw_viewport_neg_x` | 視埠繪製（負 X） |
| `sub_F3D` | `0xF3D` | `draw_viewport_flip` | 視埠繪製（翻轉） |
| `sub_11A0` | `0x11A0` | `multiply_words` | 16-bit 乘法 |
| `sub_11CE` | `0x11CE` | `divide_words` | 16-bit 除法 |
| `sub_176A` | `0x176A` | `handle_minimap_input` | 處理小地圖輸入 |
| `sub_19C7` | `0x19C7` | `plot_minimap_resource` | 繪製小地圖資源 |
| `sub_1A10` | `0x1A10` | `draw_minimap_from_data6820` | 從 DATA6820 繪製小地圖 |
| `sub_1A72` | `0x1A72` | `draw_player_status_panel` | 繪製玩家狀態面板 |
| `sub_1C70` | `0x1C70` | `extract_string_and_draw` | 提取字串並繪製 |
| `sub_280E` | `0x280E` | `draw_ui_header` | 繪製 UI 標題 |
| `sub_28B0` | `0x28B0` | `wait_for_event` | 等待事件（鍵盤/滑鼠） |
| `sub_2CF5` | `0x2CF5` | `update_random_seed` | 更新隨機種子 |
| `sub_3F2F` | `0x3F2F` | `save_gamestate_vars` | 儲存遊戲狀態變數 |
| `sub_3F7E` | `0x3F7E` | `divide_and_store` | 除法並儲存結果 |
| `sub_4A79` | `0x4A79` | `get_bit_mask` | 取得位元遮罩 |
| `sub_4D37` | `0x4D37` | `init_monster_anim` | 初始化怪物動畫 |
| `sub_4D97` | `0x4D97` | `update_monster_anim` | 更新怪物動畫 |
| `sub_54D8` | `0x54D8` | `get_map_tile` | 取得地圖圖塊 |
| `sub_536B` | `0x536B` | `move_player` | 移動玩家 |
| `sub_587E` | `0x587E` | `release_flagged_resources` | 釋放標記的資源 |
| `sub_50B2` | `0x50B2` | `play_sound_50B2` | 播放音效 |
| `sub_5088` | `0x5088` | `play_sound_5088` | 播放音效 |
| `sub_5080` | `0x5080` | `play_sound_door` | 播放開門音效 |
| `sub_5090` | `0x5090` | `play_sound_bump` | 播放撞牆音效 |
| `sub_194A` | `0x194A` | `calc_minimap_position` | 計算小地圖位置 |
| `sub_1A13` | `0x1A13` | `draw_minimap_segment` | 繪製小地圖段 |
| `sub_1861` | `0x1861` | `draw_minimap_row` | 繪製小地圖行 |
| `sub_1967` | `0x1967` | `set_viewport_size` | 設定視埠大小 |
| `sub_1DCA` | `0x1DCA` | `convert_number_to_string` | 數字轉字串 |
| `sub_1DBB` | `0x1DBB` | `print_number` | 印出數字 |
| `sub_1DC8` | `0x1DC8` | `print_number_9` | 印出數字（9位） |
| `sub_4C07` | `0x4C07` | `check_direction_icon` | 檢查方向圖示 |
| `sub_46A1` | `0x46A1` | `run_level_script` | 執行關卡腳本 |
| `sub_2ADC` | `0x2ADC` | `clear_event_flag` | 清除事件旗標 |
| `sub_2A4C` | `0x2A4C` | `handle_key_event` | 處理按鍵事件 |
| `sub_2C00` | `0x2C00` | `wait_escape_key` | 等待 ESC 鍵 |
| `sub_4C40` | `0x4C40` | `trigger_random_encounter` | 觸發隨機遭遇 |
| `sub_1EBF` | `0x1EBF` | `draw_input_box` | 繪製輸入框 |
| `sub_1EBB` | `0x1EBB` | `draw_input_box_clr` | 繪製輸入框（清除） |
| `sub_1EBE` | `0x1EBE` | `draw_input_box_carry` | 繪製輸入框（進位） |
| `sub_1E49` | `0x1E49` | `read_string_input` | 讀取字串輸入 |
| `sub_5868` | `0x5868` | `cache_level_components` | 快取關卡元件 |
| `sub_5523` | `0x5523` | `check_map_boundary_x` | 檢查地圖 X 邊界 |
| `sub_5559` | `0x5559` | `check_map_boundary_y` | 檢查地圖 Y 邊界 |
| `sub_504B` | `0x504B` | `set_map_event` | 設定地圖事件 |
| `sub_4FD9` | `0x4FD9` | `init_map_events` | 初始化地圖事件 |
| `sub_5764` | `0x5764` | `load_level_data` | 載入關卡資料 |
| `sub_CE7` | `0xCE7` | `draw_sprite_to_viewport` | 繪製 sprite 到視埠 |

### 3.3 還原優先順序

| 優先級 | 函式 | 影響 |
|--------|------|------|
| **P0** | `sub_CF8` → `decode_viewport_data` | 所有地圖顯示 |
| **P0** | `sub_3AA0` → `game_main_loop` | 遊戲主迴圈 |
| **P0** | `sub_28B0` → `wait_for_event` | 所有事件處理 |
| **P1** | `sub_54D8` → `get_map_tile` | 地圖圖塊讀取 |
| **P1** | `sub_536B` → `move_player` | 玩家移動 |
| **P1** | `sub_176A` → `handle_minimap_input` | 小地圖互動 |
| **P1** | `sub_19C7` → `plot_minimap_resource` | 小地圖繪製 |
| **P1** | `sub_1A10` → `draw_minimap_from_data6820` | 小地圖繪製 |
| **P1** | `sub_1A72` → `draw_player_status_panel` | 玩家狀態面板 |
| **P1** | `sub_1E49` → `read_string_input` | 字元輸入 |
| **P1** | `sub_1DCA` → `convert_number_to_string` | 數字顯示 |
| **P2** | `sub_11A0` → `multiply_words` | 數學運算 |
| **P2** | `sub_11CE` → `divide_words` | 數學運算 |
| **P2** | `sub_2CF5` → `update_random_seed` | 隨機數 |
| **P2** | `sub_4A79` → `get_bit_mask` | 位元運算 |
| **P2** | `sub_4C40` → `trigger_random_encounter` | 隨機遭遇 |
| **P2** | `sub_5764` → `load_level_data` | 關卡載入 |
| **P2** | `sub_4FD9` → `init_map_events` | 地圖事件 |
| **P2** | `sub_5523` → `check_map_boundary_x` | 地圖邊界 |
| **P2** | `sub_5559` → `check_map_boundary_y` | 地圖邊界 |
| **P3** | `sub_5080/5088/5090/50B2` → sound effects | 音效 |
| **P3** | `sub_5C3B-5D1D` → PIT/music | 音樂 |
| **P3** | `sub_627-0x963` → `check_config` | 設定選單 |

---

## 四、opendw 中「反組譯狀態」最嚴重的區段

### 4.1 engine.c 中的全域變數（應改為 struct）

目前 engine.c 有 **100+ 個全域變數**，其中許多應該是 struct 成員：

```c
// 現狀：散落的 global variables
uint16_t counter_104D;
unsigned char byte_104E;
uint16_t word_104F = 0;
struct resource *word_1051;
uint16_t word_1053 = 0;
uint16_t word_1055;
// ... 更多

// 建議：改為 struct
struct engine_state {
    uint16_t counter_104D;
    uint8_t  byte_104E;
    uint16_t word_104F;
    struct resource *word_1051;
    uint16_t word_1053;
    uint16_t word_1055;
    // ...
};
```

### 4.2 ui.c 中的魔術數字

`ui.c` 中有大量魔術數字，應該改為命名常數：

```c
// 現狀
if (draw_point.x < 0x27) { ... }  // 0x27 = 39
if (draw_point.y < 0xB8) { ... }  // 0xB8 = 184
if (al == 0x8D) { ... }           // 0x8D = 換行
if (al == 0x80) { ... }           // 0x80 = 左上角

// 建議
#define UI_RIGHT_PILLAR  0x27
#define UI_BOTTOM_BORDER 0xB8
#define UI_CHR_NEWLINE   0x8D
#define UI_CHR_CORNER_TL 0x80
```

### 4.3 尚未實作的 opcodes（117/256）

其中有 **32 個 opcode 位址指向 dragon.com 內部的程式碼**（非 opcodes），這些是 Devin Smith 反組譯時的跳躍表項，不是真正的 opcodes。

**真正需要實作的 opcodes**（約 85 個）：
- 許多是低優先級的功能（音效、音樂、設定選單）
- 核心遊戲邏輯的 opcodes 大部分已完成

---

## 五、關鍵發現

### 5.1 遊戲資料壓縮

DATA1 使用 **LZSS 壓縮**（`compress.c` 中的 `decompress_data1`），這與 dragon.asm 中的 `0x4F8E` 位址對應。

### 5.2 字串編碼

遊戲使用 **5-bit 壓縮字母表**（`compress.c` 中的 `alphabet[]`），包含：
- 大寫字母（A-Z）
- 小寫字母（a-z）
- 數字（0-9）
- 標點符號
- 特殊控制碼（0xAF = 斜線, 0xDC = 反斜線）

**中文化影響**：這套編碼系統只支援 ASCII，中文需要完全不同的編碼層。

### 5.3 角色資料結構

`player_record`（512 bytes）在 `player.c` 中已有完整定義，包含：
- 名稱（12 bytes）
- 屬性（strength/dexterity/intel/spirit）
- 生命值/昏迷值/法力值
- 技能（27 bytes）
- 法術（9 bytes）
- 物品（12 個物品 × 16 bytes）
- 狀態/gender/level/xp/gold

### 5.4 視埠系統

視埠是 320×200 framebuffer 中的一個 **80×136 字元**（8×8 pixel/字元）區域：
- 每 row = 80 字元 = 640 pixels
- 每 column = 136 字元 = 1088 pixels
- 視埠資料儲存在 `viewport_memory`（10880 bytes = 136 × 80）

**中文化影響**：若改用 24×24 字元，視埠將變為 **13×8 字元**，需要重新設計 UI 佈局。

---

## 六、建議的下一步

1. **優先還原 P0 函式**：
   - `sub_CF8` → `decode_viewport_data`
   - `sub_28B0` → `wait_for_event`
   - `sub_54D8` → `get_map_tile`

2. **重構全域變數為 struct**：
   - 建立 `struct engine_state` 和 `struct ui_state`
   - 逐步遷移現有的 global variables

3. **消除魔術數字**：
   - 為所有 8×8 cell 座標定義命名常數
   - 為所有 color index 定義命名常數

4. **實作缺失的 opcodes**：
   - 從 dragon.asm 提取邏輯
   - 優先實作遊戲核心功能（非音效/音樂）

5. **中文化準備**：
   - 修改 `draw_character()` 以支援 24×24 cell
   - 修改 `ui_draw_chr_piece()` 以支援 CJK
   - 修改 `extract_string()` 以支援 Big5 編碼
