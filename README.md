# OpenDW Dragon Wars 中文化專案

OpenDW 是 Interplay 1989/1990 年遊戲 **Dragon Wars** 的開源重製版。
本專案旨在將 OpenDW 中文化（繁體中文），並整合 SDL2 顯示層。

## 專案結構

```
opendw_dragon_wars_cht/
├── docs/                    # 文件
│   ├── PLAN.md              # 中文化規劃
│   ├── ANALYSIS.md          # 反組譯還原分析
│   ├── dragon.asm           # 原始 DOS 反組譯（參考）
│   └── README.md            # 本說明
├── src/                     # OpenDW 原始碼
│   ├── fe/                  # 前端（SDL2 輸出層）
│   │   ├── main.c           # 程式進入點
│   │   ├── vga_sdl.c        # SDL2 顯示驅動
│   │   ├── vga_dos.c        # DOS VGA 驅動
│   │   └── vga_null.c       # 空驅動
│   ├── lib/                 # 核心引擎
│   │   ├── engine.c         # 虛擬 CPU + 115 個 opcode
│   │   ├── ui.c             # UI 繪製
│   │   ├── resource.c       # 資源載入
│   │   ├── tables.c         # 字型表
│   │   ├── state.c          # 遊戲狀態
│   │   ├── player.c         # 角色資料
│   │   └── ...
│   ├── tools/               # 輔助工具
│   └── tests/               # 單元測試
├── CMakeLists.txt           # CMake 建置
└── Makefile                 # Make 建置
```

## 原始 OpenDW 資訊

