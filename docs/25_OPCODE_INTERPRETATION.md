# Dragon Wars 虛擬 CPU Opcode 判讀報告

> 版本：初版（2026-06-10）
> 參考：`src/lib/engine.c`（targets[] 第 583 行）、`docs/dragon.asm`、`opendw/doc/script.md`

---

## 摘要

| 項目 | 數值 |
|------|------|
| 總 opcode 數 | 256（0x00–0xFF） |
| 已實作 | 139 |
| 未實作（NULL） | 117 |
| 有效 NULL（ASM 位址合理）| 約 22 個（0x00–0x9F 範圍） |
| 推測為原始碼殘留資料（0xA0–0xFF）| 約 95 個 |
| 分類 C（文字/UI，中文化高優先）未實作 | 4 個（0x79, 0x7E, 0x7F, 0x8F） |

**重要發現**：0xA0–0xFF 的 NULL 項目 ASM 位址格式異常（如 `0x8A06`、`0xE80E`），疑似是
原始 COM 檔案反組譯過程中產生的 x86 機器碼位元組片段誤植，而非真正的腳本 opcode。
真正需要實作的 NULL opcode 集中在 **0x02–0x9F** 範圍。

---

## 分類說明

| 分類 | 說明 |
|------|------|
| A | 圖形/繪圖（視口、精靈、小地圖） |
| B | 音效 |
| C | 文字/UI 顯示（**中文化高優先**） |
| D | 遊戲邏輯/流程（資料操作、跳轉、角色屬性等） |
| E | 無法判讀（ASM 位址無效或不在反組譯範圍） |

---

## 完整 256-opcode 對照表

格式：`索引 | ASM 位址 | 狀態 | C 函式名或推測語意 | 分類`

