# VIEWPORT_COMPOSE — 第一人稱 viewport「組景」深度逆向

> 本檔聚焦 `refresh_viewport`(engine.c:5618,原版 0x51B0)這一層:**怎麼從玩家
> (x, y, facing) + 地圖 tile 的牆屬性,選出該畫哪些牆/地面元件 sprite,並算出每個
> sprite 在 viewport 上的位置與距離槽**。描線原語(run-length nibble blit)、上螢幕
> (`ui_update_viewport`)、4 象限靜態框架(`update_viewport`)已在 `VIEWPORT.md` 記過,
> 這裡只在交界處引用、不重複展開。
>
> **純分析文件,未改任何 code。** 不確定處標「待確認」+ 最佳推測 + 該看哪段釐清。
>
> 來源(只讀):`opendw/src/lib/{engine.c, ui.c, tables.c, offsets.c}`。

---

## 0. 名詞與全域變數對照(canonical terms)

| 名稱 | 位址 | 角色 |
|---|---|---|
| `game_state[0]` | — | 玩家 x(map col) |
| `game_state[1]` | — | 玩家 y(map row) |
| `game_state[3]` | — | **朝向 facing**:0=N、1=E、2=S、3=W(由 `data_5303` 索引推得) |
| `data_5303[4]` | 0x5303 | FOV 走訪起始索引 `{0x16, 0x2E, 0x46, 0x5E}`(進 `data_530B` 前要 −8,見 §1) |
| `data_530B[96]` | 0x530B | **FOV 步進表**:每朝向 12 個 (dy, dx) pair,逆序走訪 |
| `word_11C6` | 0x11C6 | 當前取樣 tile 的**牆屬性 word**(由 `get_map_tile_data` 填) |
| `word_11CA` | 0x11CA | `move_player_on_map` 旋轉後的「相對玩家朝向」牆屬性(寫進 `data_5A56`) |
| `data_5A56[128]` | 0x5A56 | 12 槽 FOV 取樣結果。`[di]`=other/wall nibble、`[di+0xC]`=ground nibble |
| `data_56E5[128]` | 0x56E5 | 關卡元件「資源索引 by type」:[0..3]=sky/wall 類、[4..6]=ground、[7..]=other(由 `cache_level_components` 填) |
| `data_59E4[128]` | 0x59E4 | 已載入的關卡資源指標(`struct resource*`),由 `cache_resources` 填 |
| `data_5897[256]` | 0x5897 | 元件型/旗標 + 資源 index 暫存([bx]=型旗標,[bx+0xF]=已載入 index) |
| `data_56C6[128]` | 0x56C6 | wall/door 元件選擇表([1..]=型、[0xF+1..]=替代型),由 `read_level_metadata` 填 |
| `ground_points[9]` | 0x567F | 9 個地面格的 viewport (x,y) 落點 |
| `sprite_indices[9]` | 0x56B5 | 9 個地面格對應的 sprite slot 偏移 `{18,16,20,12,10,14,6,4,8}` |
| `data_56A3[9]` | 0x56A3 | 地面格 → `data_5A56` 槽的對應(繪製順序) |
| `data_55EF[24]` | 0x55EF | 24 個「other 元件」(牆/門面)對應的 `data_5A56` 槽索引(高 bit=取高 nibble) |
| `data_558F[24]` | 0x558F | 24 個 other 元件的 viewport xpos |
| `data_55BF[24]` | 0x55BF | 24 個 other 元件的 viewport ypos |
| `data_561F[24]` | 0x561F | 24 個 other 元件的 sprite slot 偏移 |
| `data_564F[24]` | 0x564F | 24 個 other 元件的 `byte_104E` 象限旗標(+ bit0=用替代型表) |
| `data_575C[4]` | 0x575C | 天空/地板兩色交替 pattern `{0x4040, 0x0404, 0, 0}` |
| `viewport_data{xpos,ypos,runlength,numruns,...,data}` | — | 單次 blit 的位置/run-length 描述(ui.h:33) |

> 命名提醒(對齊 `VIEWPORT.md`):本檔用 **「組景」= compose**(refresh_viewport)、
> **「框架」= frame**(update_viewport 的 4 象限靜態地板/側框)、**「上螢幕」= present**
> (ui_update_viewport)。`facing` 一律 N/E/S/W,不用「方向 0/1/2/3」對外。

