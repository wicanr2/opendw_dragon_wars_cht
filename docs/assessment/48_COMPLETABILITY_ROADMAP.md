# 48 — 可通關性分析 + Roadmap(序盤 demo → 可通關完整 RPG)

> 日期:2026-06-15
> 對象:`opendw_remake/`(C++20 / SDL2 重製《火龍之戰》Dragon Wars, Interplay 1989)
> 方法:**唯讀分析**。讀攻略(主線真值)+ remake 現況文件 + remake 原始碼/資產 + 跑現有 `probe_areaswitch` / `verify_*` 觀察。本報告為唯一新增產物,未改 `src` / `CMakeLists` / 驗證程式 / git。
> 定位:接續 `docs/47_REMAKE_ASSESSMENT.md`(可玩性 62/100)的盤點,聚焦「**能不能從頭打到尾**」這條軸,並把缺口排成可執行的優先序。

---

## 0. 摘要(先讀這段)

- **主線真值已完整掌握**:攻略(38/39)把「波卡城開局 → … → 尼塞山腹決戰 Namtar」逐地點、逐事件、逐手冊段落串清楚了(§1)。SDA 勝利條件三件套(取回**自由之劍 Sword of Freedom**、解放/借力 **Irkalla 伊爾卡拉**、把 **Namtar 納達**送回深淵)在攻略結局段有對應敘述。
- **地圖層基礎扎實**:40 關 `.lvl` 真實地圖已抽出,**40 個關卡名 100% 對應攻略地區**(§2 表)。這是最高槓桿的既有資產 —— 「世界已經在那裡」。
- **但跨區連通是最大結構性缺口**:遊戲在 remake 裡**目前幾乎走不通主線**。實測(`probe_areaswitch`,含與不含 DATA1 皆同)全 40 關只找到 **8 個會改 area/入口的 tile 事件**,其中**只有 1 個是真正的跨區換場**(area 28 Old Dock → area 26 Pilgrim Dock),其餘 7 個都是 area 27 內部的成對樓梯(同區傳送)。原版主要靠 **area 0 世界圖(航海)** 與 **area 18 瑪根地底世界(隧道網)** 當「轉運樞紐」在地區間移動,而這兩張地圖**都是 wrap 邊界關卡 → opendw 自身未實作 → remake 明確跳過**(§3)。
- **三大缺口**:(A)**跨區連通**(wrap 樞紐 + 換區事件鏈未串)、(B)**戰鬥真值閉環**(結算公式已 bytecode 真值化,但卡在 op_89 後的「逐角色動作指派狀態機不收斂」)、(C)**主線內容/quest 體系**(事件在地化僅序盤 ~13 條、quest flag / 物品 gate / 結局判定未成體系)。
- **高槓桿結論**:**沒有一個「補 X 就能串起來」的單點解**。最接近的高槓桿項是 **wrap 邊界的最小可行實作(尤其 area 0 世界圖 + area 18 瑪根)**:這兩張一通,大量本來孤立的非 wrap 主線地區就能在玩家視角被「連起來」。但它**受架構阻**(oracle opendw 自身 `exit(1)` 未實作 wrap,需乾淨室補,且要重逆 tile-edge → area 的對映),屬中大型工項而非一行修補。

**一句話**:remake 目前是「40 座做好的房間 + 一條寫清楚的路線圖,但房間之間的門大多沒裝上」。要通關,**主戰場是連通(A),其次是戰鬥閉環(B)與內容體系(C)**。

---

## 1. 主線鏈 / 勝利條件(攻略真值,出自 docs/38、39)

### 1.1 勝利條件(三件套 + 終戰)

依攻略結局段(§5.20)與 SDA 共識,通關 = 完成下列鏈:

1. **鑄成自由之劍(Sword of Freedom)** —— 跨多地、多步驟(攻略 §5.11 列出「鑄劍 SOP」):
   - 解除 **Irkalla 伊爾卡拉** 的詛咒(她被銀鍊綁在瑪根地底世界小島,段落 137/138)。
   - 取得**水中呼吸藥水** → 進 **Sunken Ruins 沉沒之城(水下,area 22)** 尋英雄魂。
   - 解救矮人 → **Dwarf Forge 矮人鑄爐**(矮人城堡 area 16)請鐵匠鑄劍(把 Skull 骷髏頭交鐵匠)。
   - 劍經地獄之火、英雄羅拔精神、Apsu Waters 亞蘇水淬煉重生(段落 138)。
