# 54 — 世界圖逐地點可達性盤點(權威 Dilmun 圖 × remake)

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
| 從 area 1(波卡城)/ area 0(世界圖)flood-fill 連通分量 | **27 / 40**(area 0 自身 + 26 目的地) |
| 主線**地表**必經地區(docs/48 §1.2,16 區)可達 | **15 / 16**(唯缺 area 6 菲巴斯) |
| 仍未連通 | 13 area:**6(菲巴斯)、9(拜占儂)** + 11 個 dungeon/地下/水下/地底子區 |

**Byzanople(拜占儂)/ Phoebus(菲巴斯)結論**:**不是漏掃的世界圖 tile**。已逐格反推 area 0
全部 27 個 `58 08 06 00` 樣式格,IDX 集合 = `{1,2,3,5,7,8,10,11,12,13,14,15,17,20,21,23,
24,25,26,28,29,30,31,32,37}`(26 個)+ 1 個特殊格(tile 0x1C,IDX=0x89 超範圍),
**完全不含 6 或 9**。與權威 Dilmun 圖一致:

- **Phoebus 菲巴斯**:圖上位於 **Isle of the Sun**(太陽之島)區,是子區;攻略 38 §5.4-5.5
  確認由 **Mud Toad 黃泥蟾蜍(area 8)** 進入。
- **Byzanople 拜占儂**:圖上位於 **Kings Isle**,**Siege Camp 軍營(area 29)** 緊鄰其旁;
  攻略 38 §5.14 確認「要進拜占儂市須先穿越軍營」→ 由 area 29 進入。

兩者母區(8、29)皆世界圖可達,但進入子區的踩格事件 script 在 remake VM **halt 於
op_79**(未實作的 set_msg 變體),且 relocate 目的地語意需執行 resource 5/8 的未逆出子常式
→ **目的地逆不出,本輪不補邊**(符合鐵則;opendw oracle 對 op_79/68/6B/70 亦標 NULL,無 C 參考)。

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

| area | 地點 | 權威圖區 | 母區(進入來源) | remake 可達? | 進入鏈狀態 |
|:---:|---|---|---|:---:|---|
| **6** | **Phoebus 菲巴斯** | Isle of the Sun | **Mud Toad 8** | **否** | 母區進入格 halt 於 **op_79**(目的地逆不出) |
| **9** | **Byzanople 拜占儂** | Kings Isle | **Siege Camp 29** | **否** | 母區進入格 halt 於 **op_79**(目的地逆不出) |
| 16 | Dwarf Clan Hall 矮人城堡 | Kings Isle | Dwarf Ruins 15 | 否 | 子區 relocate 鏈(同上,未逆出) |
| 18 | Magan 瑪根地底 | (地底) | Necropolis 14 等 | 否 | wrap 區;隧道口走 `1A 41/43/45 + 58 05` relocate,res5 語意未逆出(docs/51 §2.3) |
| 19 | Mines 礦場 | (地底) | Slave Camp 2 / Slave Estate 37 | 否 | wrap 區;relocate 鏈未逆出 |
| 22 | Sunken 沉沒(水) | Eastern Isles | Sunken Ruins 21 | 否 | wrap 區(水下);未逆出 |
| 27 | Depths of Nisir 尼塞山腹 | (地底) | Pilgrim Dock 26 / Magan 18 | 否 | wrap 區(終局);未逆出 |
| 33 | Phoeban Dungeon 菲巴斯地牢 | Isle of the Sun | Phoebus 6 | 否 | 母區 6 本身未達 |
| 34 | Lanac'toor Lab 拉娜實驗室 | (地底) | — | 否 | 深層子區;未逆出 |
| 35 | Byzan Dungeon 拜占儂地下 | Kings Isle | Byzanople 9 | 否 | 母區 9 本身未達 |
| 36 | Kingshome Dungeon 京雄地牢 | Kings Isle | Kingshome 25 | 否 | 子區 relocate 鏈;未逆出 |
| 38 | Lansk Undercity 蘭斯克地下 | Isle of the Sun | Lansk 20 | 否 | 子區 relocate 鏈;未逆出 |
| 39 | Tars Under 塔斯地下 | Forlorn | Tars Ruins 5 | 否 | wrap 區(地下);未逆出 |

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

### 2.2 確認「由母區進入的子區」

`probe_subarea_entry assets/bundle 6 9 8 29` 跑母區 Mud Toad(8)/ Siege Camp(29)的
每個特殊格事件 script(BundleProvider 供 op_58 跨資源),觀測:

- **無任何格寫 gs[2]**(area 變數)→ 子區進入**不是**世界圖那套「直接寫 area」機制。
- 進入相關格普遍走 `op_58 資源 8 @off=0x18`(訊息/處理常式)後 **halt 於 op_79**
  (area 8 tile 0x0D / 0x0F、area 29 tile 0x05 / 0x08 等)。