Original game engine by [Rebecca Ann Heineman](https://www.burgerbecky.com/).

This game can be purchased at [GOG](https://www.gog.com/game/dragon_wars).

## 快速開始

### 建置

```bash
mkdir build && cd build
cmake ..
make
```

### 執行

需要原始遊戲檔案（dragon.com, data1, data2）：

```bash
./src/fe/sdldragon
```

## 中文化狀態

- [ ] 640×480 + 24×24 CJK 顯示
- [ ] 外部字型載入
- [ ] Big5 編碼支援
- [ ] 字串萃取工具
- [ ] UI 佈局調整

## 反組譯還原進度

### ✅ 已成功還原的函式（50+）

| 原名稱 | 新名稱 | 功能 |
|--------|--------|------|
| `sub_CF8` | `decode_viewport_data` | 視埠資料解碼 |
| `sub_37C8` | `init_viewport_for_map` | 地圖視埠初始化 |
| `sub_28B0` | `wait_for_event` | 等待鍵盤/滑鼠事件 |
| `sub_2D0B` | `get_key_from_buffer` | 從緩衝區取得按鍵 |
| `sub_54D8` | `get_map_tile_data` | 取得地圖圖塊資料 |
| `sub_536B` | `move_player_on_map` | 移動玩家 |
| `sub_176A` | `handle_minimap_input` | 處理小地圖輸入 |
| `sub_19C7` | `plot_minimap_resource` | 繪製小地圖資源 |
| `sub_1A10` | `draw_minimap_from_data6820` | 從 DATA6820 繪製小地圖 |
| `sub_1A72` | `draw_player_status_panel` | 繪製玩家狀態面板 |
| `sub_1E49` | `read_string_input` | 讀取字串輸入 |
| `sub_1DCA` | `convert_number_to_string` | 數字轉字串 |
| `sub_1DBB` | `print_number` | 印出數字 |
| `sub_1DC8` | `print_number_9_digits` | 印出 9 位數字 |
| `sub_194A` | `calc_minimap_position` | 計算小地圖位置 |
| `sub_1A13` | `draw_minimap_segment` | 繪製小地圖段 |
| `sub_1861` | `draw_minimap_row` | 繪製小地圖行 |
| `sub_1967` | `set_viewport_size` | 設定視埠大小 |
| `sub_17F7` | `draw_minimap` | 繪製小地圖 |
| `sub_1750` | `process_minimap_commands` | 處理小地圖指令 |
| `sub_280E` | `flush_ui_header` | 刷新 UI 標題 |
| `sub_11A0` | `multiply_16bit` | 16-bit 乘法 |
| `sub_11CE` | `divide_16bit` | 16-bit 除法 |
| `sub_2CF5` | `update_random_seed` | 更新隨機種子 |
| `sub_4A79` | `get_bit_mask_from_table` | 取得位元遮罩 |
| `sub_4C07` | `check_and_update_direction` | 檢查方向圖示 |
| `sub_46A1` | `run_level_script` | 執行關卡腳本 |
| `sub_4FD9` | `init_map_events` | 初始化地圖事件 |
| `sub_5764` | `load_level_resources` | 載入關卡資源 |
| `sub_5868` | `cache_level_components` | 快取關卡元件 |
| `sub_5523` | `check_map_boundary_x` | 檢查地圖 X 邊界 |
| `sub_5559` | `check_map_boundary_y` | 檢查地圖 Y 邊界 |
| `sub_504B` | `set_map_event_flag` | 設定地圖事件旗標 |
| `sub_3F23` | `compute_division_vars` | 計算除法變數 |
| `sub_3F2F` | `save_gamestate_vars` | 儲存遊戲狀態變數 |
| `sub_3F7E` | `divide_and_save_results` | 除法並儲存結果 |
| `sub_2C00` | `wait_for_escape_key` | 等待 ESC 鍵 |
| `sub_1C70` | `extract_and_draw_string` | 提取字串並繪製 |
| `sub_1EBF` | `draw_input_box_with_flag` | 繪製輸入框 |
| `sub_1EBB` | `draw_input_box_clear` | 清除輸入框 |
| `sub_1EBE` | `draw_input_box_carry` | 進位輸入框 |
| `sub_587E` | `release_flagged_resources` | 釋放標記的資源 |
| `sub_2ADC` | `clear_event_flag` | 清除事件旗標 |
| `sub_2A4C` | `handle_key_event` | 處理按鍵事件 |
| `sub_2BD9` | `check_timer_events` | 檢查計時器事件 |
| `sub_4C40` | `trigger_random_encounter` | 觸發隨機遭遇 |
| `sub_4D82` | `release_flagged_resource` | 釋放標記資源 |
| `sub_4D37` | `init_monster_animation` | 初始化怪物動畫 |
| `sub_4D97` | `update_monster_animation` | 更新怪物動畫 |
| `sub_4D5C` | `check_random_encounter_timer` | 檢查隨機遭遇計時 |
| `sub_27CC` | `draw_right_pillar` | 繪製右側支柱 |
| `sub_35A0` | `draw_ui_piece_by_index` | 繪製 UI 元件 |
| `sub_4EF4` | `draw_minimap_cell` | 繪製小地圖單格 |
| `sub_4CB2` | `draw_random_encounter_graphic` | 繪製隨機遭遇圖形 |
| `sub_4C95` | `show_random_encounter` | 顯示隨機遭遇 |
| `sub_4DE3` | `draw_graphic_to_viewport` | 繪製圖形到視埠 |
| `sub_4D26` | `clear_viewport_save` | 清除視埠儲存 |
| `sub_2AEE` | `check_mouse_in_bounds` | 檢查滑鼠邊界 |
| `sub_2061` | `draw_mouse_cursor` | 繪製滑鼠游標 |
| `sub_CE7` | `draw_sprite_to_viewport` | 繪製 sprite 到視埠 |
| `sub_DEB` | `draw_viewport_word_mode` | 視埠繪製（word 模式） |
| `sub_EC5` | `draw_viewport_neg_x_alt` | 視埠繪製（負 X 變體） |
| `sub_E6D` | `draw_viewport_neg_x` | 視埠繪製（負 X） |
| `sub_F3D` | `draw_viewport_flip_y` | 視埠繪製（Y 翻轉） |

### ⚠️ 無法判斷 / 未實作的函式

| 位址 | 狀態 | 說明 |
|------|------|------|
| `0x627-0x963` | 未實作 | DOS 設定選單（CGA/EGA/VGA/Tandy 設定） |
| `0x5C3B-0x5D1D` | 未實作 | PC speaker 音樂播放（PIT timer） |
| `op_02` | 未實作 | 未知功能 |
| `op_1B` | 未實作 | 未知功能 |
| `op_1E` | 未實作 | 未知功能 |
| `op_29` | 未實作 | 未知功能 |
| `op_2C` | 未實作 | 未知功能 |
| `op_37` | 未實作 | 未知功能 |
| `op_46` | 未實作 | 未知功能 |
| `op_64-6B` | 未實作 | 未知功能 |
| `op_6E-70` | 未實作 | 未知功能 |
| `op_79` | 未實作 | 未知功能 |
| `op_7E` | 未實作 | 未知功能 |
| `op_8E-8F` | 未實作 | 未知功能 |
| `op_9C` | 未實作 | 未知功能 |
| `op_9F` | 未實作 | 未知功能 |
| `op_A0-FF` (大部分) | 未實作 | 未知功能（可能是未使用的 opcode） |

### 📊 統計

| 項目 | 數量 |
|------|------|
| 已還原的 `sub_XXX` 函式 | 52 |
| 已命名的 `op_XX` opcode | 143 |
| 未實作的 opcode | ~85 |
| 未實作的系統功能 | 2（音樂、設定選單） |

## 授權

OpenDW 原始碼採用 BSD 授權。
Dragon Wars 是 Interplay 的商標，原始遊戲檔案僅供個人使用。

## 貢獻者

- Chun-Yu Wang