---

## Q1. FOV 走訪:`data_5303[facing]` 與 11 格深度迴圈

### 1.1 起始索引換算(關鍵且容易踩雷)

`refresh_viewport` 取 `cpu.si = data_5303[facing]`,值是 `{0x16, 0x2E, 0x46, 0x5E}`。
這些是**相對於 0x5303 的 byte 偏移**;但實際走訪的陣列是 `data_530B`(位址 0x530B)。
兩者差 `0x530B − 0x5303 = 8` bytes。所以進 `data_530B` 的 index 要 **−8**:

| facing | `data_5303` | `data_530B` index | 解讀 |
|---|---|---|---|
| N (0) | 0x16 | 14 | |
| E (1) | 0x2E | 38 | |
| S (2) | 0x46 | 62 | |
| W (3) | 0x5E | 86 | |

> 在 opendw 的 C 移植裡 `data_530B` 是自己的陣列,`si` 仍用 0x16.. 直接索引——
> **這在原版是因為兩個符號在同一段連續記憶體**;移植時若把 `data_530B` 當獨立陣列,
> 必須記得 −8。**待確認**:opendw 是否真的在執行期 −8?從 5633 行 `cpu.si = data_5303[bx]`
> 直接拿來當 `data_530B[cpu.si]` 用、且 5641 行 `data_530B[cpu.si]`——若 `data_530B` 在
> opendw 編譯出來剛好接在 `data_5303` 後面就「碰巧對」,否則應該 −8。port 時**一律用 −8 後的 index**才安全(下面的取樣序列即以 −8 計算,結果自洽:N 是正前方 fan、S 是其鏡像,符合 Bard's Tale 幾何)。

### 1.2 深度迴圈結構(engine.c:5633–5661)

```
si = data_530B_index(facing)        // 見上表
for di in 11,10,...,1,0:            // cpu.di = 0xB; do {...} while(di != 0xFFFF)
    dy = (int8) data_530B[si]        // 先 y delta
    dx = (int8) data_530B[si+1]      // 後 x delta
    sample_y = game_state[1] + dy
    sample_x = game_state[0] + dx
    move_player_on_map(sample_y, sample_x)   // 內部 get_map_tile_data → word_11C6 → 旋轉 → word_11CA
    data_5A56[di]      = word_11CA & 0xFF            // wall/other nibble 來源
    data_5A56[di+0xC]  = (word_11CA>>8) & 0xF7       // ground nibble 來源(清 bit3)
    si -= 2
```

注意:`move_player_on_map` 的參數順序在原版是 `(dl=y, bl=x)`,所以 5648 行
`move_player_on_map(dl, bl)` 其實是 `(y_sample, x_sample)`。

### 1.3 每朝向的取樣序列(相對玩家的 (dx, dy),已解碼)

下表 `(dx, dy)` 是「相對玩家」的格位移;`di` 是寫入 `data_5A56` 的槽(也是後面距離/位置對應的 key)。
dx 正 = 地圖 col 增、dy 正 = 地圖 row 增。

**facing N (0)** — 正前方扇形(玩家面向 −y? 見下方註):
```
di=11 (+1,+0)   di=10 (+1,-1)   di= 9 (+2,+1)   di= 8 (+2,+0)
di= 7 (+2,-1)   di= 6 (+3,+1)   di= 5 (+3,+0)   di= 4 (+3,-1)
di= 3 (+1,+0)   di= 2 (+0,+0)   di= 1 (-1,+0)   di= 0 (+1,-1)
```

**facing E (1)**:
```
di=11 (+0,+1)   di=10 (+1,+1)   di= 9 (-1,+2)   di= 8 (+0,+2)
di= 7 (+1,+2)   di= 6 (-1,+3)   di= 5 (+0,+3)   di= 4 (+1,+3)
di= 3 (+0,+1)   di= 2 (+0,+0)   di= 1 (+0,-1)   di= 0 (+1,+1)
```

**facing S (2)** — N 的鏡像:
```
di=11 (-1,+0)   di=10 (-1,+1)   di= 9 (-2,-1)   di= 8 (-2,+0)
di= 7 (-2,+1)   di= 6 (-3,-1)   di= 5 (-3,+0)   di= 4 (-3,+1)
di= 3 (-1,+0)   di= 2 (+0,+0)   di= 1 (+1,+0)   di= 0 (-1,+1)
```

**facing W (3)** — E 的鏡像:
```
di=11 (+0,-1)   di=10 (-1,-1)   di= 9 (+1,-2)   di= 8 (+0,-2)
di= 7 (-1,-2)   di= 6 (+1,-3)   di= 5 (+0,-3)   di= 4 (-1,-3)
di= 3 (+0,-1)   di= 2 (+0,+0)   di= 1 (+0,+1)   di= 0 (-1,-1)
```

**幾何詮釋**:把每朝向拆成「深度槽」更直觀(以 N 為例,"前方" = x 增):
- di 2 = 玩家自身格 (0,0)。
- di 1/3 = 玩家左右鄰格(同深度,深度 0)。
- di 11/10/0 = 深度 1(前 1 格)的中/左/右? 實際 di 11=(+1,0) 中、10=(+1,−1) 一側、0=(+1,−1)…
  → **di 群組對應「深度層 × 左中右」**;確切「哪個 di = 哪個距離的哪一側」由 §3 的
  `data_55EF`/`ground_points`/`sprite_indices` 反查最可靠(那才是消費端)。

> **待確認 / 推測**:N 的 dx 都是正、S 都是負,代表「前方」沿 ±x 軸而非 ±y;E/W 沿 ±y。
> 也就是 facing 0/2 沿 x 軸、1/3 沿 y 軸。這與多數 Bard's Tale clone 把 N/S 綁某一軸一致。
> 若要 100% 確定 N 到底是 +x 還是 −x「前方」,看玩家移動 opcode(`op_8B`/walk)如何改
> `game_state[0]/[1]`——該 walk 邏輯與這裡的 FOV dx/dy 必須同號。**該看**:engine.c 中
> 處理前進鍵、改 `game_state[0]/[1]` 的那段(grep `game_state.unknown\[0\]` 的寫入點)。

---

## Q2. 牆屬性解讀:`word_11C6` bit 意義 + 元件選擇

### 2.1 `word_11C6` 的來源與位元

`get_map_tile_data(x, y)`(engine.c:5206)的取法:
```
word_551F = y*1? ... 實際: ax = (x*3) + data_5A04[y+1]      // 每格 3 bytes,row 起點查 data_5A04
word_11C6 = data_5521[word_551F] | data_5521[word_551F+1]<<8   // tile 的 2-byte 屬性
word_11C8 = data_5521[word_551F+2]                              // 第 3 byte(事件/額外旗標)
if (越界 byte_551E&0x80): word_11C6 &= 0x3000                   // 邊界:只保留 0x3000 兩 bit
```
所以**每格 tile = 3 bytes**:前 2 byte = `word_11C6` 牆屬性,第 3 byte = `word_11C8`。

已掌握 / 推得的位元意義:
- **low nibble `& 0xF`**:move/邊界相關(`VIEWPORT.md` 與 domain 已知,用於可否通行)。
  `move_player_on_map` 在 facing≠0 時把相鄰格的 `&0xF` 與 `&0xF0` 拼進 `word_11CA`(見 §2.2)。
- **`& 0x3000`**:邊界保留位(越界時只留這兩 bit)→ 推測 = 地圖邊緣/虛空標記。
- **high byte `& 0x08`(bit 11)**:在 `op_2x` 系列(engine.c:3011/3071/3091)被測 →
  推測 = 「門/特殊面」旗標。`draw_viewport_sky` 也用高 byte 算 sky 型(§4)。

> **待確認(重要)**:`word_11C6` 哪幾個 nibble 精確對應「前/左/右」哪一面牆,opendw
> 沒有明寫成具名 bit。**最可靠的還原路徑是「消費端反查」**:`move_player_on_map` 把當前
> 格 + 右格 + 上格的 `&0xF`/`&0xF0` 重組進 `word_11CA`(§2.2),再依朝向旋轉 nibble;
> `data_5A56` 存的就是這個旋轉後的值。也就是**牆面的「四面有無」資訊被編碼成 nibble,
> 經 facing 旋轉後落到 `data_5A56[di]`**。要拿到 ground-truth bit 表,建議在 opendw 加
> hook,對已知地圖座標 dump `word_11C6` 並與遊戲畫面對照(§5 golden)。

### 2.2 `move_player_on_map` 的 nibble 旋轉(engine.c:5332–5403)

```
word_11CA = word_11C6                         // 起點:當前格屬性
if facing != 0:
    取右格 (x+1) word_11C6 & 0x0F   → 暫存 word_11CC 高 byte
    取上格 (y-1) word_11C6 & 0xF0   → 與上面 OR → bl
    dl = word_11CA & 0xFF
    if facing > 2 (W=3): word_11CA = (dl<<4) | (bl>>4)   // 旋轉一個方向
    elif facing == 2 (S): word_11CA = bl                  // 直接用拼好的
    else (facing==1 E):   word_11CA = (bl<<4) | (dl>>4)   // 旋轉另一方向
```
**結論**:`data_5A56[di]` 的 nibble 已經是「相對玩家朝向」的牆面旗標(前/左/右已對齊),
下游選 sprite 時不必再考慮 facing。這是把「世界座標牆面」轉成「螢幕相對牆面」的關鍵步驟。

### 2.3 元件 sprite 選擇:`data_56E5` 哪個索引

`refresh_viewport` 分三批畫(都呼叫 `draw_sprite_to_viewport`):

**(A) 天空** `draw_viewport_sky`(engine.c:5533):
```
bl = data_5A56[0x16] (=最遠正前方那格?) ; rcl 3 次後 & 3   // 取某 2 bit → 0..3
bl = data_56E5[bx]        // sky/wall 類元件資源索引(data_56E5[0..3])
al = data_5897[bx] & 0x7F
if al == 1:  畫 sky sprite(sprite offset = 4) via draw_sprite_to_viewport
else:        填地板兩色(data_575C 交替,88 列,見 §4)
```

**(B) 地面 9 格**(engine.c:5701–5728,counter 8→0):
```
bx = data_56A3[counter]                  // 該地面格對應的 data_5A56 槽
bl = data_5A56[bx+0xC] >> 4 ; bl &= 3    // 取 ground nibble 高位 → 0..2(3 種地面)
bl = data_56E5[bx+4]                     // ground 元件資源索引(data_56E5[4..6])
r  = data_59E4[bl]                        // → resource
word_1051 = r ; word_104F = 0
vp.xpos = ground_points[counter].x ; vp.ypos = ground_points[counter].y ; byte_104E = 0
draw_sprite_to_viewport(&vp, sprite_indices[counter])   // sprite slot 偏移
```
繪製順序(註解,counter 8→0):8=右上、7=左上、6=中上、5=右中、4=左中、3=中中、
2=右下、1=左下、0=中下。`ground_points` 對應落點見 §3。

**(C) other 元件 24 個(牆/門面)**(engine.c:5733–5776,counter 23→0):
```
ax = data_55EF[counter] ; di = ax & 0x7F          // data_5A56 槽
bl = data_5A56[di]
if (ax & 0xFF) > 0x80:  bl >>= 4                   // 高 bit set → 取高 nibble
bl &= 0x0F
if bl != 0:                                         // 該面有牆/門才畫
    cx = data_564F[counter] ; byte_104E = cx & 0xFF // 象限旗標(x 正負/y 翻轉)
    al = data_56C6[bl]                              // wall/door 元件「型」
    if (cx & 1) == 1: al = data_56E5[bl+0x7]        // 奇數 → 改用 other 元件資源表(data_56E5[7..])
    if al <= 0x7F:
        r = data_59E4[al]                           // resource(注意:al<<1 後 >>1,等同 al)
        word_1051 = r ; word_104F = 0
        vp.xpos = data_558F[counter] ; vp.ypos = data_55BF[counter]
        draw_sprite_to_viewport(&vp, data_561F[counter])   // sprite slot 偏移
```

> **核心對應**:「某 di 槽的某 nibble (bl) ≠ 0」= 那個方向/距離有牆 → 用 `bl` 當 index
> 進 `data_56C6`(型)→ `data_56E5`(資源索引)→ `data_59E4`(實際 sprite 資源)。
> `bl` 的值(1..15)決定**牆 vs 門 vs 其它面**(`data_56C6` 是查表,16 槽)。
> **待確認**:`data_56C6[1..15]` 的具體值要在 runtime 從 `read_level_metadata` dump
> 才知道(每關不同);engine.c:5103–5106 顯示它從關卡 metadata 載入([si+1]=型、
> [si+0xF+1]=替代型)。

---

## Q3. 距離槽 → vp 參數(xpos/ypos/runlength/numruns/sprite_offset)

每面要畫的元件,`refresh_viewport` 只設 `vp.xpos`、`vp.ypos`、`byte_104E`,然後
`draw_sprite_to_viewport(&vp, sprite_offset)`。**runlength/numruns 不是在這裡設的**——
它們由 sprite 資料的開頭 2 byte 在 `decode_viewport_data` 裡填(見 Q4)。

### 3.1 sprite_offset 怎麼來

- **地面**:`sprite_indices[counter]` = `{18,16,20,12,10,14,6,4,8}`(byte 偏移,進 sprite 資源)。
- **other/牆**:`data_561F[counter]`,值為 `{4,12,12,4,...,6,14,14,6,...,8,16,16,8,...}`。

`draw_sprite_to_viewport(vp, sprite_offset)`(engine.c:5512):
```
ds = word_1051->bytes + word_104F + sprite_offset   // word_104F 累積,首次=0
size = *(uint16)ds                                   // 前 2 byte = 這個 sub-sprite 的資料長度
if size == 0: return                                 // 0 = 該 slot 無圖(空)
word_104F += size                                    // 推進到下一筆
vp->data = word_1051->bytes + word_104F
decode_viewport_data(vp->data, vp)                   // 真正描線(填 runlength/numruns)
```
所以一個 sprite 資源內部是**多筆 [size:2][run-length payload] 串接**;`sprite_offset` 選哪一筆,
`word_104F` 累積位移。**runlength/numruns 在 `decode_viewport_data` 頭 2 byte 讀**(ui.c:1010)。

### 3.2 xpos/ypos = viewport 落點(距離槽)

| 批次 | xpos 來源 | ypos 來源 | 槽數 |
|---|---|---|---|
| 地面 | `ground_points[counter].x` | `ground_points[counter].y` | 9 |
| other/牆 | `data_558F[counter]` | `data_55BF[counter]` | 24 |

`ground_points`(三排,y=120/104/88 = 下/中/上排,x=16/0/128 等):
```
{16,120} {0,120} {128,120}   // 下排(最近)
{32,104} {0,104} {112,104}   // 中排
{48, 88} {0, 88} { 96, 88}   // 上排(最遠)
```
y 越大越靠下(越近),x 中間格 0、左右格往兩側拉 → 透視收斂。

`data_558F`(xpos,24)與 `data_55BF`(ypos,24)成對,前 4 筆一組(對應某距離的 前/左/右...):
```
xpos: 0x20 0x00 0x80 0xFFC0 | 0x80 0x20 0xFFC0 0x80 | 0x30 0x20 0x70 0xFFF0 | ...
ypos: 0x10 0x00 0x00 0x10   | 0x10 0x10 0x10  0x10  | 0x20 0x10 0x10 0x20   | ...
```
注意 xpos 出現 0xFFC0/0xFFF0(負值,有號)→ 對應 `decode_viewport_data` 的負 x 象限
(`draw_viewport_neg_x` 系列,見 ui.c)。`data_564F` 提供對應 `byte_104E`:
```
data_564F: 0x01 0x00 0x80 0x01 | 0x01 0x00 0x00 0x00 | (重複 3 組距離)
```
- `byte_104E & 0x80` → x 取負方向(neg_x 描線)。
- `byte_104E & 0x40` → y 翻轉(flip_y 描線)。
- `byte_104E & 0x01`(原碼 `cx&1`)→ 改查 `data_56E5[bl+7]`(other 元件表,§2.3)。

### 3.3 距離槽 → viewport_memory 的對應

`vp.xpos/ypos` 經 `decode_viewport_data` 加上 sprite payload 頭的 (Δx, Δy),再經
`process_quadrant`/`draw_viewport_*`(ui.c)的 `get_offset(ypos)+newx` 算出 `viewport_memory`
的 byte offset(`viewport_memory` = 160×136 px,2px/byte = 80×136 byte;列 stride = `word_1053`
= 0x50)。這層細節已在 `VIEWPORT.md` §「描線原語」記過,**不重複**;這裡只強調:

> **距離槽就是 `(xpos, ypos)` 的選擇**——近的牆 ypos 偏中、xpos 偏外側且 sprite 較大;
> 遠的牆 ypos 偏上、xpos 收斂。`data_558F`/`data_55BF` 的 3 組(每組 8 筆)= 3 個距離環,
> 與 `ground_points` 的 3 排(下/中/上)在幾何上對齊。

---

## Q4. 元件 sprite 格式 + blit 方式

### 4.1 sprite 資源在 level 資源裡的位置

```
關卡 .lvl 載入 → read_level_metadata(engine.c:5061)
  ├ data_5897[]  = 元件型/旗標(讀到 byte>0x80 為止)
  ├ data_56C6[]  = wall/door 型表([1..],[0xF+1..]替代)
  └ cache_level_components(r, 0/4/8) → data_56E5[0..3]=sky/wall, [4..6]=ground, [8..]=other
cache_resources(engine.c:5468)
  ├ 對 data_5897 每個型 → resource_load(型 & 0x7F + 0x6E) → data_5897[i+0xF]=resource index
  └ 對 [0..0xE] → resource_get_by_index → data_59E4[i] = struct resource*
```
所以**牆面元件 sprite 的實際 bytes** = `data_59E4[idx]->bytes`,其中 `idx` 由 §2.3 的查表鏈
(`data_5A56` nibble → `data_56C6`/`data_56E5` → `data_59E4`)決定。`word_1051` = 選中的 resource,
`word_104F` = 該 resource 內的累積位移。

### 4.2 sprite 內部格式

一個元件 resource 的 bytes 是多筆子圖串接:
```
[size:2 LE][payload] [size:2 LE][payload] ...
```
- `size==0` → 該 slot 空(`draw_sprite_to_viewport` 直接 return,不畫)。
- payload 開頭 = `decode_viewport_data` 讀的:`runlength:1`、`numruns:1`、`Δx:1(int8)`、
  `Δy:1(int8)`,之後是 run-length nibble 資料(`vp->data + 4` 起)。

### 4.3 blit 方式 = run-length nibble blit(與 sprite/quadrant 同原理)

`decode_viewport_data` → 依象限分派到 `process_quadrant`(byte 模式)或
`draw_viewport_word_mode/neg_x/neg_x_alt/flip_y`(word 模式)。核心(process_quadrant,ui.c:188):
```
newx = xpos >> 1 (算術右移,保號)
裁切 word_104A = runlength(依 newx 與 word_1053 邊界)
p  = viewport_memory + get_offset(ypos) + newx
si = vp->data + 4
for 每列 numruns:
    for 每 byte word_104A:
        *p = (*p & and_table[*si]) | or_table[*si]   // nibble 遮罩混合,透明 nibble 不蓋
        p++; si++
    offset += word_1055 (= ±word_1053,依 y 翻轉)
    si += runlength - word_104A                       // 跳過被裁切的部分
```
**這與已驗證的一般 sprite blit(process_quadrant 那套 and_table/or_table nibble 遮罩)
完全同原理**;不是 `draw_random_encounter_graphic` 那種全螢幕路徑。word 模式(neg_x/flip_y)
用 16-bit 表 `and_table_B452/or_table_B652`(tables.c:178/214),負 x 方向遞減、翻轉 y。

> 結論:**牆面元件 sprite = run-length nibble 子圖串**,blit 用 viewport 的 4+1 個描線變體,
> 與 `VIEWPORT.md` §「描線原語」描述的同一套表(tables.c 4 張遮罩表)。port 時直接複用。

---

## Q5. Remake port 計畫(分步,每步可 golden 對拍 opendw)

前提:`VIEWPORT.md` 已規劃描線核心(process_quadrant + 4 變體 + 遮罩表 + get_offset +
viewport_memory + ui_update_viewport)。本檔補的是**組景層(refresh_viewport)**,接在描線核心之上。

### 需要從 opendw 再抽的資料(static 表,可直接複製或 dump)
| 資料 | 來源 | 抽法 |
|---|---|---|
| `data_5303[4]`、`data_530B[96]` | engine.c:191/197 | 直接複製陣列(注意 §1.1 的 −8) |
| `ground_points[9]`、`sprite_indices[9]`、`data_56A3[9]` | engine.c:264/280/272 | 直接複製 |
| `data_55EF/558F/55BF/561F/564F[24]` | engine.c:233–260 | 直接複製 |
| `data_575C[4]`(天空/地板兩色) | engine.c:291 | 直接複製 |
| `data_56E5/56C6/5897/59E4`(關卡相依) | read_level_metadata / cache_* | **runtime 從 .lvl 抽**,需 port `read_level_metadata`+`cache_*` |
| `word_11C6` bit→牆面語意 | 反查 + hook | 見 §2.1 待確認,建議 golden dump 確認 |

### 分步(每步 golden 對拍)
1. **FOV 取樣**:port §1 的迴圈 → 對固定 (level, x, y, facing) dump `data_5A56[0..23]`,
   與 opendw 加 hook dump 的 `data_5A56` byte-for-byte 對拍。**這步只驗「取樣對不對」,
   不畫圖**,最快建立 pass/fail 訊號(feedback loop 優先)。
2. **關卡 metadata**:port `read_level_metadata`+`cache_level_components`+`cache_resources`,
   dump `data_56E5/56C6/5897` + `data_59E4` 的 resource index 對拍。
3. **元件選擇**:port §2.3 的三批選擇邏輯(sky/ground/other),對每個 (counter) dump
   `(word_1051 resource tag, sprite_offset, vp.xpos, vp.ypos, byte_104E)`,對拍 opendw 的
   `printf("Drawing component %d (tag: %d)")`(engine.c:5764 已有現成 log)。
4. **描線**:接上 `VIEWPORT.md` 已規劃的 process_quadrant/word_mode 變體,把選好的 sprite
   blit 進 viewport_memory,dump 10880 bytes 對拍。
5. **整幀**:組景 → `update_viewport`(4 象限框架)→ `ui_update_viewport` 上螢幕,
   對 framebuffer 與 opendw 實機畫面逐像素對拍(比照 `verify_scene_golden.sh`)。

> 順序刻意把「資料正確性」(步驟 1–3)排在「畫得對不對」(步驟 4–5)之前——
> 組景 bug 多半在取樣/選擇,先用便宜的 byte dump 隔離,再進昂貴的像素對拍。

---

## Step 1 對拍結論(2026-06-13,golden_compose vs remake sample_fov)

以 opendw 的 `refresh_viewport` 取樣段 + `move_player_on_map` + `get_map_tile_data`
+ `check_map_boundary_*` 抽成 standalone C oracle
(`tools_build/viewport_compose_golden/golden_compose.c`),對 Purgatory(area 1,
`maps/1.lvl`,h=w=34、flags=0x1C)固定 4 組 `(facing,x,y)` dump `data_5A56[0..23]`,
remake `dw::render::sample_fov`(`src/render/viewport_compose.{hpp,cpp}`)逐 byte 對拍:

| facing | (x,y) | 場景 | 結果 |
|---|---|---|---|
| 0 (N) | (10,10) | 面牆(near 槽有 wall nibble) | PASS |
| 1 (E) | (10,10) | 面通道(wall 全 0) | PASS |
| 2 (S) | (15,12) | 含 wall(旋轉分支 al==2) | PASS |
| 3 (W) | (8,20) | 含 wall(旋轉分支 al>2) | PASS |

→ **4/4 byte-for-byte 一致**。四個朝向各自命中 `move_player_on_map` 的不同 nibble
旋轉分支(`al==0` 不旋轉、`al==1` E 旋轉、`al==2` S 直拼、`al>2` W 旋轉),全數對齊。

### 待確認項的最終答案

1. **`data_530B` 索引 −8 ?** → **不需要 −8**。opendw 的 C port 把 `data_530B` 當獨立
   陣列,直接用 `data_5303[facing]`(0x16/0x2E/0x46/0x5E)當 index;0x5E=94,讀到
   `data_530B[95]`,仍落在 96-entry 陣列內。本文件 §1.1 的「−8」是對「原版兩符號連續
   記憶體」的推測,**不適用於 Devin Smith 的 C 反組譯**(也就是我們的 oracle)。
   remake 比照 oracle,**直接用 0x16.. 不減 8**,取樣 (x,y) 序列(N 沿 +x 前進扇形、
   E/W 沿 ±x 偏移、S 為 N 鏡像)幾何自洽 → 確認無誤。
2. **facing 0/1/2/3 ↔ N/E/S/W** → 由 `state.h:41` 註解 + 取樣幾何雙重確認:
   **0=N、1=E、2=S、3=W**。N(facing 0)「前方」沿 **+x**(col 增),取樣 dx 全為 +1..+3;
   S 為其鏡像(dx 全負);E/W 前方沿 ±y(row),dx 在 −1..+1 間。
3. **`word_11C6` bit 語意 + 牆面定位** → 牆面「四面有無」**不是**靠 `word_11C6` 的具名
   bit 直讀,而是 `move_player_on_map` 把「當前格 / 右格(x+1)`&0x0F` / 上格(y−1)`&0xF0`」
   三者拼成 `word_11CC`,再依 facing 做 nibble 旋轉寫進 `word_11CA`,低 byte → `data_5A56[di]`
   (牆/門面)、高 byte `&0xF7` → `data_5A56[di+0xC]`(地面)。也就是 **`data_5A56[di]` 的
   low nibble 已是「相對玩家朝向」的牆面旗標**,下游選 sprite 不需再考慮 facing。
   Purgatory 觀測到的 nibble 值:地面 0x10/0x11/0x12(高 nibble 1 = 有地面,低 nibble
   0/1/2 = 地面型);牆 nibble 0x10/0x11/0x01(低/高 nibble 各代表一面)。精確「型→sprite」
   仍待 step 2 的 `data_56C6`/`data_56E5` 查表(每關不同)。
   注意 `byte_551E` 邊界旗標:越界格 `word_11C6 &= 0x3000` 後再進旋轉,oracle 與 remake
   都已含此路徑(`check_map_boundary_x/y`)。

### 復現指令

```
# 產 golden(docker dwsdl)
bash tools_build/viewport_compose_golden/build_run.sh
# remake 對拍(docker dwsdl):4/4 PASS
g++ -std=c++20 -Isrc src/resource/level.cpp src/resource/text_codec.cpp \
    src/render/viewport_compose.cpp tools/verify/verify_fov.cpp -o /tmp/verify_fov
/tmp/verify_fov assets/bundle/maps/1.lvl assets/bundle/maps/golden
# 或 cmake target: verify_fov
```

---

## 待確認清單(原始推測;最終答案見上方「Step 1 對拍結論」)

1. **`data_530B` 索引 −8**:opendw 是否在 runtime 真的 −8?
   推測:port 一律用 −8 後 index(取樣序列自洽,N/S、E/W 互為鏡像)。
   該看:engine.c:5633/5641 的 `data_5303`/`data_530B` 連續記憶體佈局;或加 hook 印 sample (x,y)。
2. **facing 0/1/2/3 ↔ N/E/S/W 的絕對對應**:推測 0=N(沿 +x 前進)、2=S(−x)、1=E(+y)、3=W(−y)。
   該看:前進/walk opcode 改 `game_state[0]/[1]` 的方向,必須與 FOV dx/dy 同號。grep `game_state.unknown[0]`/`[1]` 寫入點。
3. **`word_11C6` 各 bit 的牆面語意**:推測 high byte bit3(0x08)=門/特殊面、`&0x3000`=邊界、
   low nibble=通行;但「前/左/右」是經 `move_player_on_map` 旋轉到 `word_11CA` 的 nibble 才定位。
   該看:engine.c:5347–5401(nibble 旋轉);最終以 §5 步驟 1 的 golden dump 為準。
4. **`data_56C6[1..15]` 的型值**:每關不同,runtime 從 .lvl 載入。
   該看:engine.c:5103–5106(read_level_metadata 填表);需實際 dump 某關確認 wall/door 對應。
5. **`draw_viewport_sky` 取 `data_5A56[0x16]` 的 0x16 槽**:0x16=22 超出 0..23 的 other 範圍?
   推測:`data_5A56` 是 128 bytes,0x16 是某個遠景槽;rcl 3 次 &3 取 2 bit → sky 型 0..3。
   該看:engine.c:5540 上下文 + 該槽在 §1 迴圈是否被寫(di 只到 11,+0xC 到 23,0x16=22 在 ground 範圍 [12..23])
   → **修正推測**:0x16 落在 ground nibble 槽(di+0xC,di=10),即「最遠正前方那格的 ground 高 bit」決定天空型,合理。
6. **`size` 0 的 sprite slot 語意**:推測 = 該距離/方向無牆面元件(空槽,跳過不畫)。
   該看:engine.c:5521(`if cpu.ax == 0 return`)——已相當確定,列此供 port 時別誤判為錯誤。
