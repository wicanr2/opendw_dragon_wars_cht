# 54 — 世界圖逐地點可達性盤點(權威 Dilmun 圖 × remake)

> **更新(2026-06-16,op_79/op_5B/子區 relocate 逆出後)**:
> - op_79(= draw_pattern + op_7A,DRAGON.COM @0x47FA 反組譯)、op_5B(opendw 對拍)已實作。
> - **子區 relocate 機制已完整逆出**:子區進入格 `1A 41 X / 1A 43 Y / 1A 45 AREA / 58 05`
>   → resource 5 的 Y/N 提示「Yes」分支(@0x005F)`19 41 00 / 19 43 01 / 19 45 02`
>   把 (X,Y,area) 寫進 gs[0]/gs[1]/gs[2]。即 **var 0x45 = 目的地 area(byte-exact 入口座標
>   = var 0x41/0x43)**。詳見 §2 更新。
> - flood-fill 連通 **27/40 → 33/40**(新增 24 條子區 relocate 邊;新通 area 16/18/34/36/38/39)。
>   主線地表仍 15/16(唯缺 area 6 菲巴斯)。
> - **更正**:原推測「菲巴斯 6 / 拜占儂 9 卡 op_79」**有誤**。實測:母區(8/29)進入格 op_79
>   實作後全部跑完不換區;op_79 擋的是城內商店對話文字。6/9 的真正缺口見 §2 更新。
> - 改動 + 驗證見章末(ctest 20/20、vm_selftest 新增 op_79/op_5B 2 項)。

> 日期:2026-06-16
> 對象:`opendw_remake/`(C++20/SDL2 重製《火龍之戰》)
> 接續:docs/51(世界圖 tile→area 轉移機制反組譯)、docs/48 roadmap P0(主線連通)
> 方法:**0.lvl bytecode 靜態反推**(`dump_worldmap_tiles`)+ **事件 script 動態探測**
>   (`probe_subarea_entry`,跑母區 tile script 觀測 gs 寫入/op58 tag/halt opcode)
>   + 攻略 38/39 地點交叉驗證 + 權威 Dilmun 世界圖(classicgaming.cc)位置交叉判定。
> 工具:`opendw_remake/tools/verify/{dump_worldmap_tiles,probe_subarea_entry,
>   verify_mainline_reachable}.cpp`(皆已註冊 CMake;前二者為 grounding/觀測,非 ctest)。
>
> **誠實標示**:area 0 世界圖 26 格直連 tile→area 對映**高信心逆出 + 交叉驗證**並實作;
>   非世界圖子區(菲巴斯 6 / 拜占儂 9 等)的母區進入鏈**卡在未實作 opcode(op_79 等)**,
>   目的地**逆不出 → 不補邊**(不臆造)。圖檔(第三方浮水印)**不入庫**,僅以文字記錄佈局事實。

---

## 0. 結論摘要

| 項目 | 數值 |
|---|---|
| 全部 area | 40(0..39,**全部可正常載入**) |
| area 0 世界圖直接可達(直連 tile→area) | **26** |
| 從 area 1(波卡城)/ area 0(世界圖)flood-fill 連通分量 | ~~27~~ → **33 / 40**(+24 條子區 relocate 邊) |
| 主線**地表**必經地區(docs/48 §1.2,16 區)可達 | **15 / 16**(唯缺 area 6 菲巴斯) |
| 仍未連通 | 7 area:**6(菲巴斯)、9(拜占儂)、19(礦場)、22(沉沒水)、27(尼塞山腹)、33(菲巴斯地牢)、35(拜占儂地下)** |

> **連通躍升**:子區 relocate 機制逆出後,新通 area **16(矮人城堡)、18(瑪根)、34(拉娜實驗室)、
>   36(京雄地牢)、38(蘭斯克地下)、39(塔斯地下)**。仍缺的 7 區中,33/35 只有「子→母」反向邊
>   (33→6、35→9),母區(6/9)本身無入口故連帶不達;19/22/27 為深層 wrap 區,入口鏈待逆出。

**Byzanople(拜占儂)/ Phoebus(菲巴斯)結論(已更正)**:**不是漏掃的世界圖 tile**。已逐格反推
area 0 全部 27 個 `58 08 06 00` 樣式格,IDX 集合 = `{1,2,3,5,7,8,10,11,12,13,14,15,17,20,21,23,
24,25,26,28,29,30,31,32,37}`(26 個)+ 1 個特殊格(tile 0x1C,IDX=0x89 超範圍),
**完全不含 6 或 9**。與權威 Dilmun 圖一致:

