# 54 — 世界圖逐地點可達性盤點(權威 Dilmun 圖 × remake)

> **更新 3(2026-06-17,新完整 opcode 集重跑 Phoebus 6 — 仍逆不出,守門強化)**:
> op_6B/op_8D/op_91/92/97/98 實作 + 資源 buffer 持久性修復後,**公平重跑全 40 關**
> 換區/事件 trace(動態注入 'Y' + 靜態 raw 掃描 + quest-flag 盤點),覆蓋先前因未實作
> opcode 而 halt 的 script。結論:**Phoebus 6 仍無任何正向入邊 → 不補邊**。
> - **先前 halt 全消**:area 8(Mud Toad)/ area 33(Phoeban Dungeon)及全 40 關動態
>   trace,`halt_unimpl` 全 **0x00**(op_6B 在 dungeon 模式純座標 mutation 不 halt、
>   op_8D say-word 輸入框實作)。先前卡未實作 opcode 在「算出目的地前 halt」的疑慮
>   **被排除** —— script 現在全部跑到底,**仍未在更深處算出指向 6 的 gs[2]**。
> - **area 8 全 tile 動態 driven(--yes 注入)**:零 AREA CHANGE,全 `gs2:8->8`,從不換到 6
>   (tile 0x12 跑 2320 步深仍 8→8)。
> - **area 33 say-word puzzle(tile 0x14,op_8D 新解鎖)深追**:`8d read_string` → `call`
>   比對暗語 → 答對分支 @0x0667「You may pass.」**只做 op_73(清事件格)**,**不寫 gs[2]**;
>   答錯 @0x064B「You are not allowed to enter.」+ op_6B(後退)。即說對暗語是「解鎖
>   33 內部通道」**非「傳送到 area 6」**(6/33 同隔離分量,33 內走動不換 area)。
> - **全 40 關 raw 掃指向 6 的所有樣式(`1A 02 06` 直接 / `1A 45 06` 子區 / `09 06..12 02`
>   計算式)**:唯一命中 = **area 33 @0x0389 `1A 45 06`**(33→6 反向/內部)。
>   area 0 worldmap 雖有 `60 06 @script-off 0x1297`,但 **0x1297 非任何地圖 tile 的 script
>   入口(落在 tile 0x02 腳本中段資料)→ 資料區假命中,非可踩格**(`dump_worldmap_tiles`
>   列出的真 tile 27 格 dest 集合不含 6)。
> - **quest flag 盤點(`trace_quest_gates`)**:area 6 自身測 gs06.5/6/7(area 6 **內部**
>   gate,且全 `SET={}`,無 area 設定),**無任何 flag 門控會改變指向 6 的目的地**。
> - **守門強化**:`verify_city_entry` 新增 (4) 計算式 `09 06..12 02` 算出 area 6 路徑數 = 0、
>   (5) area 8 Mud Toad 對 6 零引用,兩條回歸鎖。**ctest 31/31**(本輪未新增 ctest 項,
>   既有 `verify_city_entry` 內擴充斷言)。詳見 §2.4。
>
> **更新 2(2026-06-16,動態 trace 逆出進城第三機制後 — 33/40 → 38/40)**:
> 改用**動態逆向**(remake VM 實機執行 + 注入鍵盤 'Y' 通過 op_8C/op_89,逐指令 trace
> 跨資源 call)攻克 Byzanople(9)/Phoebus(6) 缺口。工具:`tools/verify/trace_subarea_dyn.cpp`
> (新增 op_58 XCALL observer hook + headless 鍵注入)。
>
> - **Byzanople 拜占儂(9)逆出成功 — 共兩條正向入口,皆為先前未識別的「第三種機制」**:
>   1. **世界圖直連(area 0 tile 0x1C @(27,7))**:此格 IDX byte = **0x89**,**bit7 是
>      「需 Y/N 確認」旗標**,目的地 = `0x89 & 0x7F = 9`。resource 8 @off=6 的目的地解碼段
>      @0x01ad **`38 7F`(AND 0x7F)→ `12 02`(gs[2]=r2)**實證(動態 trace `area 0 --yes`
>      跑出 `gs2:0->9`)。舊版 `worldmap_dest()` 誤判 `0x89 > 39` 為「非換區格」而漏掉 →
>      **已修(加 `& 0x7F` 遮罩)**;原 26 條邊不受影響(bit7=0)。
>   2. **母區直接進入(area 29 Siege Camp tile 0x0D)**:`1A 00 07 / 1A 01 09 / 1A 02 09`
>      (op_8C 門控),即答 Y → **直接寫 gs[2]=9**(入口 (7,9))。**這就是 §2.3 所述「無
>      `1A 45 09`」的真相** —— 它走 **`1A 02 09`(直接寫 area 變數 gs[2])**,不經 resource 5
>      的 `1A 45`→gs[0x45] 中轉。動態 trace `area 29 --yes` 跑出 `gs2:29->9`。
> - **同一第三機制(`1A 00/01/02` 直接寫 gs[2],多數 op_8C 門控)還連通了一批舊缺區**:
>   1→19(礦場)、21→22(沉沒水)、4→18/27、26→4、9→35/36/29 等。**連通 33→38/40**;
>   新通 **9、19、22、27、35**。
> - **Phoebus 菲巴斯(6):動態也確認逆不出 → 不補邊(誠實鐵則)**。全 40 關用三套機制
>   (worldmap_dest off=6 遮罩後、subarea_relocs `1A 45`、area_entry `1A 02`)窮舉,
>   **無任何正向邊指向 6**;唯一含 6 的邊是 **area 33(Phoeban Dungeon)→ 6 的 `1A 45 06`**
>   (反向/內部)。{6,33} 是與其餘圖**完全隔離**的分量(6→29、6→33、33→6,無外部入邊)。
>   攻略「Mud Toad → Phoebus」是**敘事順序非地圖鄰接**:Mud Toad(8)的 level script 對
>   Phoebus(6) 零引用(無 `1A 02 06`/`1A 45 06`/computed-6)。詳見 §2.4。
> - 仍缺 **2 區:6(Phoebus)、33(Phoeban Dungeon)**,均屬隔離的 {6,33} 分量。
> - 改動 + 驗證見章末「附錄:動態 RE 改動(更新 2)」(ctest 21/21、diff_trace 逐指令一致、
>   新增 `verify_city_entry` 回歸守門)。

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