| 索引 | ASM 位址 | 狀態 | 函式名 / 推測語意 | 分類 |
|------|----------|------|-------------------|------|
| 0x00 | 0x3B18 | ✅ | `set_word_mode` — 切換 16-bit 模式（byte_3AE1=0xFF） | D |
| 0x01 | 0x3B0E | ✅ | `set_byte_mode` — 切換 8-bit 模式（byte_3AE1=0） | D |
| 0x02 | 0x3B1F | ❌ | 推測：同 0x00/0x01 的模式切換變體，或清除 word_3AE2 高位元組 | D |
| 0x03 | 0x3B2F | ✅ | `op_03` — 彈出堆疊 1 byte→AX，重設資源指標（word_3AEA） | D |
| 0x04 | 0x3B2A | ✅ | `op_04` — 推入 word_3AE8 低位元組到堆疊 | D |
| 0x05 | 0x3B3D | ✅ | `op_05` — game_state[arg]→word_3AE4 | D |
| 0x06 | 0x3B4A | ✅ | `op_06` — 1-byte 立即數→word_3AE4（迴圈計數器） | D |
| 0x07 | 0x3B52 | ✅ | `op_07` — AX 高位元組→word_3AE4 | D |
| 0x08 | 0x3B59 | ✅ | `op_08` — game_state[arg]←word_3AE4 | D |
| 0x09 | 0x3B67 | ✅ | `set_word3AE2_arg` — 從指令流載入 byte/word→word_3AE2 | D |
| 0x0A | 0x3B7A | ✅ | `load_word3AE2_gamestate` — word_3AE2←game_state[arg] | D |
| 0x0B | 0x3B8C | ✅ | `op_0B` — word_3AE2←game_state[arg+word_3AE4] | D |
| 0x0C | 0x3BA2 | ✅ | `op_0C` — word_3AE2←word_3ADF->bytes[arg16] | D |
| 0x0D | 0x3BB7 | ✅ | `op_0D` — word_3AE2←word_3ADF->bytes[arg16+word_3AE4] | D |
| 0x0E | 0x3BD0 | ✅ | `op_0E` — word_3AE2←word_3ADF->bytes[game_state[arg]+word_3AE4]（間接） | D |
| 0x0F | 0x3BED | ✅ | `op_extract_resource_data` — 從資源解壓縮資料→word_3AE2 | D |
| 0x10 | 0x3C10 | ✅ | `op_10` — word_3AE2←word_3ADF[game_state[arg1]+arg2] | D |
| 0x11 | 0x3C2D | ✅ | `op_11` — game_state[arg]←AH | D |
| 0x12 | 0x3C59 | ✅ | `op_12` — game_state[arg]←word_3AE2 | D |
| 0x13 | 0x3C72 | ✅ | `set_gamestate_offset` — game_state[arg+word_3AE4]←word_3AE2 | D |
| 0x14 | 0x3C8F | ✅ | `op_14` — word_3ADF->bytes[arg16]←word_3AE2 | D |
| 0x15 | 0x3CAB | ✅ | `op_15` — word_3ADF->bytes[arg16+word_3AE4]←word_3AE2 | D |
| 0x16 | 0x3CCB | ✅ | `op_16` — word_3ADF->bytes[game_state[arg]+word_3AE4]←word_3AE2 | D |
| 0x17 | 0x3CEF | ✅ | `store_data_into_resource` — word_3AE2 寫入資源位元組 | D |
| 0x18 | 0x3D19 | ✅ | `op_18` — word_3ADF->bytes[game_state[arg1]+arg2]←word_3AE2 | D |
| 0x19 | 0x3D3D | ✅ | `op_19` — 複製 game_state[src]→game_state[dst] | D |
| 0x1A | 0x3D5A | ✅ | `op_1A` — game_state[arg]←立即數（1 或 2 byte） | D |
| 0x1B | 0x3D73 | ❌ | 推測：另一種 game_state 立即數寫入變體（夾在 op_1A 0x3D5A 與 op_1C 0x3D92 之間） | D |
| 0x1C | 0x3D92 | ✅ | `op_1C` — word_3ADF->bytes[arg16]←立即數 byte/word | D |
| 0x1D | 0x4ACC | ✅ | `op_1D` — memcpy 0x700 bytes 到/從 data_D760 | D |
| 0x1E | 0x01B2 | ❌ | ASM 0x01B2 = COM 啟動區域，推測為「重啟腳本/跳轉到初始腳本」 | E |
| 0x1F | 0x4AF6 | ✅ | `op_1F` — 載入 DATA1（stub，未完整） | D |
| 0x20 | 0x0000 | ❌ | 位址 0x0000，無效；可能是「NOP」或「填充/未使用」 | E |
| 0x21 | 0x3DAE | ✅ | `op_21` — word_3AE4←(word_3AE2 & 0xFF) | D |
| 0x22 | 0x3DB7 | ✅ | `op_22` — word_3AE2←word_3AE4 | D |
| 0x23 | 0x3DC0 | ✅ | `op_23` — 遞增 game_state[arg]（帶進位） | D |
| 0x24 | 0x3DD7 | ✅ | `op_24` — 遞增 word_3AE2（帶掩碼） | D |
| 0x25 | 0x3DE5 | ✅ | `inc_byte_word_3AE4` — 遞增 word_3AE4 低位元組 | D |
| 0x26 | 0x3DEC | ✅ | `op_26` — 遞減 game_state[arg] | D |
| 0x27 | 0x3E06 | ✅ | `op_27` — 遞減 word_3AE2（帶掩碼） | D |
| 0x28 | 0x3E14 | ✅ | `op_28` — 遞減 word_3AE4 低位元組 | D |
| 0x29 | 0x3E1B | ❌ | 推測：遞減 word_3AE2 高位元組，或 word_3AE4 高位元組遞減（夾在 op_28 0x3E14 與 op_2A 0x3E36 之間） | D |
| 0x2A | 0x3E36 | ✅ | `op_2A` — word_3AE2 左移1位（SHL） | D |
| 0x2B | 0x3E45 | ✅ | `op_2B` — word_3AE4 低位元組左移1位 | D |
| 0x2C | 0x3E4C | ❌ | 推測：word_3AE4 高位元組左移1位，或 word_3AE2/word_3AE4 右移（夾在 op_2B 0x3E45 與 op_right_shift 0x3E67 之間） | D |
| 0x2D | 0x3E67 | ✅ | `op_right_shift` — word_3AE2 右移1位（SHR） | D |
| 0x2E | 0x3E6E | ✅ | `op_2E` — word_3AE4 低位元組右移1位 | D |
| 0x2F | 0x3E75 | ✅ | `op_2F` — 進位旗標右移＋word_3AE2 加 game_state[arg]（RCR+加法） | D |
| 0x30 | 0x3E9D | ✅ | `op_30` — 進位旗標右移＋word_3AE2 加立即數 | D |
| 0x31 | 0x3EC1 | ✅ | `op_31` — 進位旗標右移＋word_3AE2 減 game_state[arg] | D |
| 0x32 | 0x3EEB | ✅ | `op_32` — 進位旗標右移＋word_3AE2 減立即數 | D |
| 0x33 | 0x3F11 | ✅ | `op_33` — 乘法（word_3AE2 × game_state[arg]），結果存 game_state[55-58] | D |
| 0x34 | 0x3F4D | ✅ | `op_34` — 乘法（word_3AE2 × 立即數） | D |
| 0x35 | 0x3F66 | ✅ | `op_35` — 除法（game_state[arg] ÷ word_3AE2） | D |
| 0x36 | 0x3F8C | ✅ | `op_36` — 除法（word_3AE2 ÷ 立即數） | D |
| 0x37 | 0x3FAD | ❌ | 推測：乘除法變體或 modulo 運算（夾在 op_36 0x3F8C 與 op_38 0x3FBC 之間）；也可能是「word_3AE2 % game_state[arg]」 | D |
| 0x38 | 0x3FBC | ✅ | `op_38` — word_3AE2 AND 立即數/game_state[arg] | D |
| 0x39 | 0x3FD4 | ✅ | `op_39` — word_3AE2 OR game_state[arg] | D |
| 0x3A | 0x3FEA | ✅ | `op_3A` — word_3AE2 OR 立即數 | D |
| 0x3B | 0x4002 | ✅ | `op_3B` — word_3AE2 XOR game_state[arg] | D |
| 0x3C | 0x4018 | ✅ | `op_3C` — word_3AE2 XOR 立即數 | D |
| 0x3D | 0x4030 | ✅ | `op_3D` — 比較 word_3AE2 vs game_state[arg]，設旗標 | D |
| 0x3E | 0x4051 | ✅ | `op_3E` — 比較 word_3AE2 vs 立即數，設旗標 | D |
| 0x3F | 0x4067 | ✅ | `op_3F` — 比較 word_3AE4 vs game_state[arg]，設旗標 | D |
| 0x40 | 0x4074 | ✅ | `op_40` — 比較 word_3AE4 vs 立即數，設旗標 | D |
| 0x41 | 0x407C | ✅ | `op_41` — JC：若 CF=0 則跳轉到立即數位址 | D |
| 0x42 | 0x4085 | ✅ | `op_42` — JNC：若 CF≠0 則跳轉 | D |
| 0x43 | 0x408E | ✅ | `op_check_flags_jmp` — 若 (flags & 0x41)==1 則跳轉 | D |
| 0x44 | 0x4099 | ✅ | `op_jz` — JZ：若 ZF≠0 則跳轉 | D |
| 0x45 | 0x40A3 | ✅ | `op_jnz` — JNZ：若 ZF=0 則跳轉 | D |
| 0x46 | 0x40AF | ✅ | `op_js` — JS：若 SF≠0 則跳轉 | D |
| 0x47 | 0x40B8 | ✅ | `op_47` — JNS：若 SF=0 則跳轉 | D |
| 0x48 | 0x40ED | ✅ | `op_48` — 若 game_state[arg] < 0x80，設 0x80 bit 並設 sign flag | D |
| 0x49 | 0x4106 | ✅ | `loop` — LOOP：word_3AE4-- 若非 0xFF 則跳轉 | D |
| 0x4A | 0x4113 | ✅ | `op_4A` — word_3AE4++ 若 != arg 則跳轉 | D |
| 0x4B | 0x4122 | ✅ | `op_stc` — STC：設定 carry flag | D |
| 0x4C | 0x412A | ✅ | `op_clc` — CLC：清除 carry flag | D |
| 0x4D | 0x4132 | ✅ | `op_prng` — 偽隨機數產生器→word_3AE2 | D |
| 0x4E | 0x414B | ✅ | `op_4E` — 設定 game_state 某 bit（OR bitmask） | D |
| 0x4F | 0x4155 | ✅ | `op_4F` — 清除 game_state 某 bit（AND ~bitmask） | D |
| 0x50 | 0x4161 | ✅ | `op_50` — TEST game_state 某 bit，設旗標 | D |
| 0x51 | 0x418B | ✅ | `op_51` — 在 word_3ADF[arg] 中找最大值→word_3AE2 | D |
| 0x52 | 0x41B9 | ✅ | `op_52` — JMP（無條件跳轉，不存返回位址） | D |
| 0x53 | 0x41C0 | ✅ | `op_53` — CALL（跳轉並推入返回位址） | D |
| 0x54 | 0x41E1 | ✅ | `op_54` — RET（彈出返回位址並跳轉） | D |
| 0x55 | 0x41E5 | ✅ | `op_peek_and_pop` — peek word, pop byte→word_3AE2 | D |
| 0x56 | 0x41FD | ✅ | `op_56` — 推入 word_3AE2（byte 或 word 模式） | D |
| 0x57 | 0x4215 | ✅ | `op_57` — 載入新資源並切換腳本 | D |
| 0x58 | 0x4239 | ✅ | `op_58` — 載入 tag 資源，切換腳本執行 | D |
| 0x59 | 0x41C8 | ✅ | `op_59` — 從堆疊恢復資源索引並彈出返回位址 | D |
| 0x5A | 0x3AEE | ✅ | `op_5A` — 結束腳本（恢復堆疊/資源狀態，engine 退出） | D |
| 0x5B | 0x427A | ✅ | `op_5B_unused` — 取 game_state[0,1] 座標，get_map_tile_data | D |
| 0x5C | 0x4295 | ✅ | `op_5C` (op_for_call) — 對隊伍每個角色執行腳本 | D |
| 0x5D | 0x42D8 | ✅ | `get_character_data` — 從角色資料讀取屬性→word_3AE2 | D |
| 0x5E | 0x4322 | ✅ | `set_character_data` — 將 word_3AE2 寫入角色屬性 | D |
| 0x5F | 0x4372 | ✅ | `op_or_char_data` (op_or_with_char_data) — 角色屬性 OR bitmask | D |
| 0x60 | 0x438B | ✅ | `op_and_char_data` — 角色屬性 AND ~bitmask | D |
| 0x61 | 0x43A6 | ✅ | `test_player_property` — TEST 角色屬性某 bit，設旗標 | D |
| 0x62 | 0x43BF | ✅ | `op_scan_for_char` — 掃描角色找屬性值 >= arg，設旗標 | D |
| 0x63 | 0x43F7 | ✅ | `op_set_char_data_word` — 設定角色 word 屬性（data_CA4C） | D |
| 0x64 | 0x446E | ❌ | 推測：另一種角色屬性操作（夾在 op_63 0x43F7 與 op_65 0x44B8 之間）；可能是 get_char_data_word | D |
| 0x65 | 0x44B8 | ❌ | 推測：角色屬性相關（0x44B8 在 0x446E 之後，0x44CB 之前）；可能是清除/測試 data_CA4C | D |
| 0x66 | 0x40C1 | ✅ | `op_66` — game_state[arg] 載入，設 ZF/SF 旗標（類似 TEST） | D |
| 0x67 | 0x44CB | ❌ | 推測：角色屬性/data_CA4C 相關操作 | D |
| 0x68 | 0x450A | ❌ | 推測：地圖/座標相關（0x450A 在 0x44CB 後、op_69 0x453F 前） | D |
| 0x69 | 0x453F | ✅ | `op_69` — 寫入角色 data_CA4C 資料 | D |
| 0x6A | 0x4573 | ✅ | `op_6A` — 檢查 game_state[0,1] 是否在 4-byte 矩形範圍內，設 sign flag | D |
| 0x6B | 0x45A1 | ❌ | 推測：另一種座標範圍檢查或移動指令（夾在 op_6A 0x4573 與 op_6C 0x45A8 之間） | D |
| 0x6C | 0x45A8 | ✅ | `op_6C` — 依 game_state[3] 方向移動玩家 | D |
| 0x6D | 0x45F0 | ✅ | `op_6D` — 處理小地圖命令（process_minimap_commands） | A |
| 0x6E | 0x45FA | ❌ | 推測：小地圖相關（夾在 op_6D 0x45F0 與 op_6F 0x4607 之間）；可能是小地圖輸入或繪製 | A |
| 0x6F | 0x4607 | ✅ | `op_6F` — 移動玩家並儲存地圖 tile 資料到 game_state | D |
| 0x70 | 0x4632 | ❌ | 推測：地圖/關卡腳本相關（夾在 op_6F 0x4607 與 op_71 0x465B 之間）；可能是 run_level_script 變體 | D |
| 0x71 | 0x465B | ✅ | `op_71` — 執行關卡腳本（地圖 tile 觸發） | D |
| 0x72 | 0x46B6 | ✅ | `op_72_unused` — 地圖 tile 腳本執行（多處未完整） | D |
| 0x73 | 0x47B7 | ✅ | `op_73` — game_state[0x3E]←game_state[0x3F] | D |
| 0x74 | 0x47C0 | ✅ | `op_74` — 繪製矩形框（draw_rectangle，4 byte 引數 x,y,w,h） | A |
| 0x75 | 0x47D1 | ✅ | `op_75` — 全螢幕重繪（ui_draw_full） | A |
| 0x76 | 0x47D9 | ✅ | `op_76` — 繪製圖案（draw_pattern & draw_rect） | A |
| 0x77 | 0x47E3 | ✅ | `op_77` — draw_pattern 後 set_msg（繪製圖案並輸出訊息） | C |
| 0x78 | 0x47EC | ✅ | `set_msg` — 從腳本流提取壓縮字串並呼叫 handle_byte_callback | C |
| 0x79 | 0x47FA | ❌ | **中文化關鍵**：ASM 0x47FA 夾在 set_msg(0x47EC) 與 op_7A(0x4801) 之間，推測為另一種 set_msg 變體或帶參數的字串顯示（可能是帶顏色/位置參數的字串輸出） | C |
| 0x7A | 0x4801 | ✅ | `op_7A` — 從 word_3ADF 資源流提取壓縮字串 | C |
| 0x7B | 0x482D | ✅ | `read_header_bytes` — 設定 UI header（set_ui_header） | C |
| 0x7C | 0x4817 | ✅ | `op_7C` — 隨機遭遇，設定 UI header（set_ui_header） | C |
| 0x7D | 0x483B | ✅ | `op_7D` — 輸出角色名稱（write_character_name） | C |
| 0x7E | 0x4845 | ❌ | **中文化關鍵**：ASM 0x4845 夾在 op_7D(0x483B) 與 op_80(0x487F) 之間（跳過 op_7F 0x486D），推測為帶位置參數的字串輸出，或輸出角色名稱變體 | C |
| 0x7F | 0x486D | ❌ | **中文化關鍵**：ASM 0x486D 在 0x4845 之後、advance_cursor(0x487F) 之前，推測為 append_spaces 或設定游標位置的變體 | C |
| 0x80 | 0x487F | ✅ | `advance_cursor` — 推進游標並填補空格（1 byte 引數） | C |
| 0x81 | 0x48C5 | ✅ | `op_81` — word_3AE2 轉數字字串並輸出 | C |
| 0x82 | 0x48D2 | ✅ | `op_82` — 從 game_state 4 bytes 讀取大數並輸出 | C |
| 0x83 | 0x48EE | ✅ | `op_83` — 直接輸出 word_3AE2 的 byte 值 | C |
| 0x84 | 0x4907 | ✅ | `op_84` — 分配 game_memory | D |
| 0x85 | 0x4920 | ✅ | `op_85` — 釋放資源（resource_index_release） | D |
| 0x86 | 0x493E | ✅ | `load_word3AE2_resource` — 載入資源→word_3AE2 索引 | D |
| 0x87 | 0x4955 | ✅ | `op_87` — 將資源寫入磁碟 | D |
| 0x88 | 0x496D | ✅ | `op_wait_escape` — 等待 Escape 鍵後繼續 | C |
| 0x89 | 0x4977 | ✅ | `wait_event` — 等待事件（2 byte flag + 按鍵對照表，直到 0xFF） | C |
| 0x8A | 0x498E | ✅ | `op_8A` — 觸發隨機遭遇（trigger_random_encounter） | D |
| 0x8B | 0x499B | ✅ | `op_8B` — 刷新視口（refresh_viewport） | A |
| 0x8C | 0x49A5 | ✅ | `prompt_no_yes` — 顯示 (N)o/(Y)es 提示，等待按鍵，設旗標 | C |
| 0x8D | 0x49D3 | ✅ | `op_read_string` — 讀取鍵盤字串輸入（read_string_input） | C |
| 0x8E | 0x0000 | ❌ | 位址 0x0000，無效；可能是「NOP」或「未使用」 | E |
| 0x8F | 0x49DD | ❌ | **中文化關鍵**：ASM 0x49DD 夾在 op_read_string(0x49D3) 與 op_sound_effect(0x49E7) 之間，推測為字串讀取變體（可能是帶不同 flag 的 read_string，或讀取後立即顯示） | C |
| 0x90 | 0x49E7 | ✅ | `op_sound_effect` — 播放音效（1 byte 引數） | B |
| 0x91 | 0x49F3 | ✅ | `op_91` — 繪製玩家狀態面板（draw_player_status_panel） | A |
| 0x92 | 0x49FD | ✅ | `op_92` — 繪製狀態面板並等待 UI 互動（含計時器輪詢） | A |
| 0x93 | 0x4A67 | ✅ | `op_93` — 推入 word_3AE4 低位元組到堆疊 | D |
| 0x94 | 0x4A6D | ✅ | `op_94` — 從堆疊彈出→word_3AE4 低位元組 | D |
| 0x95 | 0x4894 | ✅ | `op_95` — 設定游標 x,y 位置（相對 draw_rect 偏移） | C |
| 0x96 | 0x48B5 | ✅ | `op_96` — 輸出字串並填補到欄位寬度（append_spaces） | C |
| 0x97 | 0x42FB | ✅ | `op_97` — 從角色資料（偏移+word_3AE4）載入→word_3AE2 | D |
| 0x98 | 0x4348 | ✅ | `op_98` — word_3AE2 寫入角色資料（偏移+word_3AE4） | D |
| 0x99 | 0x40E7 | ✅ | `op_99` — TEST word_3AE2 自身，設 ZF/SF 旗標 | D |
| 0x9A | 0x3C42 | ✅ | `op_9A` — game_state[arg]←0xFF（清除 flag） | D |
| 0x9B | 0x416B | ✅ | `op_9B` — 設定 game_state 某 bit（OR bitmask） | D |
| 0x9C | 0x4175 | ❌ | 推測：清除 game_state 某 bit（AND ~bitmask），夾在 op_9B(OR) 與 op_9D(TEST) 之間，與 op_4F 類似 | D |
| 0x9D | 0x4181 | ✅ | `op_9D` — TEST game_state 某 bit，設旗標 | D |
| 0x9E | 0x492D | ✅ | `op_9E` — 取得資源大小→word_3AE2 | D |
| 0x9F | 0x4AF0 | ❌ | 推測：另一種資源操作（ASM 0x4AF0 在 0x4ACC/op_1D 之後，0x4AF6/op_1F 之前），可能是 DATA1 讀取相關 | D |
| 0xA0–0xFF | 各異 | ❌ | **推測為原始碼殘留/非有效 opcode**（見下方「高位索引說明」） | E |

