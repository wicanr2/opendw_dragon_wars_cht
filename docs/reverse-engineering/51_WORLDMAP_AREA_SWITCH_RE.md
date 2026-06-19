# 49 — 世界圖/地底樞紐「踩格進區」轉移機制(DRAGON.COM 反組譯)

> 日期:2026-06-15
> 對象:`opendw_remake/`(C++20/SDL2 重製《火龍之戰》)
> 方法:**16-bit binary 反組譯**(DRAGON.COM,opendw 對此路徑 `exit(1)` 未實作 → 無 C oracle)
>   + `0.lvl` / `18.lvl` bytecode 靜態反推 + 攻略(38/39)地點交叉驗證 + headless 端到端驗證。
> 定位:接續 docs/assessment/48 roadmap **P0(解鎖主幹連通)**。逆出 area 0 Dilmun 世界圖的城鎮/地點格 → area
>   轉移對映,並實作,使連通分量從「幾乎只剩波卡城」一次擴張到 **27/40 area**。
>
> **誠實標示**:area 0 世界圖的 tile→area 對映已**高信心逆出 + 交叉驗證**並實作;area 18 瑪根地底
>   隧道口走**不同的參數路徑**(op_58 off=3 + var 0x41/0x43/0x45),其精確對映**未完整逆出**(卡點見 §4)。
>   進區後的**入口座標/朝向**因受 resource 8 runtime 控制流阻,**未能靜態逆出** → 落點採目標關卡第一可走格
>   (連通正確,**非 byte-exact 入口**)。

---

## 0. 摘要

- **opendw 在讀任何轉移表前就 `exit(1)`** 的位址(`check_map_boundary_x/y` @0x5530/0x5566,`flags&2`)
  **不是**世界圖進城的機制 —— 反組譯證實那兩段是**純座標 modular wrap**(走出東緣→西緣),
  與換區無關(§1)。remake 既有的 walkable_wrap 已對齊此行為。
- **世界圖進城是獨立機制**:area 0 上每個城鎮/地點格都有事件腳本,固定樣式
  `58 08 06 00 | <NN> 30 <MM> | <op∈{60,68,70}> <IDX> | <座標 tail>`,即 `op_58` 呼叫**共享資源 8**
  (世界圖地點處理常式)@off=6,其後第 1 個 byte **`<IDX>` = 目的地 area id**(§2)。
- **`<IDX>` = area id 已交叉驗證**:area 0 的 26 個城鎮格 IDX 全落在合法 area(0..39),且與攻略世界圖
  地點 1:1(波卡城=1、奴隸營=2、塔斯=5、黃泥蟾蜍=8、自由港=17、京雄城=25、龍谷=32…);
  唯 tile 的 IDX=0x89(137)超範圍 = 特殊/非換區格,排除(§2.2)。
- **實作**:`Level::worldmap_dest()` 解 IDX → `run_event` 在 wrap 樞紐關卡命中時設 `gs[2]=目的地` →
  `sync_relocation` 載入(§3)。
- **連通驗證(flood-fill)**:從 area 0 世界圖可達 **27/40 area**;主線**地表**必經地區 **15/16** 可達(§3.3)。
  headless 端到端:站上世界圖城鎮格 → 切到對應 area(波卡城/龍谷/京雄城/黃泥蟾蜍皆 PASS,§3.4)。

---

## 1. 澄清:opendw `exit(1)` 的 0x5530/0x5566 是座標 wrap,不是換區

opendw `engine.c` 在 `check_map_boundary_x` (0x5523) / `check_map_boundary_y` (0x5559) 對
`game_state.unknown[0x23] & 0x2`(wrap 旗標)走 `printf("…0x5530 unimplemented"); exit(1);`。

反組譯 DRAGON.COM 對應段(COM addr,file off = addr-0x100):

