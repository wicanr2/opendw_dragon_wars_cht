# 進入遊戲後的畫面 — 第一人稱 viewport(與原版一致的計畫)

> 目標:**進入遊戲後的畫面也要跟原版一樣**。本檔記錄原版 in-game 畫面的渲染架構
> 與 remake 的 port 計畫 + golden 對拍策略。**誠實現況**:目前 remake 進入地圖顯示的
> 是「tile 俯視彩色格」——那是 tile 資料的**自製視覺化 / 導覽輔助,不是原版畫面**。

## 原版 in-game 畫面 = 第一人稱 viewport + UI 面板

原版進入遊戲後,主畫面是一個 **Bard's Tale 式的偽 3D 第一人稱 viewport**(透視牆面)
加上周邊 UI(隊伍狀態、訊息列、指令)。viewport 內容依玩家**位置 + 朝向**,把前方數格的
牆/門/通道以**run-length 向量描線**畫成透視圖。

## 原版渲染架構(opendw,src/lib/ui.c)

- `init_viewport_for_map()`(ui.c:1187):初始化;`data_6820 = com_extract(0x6820, 4+0xD*0x18)`
  = viewport 繪製模板(從 DRAGON.COM 取)。
- `viewports[]`(ui.c:137):4 個象限的 `viewport_data{xpos,ypos,runlength,numruns,...}`。
- `decode_viewport_data(data, vp)`(ui.c:1003):讀 run-length 串,依 `byte_104E` 的
  象限旗標(x 正負 / y 翻轉)分派到:
  - `process_quadrant`(D88)
  - `draw_viewport_word_mode`(235)
  - `draw_viewport_neg_x` / `draw_viewport_neg_x_alt`(383/314)
  - `draw_viewport_flip_y`(442)
  這些把向量描線畫進 `viewport_memory`。
- `viewport_memory` 由關卡的**牆/地面元件資源**填入(`cache_level_components` 在
  `read_level_metadata` 取出 ground/wall/other 元件的資源 index → `data_56E5`)。
- 牆面 sprite:`draw_sprite_to_viewport`(engine.c:379)。
- minimap(`?` 鍵的平面地圖)走 `draw_minimap_from_data6820` → 同一 `decode_viewport_data`。

## 與 tile 資料的關係(已掌握)

- 每格 `word_11C6`(2 byte)= 牆屬性(低 nibble &0xF 用於 move/邊界;決定四面有無牆/門)。
  這正是 viewport 要用的「前方有沒有牆」資訊。
- 玩家朝向(N/E/S/W)+ 前方數格的 `word_11C6` → 決定畫哪些牆面 run-length。

## Remake port 計畫(分階段,每階段 golden 對拍 opendw)

1. **資料層**:`data_6820` 模板(可從 DRAGON.COM 抽成 bundle asset,如 dw8x8.bin 那樣)
   + `viewports[]` 表 + 關卡 wall/ground 元件資源 index(已能從 .lvl 的 component lists 取)。
2. **描線核心**:port `decode_viewport_data` + `process_quadrant` + 4 個 `draw_viewport_*`
   到 remake 的 framebuffer(320×200 indexed)。
3. **組景**:依玩家 (x,y,facing) + 前方 tile 的 `word_11C6` 選 run-length,組成一幀。
4. **UI 面板**:viewport 視窗位置 + 隊伍/訊息列框(對齊原版 `draw_rect` x=1,y=8,w=39,h=184)。
5. **golden 對拍**:在 opendw 加 hook,對固定 (level, x, y, facing) dump viewport framebuffer;
   remake 渲染同條件,`cmp` byte-for-byte(比照 `verify_scene_golden.sh`)。

## 現況與下一步

- ✅ 片頭/場景圖、sprite 渲染**已對拍原版一致**(`verify_scene_golden.sh`、sprite byte-for-byte)。
- ✅ 真實地圖 tile / 事件腳本已抽出、事件文字 emit 與攻略吻合。
- ⏳ **第一人稱 viewport 尚未 port** —— 這是讓「進入遊戲後與原版一致」的關鍵且最大的一塊。
  目前地圖俯視圖為占位輔助,非原版畫面。
- 待 VM 補 `op_58`(跨資源 script call)等,event/互動才完整;viewport 與其平行進行。

> 這塊是獨立的大型子系統(run-length 偽 3D 渲染),建議當作專注的多步任務推進,
> 每階段以 golden 對拍確保與原版逐像素一致。

---

## 完整 port 規格(已逆向到 byte 級,turnkey)