2. **借力 Irkalla / 永恆之神(Universal God)** —— 劍由 Irkalla 與 Universal God 祝福,一擊可削敵 100 點生命(攻略 §5.11)。
3. **取得 Dragon Gem 龍寶石** —— 在 **Dragon Valley 龍谷(area 32)** 取得;決戰時連續召喚 **Dragon Queen 龍后** 三次。
4. **終戰 Namtar 納達** —— 在 **Depth of Nisir 尼塞山腹(area 27)** 深處,以自由之劍劈死「深淵之獸 The Beast From The Pit」(納達),屍體送回瑪根 **Well of Souls 靈魂之泉**,走向 **Namtar's Pit 納達之坑** → 結局(攻略 §5.20)。

### 1.2 主線必經地區順序(攻略三期 → 對應 remake area id)

> area id 來自 `assets/bundle/maps/README.md`,該表「40 關卡名 100% 對應攻略 38/39」。

| 階段 | 地點(攻略) | area | wrap? | 主線角色 |
|---|---|:---:|:---:|---|
| 序盤 | Purgatory 波卡城(**起點**) | 1 | 否 | 開局,擊敗 Humbaba/盜賊王,出城 |
| 序盤 | Slave Camp 奴隸營 | 2 | 否 | 取市民證、線索 |
| 序盤 | Slave Mines 礦場 | **19** | **是** | 龍石/取得自由 |
| 序盤 | Guard Bridge 守橋 | 3 | 否 | 過路關卡 |
| 序盤 | Mog's Slave Estate 莫格宅院 | 37 | 否 | 線索 |
| 序盤 | Tars Ruins 塔斯廢墟(+地下) | 5 / **39** | 否/**是** | 地底世界入口 |
| 序盤 | Phoebus 菲巴斯城(+地牢) | 6 / 33 | 否 | 太陽神勢力、龍 |
| 中期 | Mystic Wood 神祕林 | 23 | 否 | Enkidu、Golden Boots(一隻) |
| 中期 | War Bridge / Lansk 蘭斯克(+地下城) | 11 / 20 / 38 | 否 | 官僚/船票/四神像 |
| 中期 | Yellow Mud Toad 黃泥蟾蜍城 | 8 | 否 | Lanac'toor 拉娜碎片、Ankh |
| 中期 | Lanac'toor's Lab 拉娜實驗室 | **34** | **是** | 拉娜碎片、Spectacles 線索 |
| 後期 | Smuggler's Cove 海盜竊穴 | 10 | 否 | Jade Eye、海盜醜約翰 |
| 後期 | Necropolis 奈羅波裡(陳屍所) | 14 | 否 | 復活亡魂 |
| 後期 | **Magan Underworld 瑪根地底世界(轉運樞紐)** | **18** | **是** | Irkalla、Well of Souls、鑄劍 |
| 後期 | Old Dock 老碼頭 | 28 | 否 | → Nisir(需朝聖者之袍) |
| 後期 | Bridge of Exiles 放逐橋 / Snake Pit 蛇窟 | 13 / 24 | 否 | King's Signet Ring |
| 後期 | Dwarf Ruins / Clanhall 矮人廢墟與城堡(鑄爐) | 15 / 16 | 否 | 救矮人、鑄劍 |
| 後期 | Siege Camp 軍營 → Byzanople 拜占儂(+地下) | 29 / 9 / **35** | 否/否/**是** | 救 Prince Jordan 喬丹王子 |
| 後期 | Kingshome 京雄城(+地牢) | 25 / 36 | 否 | 朝聖者之袍、王冠 |
| 後期 | Freeport 自由港 | 17 | 否 | 自由之劍傳說、Roba |
| 後期 | Game Preserve 皇家獵區 / Scorpion Bridge 蠍橋 | 30 / 12 | 否 | magic bow |
| 後期 | College of Magic 魔法學院(七測驗) | 31 | 否 | **Spectacles 眼鏡** / Soul Bowl |
| 後期 | Dragon Valley 龍谷 | 32 | 否 | **Dragon Gem 龍寶石** / 龍后 |
| 後期 | Sunken Ruins 沉沒之城(陸/水) | 21 / **22** | 否/**是** | 水中呼吸藥水、英雄魂 |
| 後期 | Pilgrim Dock 朝聖者碼頭 | 26 | 否 | → Nisir |
| 結局 | Salvation 救贖之山 / Nisir 尼塞山 | 4 / (Nisir) | 否 | 永恆神殿、Inferno |
| 結局 | **Depths of Nisir 尼塞山腹(終戰)** | **27** | **是** | **決戰 Namtar** |
| 全域 | **Dilmun 世界圖(航海樞紐)** | **0** | **是** | 地區間航行 |

**關鍵觀察**:主線**必經** wrap 關卡至少 8 個(area 0/18/19/22/27/34/35/39),其中:
- **area 27 尼塞山腹** = 最終決戰地(無它無法通關)。
- **area 18 瑪根地底世界** = 鑄劍/Irkalla/靈魂之泉所在 + 多地區的地底轉運樞紐。
- **area 0 Dilmun 世界圖** = 地表航海的轉運樞紐。
- **area 19/22/34/35/39** = 序盤礦場、沉沒之城水下、拉娜實驗室、拜占儂地下、塔斯地下 —— 各含主線物品/事件。

→ **wrap 關卡不是支線旁枝,而是主線骨幹。** 這直接決定了缺口優先序(§4)。

### 1.3 關鍵物品 / 法術 gate(攻略散見,需做成 quest 體系)

| 物品 / 法術 | 用途 | 取得地 | 性質 |
|---|---|---|---|
| Citizen Papers 市民證 / Governer's Pass 總督通行證 | 過守橋/海關 | 競技場 / Slave Camp / Lansk | gate(通行) |
| Golden Boots 黃金之靴(一雙) | 渡黃泥蟾蜍城泥水 | 神祕林 + 另一隻 | gate(地形) |
| Lanac'toor 拉娜碎片 ×4(頭/手/臂/身) | 重組拉娜觸發劇情 | 散布迪瑪大陸多地 | quest(收集) |
| Spectacles 眼鏡 | 看見地底世界入口 | College of Magic 七測驗 | gate(視覺) |
| Jade Eye 翠玉之眼 | 開矮人城堡通道 | Smuggler's Cove | gate(機關) |
| Skull 骷髏頭 | 交鐵匠鑄劍 | (鑄劍 SOP) | quest(鑄劍) |
| Pilgrim's Garb 朝聖者之袍 | 進 Nisir 尼塞山 | Kingshome | gate(通行) |
| 水中呼吸藥水 | 潛 Sunken Ruins 水下 | (鑄劍 SOP) | gate(地形) |
| **Sword of Freedom 自由之劍** | **終戰唯一可傷 Namtar** | 鑄爐重生 | **勝利條件** |
| **Dragon Gem 龍寶石** | 召喚龍后(終戰 ×3) | Dragon Valley | **勝利條件** |
| Soften Stone 軟化石(法術) | 穿牆進 Namtar 基地 | 法術書 | gate(地形) |
| Inferno 地獄之火(法術) | 尼塞山/淬劍 | Nisir 神殿 | gate |

**現況**:remake 道具/法術的**格式與表格層**已 grounded(`verify_equipment` / `verify_spells`,見 docs/47 §5),但**「使用物品改變世界狀態」「quest flag」「gate 判定」尚未成體系**(docs/47 §7 明列)。這是缺口 C 的核心。

---

## 2. 跨區連通現況(remake 實測)

### 2.1 area-switch 機制(讀碼 + 實測,出自 `src/main.cpp` `run_event`/`sync_relocation`)

踩格 → `op_71` → level script(VM)→ 事件腳本用 `op_11/op_12` 寫 `game_state[2]`(新 area)+ `gs[0]/gs[1]/gs[3]`(入口 X/Y/朝向)→ 跑完 `sync_relocation()` 比對 `gs[2]` vs `current_area`:

- `gs[2] == current_area` → **同區傳送**(只挪位置,回傳 1)。area 27 內部樓梯走這條,**已驗證**(`verify_areaswitch` 案例 1–6 PASS)。
- `gs[2] != current_area` → 載入目標 `.lvl`;**若目標 `flags & 0x2`(wrap 邊界)→ 明確跳過 + log + 還原 gs[2](回傳 -1)**;否則重載地圖換區(回傳 2)。

### 2.2 實測:全 40 關只有 8 個「會改 area/入口」事件

跑 `./build/probe_areaswitch assets/bundle`(以及 `--data1` 補齊跨資源 call,結果相同):

```
area 27 "Depths of Nisir" tile 0x1B..0x21 — 7 個同區成對樓梯(area 27 內部)
area 28 "Old Dock"        tile 0x06 @(6,3) — AREA 28->26 (Old Dock → Pilgrim Dock)  ← 唯一真跨區
== 8 area/pos-changing events found ==
```

**解讀**:
- **唯一一個真正的 tile-觸發跨區換場是 area 28 → 26**(且 26 非 wrap,理論上 remake 能走通這一步)。
- area 27 的 7 個是**同區**位置傳送(終戰地內部移動),不跨區。
- **其餘 38 關沒有任何「踩格寫 gs[2] 換到別區」的事件被掃到。** 加 DATA1 也一樣 —— 代表這**不是 bundle 缺 script 的問題**,而是**原版地區間移動主要不靠「tile 事件寫 area」這個機制**。

### 2.3 為何走不通:原版靠「世界圖 + 地底樞紐」轉運,而那是 wrap

`assets/bundle/maps/README.md` 的尺寸欄揭示:**area 0 Dilmun = 47×32 世界圖**、**area 18 Magan Underworld = 32×32 大地底圖**。攻略 §5.11 明說瑪根「上面有許多出入口連往地面」。Dragon Wars 的地表/地底移動,是玩家在這兩張大圖上**走到邊緣/出入口**而切換到對應地區 —— 屬「地圖邊界 wrap / exit」機制,**不是踩單格事件寫 area**。

而這兩張圖:
- `flags=14`(area 0)、`flags=22`(area 18)→ **bit1(0x2)= wrap 都成立**。
- `sync_relocation` 對 wrap 目標**明確跳過**(`src/main.cpp`:`"target uses wrap boundary (flag&2), opendw leaves this unimplemented"`)。
- 根因:**oracle opendw 自身對 wrap 邊界是 `exit(1)` 未實作**(故 remake 為求不假裝、不崩潰,選擇明確跳過 + log)。

實測 wrap 旗標(讀 `.lvl` 前 3 byte,bit1):**area 0/18/19/22/27/34/35/39 = wrap=1**,其餘 32 關 wrap=0。與 docs/47 / 任務描述完全一致。

### 2.4 「area23→0 stub bug 假象」澄清(已修正,非真連通)

`verify_areaswitch` 案例 7 註解明載:神祕林(area 23)tile 0x04 的敘述事件,**舊版**因「op_62 stub 永遠 `flags|=carry`」的**錯誤**,使 op_42 誤跳到換場分支、誤把 gs[2] 寫成 0(看似 area23→0 連通)。**補完忠實 op_62(byte-identical 對拍 opendw)後修正**:op_62 未命中 → 不寫 carry → op_42 不跳 → 落入空地敘述、gs[2] 維持 23(**不換場**)。

→ 結論:**area23→0 從來不是真連通,是 stub bug 製造的假象,現已消除。** 神祕林那格本就只是敘述事件。

### 2.5 連通現況總評:幾成走得通?

| 連通類型 | 現況 | 證據 |
|---|---|---|
| **同區傳送**(area 27 樓梯) | ✅ 走得通 | `verify_areaswitch` 1–6 PASS |
| **單一 tile 跨區換場**(area 28→26) | ⚠ 機制就緒,目標非 wrap,理論可走;但是孤例 | `probe_areaswitch` |
| **wrap 樞紐換區**(area 0 世界圖 / 18 瑪根 + 其餘 6 張 wrap) | ❌ 明確跳過(未實作) | `sync_relocation` flag&2 分支 |
| **非 wrap 地區「之間」的連通** | ❌ 幾乎無 tile 事件承載;原版靠 wrap 樞紐轉運 | `probe_areaswitch` 全掃只 8 hit |

**估計**:以「主線必經地區能否依序到達」衡量,**目前可串通的主線比例極低(< 10%)**。可單獨載入並在第一人稱探索的非 wrap 地區有 32 張(房間本身可玩),但**地區之間的主線路徑幾乎全斷**:序盤就卡(波卡城出城多半要經 Apsu Waters → 瑪根 area 18 = wrap;或競技場 Hide 出城後的下一段移動依賴世界圖 area 0 = wrap)。終戰地 area 27 本身是 wrap,連「進得去打 Namtar」都受同一阻礙。

> 註:area 27 內部樓梯能 PASS,是因為「同區傳送」不跨 wrap 邊界(`gs[2]` 不變);但「從別區**進入** area 27」仍是 wrap 換區,被擋。

---

## 3. 三大缺口盤點(對照「可通關」)

### 缺口 A — 跨區連通(最大結構性阻塞)

- **現象**:40 房間做好了,門沒裝。主線必經的 wrap 樞紐(world map area 0、Magan area 18)+ 6 張 wrap 地區全部跳過;非 wrap 地區之間缺承載換區的事件。
- **根因**:(1)wrap 邊界機制 oracle 自身 `exit(1)` 未實作(架構阻);(2)原版「走到地圖邊緣/出入口 → 切到對應地區」的對映(哪條邊 / 哪個出入口 → 哪個 area + 入口座標)**未逆向**,probe 只能掃 tile-事件,掃不到 edge/exit 機制。
- **可達性心法(flood-fill 連通分量)**:把 area 當節點、可走通的換區當邊。目前邊集合 ≈ {area27 內部成對, area28→26}。**從落點 area 1(波卡城)出發的連通分量幾乎只含 area 1 自己**(出城的下一跳就撞 wrap)。要讓連通分量涵蓋全部主線地區,**必須補上 wrap 樞紐這條「主幹邊」**。
- **分級**:**受架構阻 + 需逆向工**(非單點修補)。但**槓桿最高** —— 通了 area 0 + area 18,大量孤立地區一次被接上。

### 缺口 B — 戰鬥真值閉環(卡在動作指派狀態機)

- **現況**(docs/42 §11–14 + 47 §4):**結算公式已 bytecode 真值化**並端到端對拍 ——
  - to-hit:`roll = 1d16+3`,`HIT ⟺ roll ≤ 13 + AV − (DV+AC)`(✅ bytecode 真值)。
  - 徒手傷害:`骰 + floor(STR/5)`(✅ bytecode 真值,證偽舊 ×3/2 假說)。
  - 武器主傷害骰來源/解碼(op_68 0x08 = byte[8])、武器定傷(byte[2]!=0):✅(第七輪反組譯 DRAGON.COM op_68 handler)。
- **卡點**:不是 opcode 缺失(`last_unimpl=0`),而是 **op_89 之後「逐角色動作指派狀態機不收斂」**(res18 主選單 → res4 目標選擇 → 角色動作選單三層 op_89,headless 餵完 Fight→Attack×4 後 gs[6] 卡在 3、迴圈不退出 → 到不了 actor 迴圈 res3@0x0075 → 怪物 HP 不被扣)。
- **重要修正**:docs/47 §3/§10 仍寫「op_89(res3@0x08b6)未逆出 → 戰鬥走不完」;docs/42 第八輪已澄清 **op_89 本身已實作**,真正卡點是「per-character 動作完成標記 + 全員完成偵測」這個跨 res3/res18/res4 的互動狀態機(res3@0x08b6 的動作輸入 driver 計數器)。**這是文件漂移,本報告以 docs/42 最新輪為準。**
- **另一根本限制**:opendw C 碼**無法獨立跑一場完整戰鬥並 dump 逐回合 char_data** → 即使狀態機補齊,「byte-identical HP 對拍」仍需先在 opendw 加 headless 戰鬥入口 + instrumentation(無現成 oracle 路徑)。
- **分級**:**部分受 oracle 限制**。狀態機收斂屬可逆向工(已定位到 op/gs 層級);但「真值對拍 HP」受 oracle 缺路徑阻。**對「能不能玩戰鬥」**:互動主迴圈進得去、能下令、能看勝負(`combat_loop.cpp` 確定性模型),只是**數值閉環未經 oracle 蓋章**。

### 缺口 C — 主線內容 / quest 體系 / 結局

- **事件在地化僅序盤**:`assets/i18n/zh-TW/events.tsv` 僅 ~13 條(全為波卡城序盤),完整遊戲 ~100+。日文 events 13 條 100%(X68000 反萃取,亮點),其餘層薄。
- **quest flag / 物品 gate / NPC 狀態機未成體系**(docs/47 §7):§1.3 列的市民證/拉娜碎片/眼鏡/自由之劍鑄造鏈/朝聖者之袍等 gate,目前**沒有「持有 X → 改變世界狀態 / 解鎖 Y」的判定層**。
- **結局觸發未實作**:決戰 Namtar 後的「屍體送靈魂之泉 → 走向納達之坑 → 結局畫面」整條未做(且決戰地 area 27 受缺口 A 阻,進不去)。
- **段落書(防拷手冊)完整**:1–147 已轉寫並 bundle(`assets/bundle/paragraphs/`),ParaViewer 可捲動 —— 這層**已就緒**,不算缺口。
- **分級**:**需大量內容工 + 設計工**(quest 體系是 remake 待做,非 oracle 阻;在地化是純內容工;結局是設計 + 觸發)。

---

## 4. Roadmap(優先序:每項標 價值 / 難度 / 阻力性質)

> 阻力性質:**[可做]** = 乾淨室/逆向可推進,無硬 oracle 阻;**[受 oracle 阻]** = oracle opendw 缺對應實作或無可對拍路徑;**[架構阻]** = 需動既有對拍資產或大改結構;**[內容工]** = 主要是翻譯/資料填充量。
> 價值 = 對「可通關」的貢獻;難度 = 工程量級。

### P0 — 解鎖主幹連通(最高槓桿,但是中大型工項)

| # | 項目 | 價值 | 難度 | 阻力 |
|---|---|:---:|:---:|---|
| P0-1 | **逆向 wrap 邊界 / 地圖出入口 → area 的對映**(尤其 area 0 世界圖、area 18 瑪根):釐清「走到哪條邊 / 哪個出入口格 → 切到哪個 area + 入口座標」 | 極高 | 高 | **[受 oracle 阻]** opendw 自身 `exit(1)`,須乾淨室補 + 從 DATA1 地圖資料 / DRAGON.COM 反組譯重建邊界轉移表 |
| P0-2 | **實作 wrap 邊界換區**(在 `sync_relocation` / `enter_map` 補 wrap 分支,取代目前「明確跳過」) | 極高 | 中高 | **[架構阻]** 需確保不破壞既有 154 case viewport / minimap 對拍;wrap 移動的渲染與座標換算要對齊 |
| P0-3 | **驗證主線 flood-fill 連通**:補一支 `verify_mainline_reachable`,從 area 1 出發確認能依 §1.2 順序到達全部主線地區(含 wrap) | 高 | 中 | **[可做]** 屬測試/驗證,串接 P0-1/P0-2 後即可寫 |

> **這是「補 X 就能串起來」最接近的答案 —— 但 X 不小**:P0-1+P0-2 通了世界圖(area 0)與瑪根(area 18),連通分量會從「幾乎只有波卡城」一次擴張到涵蓋大部分主線地區。**這是 demo → 可通關的關鍵轉折點**,但屬中大型逆向 + 實作,非一行修補。

### P1 — 換區事件鏈 + 物品/通行 gate(連通之後立刻需要)

| # | 項目 | 價值 | 難度 | 阻力 |
|---|---|:---:|:---:|---|
| P1-1 | **補齊主線 op_58 事件 script 進 bundle**:目前 `event_script_tags=[0,1,3,5,8,9,10,11,17,19]`(10 個 tag);掃全 40 關主線事件實際 call 的跨資源 tag 聯集,補進 `assets/bundle/scripts/` | 高 | 中 | **[可做]** `extract_eventscripts` 工具已存在;先用 `--data1` 跑 probe 列出缺的 tag |
| P1-2 | **quest flag / 物品 gate 判定層**:把 §1.3 的 gate(市民證過橋、Golden Boots 渡泥、Spectacles 看入口、Jade Eye 開城堡、朝聖者之袍進 Nisir)做成「持有/狀態 → 解鎖」 | 高 | 中高 | **[可做]**(remake 設計);需先把對應事件 script 跑通看原版判定條件 |
| P1-3 | **拉娜碎片 ×4 收集 + 重組觸發**(主線轉折) | 中 | 中 | **[可做]** 需逆向碎片事件的 game_state flag |

### P2 — 戰鬥真值閉環(可玩已達標,差 oracle 蓋章)

| # | 項目 | 價值 | 難度 | 阻力 |
|---|---|:---:|:---:|---|
| P2-1 | **逆向 res3@0x08b6 動作指派 driver**:逐角色「已選動作」標記 + 全員完成偵測,讓 headless / 互動主迴圈跑到 actor 迴圈(res3@0x0075)、怪物 HP 真扣 | 中高 | 中高 | **[可做]** 已定位到 op/gs 層級(docs/42 §14);非 opcode 缺失 |
| P2-2 | **opendw 加 headless 戰鬥入口 + dump char_data** → 建立可對拍的「整場戰鬥 oracle」 | 中 | 中 | **[受 oracle 阻]** 需動 oracle 程式(目前 opendw 無法獨立跑一場戰鬥輸出逐回合 HP) |
| P2-3 | 武器 STR bonus 真值定論(self-modifying-code 矛盾) | 低 | 中 | **[受 oracle 阻]** 需 P2-1+P2-2 跑完整場武器攻擊觀察自改碼殘留;在此之前維持 best-fit 標示 |

> **戰鬥對「可通關」不是死結**:即便 P2-2/P2-3 卡 oracle,`combat_loop.cpp` 的確定性模型已能讓玩家打完一場(進入/下令/勝負/XP),足以支撐通關流程;P2 是把數值從「grounded 模型」升級成「oracle 真值」,屬品質而非可玩性阻塞。終戰 Namtar 的「自由之劍一擊削 100 HP」可先用 grounded 模型實現。

### P3 — 內容在地化 + 結局(大量內容工,無硬阻)

| # | 項目 | 價值 | 難度 | 阻力 |
|---|---|:---:|:---:|---|
| P3-1 | **事件文字在地化全覆蓋**(events.tsv ~13 → ~100+ 條 zh-TW;ja 補齊) | 高 | 中 | **[內容工]** VM emit 的英文鍵已可逐條抽出;翻譯量大但無技術阻 |
| P3-2 | **結局事件**:決戰 Namtar 後屍體送靈魂之泉 → 納達之坑 → 結局畫面/段落 | 高 | 中 | **[可做]**(設計 + 觸發);依賴 P0(進得去 area 27) |
| P3-3 | NPC 對話狀態機 / 連貫劇情推進(把段落 + 事件串成有因果的主線體驗) | 中 | 中高 | **[可做]**(remake 設計) |
| P3-4 | 譯名收斂(Lanac'toor 拉娜/拉哥、Nergal/Namtar 區分、Enkidu/Utnapishtim 待核 → 進 CONTEXT.md) | 中 | 低 | **[內容工]** docs/38 §7 Flagged 已列 |

### 依賴關係(關鍵路徑)

```
P0-1 (逆向 wrap 對映) ──► P0-2 (實作 wrap 換區) ──► P0-3 (主線連通驗證)
                                  │
                                  ├──► P1-1/P1-2/P1-3 (事件鏈 + gate;連通後才有意義)
                                  │
                                  └──► P3-2 (結局;需進得去 area 27)
P2-1 ──► P2-2 ──► P2-3   (戰鬥真值;與連通正交,可平行)
P3-1 / P3-4              (內容工;可隨時平行推進)
```

**最短「可通關」路徑**:**P0(全)→ P1-1/P1-2 → P3-2 + P3-1(主線必要事件)**。戰鬥用既有確定性模型(P2 留後)。

---

## 5. 結論

### 5.1 主線鏈 + 勝利條件

攻略真值清楚:**波卡城開局 → 序盤逃脫(奴隸營/礦場/塔斯/菲巴斯)→ 中期(神祕林/蘭斯克/黃泥蟾蜍/拉娜實驗室)→ 後期(海盜竊穴/陳屍所/瑪根/矮人鑄爐/拜占儂救王子/京雄城/自由港/魔法學院取眼鏡/龍谷取龍寶石/沉沒之城)→ 結局(尼塞山腹決戰)**。勝利 = **鑄成自由之劍 + 借 Irkalla/永恆神之力 + 取 Dragon Gem 召龍后 ×3 + 尼塞山腹劈死 Namtar → 屍體送靈魂之泉 → 納達之坑**。40 個 remake area 與這條鏈 1:1 對應(§1.2 表)。

### 5.2 跨區連通現況(走通幾成)

**目前主線幾乎走不通(連通 < 10%)**。實測全 40 關只有 8 個改 area/入口的 tile 事件,真跨區的僅 area 28→26 一例;其餘靠 wrap 樞紐(世界圖 area 0、瑪根 area 18)+ 6 張 wrap 地區轉運,而 wrap 因 oracle 自身未實作被全數跳過。area23→0「連通」是已修正的 stub bug 假象。**40 座房間可單獨進去探索,但門大多沒裝。**

### 5.3 三大缺口

- **A 跨區連通**:最大結構性阻塞;wrap 樞紐未實作 + edge/exit→area 對映未逆向。**受 oracle/架構阻 + 需逆向工**。
- **B 戰鬥真值閉環**:結算公式已 bytecode 真值化,卡在逐角色動作指派狀態機(非 opcode 缺失);整場 HP 對拍受 oracle 無獨立戰鬥路徑阻。**對可玩性不是死結**(確定性模型已可打完一場)。
- **C 主線內容/quest/結局**:事件在地化僅序盤、quest flag/gate/結局觸發未成體系。**需大量內容工 + 設計工,無硬技術阻**。

### 5.4 高槓桿項(「補 X 就能串起來」?)

**沒有單點解,但最接近的高槓桿是 P0(wrap 樞紐連通)** —— 通了 **area 0 世界圖** 與 **area 18 瑪根地底世界** 這兩條主幹邊,連通分量會從「幾乎只剩波卡城」一次擴張到涵蓋大部分主線地區,是 demo → 可通關的關鍵轉折。代價:**它受 oracle(opendw `exit(1)`)與架構(不破壞既有像素對拍)阻,屬中大型逆向 + 實作工項**,而非一行修補。其餘高價值低阻力的「現在就能做」項是 **P1-1(補主線事件 script 進 bundle)** 與 **P3-1(事件在地化全覆蓋)**,可在 P0 推進的同時平行進行。

---

## 附:本報告引用的實據(檔案絕對路徑)

- 主線真值:`docs/38_SOFTWORLD_WALKTHROUGH.md`、`docs/39_SOFTWORLD_FULLTEXT_AND_MAPS.md`
- remake 現況:`docs/47_REMAKE_ASSESSMENT.md`、`docs/42_COMBAT_BYTECODE.md`(§11–14 戰鬥真值/卡點)
- area 對照表:`opendw_remake/assets/bundle/maps/README.md`(40 關名 ↔ 攻略)
- area-switch 機制:`opendw_remake/src/main.cpp`(`run_event` / `sync_relocation` / `enter_map`,wrap flag&2 跳過分支)
- 連通實測:`opendw_remake/build/probe_areaswitch`(`assets/bundle` 與 `--data1`,皆 8 hit);`opendw_remake/tools/verify/verify_areaswitch.cpp`(案例註解:area23→0 stub bug 假象修正)
- wrap 旗標:全 40 關 `.lvl` 前 3 byte bit1(area 0/18/19/22/27/34/35/39 = wrap)
- bundle 覆蓋:`opendw_remake/assets/bundle/manifest.json`(`event_script_tags` / `combat_script_tags`)、`assets/bundle/scripts/`(16 個 .bin)
- 在地化:`opendw_remake/assets/i18n/zh-TW/events.tsv`(~13 條序盤)、`assets/bundle/paragraphs/`(段落 1–147 完整)