- **Phoebus 菲巴斯**:圖上位於 **Isle of the Sun**(太陽之島)區,是子區;攻略 38 §5.4-5.5
  確認由 **Mud Toad 黃泥蟾蜍(area 8)** 進入。
- **Byzanople 拜占儂**:圖上位於 **Kings Isle**,**Siege Camp 軍營(area 29)** 緊鄰其旁;
  攻略 38 §5.14 確認「要進拜占儂市須先穿越軍營」→ 由 area 29 進入。

**更正(原推測 op_79 為卡點 → 已否證)**:op_79 實作後重跑母區(8/29)全部特殊格事件,
**沒有任何格 halt、也沒有任何格寫 gs[2]**(`gs2:8->8`、`gs2:29->29`)。被 op_79 擋住的其實是
**城內商店/酒館/募兵對話文字**(如「Cavern Tavern」「Magical Mud Inc.」「black marketeer」),
不是子區進入鏈。更關鍵的證據:
- **掃全 40 關 level bytecode,沒有任何 `1A 45 06` 或 `1A 45 09`**(= 目的地 area=6/9 的
  relocate),除了 **area 33(菲巴斯地牢)→ 6** 與 **area 35(拜占儂地下)→ 9** 這兩條
  「子→母」**反向**邊。母區(8/29)的 level script **完全沒有指向 6/9 的換區**(area 8 唯一
  `58 05` relocate 指向 area 34 拉娜實驗室;area 29 無任何 `58 05`)。
→ **菲巴斯(6)/ 拜占儂(9)的「地表正向進入」不在任何可解碼的 level script 中**。攻略所述
  「由黃泥蟾蜍/軍營進入」的觸發機制與 remake 已逆出的兩套換區路徑(世界圖樞紐、子區 relocate)
  皆不符,屬**尚未識別的第三種機制**(可能由 resource 8/9 的 runtime 控制流或道具/旗標門控)。
  **逆不出 → 不臆造邊,精確記錄**(符合鐵則)。

---

## 1. 逐地點盤點表

### 1.1 area 0 世界圖直接可達(26 格,0.lvl 反推 + 攻略交叉,高信心)

`dump_worldmap_tiles` 對每格驗 `58 08 06 00`(op_58→資源 8 @off=6)頭 +
op∈{0x60,0x68,0x70} + 取 `<IDX>`(目的地 area)。座標為該 tile 在 area 0 的首現位置。

| 世界圖 tile | 座標(x,y) | IDX=area | 地點(權威圖/攻略) | 權威圖區 |
|:---:|:---:|:---:|---|---|
| 0x03 | (4,13) | 1 | Purgatory 波卡城 | Forlorn |
| 0x04 | (3,11) | 2 | Slave Camp 奴隸營 | Forlorn |
| 0x05 | (7,12) | 3 | Guard Bridge 守橋 | Forlorn/Quag |
| 0x06 | (4,21) | 5 | Tar Ruins 塔斯廢墟 | Forlorn |
| 0x08 | (12,14) | 7 | Bridge 橋 | Quag |
| 0x09 | (18,7) | 13 | Bridge of Exiles 放逐橋 | Kings Isle |
| 0x0A | (12,18) | 11 | War Bridge | Quag |
| 0x0B | (8,25) | 8 | Mud Toad 黃泥蟾蜍 | Quag |
| 0x0C | (13,24) | 10 | Smuggler's Cove 海盜竊穴 | Quag |
| 0x0D | (19,31) | 12 | Scorpion Bridge 蠍橋 | Quag/Forlorn |
| 0x0E | (15,27) | 14 | Necropolis 奈羅波裡 | Eastern Isles |
| 0x10 | (17,14) | 28 | Old Dock 老碼頭 | Kings Isle |
| 0x11 | (21,17) | 26 | Pilgrim's Dock 朝聖碼頭 | Kings Isle |
| 0x12 | (27,25) | 30 | Game Preserve 獵區 | Rustic |
| 0x13 | (19,2) | 24 | Snake Pit 蛇窟 | Kings Isle |
| 0x14 | (27,18) | 25 | Kingshome 京雄城 | Kings Isle |
| 0x15 | (21,10) | 15 | Dwarf Ruins 矮人廢墟 | Kings Isle |
| 0x16 | (6,2) | 23 | Mystic Woods 神祕林 | Isle of the Sun |
| 0x17 | (15,38) | 21 | Sunken Ruins 沉沒(陸)| Eastern Isles |
| 0x18 | (24,36) | 31 | Magic College 魔法學院 | Eastern Isles |
| 0x19 | (15,34) | 32 | Dragon Valley 龍谷 | Eastern Isles |
| 0x1A | (19,19) | 4 | Salvation 救贖之山 | Quag |
| 0x1B | (14,16) | 20 | Lansk 蘭斯克 | Isle of the Sun |
| 0x1D | (23,43) | 17 | Freeport 自由港 | Eastern Isles |
| 0x1E | (7,17) | 37 | Slave Estate 莫格宅院 | Forlorn |
| 0x1F | (26,7) | 29 | Siege Camp 軍營 | Kings Isle |