> **§2.3 更正(更新 2,動態 trace)**:上述「area 19/22/27 入口鏈未逆出」與「9 無正向入口」
>   **已被動態 trace 推翻/補上**。改用 remake VM 實機執行 + 注入鍵盤 'Y'(通過 op_8C/op_89),
>   發現第三種機制 = **`1A 00 X / 1A 01 Y / 1A 02 <AREA>`(op_1A 直接寫 area 變數 gs[2],
>   多數 op_8C「Do you wish to enter…?」門控)**。這條與已知兩套(worldmap off=6、subarea
>   `1A 45`)正交,故先前靜態掃 `1A 45 09` 掃不到。實證:1→19、21→22、9→35/29/36、27→18 等
>   皆此形;Byzanople 9 由 **area 29 @0x04FA `1A 02 09`** + **世界圖 tile 0x1C(IDX 0x89&0x7F)**
>   兩路進入。詳見 §2.4 與「附錄:動態 RE 改動」。

### 2.4 Phoebus 菲巴斯(6):動態窮舉後仍逆不出 → 不補邊(誠實鐵則)

動態 + 靜態三套機制窮舉全 40 關,**無任何正向邊指向 area 6**:

| 機制 | 掃描內容 | 指向 6 ? |
|---|---|---|
| worldmap_dest(off=6) | area 0 全部實際出現的世界圖格(IDX & 0x7F) | **無**(28 格 dest = {1,2,3,4,5,7,8,9,10,11,12,13,14,15,17,20,21,23,24,25,26,28,29,30,31,32,37};無 6) |
| subarea_relocs(`1A 45 <AREA>`) | 全 40 關 | 僅 **area 33 → 6**(`1A 45 06` @0x0389,= Phoeban Dungeon 回 Phoebus,反向/內部) |
| area_entry(`1A 02 <AREA>`) | 全 40 關 | **無 `1A 02 06`** |
| 計算式 op_11/op_12 gs[2]=r2 | 母區 8/29 driven with --yes/--key | r2 從未算出 6 |

- **{6, 33} 是與其餘圖完全隔離的分量**:6→29(`1A 02 1D`)、6→33(`1A 02 21`)、33→6
  (`1A 45 06`);無任何外部 area 能進 6 或 33。