```
; check_map_boundary_x @0x5523
5523: 3a 1e 82 38   cmp bl, [0x3882]      ; bl(X) >= map_width?
5527: 72 2f         jb  0x5558            ; 在界內 → ret
5529: f6 06 83 38 02 testb $2, [0x3883]   ; flags & 2 (wrap)?
552e: 74 16         je  0x5546            ; 非 wrap → clamp 分支
; --- WRAP 分支(opendw exit(1) 處)---
5530: 3a 1e 82 38   cmp bl, [0x3882]      ; while bl >= width
5534: 72 22         jb  0x5558            ;   bl < width → done, ret
5536: 84 db         test bl,bl
5538: 78 06         js  0x5540            ;   bl 為負 → 加
553a: 2a 1e 82 38   sub bl, [0x3882]      ;   bl -= width
553e: eb f0         jmp 0x5530
5540: 02 1e 82 38   add bl, [0x3882]      ;   bl += width
5544: eb ea         jmp 0x5530
```

→ 這是把座標折回 `[0, width)` 的**取模迴圈**(Y 同理 @0x5559/0x5566,對 height `[0x3881]`)。
**完全沒有讀任何「位置→area」表,沒有換區。** opendw 只是沒實作環繞顯示就 `exit(1)` 了。
remake 的 `Level::wrap_x/wrap_y/walkable_wrap` 已等價實作此環繞。

**關鍵結論:世界圖進城必走別的路徑(§2),不在邊界檢查裡。**

---

## 2. 世界圖進城機制:op_58 → 資源 8 → IDX = 目的地 area

### 2.1 VM 與狀態(反組譯確立)

- dispatch @0x3ACF:`lodsb es:` → `*2` → `jmp *0x3960(bx)`(跳表 base 0x3960,160 項)。
- 玩家/世界狀態(亦即 VM 變數陣列,base 0x3860):`0x3860`=X(var0)、`0x3861`=Y(var1)、
  **`0x3862`=current area(var2)**、`0x3863`=facing(var3);`0x38b7`=已載入 area。
- 換 area = 把目的地寫進 var2(`0x3862`)→ commit/load 由 `op_8B`(@0x499b→0x51b0→
  `load_level_resources` @0x5764)做,讀 `0x3862` 載入資源 `area + 0x46`(@0x5790 `add $0x46,%bx`)。
- 踩格觸發 = `op_71`(@0x465b):每步讀目前格的事件 byte(`0x11c8`,由 `get_map_tile_data`
  @0x54d8 填),非 0 且與上次不同 → 跑該格的事件 script(`(event+1)*2` 索引 script 表)。
  **opendw 有實作 op_71**(`{ op_71, "0x465B" }`),故「踩格跑事件」這層已對齊。

### 2.2 世界圖城鎮格 script 樣式(0.lvl 靜態反推)

area 0「Dilmun」(32×47,flags=0x0E,wrap)上每個城鎮/地點格的事件 script(script 表
= `grid + W*3*H`,entry = `(tile_value+1)*2`)固定為:

```
58 08 06 00      op_58:CALL 資源 8 @off=0x0006(世界圖地點處理常式)
<NN> 30 <MM>     資料 byte NN + op_30(ACC += MM)
<op> <IDX>       op∈{0x60,0x68,0x70};**<IDX> = 目的地 area id**
<座標 tail...>   入口座標相關子資料
```

`op_58`(bytecode opcode 0x58)= **跨資源 script call**:`58 <tag> <off16>`,載入資源 <tag>、
切換 running script 到它、跳到 <off>(opendw `op_58` @0x4239 有實作)。世界圖每格都 call
資源 8 同一個 off=6 入口,差別只在其後的 **<IDX>**。

**逆出對映(area 0,正確 `(tv+1)*2` 索引;與攻略 38/39 交叉驗證):**