### 1.2 area 0 上的非換區特殊格

| 世界圖 tile | 座標 | 樣式 | 判定 |
|:---:|:---:|---|---|
| 0x1C | (27,7) | `58 08 06 00 … 60 89` | IDX=0x89(137)超 0..39 → **特殊格,非普通換區**(排除)。位置在東緣,疑為需道具/旗標的條件門或裝飾。 |
| 0x0F | (0,0) | `58 08 03 00 …`(off=3) | resource 8 另一入口(dispatch[1]),非 off=6 世界圖換區樣式。 |
| 0x02 / 0x20-0x2B | 各處 | 5-bit 文字 / 其他 op | 文字事件、運算格,非世界圖地點。 |

### 1.3 子區(由母區踩格進入,非世界圖直連)

權威圖上是地點但不在 26 格直連表者,皆為「母區子區」(dungeon / 地下 / 水下 / 地底)。
以下標 **可達性**(從 area 1 flood-fill)與 **進入機制**:

> **更新後可達性**(子區 relocate 補邊後;`1A 45 <AREA>` 靜態逆出)。

| area | 地點 | 權威圖區 | 母區(進入來源) | remake 可達? | 進入鏈狀態 |
|:---:|---|---|---|:---:|---|
| **6** | **Phoebus 菲巴斯** | Isle of the Sun | **Mud Toad 8** | **否** | 無正向入口邊(母區 8 無指向 6 的換區);僅 33→6 反向。§2.3 |
| **9** | **Byzanople 拜占儂** | Kings Isle | **Siege Camp 29** | **否** | 無正向入口邊(母區 29 無 `58 05`);僅 35→9 反向。§2.3 |
| 16 | Dwarf Clan Hall 矮人城堡 | Kings Isle | Dwarf Ruins 15 | **是** | 子區 relocate(`1A 45` → 16)逆出,15→16 連通 |
| 18 | Magan 瑪根地底 | (地底) | Necropolis 14 等 | **是** | 子區 relocate(14→18 等)逆出 |
| 19 | Mines 礦場 | (地底) | Slave Camp 2 / Slave Estate 37 | 否 | 深層 wrap 區;入口鏈未逆出 |
| 22 | Sunken 沉沒(水) | Eastern Isles | Sunken Ruins 21 | 否 | wrap 區(水下);未逆出 |
| 27 | Depths of Nisir 尼塞山腹 | (地底) | Pilgrim Dock 26 / Magan 18 | 否 | wrap 區(終局);未逆出 |
| 33 | Phoeban Dungeon 菲巴斯地牢 | Isle of the Sun | Phoebus 6 | 否 | 母區 6 本身未達(33→6 反向邊已逆出) |
| 34 | Lanac'toor Lab 拉娜實驗室 | (地底) | Mud Toad 8 | **是** | 子區 relocate(8→34)逆出 |
| 35 | Byzan Dungeon 拜占儂地下 | Kings Isle | Byzanople 9 | 否 | 母區 9 本身未達(35→9 反向邊已逆出) |
| 36 | Kingshome Dungeon 京雄地牢 | Kings Isle | Kingshome 25 | **是** | 子區 relocate(25→36)逆出 |
| 38 | Lansk Undercity 蘭斯克地下 | Isle of the Sun | Lansk 20 | **是** | 子區 relocate(20→38)逆出 |
| 39 | Tars Under 塔斯地下 | Forlorn | Tars Ruins 5 | **是** | 子區 relocate(5→39)逆出 |

> **權威圖小點歸類**:Bridge A/B/C、Energy Pool、Hidden Cache 等在權威圖上是 **Quag/Forlorn
>   區內的地形註記或 area 0 上的格**,非獨立 area。Bridge 類已對映到 area 0 的橋格
>   (守橋=3、放逐橋=13、War Bridge=11、蠍橋=12、Bridge=7)。Energy Pool/Hidden Cache 為
>   Forlorn 區內景點,屬母區(波卡城/奴隸營一帶)內部格,非換區目的地。

---

## 2. Byzanople / Phoebus 缺口:逆向判定過程