- **攻略「Mud Toad(8) → Phoebus(6)」是敘事順序,非地圖鄰接**:Mud Toad 的 level script
  對 Phoebus 零引用(無 `1A 02 06`/`1A 45 06`/computed-6;area 8 連 `area_entry_relocs`
  都空)。動態 driven 全 area 8 tile(注入 'Y' 及多種 op_89 鍵)均不換到 6。
- **誠實標示**:Phoebus 入城的真實觸發(可能由劇情旗標 / 道具 / 尚未進入該分量的前置
  條件控制,甚至原版設計即「需先到 33 再到 6」)在可解碼的 level/shared-resource bytecode
  中**找不到正向入邊**;opendw 對相關 op_68/70/6B 亦標 NULL。**逆不出 → 不臆造 8→6 邊**。
  `verify_city_entry` 測試把「無任何正向邊指向 6」鎖成回歸守門。

#### 2.4.1 新完整 opcode 集重跑(2026-06-17)— 公平再驗,結論不變

先前窮舉(更新 2)時 op_6B/op_8D 仍標 NULL,存在「script 在算出指向 6 的 gs[2] 之前
卡未實作 opcode 而 halt」的疑慮。本輪 op_6B/op_8D/op_91/92/97/98 已實作 + 資源 buffer
持久性修復,**公平重跑**,逐項驗證該疑慮:

| 驗證角度 | 工具/方法 | 觀測 | 是否現出 area 6 邊 |
|---|---|---|:---:|
| 先前 halt 是否消失 | `trace_subarea_dyn` area 8 / 33 / 全 40 關 `--yes` | `halt_unimpl` 全 **0x00**(先前 op_6B/op_8D/op_79 halt 全消) | **否**(跑到底仍不指 6) |
| Mud Toad(8)母區全 tile | area 8 全 tile `--yes` 注入 | 零 AREA CHANGE,全 `gs2:8->8`(含 tile 0x12 跑 2320 步) | **否** |
| 全 40 關動態換區 | 全 40 關 `--yes`,列所有 AREA CHANGE | 53 條 AREA CHANGE,目的地集合不含 6 | **否** |
| area 33 say-word puzzle | tile 0x14(`8d read_string` 新解鎖)反組譯 + trace | 答對暗語分支 @0x0667「You may pass.」**只 op_73 清事件格,不寫 gs[2]**;答錯 @0x064B + op_6B | **否**(解鎖內部通道,非傳送) |
| 計算式目的地 | 全 40 關 raw 掃 `09 06`(set_r2=6)→ `12 02`(gs[2]=r2) | 0 命中 | **否** |
| 直接/子區 immediate | 全 40 關 raw 掃 `1A 02 06` / `1A 45 06` | 唯 area 33 @0x0389 `1A 45 06`(33→6 反向) | **否(正向)** |
| worldmap bit7 變體 | area 0 全 `58 08 06 00 .. 60/68/70 <IDX>` 的 IDX&0x7F | 真 tile dest 集合不含 6;`60 06 @0x1297` 為**資料區假命中**(非 tile script 入口) | **否** |
| story-flag 門控目的地 | `trace_quest_gates` 全 40 關 flag SET/TEST 圖 | area 6 測 gs06.5/6/7(**內部** gate,全 SET={});無 flag 改變指向 6 的目的地 | **否** |

- **story-flag 門控結論**:幾乎所有 quest flag 都是「TEST 但 level tile 不 SET」(由戰鬥 /
  NPC 對話 / 道具等 resource 層事件設定)。即使如此,**動態全掃已證實無任何 tile(無論
  flag 狀態)寫 gs[2]=6** —— 沒有「設了某前置旗標後世界圖某格才指向 6」的路徑存在於
  可解碼的 level bytecode。
- **C 最終結論**:用最新完整 opcode 集重跑,**Phoebus(6)正向入口不在可解碼的 bytecode**
  (level + shared resource 5/8)。{6,33} 仍是與其餘圖完全隔離的分量(6→29、6→33、
  33→6;無外部入邊)。原版設計上 Phoebus 入城可能經 area 33(Phoeban Dungeon)/ 劇情旗標 /
  外部機制(resource 層 runtime 控制流非 tile-script 可解碼),**逆不出 → 不補邊**,保留並
  **強化** `verify_city_entry` 守門(新增計算式 `09 06..12 02`→6 = 0、area 8→6 = 0 兩鎖)。

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