### 高位索引（0xA0–0xFF）說明

這 96 個 NULL 項的 ASM 位址呈現典型的 x86 機器碼位元組對（如 `0x8A06`、`0xE80E`、`0xA23A`），
與低位索引的有效程式碼段位址（0x3B00–0x4AF6）完全不同。
推測這些是 targets[] 陣列的「溢位」或「填充」區域，對應到原始 DOS COM 檔案的
資料段（非程式碼段）位址。**不建議嘗試實作這些 opcode**。

---

## 未實作 opcode 判讀

### 有效 NULL opcode（0x02–0x9F 範圍，共 22 個）

#### 0x02（ASM: 0x3B1F）
- **位置**：夾在 `set_word_mode`（0x3B18）與 `op_04`（0x3B2A）之間
- **推測語意**：`set_byte_mode` 的另一種模式切換，或是清除 word_3AE2 的高位元組
  （與 0x3B0E 的 set_byte_mode 不同，可能是清除 word_3AE2 而非 byte_3AE1）
- **分類**：D
- **dragon.asm 證據**：0x3B1F 介於 0x3B18 和 0x3B2A 之間，但 dragon.asm 未涵蓋該範圍

#### 0x1B（ASM: 0x3D73）
- **位置**：夾在 `op_1A`（0x3D5A，game_state←立即數）與 `op_1C`（0x3D92，word_3ADF←立即數）之間
- **推測語意**：game_state 立即數寫入的另一種尋址模式（可能是雙 byte 立即數寫入 game_state 的變體）
- **分類**：D
- **dragon.asm 證據**：未涵蓋