### 2.1 排除「漏掃的世界圖 tile」

`dump_worldmap_tiles assets/bundle` 對 area 0 全部 41 種特殊 tile 逐格反推。符合
`58 08 06 00`(世界圖換區)頭的共 27 格,逐一解出 `<IDX>`:**沒有任何一格 IDX=6 或 9**。
唯一超範圍格 tile 0x1C(IDX=0x89)位置 (27,7),與 Phoebus(圖上 Isle of the Sun 西側)、
Byzanople(Kings Isle 西北)的圖上位置都不符。**→ 排除漏掃假設,信心高。**

### 2.2 子區 relocate 機制(本輪完整逆出)

子區進入格(及子→母回程格)固定樣式,掃全 40 關 bytecode 共 30 處:

```
1A 41 <X>      op_1A:gs[0x41] = X(入口 X 座標)
1A 43 <Y>      op_1A:gs[0x43] = Y(入口 Y 座標)
1A 45 <AREA>   op_1A:gs[0x45] = 目的地 area
58 05 <off16>  op_58:CALL 資源 5(relocate 確認 handler);off ∈ {0,3,6,9}
```

**resource 5 動態 trace**(`58 05` 任一 off 都匯流到同一段):
```
… op_78(emit「Do you wish to enter <子區>?」)→ op_8C(prompt_no_yes)→ op_4B(STC)
  → op_45 JNZ 0x006D ── No 分支 → op_75 / op_59(返回,不換區)
  Yes 分支(落到 @0x005F):
    19 41 00   gs[0] = gs[0x41]   (= 入口 X)
    19 43 01   gs[1] = gs[0x43]   (= 入口 Y)
    19 45 02   gs[2] = gs[0x45]   (= 目的地 area)   ← relocate!
    11 3D / 11 3E / 4C(CLC)/ 75 / 59(返回)
```

(op_19 bytes = `19 <src> <dst>`,即 `gs[dst]=gs[src]`。)

**→ 結論(byte-exact,信心高)**:`1A 41/43/45` 三常數 = **(入口 X, 入口 Y, 目的地 area)**。
relocate 在玩家對「Do you wish to enter…?」答 **Yes** 時生效(headless 無鍵盤 → op_8C 取
No,故動態跑不到;但靜態讀 `1A 45 <AREA>` 即得目的地、`1A 41/1A 43` 即得入口座標)。
逆出對映抽樣:area 9→35(拜占儂→拜占儂地下)、area 35→9(回拜占儂)、area 5→39(塔斯→塔斯地下)、
area 14→18(奈羅波裡→瑪根)、area 25→36(京雄→京雄地牢)、area 8→34(黃泥蟾蜍→拉娜實驗室)。

> **誠實標示**:resource 5 的 relocate 段以 DRAGON.COM dispatch + 動態 trace 反組譯逆出
>   (opendw `targets[]` 對 op_58 子常式路徑無 C oracle)。三常數 → (X,Y,area) 由
>   「res5 Yes 分支 19 41 00 / 19 43 01 / 19 45 02」直接證實。**入口座標 byte-exact**
>   (不同於世界圖那套採「第一可走格」哨兵)。

### 2.3 仍逆不出的部分(鐵則:精確記錄)

- **菲巴斯 6 / 拜占儂 9 的地表正向入口**:掃全 40 關**無任何 `1A 45 06` / `1A 45 09`**
  指向 6/9(除 area 33→6、35→9 的子→母反向邊)。母區(8/29)level script 完全無指向
  6/9 的換區。攻略所述「由黃泥蟾蜍/軍營進入」與已逆出的兩套路徑(世界圖樞紐、子區 relocate)
  皆不符 → 屬**第三種未識別機制**(疑 resource 8/9 runtime 控制流 / 道具旗標門控)。
  **逆不出 → 不補 6/9 邊**(33/35 因此連帶不達)。
- **area 19/22/27 深層 wrap 子區**:入口鏈(疑經 op_6B/op_70 或多層 relocate)未逆出。
- op_68(0x450A)/op_70(0x4632)/op_6B(0x45A1)在 opendw `targets[]` 仍標 NULL,無 C oracle;
  本輪未硬補(屬另案)。

---

## 3. flood-fill 連通(子區 relocate 補邊後重驗)

`verify_mainline_reachable assets/bundle`(新增「子區 relocate 邊」= `Level::subarea_relocs()`
靜態解 `1A 45 <AREA>`):