| 世界圖 tile | IDX = **area** | 地點(攻略) | 世界圖 tile | IDX = **area** | 地點 |
|:---:|:---:|---|:---:|:---:|---|
| 0x03 | 1 | Purgatory 波卡城 | 0x13 | 24 | Snake Pit 蛇窟 |
| 0x04 | 2 | Slave Camp 奴隸營 | 0x14 | 25 | Kingshome 京雄城 |
| 0x05 | 3 | Guard Bridge 守橋 | 0x15 | 15 | Dwarf Ruins 矮人廢墟 |
| 0x06 | 5 | Tars Ruins 塔斯廢墟 | 0x16 | 23 | Mystic Wood 神祕林 |
| 0x08 | 7 | Bridge 橋 | 0x17 | 21 | Sunken Ruins 沉沒(陸) |
| 0x09 | 13 | Bridge of Exiles 放逐橋 | 0x18 | 31 | Magic College 魔法學院 |
| 0x0A | 11 | War Bridge | 0x19 | 32 | Dragon Valley 龍谷 |
| 0x0B | 8 | Mud Toad 黃泥蟾蜍 | 0x1A | 4 | Salvation 救贖之山 |
| 0x0C | 10 | Smugglers Cove 海盜竊穴 | 0x1B | 20 | Lansk 蘭斯克 |
| 0x0D | 12 | Scorpion Bridge 蠍橋 | 0x1C | **0x89** | **特殊(超範圍,排除)** |
| 0x0E | 14 | Necropolis 奈羅波裡 | 0x1D | 17 | Freeport 自由港 |
| 0x10 | 28 | Old Dock 老碼頭 | 0x1E | 37 | Slave Estate 莫格宅院 |
| 0x11 | 26 | Pilgrim Dock 朝聖碼頭 | 0x1F | 29 | Siege Camp 軍營 |
| 0x12 | 30 | Game Preserve 獵區 | | | |

> **交叉驗證**:已知 area 名比對 **7/7 吻合**(波卡城/奴隸營/塔斯/黃泥蟾蜍/自由港/京雄城/龍谷),
>   且 26 個 IDX 全落在合法 area 範圍,**信心高**。唯 tile 0x1C 的 IDX=0x89(137)超出 0..39
>   → 判定為特殊格(非普通換區),排除。

**世界圖直接可達 26 個地表 area**;未直接列入者(6/9/16/18/19/22/27/33-39)皆為「由母區進入的
子區/地牢/水下」,與遊戲設計一致(菲巴斯地牢、拜占儂、矮人城堡、瑪根、礦場、沉沒水下、尼塞山腹、
各 dungeon/undercity)。

### 2.3 area 18 瑪根:不同參數路徑(未完整逆出,見 §4)

area 18「Magan Underworld」(32×32,flags=0x16,wrap)的隧道口格走**不同**樣式:
`58 08 03 00`(op_58 資源 8 @off=**3**,= dispatch 另一入口)+ `1A 41 NN 1A 43 NN 1A 45 NN`
(op_1A 把 var 0x41 / 0x43 / 0x45 設成常數)。這三個常數疑為(area, X, Y)或(area, 子參數),
但須執行 resource 8 的 off=3 子常式才能確定語意 —— **此對映本輪未確證**(卡點 §4)。

---

## 3. 實作

### 3.1 `Level::worldmap_dest(tile_value)`(`src/resource/level.hpp`)

解 §2.2 樣式:驗 `58 08 06 00` 頭 + op∈{0x60,0x68,0x70} + 取 `<IDX>`;IDX∈[0,39] 才回傳,
否則 -1。**只認 area 0 世界圖樣式**;area 18 的 off=3 樣式不在此(誠實:未逆出)。

### 3.2 `run_event` hook(`src/main.cpp`)

在跑事件 VM **之前**,若當前是 wrap 樞紐關卡(`level->wraps()`)且 `worldmap_dest(tv) >= 0`:
直接設 `gs[2]=目的地 area`、`gs[0]=gs[1]=0`(入口未逆出哨兵),回空字串。後續 `sync_relocation`
偵測 `gs[2]!=current_area` → `enter_map(目的地)`。`sync_relocation` 對「入口哨兵 (0,0) 且
(0,0) 不可走」保留 enter_map 取的**第一可走格**作落點。

> **為何繞過 resource 8 VM**:resource 8 @0x0173 的子常式用 op_68/op_70(opendw `targets[]` 標
>   **NULL**,未逆出)+ flag16 寬度切換 + 條件跳轉,remake VM 尚無法忠實執行(實測 trace 會漂移)。
>   §2.2 既已**靜態高信心**逆出 IDX=area,直接用之是務實且忠於發現的語意的做法。

### 3.3 連通驗證(`tools/verify/verify_mainline_reachable.cpp`,新增)

把 area 當節點、可走通換區當邊(世界圖樞紐邊 = worldmap_dest,雙向;tile 事件邊 = 跑 script 看
gs[2] 變更,如 area28→26),從 area 0 / area 1 做 BFS。結果:

```
邊統計:世界圖樞紐邊 26(雙向)  tile 事件邊 1
從 area 0(世界圖)BFS 可達:27 / 40
主線地表可達:15 / 16   (唯 area 6 Phoebus 不在世界圖直連表 — 由他途進入)
```