#### 0x1E（ASM: 0x01B2）
- **位置**：ASM 0x01B2 對應到 COM 啟動區域（dragon.asm 中 0x01B2 是 `loc_1B5` 的前一行 `mov dx, offset empty_string`）
- **推測語意**：可能是「跳轉/呼叫到初始化程式碼」或「重設引擎」；也可能是 targets[] 的 stub 位址誤植
- **分類**：E（特殊 — ASM 位址指向非腳本引擎區域）
- **dragon.asm 證據**：dragon.asm 第 105–106 行，對應到 COM 退出/空字串區域

#### 0x20（ASM: 0x0000）
- **位置**：位址 0x0000（無效）
- **推測語意**：NOP、未使用、或填充
- **分類**：E

#### 0x29（ASM: 0x3E1B）
- **位置**：夾在 `op_28`（0x3E14，DEC word_3AE4 低位元組）與 `op_2A`（0x3E36，SHL word_3AE2）之間
- **推測語意**：SHL word_3AE4 低位元組，或另一種遞減操作（如 DEC word_3AE2 高位元組）
- **分類**：D
- **相鄰模式**：0x28=DEC 3AE4-lo，0x29=?，0x2A=SHL 3AE2，0x2B=SHL 3AE4-lo

#### 0x2C（ASM: 0x3E4C）
- **位置**：夾在 `op_2B`（0x3E45，SHL word_3AE4 lo）與 `op_right_shift`（0x3E67，SHR word_3AE2）之間
- **推測語意**：SHR word_3AE4 低位元組（對應 0x2B 的 SHL）；或 word_3AE2 的另一種移位操作
- **分類**：D
- **相鄰模式**：0x2B=SHL 3AE4-lo，0x2C=?，0x2D=SHR 3AE2，0x2E=SHR 3AE4-lo