```
邊統計:世界圖樞紐邊 26(雙向)  tile 事件邊 1(area 28→26)  子區 relocate 邊 24
從 area 1(波卡城)BFS 可達:33 / 40
從 area 0(世界圖)BFS 可達:33 / 40
主線地表可達:15 / 16(唯缺 area 6 菲巴斯)
```

→ **連通 27/40 → 33/40**(新增 24 條子區 relocate 邊;新通 area 16/18/34/36/38/39)。無退步。

`verify_mainline_reachable` 的「非世界圖子區入口診斷」段(更新後,op_79 已實作 → halt 消失):
```
-- 非世界圖子區入口診斷(逆不出 → 不補邊,記錄 halt opcode)--
  area  6 Phoebus菲巴斯   reached=NO  母區 8 res5-relocate格=無  halt opcodes: (無)
  area  9 Byzanople拜占儂 reached=NO  母區 29 res5-relocate格=無  halt opcodes: (無)
```
(halt opcode 由原 0x79 → 無,證實 op_79 不再是卡點;6/9 缺口為「無正向入口邊」,見 §2.3。)

**主線「全可達」的下一步(明確記錄,非本任務範圍)**:逆出菲巴斯(6)/拜占儂(9)的第三種
正向入口機制(resource 8/9 runtime 控制流);補上 8→6、29→9 後,主線地表將達 16/16,
33/35 也隨之連通。

---

## 4. 改動檔案(本輪:op_79 / op_5B / 子區 relocate)

- `opendw_remake/src/vm/interpreter.{hpp,cpp}` — 實作 + 掛 dispatch:
  - **op_79**(0x47FA,DRAGON.COM 反組譯)= `op79_draw_and_emit_data()` ≡ op_7A(draw_pattern
    為 render 副作用,略)。
  - **op_5B**(0x427A,opendw 對拍移植)= `op5B_get_map_tile()`:dx=gs[1]、bx=gs[0]、清 cf
    (level-grid 重算 word_551F/11C6/11C8 在 headless 不復刻,誠實標示)。
- `opendw_remake/src/resource/level.hpp` — 新增 `subarea_relocs()`:靜態解 `1A 41/43/45 + 58 05`
  → (目的地 area, 入口 X, 入口 Y)。
- `opendw_remake/tools/verify/verify_mainline_reachable.cpp` — 新增「子區 relocate 邊」(用
  `subarea_relocs()` 補邊);診斷段更新。
- `opendw_remake/tests/vm_selftest.cpp` — 新增 2 項:op_79(≡ op_7A、r2 推進)、op_5B(暫存器/旗標契約)。
- `docs/52_*.md` / `docs/54_*.md`(本檔)。
- **未改 opendw;DRAGON.COM(md5 3aa427d4…,56673 bytes)/ 權威圖檔 未入庫。**

## 5. 驗證

- `ctest`:**20/20 PASS**(含 `vm_selftest`〔+2 新項〕、`render_sweep` 154-case、`verify_combat*`、
  `verify_areaswitch`、`verify_wrap`、`verify_op58`、`smoke_app`)。無回歸。
- `mainline_events`:op_79×15 / op_5B×3 卡點全消;唯一字串 53→72(area 8/29 等城內對話 emit)。
- `verify_mainline_reachable`:連通 **27/40 → 33/40**(子區 relocate 邊 24)。
- docker `dwsdl`(先 `rm -rf build build_*`,Make 產生器;image 無 ninja)。

## 附:實據(絕對路徑)

- 世界圖資料:`opendw_remake/assets/bundle/maps/0.lvl`(Dilmun 32×47 flags=0x0E wraps)。
- 全 40 關 `.lvl`:`opendw_remake/assets/bundle/maps/<0..39>.lvl`(本輪確認全部可載入)。
- 共享處理常式:`opendw_remake/assets/bundle/scripts/{5,8}.bin`(皆 dispatch 表起頭;
  resource 5 relocate Yes 分支 @0x005F = `19 41 00 / 19 43 01 / 19 45 02`)。
- DRAGON.COM 反組譯:op_77@0x47E3 / op_78@0x47EC / **op_79@0x47FA**(call 0x3380 draw_pattern
  後落入 op_7A@0x4801)/ op_7A@0x4801(extract_string from word_3ADF[r2])/ 跳表 base 0x3960。
- op_5B opendw body:`opendw/src/lib/engine.c` `op_5B_unused()`(line 2510)+ `get_map_tile_data()`
  (line 5206)。
- 攻略交叉:`docs/38_SOFTWORLD_WALKTHROUGH.md`(§5.4-5.5 菲巴斯、§5.14 拜占儂)。
- 機制反組譯來源:`docs/51_WORLDMAP_AREA_SWITCH_RE.md`。