- 子區(6、9、18)內另有 `1A 41 NN 1A 43 NN 1A 45 NN + 58 05 00 00` 樣式(設 var
  0x41/0x43/0x45 後 call 資源 5)——此為 **relocate handler 候選**,但:
  - 直接跑 resource 5 @off=0(`trace`)10 步乾淨返回,**未寫 gs[2]**;其 0x0023 入口為
    op_9A + 5-bit 文字,**靜態線性反組譯不可靠**,語意需執行子常式才能切運算元。
  - 母區(8、29)的**進入格本身**並無 res5-relocate 樣式,而是先 halt 於 op_79。

**→ Byzanople/Phoebus 是子區,進入鏈卡在未實作 opcode;目的地語意逆不出。**

### 2.3 不補的理由(鐵則)

- op_79(0x47FA)、op_68(0x450A)、op_70(0x4632)、op_6B(0x45A1)在 **opendw `targets[]`
  標 NULL**(`OPCODE_REFERENCE.md`),完全無 C oracle。
- relocate 目的地 area 編碼在 resource 5/8 的未逆出子常式 + var 0x41/0x43/0x45 的語意中;
  靜態反推(`1A 41 0C / 04 / 05 / 08 / 07 / 0F` 等值)**無法乾淨對映到 area 0..39**
  (看似局部座標/索引而非 area id),動態跑又 halt 於 op_79。
- 按鐵則「**逆得出才補,逆不出精確記錄**」→ **不補 6/9 邊**,本檔精確記錄卡點;
  op_79/68/6B/70 的實作為**另案**(本任務不硬補)。

---

## 3. flood-fill 連通(補缺口後重驗)

本輪**未新增任何換區邊**(Byzanople/Phoebus 逆不出),連通數與 docs/51 一致、無退步:

```
邊統計:世界圖樞紐邊 26(雙向)  tile 事件邊 1(area 28→26)
從 area 1(波卡城)BFS 可達:27 / 40
從 area 0(世界圖)BFS 可達:27 / 40
主線地表可達:15 / 16(唯缺 area 6 菲巴斯)
```

`verify_mainline_reachable` 新增「非世界圖子區入口診斷」段,可重現本檔結論:

```
-- 非世界圖子區入口診斷(逆不出 → 不補邊,記錄 halt opcode)--
  area  6 Phoebus菲巴斯   reached=NO  母區 8 res5-relocate格=無  halt opcodes: 0x79
  area  9 Byzanople拜占儂 reached=NO  母區 29 res5-relocate格=無  halt opcodes: 0x79
```

**主線「全可達」的下一步(明確記錄,非本任務範圍)**:實作 **op_79**(set_msg 變體,中文化
關鍵 opcode,見 docs OPCODE_REFERENCE §0x79)後,母區進入格才不會中途 halt;再逆出
resource 5/8 的 relocate 子常式(讀 var 0x41/0x43/0x45 → 寫 gs[2])即可補上 8→6、29→9
兩條邊,主線地表將達 16/16。此屬 op_79 / resource-5 逆向另案。

---

## 4. 改動檔案

- `opendw_remake/tools/verify/dump_worldmap_tiles.cpp`(新增)— area 0 逐格 → IDX 盤點 grounding。
- `opendw_remake/tools/verify/probe_subarea_entry.cpp`(新增)— 母區事件 script 動態探測
  (gs 寫入 / op58 tag / halt opcode)。
- `opendw_remake/tools/verify/verify_mainline_reachable.cpp` — 新增「非世界圖子區入口診斷」段。
- `opendw_remake/CMakeLists.txt` — 註冊上述兩支新工具(觀測用,非 ctest)。
- `opendw_remake/src/resource/level.hpp` — 修正 `worldmap_dest()` 註解:特殊格為 **tile 0x1C**
  (原誤作 0x1D)、27 格中 26 格合法、移除誤列的「菲巴斯=6」(菲巴斯非世界圖格)、
  補註菲巴斯/拜占儂為子區。
- `docs/54_*.md`(本檔)。
- **未改 opendw;DRAGON.COM / 權威圖檔 未入庫。**

## 5. 驗證

- `ctest`:**20/20 PASS**(含 `render_sweep` 154-case、`verify_areaswitch`、`verify_wrap`、
  `verify_op58`、`smoke_app`)。無回歸。
- docker `dwsdl`(先 `rm -rf build build_*`,Make 產生器;image 無 ninja)。

## 附:實據(絕對路徑)

- 世界圖資料:`opendw_remake/assets/bundle/maps/0.lvl`(Dilmun 32×47 flags=0x0E wraps)。
- 全 40 關 `.lvl`:`opendw_remake/assets/bundle/maps/<0..39>.lvl`(本輪確認全部可載入)。
- 共享處理常式:`opendw_remake/assets/bundle/scripts/{5,8}.bin`(皆 dispatch 表起頭)。
- 攻略交叉:`docs/38_SOFTWORLD_WALKTHROUGH.md`(§5.4-5.5 菲巴斯、§5.14 拜占儂)、
  `docs/39_SOFTWORLD_FULLTEXT_AND_MAPS.md`。
- opendw 未實作證據:`OPCODE_REFERENCE.md`(op_79/68/6B/70 標 ❌ NULL)。
- 機制反組譯來源:`docs/51_WORLDMAP_AREA_SWITCH_RE.md`。