#### 0x37（ASM: 0x3FAD）
- **位置**：夾在 `op_36`（0x3F8C，除法）與 `op_38`（0x3FBC，AND）之間
- **推測語意**：乘除法變體（modulo 運算 word_3AE2 % arg？），或另一種除法形式
- **分類**：D
- **相鄰模式**：0x35=DIV game_state，0x36=DIV imm，0x37=?，0x38=AND，形成算術群組

#### 0x64（ASM: 0x446E）
- **位置**：夾在 `op_set_char_data_word`（0x43F7）與 NULL 0x65（0x44B8）之間
- **推測語意**：讀取角色 data_CA4C（get_char_data_word），與 op_63（write）互補
- **分類**：D

#### 0x65（ASM: 0x44B8）
- **位置**：夾在 0x64（0x446E）與 op_66（0x44CB→實為0x40C1 的 op_66，注意 0x44CB 是另一 NULL）之間
- **推測語意**：角色 data_CA4C 的清除或測試
- **分類**：D

#### 0x67（ASM: 0x44CB）
- **位置**：緊接 op_66（0x40C1，但表中 op_66 是 0x44CB 的前一項）之後
- **推測語意**：角色屬性相關，可能是清除 data_CA4C 某 bit
- **分類**：D

#### 0x68（ASM: 0x450A）
- **位置**：夾在 0x67（0x44CB）與 op_69（0x453F）之間
- **推測語意**：角色資料操作或地圖座標相關前置操作
- **分類**：D