→ **連通分量從「幾乎只剩波卡城(<10%)」擴張到 27/40。** 這是 docs/assessment/48 P0「demo → 可通關」的關鍵轉折。

### 3.4 headless 端到端

`./opendw_remake --map 0 --at X Y --dump out.ppm`(站上世界圖城鎮格):

| 世界圖座標 | tile | log | 載入 |
|---|:---:|---|---|
| (4,13) | 0x03 | `worldmap enter → area 1` | Purgatory 波卡城 34×34 ✓ |
| (15,34) | 0x19 | `worldmap enter → area 32` | Dragon Valley 龍谷 16×16 ✓ |
| (27,18) | 0x14 | `worldmap enter → area 25` | Kingshome 京雄城 17×15 ✓ |
| (8,25) | 0x0B | `worldmap enter → area 8` | Mud Toad 黃泥蟾蜍 17×17 ✓ |

截圖:`docs/wm_world.png`(Dilmun 世界圖,玩家 `>` + 各色城鎮格)、
`docs/wm_dragonvalley.png`(進龍谷後)。

### 3.5 不破壞既有

`ctest`:**20/20 PASS**(含 `render_sweep` 154-case viewport/minimap 對拍、`verify_areaswitch`、
`verify_wrap`、`smoke_app`)。

---

## 4. 卡點(誠實記錄,未逆出處不臆造)

1. **入口座標 / 朝向(area 0 + area 18 皆然)**:城鎮格 script 的座標 tail(`<op><IDX>` 之後到
   record 終止符 `0xF?` 之間的 bytes)要靠 resource 8 的 runtime 控制流 + flag16 寬度狀態才能切成
   X/Y/facing 運算元。靜態線性反組譯會漂移。**精確入口需執行 VM**(用既有 op_58 replay harness,
   對 `0x3860/0x3861/0x3863` 下 watch,逐格觸發、抓 op_8B commit 時的值)。目前落點 = 目標關卡
   第一可走格(連通正確,非 byte-exact)。
2. **area 18 瑪根隧道口對映**:走 `58 08 03 00` + var 0x41/0x43/0x45 路徑(§2.3),語意未確證。
   下一步:逆出 resource 8 @0x0160(dispatch 入口 [1],off=3)子常式如何用這三個 var。
3. **resource 8 子常式 / op_68(0x450A) / op_70(0x4632)**:opendw `targets[]` 標 **NULL**,
   完全無 C oracle。要忠實執行需逐指令逆出這幾個 handler + resource 8 的 off=3/off=6 兩條路徑。

---

## 5. 改動檔案

- `opendw_remake/src/resource/level.hpp` — 新增 `worldmap_dest()`。
- `opendw_remake/src/main.cpp` — `run_event` 世界圖樞紐 hook;`sync_relocation` 入口哨兵落點。
- `opendw_remake/tools/verify/verify_mainline_reachable.cpp`(新增)+ `CMakeLists.txt` 註冊。
- `docs/49_*.md`(本檔)+ `docs/wm_world.png` / `docs/wm_dragonvalley.png`(截圖)。
- **未改 opendw;DRAGON.COM 未入庫。**

## 附:實據(絕對路徑)

- 反組譯:DRAGON.COM(自 `Dragon Wars (1990).zip` → `3.5/1.1/DISK01.IMA` 解出,md5 `3aa427d4…`,
  56673 bytes;**不入庫**)。邊界 wrap @0x5523/0x5559;dispatch @0x3ACF / 跳表 0x3960;
  op_58 @0x4239;op_71 @0x465B;op_8B @0x499B→0x51B0;load_level @0x5764(area+0x46)。
- 世界圖資料:`opendw_remake/assets/bundle/maps/0.lvl`(Dilmun)、`18.lvl`(Magan)。
- 共享處理常式:`opendw_remake/assets/bundle/scripts/8.bin`(769 bytes,dispatch [0..9])。
- opendw 未實作證據:`opendw/src/lib/engine.c` `{ NULL, "0x450A" }`(op_68)、`{ NULL, "0x4632" }`
  (op_70);`check_map_boundary_x/y` exit(1) @5159/5189。