---

## 附錄:動態 RE 改動(更新 2 — 進城第三機制)

### A. 方法(動態為主)

- **工具**:新增 `opendw_remake/tools/verify/trace_subarea_dyn.cpp`。在 remake VM 載入母區
  (8/29 等),程式化踩遍每個事件格(`op_71`→script),**逐指令 trace 跨資源 call**
  (新增 op_58 XCALL observer hook,印目標資源 + offset)、印 gs 寫入(尤其 gs[2]=area)。
  以 `--yes` / `--key 0xNN` 注入鍵盤(通過 op_8C「Do you wish to enter…?」與 op_89 選單),
  跑出 headless 預設取 No 時跑不到的「玩家確認進城」分支。
- **關鍵觀測**:`trace_subarea_dyn assets/bundle 29 --yes` 對 tile 0x0D 跑出 `gs2:29->9`;
  其 script = `73 / 74 / 78 / 8C / 45 <No> / 1A 00 07 / 1A 01 09 / 1A 02 09 / 11 / 75 / 5A`。
  `trace_subarea_dyn assets/bundle 0 --tile 0x1C --yes` 跑出 `gs2:0->9`,resource 8 @off=6
  解碼段 @0x01ad `38 7F`→`12 02` 把 `IDX(0x89) & 0x7F = 9` 寫進 gs[2]。

### B. 逆出結論(grounded:VM 實機跑出 gs[2])

- **Byzanople 9 = 兩條正向入口**:世界圖 tile 0x1C(IDX 0x89,bit7=確認旗標 → 9)、
  Siege Camp 29 tile 0x0D(`1A 02 09`,op_8C 門控,入口 (7,9))。
- **第三機制泛化**:`1A 00/01/02` 直接寫 gs[0/1/2],逆出 16 條此類邊;新通 9/19/22/27/35。
- **Phoebus 6**:三套機制窮舉皆無正向入邊({6,33} 隔離分量)→ **不補邊**(§2.4)。

### C. 改動檔案

- `opendw_remake/src/vm/interpreter.{hpp,cpp}`:
  - 新增 **XcallObserver** 診斷 hook(op_58 解出 tag+offset 後回呼,**不改 VM 行為**)。
  - **op_8C** 改為消耗 `headless_keys`/`headless_key`(若有注入)→ 可動態驅動 Y/N 進城分支;
    **無注入時行為與舊版完全相同**(diff_trace 逐指令一致、既有測試不變)。
- `opendw_remake/src/resource/level.hpp`:
  - **`worldmap_dest()` 加 `IDX & 0x7F` 遮罩**(bit7=確認旗標)→ tile 0x1C → area 9
    (修舊版誤判 0x89>39 漏邊);原 26 條 bit7=0 不受影響。
  - 新增 **`area_entry_relocs()`**:解 `1A 00 X / 1A 01 Y / 1A 02 <AREA>`(直接 gs[2] 進入,
    回 dest/entry(X,Y)/op_8C 門控旗標)。
- `opendw_remake/tools/verify/verify_mainline_reachable.cpp`:加「直接 gs[2] 進入邊」
  (`area_entry_relocs`);診斷段 Byzanople 9 reached=YES。
- `opendw_remake/tools/verify/{trace_subarea_dyn,verify_city_entry}.cpp`(新增)+ CMake 註冊。
  `verify_city_entry` 為 ctest:鎖定「0x1C→9」「29→9」「無任何正向邊指向 6、唯 33→6」。

### D. 驗證(更新 2)

- `ctest`:**21/21 PASS**(原 20 + 新 `verify_city_entry`);`render_sweep` 154-case 不變。無回歸。
- `diff_trace.sh`:**remake VM == opendw oracle 逐指令一致**(op_8C 預設路徑未動)。
- `verify_mainline_reachable`:`邊統計:世界圖樞紐邊 27 … 直接gs[2]進入邊 16`;
  **從 area 1 / area 0 BFS 可達 38/40**(原 33/40);主線地表 15/16(唯缺 6)。
- 仍缺 **6 / 33**(隔離 {6,33} 分量,無外部入邊;逆不出,誠實記錄,不臆造)。
- docker `dwsdl`(remake build/ctest)、`dwtools`(diff_trace oracle);先 `rm -rf build build_*`。
- **未改 opendw;DRAGON.COM / 權威圖檔 未入庫。**