#### 0x6B（ASM: 0x45A1）
- **位置**：夾在 `op_6A`（0x4573，矩形範圍檢查）與 `op_6C`（0x45A8，移動玩家）之間
- **推測語意**：另一種座標範圍檢查，或移動玩家的前置動作
- **分類**：D

#### 0x6E（ASM: 0x45FA）
- **位置**：夾在 `op_6D`（0x45F0，小地圖命令）與 `op_6F`（0x4607，移動玩家）之間
- **推測語意**：小地圖繪製或輸入相關的輔助指令
- **分類**：A

#### 0x70（ASM: 0x4632）
- **位置**：夾在 `op_6F`（0x4607）與 `op_71`（0x465B）之間
- **推測語意**：關卡腳本執行相關指令（`run_level_script` 變體，或初始化關卡資料）
- **分類**：D

#### 0x79（ASM: 0x47FA）⭐ 中文化關鍵
- **位置**：夾在 `set_msg`（0x47EC，字串輸出）與 `op_7A`（0x4801，資源字串輸出）之間
- **推測語意**：`set_msg` 的帶參數變體，可能是帶顏色、位置或旗標的字串輸出指令
  例如：`draw_string_at_position` 或 `set_msg_with_flags`
- **分類**：C
- **中文化影響**：若此 opcode 控制字串的顯示方式，未實作可能導致部分對話/UI 文字無法顯示