### viewport_memory(畫布)
- 大小 `viewport_mem_sz = 10880` bytes(ui.c:118),**packed 2 像素/byte**(高 nibble=左像素色、低 nibble=右像素色;見 `ui_update_viewport`)。
- 列位移:`get_offset(pos)`(offsets.c:46)→ byte offset;列間 stride 由 `word_1055` 決定(= ±`word_1053`,依象限 y 翻轉旗標 `byte_104E&0x40`)。

### decode_viewport_data(data, vp)(ui.c:1003,dispatch)
1. `vp->runlength = *data++; vp->numruns = *data++;`
2. 讀 1 byte 有號 → 依 `byte_104E&0x80` 取負 → `vp->xpos +=`;`runlength&=0x7F`。
3. 讀 1 byte 有號 → 依 `byte_104E&0x40` 取負 → `vp->ypos +=`。
4. 算象限 `bx`:`(xpos&1)<<1` | (xpos<0 ? 4:0) | (byte_104E&0x80 ? 8:0)。
5. `word_1055 = (byte_104E&0x40)? -word_1053 : word_1053`。
6. switch(bx):0→`process_quadrant`、2→`draw_viewport_word_mode`、4→`draw_viewport_neg_x`、6→`draw_viewport_neg_x_alt`、8→`draw_viewport_flip_y`。

### 描線原語(run-length nibble blit)
- **byte 模式**(`process_quadrant`,ui.c:188):`newx = xpos>>1(算術)`;裁切 `word_104A=runlength`(依 `newx - word_1053`);
  `p = data + get_offset(ypos) + newx`;`si = vp->data + 4`;
  迴圈 numruns 列 × word_104A byte:`*p = (*p & and_table[*si]) | or_table[*si]; p++; si++;` 每列後 `offset += word_1055; si += runlength - word_104A`。
- **word 模式**(`draw_viewport_word_mode`/`neg_x`/`neg_x_alt`/`flip_y`,ui.c:235/383/314/442):同形,但用 16-bit 讀寫 + `and_table_B452[256]`/`or_table_B652[256]`(uint16,tables.c:178/214);neg_x 系列 x 方向遞減、flip_y 系列 y 方向翻轉。
- **遮罩表**(tables.c):`and_table[256]`/`or_table[256]`(byte,@106/142)、`and_table_B452`/`or_table_B652`(uint16,@178/214)。這套 nibble 遮罩 = 與已驗證的 sprite blit 同原理(透明 nibble 不蓋)。**port 時直接複製 tables.c 的 4 張表**(或從 DRAGON.COM 0xB452/0xB652 抽 word 表)。

### 模板資料(已抽成資產,見 assets/bundle/viewport/)
- 主視圖 4 象限:`vp0..vp3.bin`;minimap:`data6820.bin`/`minimap.bin`。各以 `runlength,numruns` 開頭 + run-length nibble 資料。

### 組景(update_viewport,ui.c:1081)
- 依玩家 (x,y,facing) + 前方 tile 的 `word_11C6` 牆屬性,選 viewport 模板/wall 元件填 viewport_memory(4 象限各跑一次 decode)。`viewport_metadata` 提供各象限 xpos/ypos。

### 上螢幕(ui_update_viewport,ui.c:517)
- viewport_memory(2px/byte)→ framebuffer:每 byte 拆 hi/lo nibble = 2 個像素色(0–15),寫到 `get_line_offset(line_num)+0x10`,共 `viewport_height × viewport_width` byte。

### golden 對拍策略(verify-first)
1. opendw 小 harness:設 byte_104E/word_1053/viewport_metadata + 載一個模板(vp/.bin)→ `decode_viewport_data` → dump viewport_memory(10880B)。
2. remake port 同條件 → `cmp` byte-for-byte。
3. 再對 `ui_update_viewport` 的 framebuffer 輸出對拍(比照 `verify_scene_golden.sh`)。

### 執行順序(phase 2+)
1. 複製 4 張遮罩表 + `get_offset` + `viewport_mem` 緩衝。
2. port `process_quadrant`(byte 模式)→ golden(decode 單模板)。
3. port 3 個 word 模式 draw 變體 → golden。
4. `ui_update_viewport` blit → framebuffer golden。
5. `update_viewport` 組景(tile→模板)→ 與 opendw 實機畫面對拍。

> 結論:in-game 第一人稱 viewport 已**完整逆向、byte 級可執行**;剩下是機械式翻譯
> 上列函式/表 + 逐階段 golden。資料層(phase 1)已入庫。