#### 0x7E（ASM: 0x4845）⭐ 中文化關鍵
- **位置**：夾在 `op_7D`（0x483B，角色名稱輸出）與 advance_cursor（0x487F）之間（跳過 0x7F）
- **推測語意**：帶格式的字串輸出，或輸出角色名稱的另一種格式（如補空格對齊）；
  也可能是 `append_spaces` 的前置版本
- **分類**：C
- **中文化影響**：角色名稱顯示路徑，可能影響名稱對齊或多語言渲染

#### 0x7F（ASM: 0x486D）⭐ 中文化關鍵
- **位置**：夾在 0x7E（0x4845）與 `advance_cursor`（0x487F）之間
- **推測語意**：設定游標位置或填補空白的變體（類似 advance_cursor 但不帶換行）；
  或是某種「結束字串輸出並重設狀態」的 opcode
- **分類**：C
- **中文化影響**：游標/文字佈局相關，影響字符位置計算

#### 0x8E（ASM: 0x0000）
- **位置**：位址 0x0000（無效）
- **推測語意**：NOP 或未使用
- **分類**：E

#### 0x8F（ASM: 0x49DD）⭐ 中文化關鍵
- **位置**：夾在 `op_read_string`（0x49D3，鍵盤輸入）與 `op_sound_effect`（0x49E7）之間
- **推測語意**：字串讀取的變體，可能是帶旗標的 `read_string`，或「讀取後立即顯示到 UI」
  的組合版本（相當於 read + display 合一）
- **分類**：C
- **中文化影響**：玩家命名/輸入流程，若未實作可能導致命名功能異常

#### 0x9C（ASM: 0x4175）
- **位置**：夾在 `op_9B`（0x416B，OR bitmask）與 `op_9D`（0x4181，TEST bitmask）之間
- **推測語意**：AND ~bitmask（清除 game_state 某 bit），與 op_4F 功能相同但操作 game_state 而非字節
- **分類**：D

#### 0x9F（ASM: 0x4AF0）
- **位置**：夾在 op_1D（0x4ACC）與 op_1F（0x4AF6）之間
- **推測語意**：DATA1 資源操作相關（可能是讀取 DATA1 的另一種方式）
- **分類**：D

---

## 中文化關鍵 opcode 優先序清單

以下按「對中文化進度的影響程度」排序。已實作的 opcode 作為背景資訊列出，
未實作的標記 ❌。

### 第一優先（字串顯示路徑核心）

| 優先 | 索引 | 狀態 | 說明 |
|------|------|------|------|
| 1 | **0x79** | ❌ | `set_msg` 帶參數變體 — 字串輸出主路徑上的缺失環節 |
| 2 | **0x7E** | ❌ | 字串輸出或角色名稱格式化 — 角色顯示路徑 |
| 3 | **0x7F** | ❌ | 游標/字串結束控制 — 影響文字佈局 |
| 4 | **0x8F** | ❌ | read_string 變體 — 玩家命名輸入流程 |

### 第二優先（已實作，確保功能完整）

| 優先 | 索引 | 狀態 | C 函式 | 說明 |
|------|------|------|--------|------|
| 5 | 0x77 | ✅ | `op_77` | 繪製圖案後輸出字串，對話框常用 |
| 6 | 0x78 | ✅ | `set_msg` | 主要字串輸出 opcode |
| 7 | 0x7B | ✅ | `read_header_bytes` | UI 標題設定 |
| 8 | 0x7C | ✅ | `op_7C` | 遭遇 UI 標題 |
| 9 | 0x7D | ✅ | `op_7D` | 角色名稱輸出 |
| 10 | 0x80 | ✅ | `advance_cursor` | 游標推進 |
| 11 | 0x81 | ✅ | `op_81` | 數字輸出 |
| 12 | 0x83 | ✅ | `op_83` | 原始 byte 輸出 |
| 13 | 0x88 | ✅ | `op_wait_escape` | 等待玩家確認 |
| 14 | 0x89 | ✅ | `wait_event` | 互動等待 |
| 15 | 0x8C | ✅ | `prompt_no_yes` | Y/N 選擇提示 |
| 16 | 0x8D | ✅ | `op_read_string` | 鍵盤輸入 |
| 17 | 0x95 | ✅ | `op_95` | 游標位置設定 |
| 18 | 0x96 | ✅ | `op_96` | 填補空格 |

---

## 無法判讀清單

以下 opcode 的 ASM 位址無效或超出反組譯範圍，無法從 dragon.asm 判讀行為：

| 索引 | ASM 位址 | 備註 |
|------|----------|------|
| 0x1E | 0x01B2 | 指向 COM 啟動區，非腳本引擎 |
| 0x20 | 0x0000 | 位址 0x0000，無效 |
| 0x8E | 0x0000 | 位址 0x0000，無效 |
| 0xA0–0xFF（共 96 個） | 各異 | ASM 位址呈現 x86 機器碼位元組特徵，非有效腳本引擎入口 |

---

## 附錄：虛擬 CPU 暫存器一覽

engine.c 中的虛擬 CPU 狀態變數：

| 變數 | 用途 |
|------|------|
| `byte_3AE1` | 模式旗標：0=8-bit 模式，0xFF=16-bit 模式 |
| `word_3AE2` | 主運算暫存器（類 AX） |
| `word_3AE4` | 次要暫存器（類 CX，迴圈計數器/偏移） |
| `word_3AE6` | 旗標暫存器（CF bit0、ZF bit6、SF bit7） |
| `word_3AE8` | 當前資源索引 |
| `word_3AEA` | 資源索引副本 |
| `word_3ADB` | 用於腳本跳轉的暫存位址 |
| `running_script` | 指向當前資源的指標 |
| `word_3ADF` | 資料資源指標 |
| `cpu.pc` | 程式計數器 |
| `cpu.base_pc` | 腳本基底位址 |
| `cpu.sp` | 堆疊指標 |
| `cpu.stack[]` | 256-byte 堆疊 |
| `game_state.unknown[]` | 遊戲狀態陣列（約 256 bytes） |

---

## 引用來源

- `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/src/lib/engine.c`：主要實作，targets[] 第 583 行
- `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/docs/dragon.asm`：部分反組譯（3721 行，涵蓋 0x100–0x5C7B 部分區段）
- `/home/anr2/tmp/longcat/opendw/doc/script.md`：opcode 文字說明（稀疏）
- `/home/anr2/tmp/longcat/opendw/doc/keypress.txt`：按鍵處理流程（op_89 相關）
