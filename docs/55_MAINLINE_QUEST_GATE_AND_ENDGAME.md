# 55 — 主線 quest gate 依賴鏈 + 結局事件狀態(bytecode 動態 trace + 攻略交叉)

> 日期:2026-06-16
> 對象:`opendw_remake/`(C++20/SDL2 重製《火龍之戰》Dragon Wars, Interplay 1989)
> 方法:**bytecode 動態 trace**(remake VM 實機跑每 area 每事件格 script)+ 攻略 38/39 真值
>   + CONTEXT.md 譯名。grounded;逆得出才接,卡點精確記錄不臆造。
> 接續:docs/48(可通關 roadmap §1 勝利條件)、docs/52(主線事件字串)、docs/54(可達性 38/40)。
> 新增工具:`tools/verify/trace_quest_gates.cpp`(逆出 flag 依賴)、`tools/verify/render_endgame.cpp`
>   (結局/gate 事件 → VM → 繁中 → PPM 截圖)。
>
> **誠實標示**:gate 邏輯(op_9b/9d/50 旗標、op_8c 確認、op_61 角色屬性)**全部跑得動、零卡點**
>   (事件 emit 路徑);唯二未實作 opcode(op_6B、op_8D)只擋「同意付命/說暗語」的選擇分支,
>   有合法繞道。**終戰 Namtar 是 combat encounter(op_8A),卡在全戰鬥閉環的遊戲層 context
>   (已知 docs/42,非 opcode 缺失 last_unimpl=0);結局訊息不在任何 level script 中**(= 勝利
>   後由戰鬥流程觸發,逆不出獨立結局事件)。

---

## 0. 結論摘要

| 項目 | 狀態 |
|---|---|
| 主線勝利條件鏈(攻略真值) | 已串清:鑄自由之劍 + Irkalla/永恆神祝福 + Dragon Gem 召龍后×3 → area 27 決戰 Namtar → 屍體送靈魂之泉 → 納達之坑 |
| quest gate 系統(bytecode) | **逆出**:level script 用 **game_state bit 旗標**(op_9b 設 / op_9d,op_50 測 → jnz/jz 分支)做進度 gate。共 **135 個唯一 flag、313 次操作**。 |
| 角色祝福旗標(flags[85]) | char_data byte 0x55:`0x80` Irkalla 祝福、`0x10` 永恆之神(+3 全屬性)、`0x20` Enkidu(德魯伊)。由 op_5F 設 / op_61 測(戰鬥/共享 script,非 level event)。 |
| gate 事件能跑嗎 | **能**。全 40 關事件格動態 trace:**唯二 halt = op_6B(area 18 tile 0x0D + area 0 世界圖 26 格)、op_8D(area 33 tile 0x14)**,皆 opendw `targets[]`=NULL 無 oracle,且只擋「選 Yes」後分支(有繞道)。 |
| 結局事件能觸發+跑完嗎 | **事件文字全部能觸發+跑+繁中顯示**(area 27 尼塞山腹 29 條 emit 零 halt;area 18 瑪根納達之坑/靈魂之泉/Irkalla 亦跑通)。**但決戰 Namtar 是戰鬥(op_8A),卡全戰鬥閉環(docs/42);勝利後的結局畫面不在 level script 中 → 逆不出獨立結局事件 script。** |
| 在地化 | events.tsv +20 條(area 18 瑪根全段 + area 33 菲巴斯地牢 + area 26/32 結局相關);area 27 結局段先前已譯齊。 |
| 截圖驗證 | `render_endgame` headless 跑事件 → 繁中渲染 6 張(`docs/screenshots/endgame/`)。 |
| 回歸 | ctest **21/21**;未改 VM source(interpreter/vm_state),diff_trace 邏輯不受影響。 |

---

## 1. 勝利條件鏈(攻略 38 §5.20 真值 → remake area/事件對映)

```
[序盤] area 1 波卡城 開局(裸身被丟入,Namtar 之令)
   │  gs 旗標:波卡城各事件設「已見」flag(gsB9.1/.3/.4/.5,res=0x47)
   ▼
[鑄劍鏈] —— 攻略 §5.11 鑄劍 SOP
   ├─ area 18 瑪根地底世界:解除 Irkalla 詛咒(她被銀鍊綁住,段落 137)
   │     → tile 0x10「唯有 Irkalla 信徒方可通行」gate(op_9d 測進度旗標)
   │     → Well of Souls 靈魂之泉(復活亡魂)+ Namtar's Pit 納達之坑(tile 0x0A
   │       「孕育納達的深淵」)
   ├─ area 22 沉沒之城(水下):水中呼吸藥水 → 取英雄魂(角色祝福 flags[85])
   ├─ area 16 矮人城堡(Dwarf Forge 鑄爐):交 Skull 鑄劍
   └─ 自由之劍經 地獄之火(Inferno)+ 英雄羅拔精神 + Apsu Waters 淬煉重生(段落 138)
   ▼
[祝福] Irkalla(0x80)+ 永恆之神 Universal God(0x10)祝福自由之劍 → 一擊削 100 HP
   ▼
[召龍] area 32 龍谷:取 Dragon Gem 龍寶石(決戰時召 Dragon Queen 龍后 ×3)
   │     → tile 0x0D gate(op_9d 測 gsAA.1/gsB9.6)+ 4 個龍 encounter(op_8A)
   ▼
[終戰] area 27 尼塞山腹(Depths of Nisir):
   │   Mystalvision(太陽高階祭司)→ Buck Ironhead(Namtar 首席將軍)→ 軍團決戰
   │   → 以自由之劍劈死 Namtar(深淵之獸 The Beast From The Pit)
   ▼
[結局] 屍體送 area 18 靈魂之泉 → 走向納達之坑 → 結局(攻略「享受最後勝利的快樂」)
```

### 1.1 關鍵物品/flag gate(攻略散見 + bytecode 出處)

| gate | 攻略用途 | bytecode 機制 | 出處 |
|---|---|---|---|
| 進 Magan「Irkalla 信徒」門 | 過瑪根妖精關 | area 18 tile 0x10 op_9d 測進度旗標 → jnz 擋路 | `trace_quest_gates 18` |
| 「付命為代價」妖精關 | 另一條瑪根通道 | area 18 tile 0x0D op_9d×2 + op_8c(Yes/No)→ **Yes 分支 op_6B(NULL)** | 同上 |
| 龍谷取龍寶石 | 召龍后 | area 32 tile 0x0D op_9d 測 gsAA.1/gsB9.6 + op_9b 設 gsB9.6 | `trace_quest_gates 32` |
| area 27 終局段門 | Mystalvision/Buck「已見」 | tile 0x12 測 gsA7.0、tile 0x22 測 gsA7.1、tile 0x26 測 gsB9.2/gsB9.7 | `trace_quest_gates 27` |
| 自由之劍祝福 | flags[85] | char_data 0x55:op_5F 設 0x80(Irkalla)/0x10(神)、op_61 測 | docs/44 §1;interpreter op5F/op61 |

---

## 2. quest gate 系統(bytecode 逆出)

### 2.1 兩套旗標層

1. **game_state bit 旗標(level event script)** —— **進度/事件 gate**。
   - `op_9B set_gs_bit`:寫旗標(= 某事件已發生)。
   - `op_9D test_gs_bit` / `op_50 test_gs_bit`:測旗標 → 下一條 `op_45 jnz`(已設則跳過,
     = 「已見/已完成 → 不重播」)。
   - 旗標編碼(對照 `interpreter.cpp` `get_bit_mask` + op9B/9D/50):
     `flag_byte = byte_base + (bit_no>>3)`、`flag_bit = bit_no & 7`。
     op_9B/9D 的 `bit_no`=script 立即數;op_50/4E/4F 的 `bit_no`=r2(前一 op_09 設)。
2. **char_data bit 屬性(戰鬥/共享 script)** —— **角色狀態/祝福**(flags[85]=byte 0x55)。
   - `op_5F or_char_data`(設 bit)、`op_60 and_char_data`(清)、`op_61 test_char_prop`(測)。
   - **未在任何 level event script 出現**(只在 res3 戰鬥 / 共享 resource);故主線「進城/開門」
     gate 走 game_state bit,**祝福/詛咒**走 char_data bit。

### 2.2 flag 依賴統計(`trace_quest_gates assets/bundle`,全 40 關)

- **唯一 flag 135 個、操作 313 次**。
- **多數為 intra-area「已見」旗標**(同 area set+test,如樓梯 Y/N、locked chest、雕像敘述)。
- **跨 area 進度旗標 = gsB9.x**(真正的主線 gate):
  | flag | SET areas | TEST areas | 推測語意 |
  |---|---|---|---|
  | gsB9.1 | 1,5,6,13,24 | 24 個 area | 全域「主線開局/通用進度」 |
  | gsB9.2 | 25,29,30 | 12 個(含 **27**) | 後期進度(京雄/軍營/獵區 → 影響終局段) |
  | gsB9.3 | 1,16,33,36 | 1,7,10,22,23,25 | 中期(矮人/地牢) |
  | gsB9.4 | 15 個 area | 1,7,18 | 通用「已造訪」(含 **18 瑪根**) |
  | gsB9.7 | 7,16 | 7,**27** | 影響 area 27 終局段(tile 0x26) |
- **TEST 但 level script 無 SET 的 flag**(= 由「使用/取得道具」在 inventory/共享 script 設,
  非 level event):如 gs00.4(8 area 測,含 19/29/34)、gs01.4(3,8,23,35)等 —— 對應攻略
  「市民證 / 朝聖者之袍 / 眼鏡 / 龍寶石」這類**物品 gate**(set 點不在 level event script,
  逆不出 set 來源 → 記錄不臆造)。

> **誠實標示**:flag→語意對映(除 gsB9.x 由「set/test area 集合 × 攻略地點」推得)多屬
>   **結構性逆出**(知道是什麼旗標、誰設誰測),**精確語意**(「這個 bit = 拿到自由之劍?」)
>   需端到端跑完整主線才能釘死,目前未做;不臆造單一 flag 的劇情語意。

### 2.3 gate 邏輯跑得動嗎 → 跑得動(零卡點,唯二 NULL opcode 有繞道)

`mainline_events assets/bundle`(全 40 關每事件格跑 VM):

| halt opcode | 格數 | 位置 | opendw oracle | 主線影響 |
|---|---|---|---|---|
| op_6B | 26 | area 0 世界圖格 | NULL | app 走獨立 worldmap_dest 進城,非主線阻斷(docs/54) |
| op_6B | 1 | **area 18 tile 0x0D** | NULL | 「同意付命為代價」**Yes 分支**才撞;tile 0x10 Irkalla 信徒門是另一條合法通道 → 有繞道 |
| op_8D | 1 | **area 33 tile 0x14** | NULL | 「說暗語」puzzle 的 say-word 分支;area 33 本身為隔離 {6,33} 分量(docs/54) |

- **所有 op_9b/9d/50/8c/61 gate 邏輯全部執行、分支正確**(op_8c「Do you wish…?」headless
  預設 No → 不換區;注入 'Y' 可走 Yes 分支,見 trace_subarea_dyn)。
- **分支正確性實證**(area 27 tile 0x22 Buck Ironhead):`9d test_gs_bit(gsA7.1) → 45 jnz`。
  旗標未設(首見)→ zf=1 → jnz 不跳 → emit「鐵頭巴克…」;旗標已設(重訪)→ 跳過 emit。
  = 標準「已見抑制」gate,**邏輯正確**。

---

## 3. 結局事件(area 27 尼塞山腹 + area 18 納達之坑)

### 3.1 能觸發 + 跑完 + 繁中顯示 —— 事件文字層 ✅

`mainline_events assets/bundle 27`:**29 條 emit、零 halt**。完整結局敘事鏈(順序):

1. tile 0x03「你穿越了許多哩、許多世界,落在濕冷的岩石上…」(墜入尼塞山腹)
2. tile 0x06「啊啊啊!是陷坑!!」+「這裡想必就是地獄…你永遠找不到 Namtar,歐西納
   永遠不會自由!」(失敗陷阱訊息)
3. tile 0x0C「黑暗中一個聲音響起:『你終於來了…我承認我低估了你。』」(Namtar 現身,
   段落 131/132/135)
4. tile 0x12 Mystalvision 太陽高階祭司、tile 0x22 **Buck Ironhead 鐵頭巴克(Namtar 首席將軍)**
5. tile 0x23-0x26 軍團決戰平原:「南方是 Namtar 的大軍!」「你獨自對抗整支軍隊!」

`mainline_events assets/bundle 18`(瑪根):tile 0x0A「孕育納達的深淵」(納達之坑)、
tile 0x10「唯有 Irkalla 信徒方可通行」、tile 0x12「黑暗中你發現一口井」(疑靈魂之泉)。

**全部 headless 跑通 + render_endgame 渲染繁中(截圖見 §5)。**

### 3.2 需戰鬥嗎 → 需。終戰 Namtar = combat encounter,卡全戰鬥閉環

- area 27 有 **2 個 op_8A encounter 觸發格**(tile 0x18/0x19);龍谷(32)4 個、瑪根(18)2 個。
- 終戰 Namtar / Buck Ironhead 是 **戰鬥(op_8A → res3 combat script)**,不是敘事 op_78。
- **卡點 = 全戰鬥閉環的遊戲層 context**(docs/42 §14):結算公式(to-hit 1d16+3 門檻 13+AV−def、
  徒手傷害 dice+STR/5、武器骰 op_68 0x08)**已 bytecode 真值化 + 端到端對拍**;但
  **per-character 動作指派狀態機不收斂**(res18↔res4 逐角色動作選單迴圈,headless 餵
  Fight→Attack×4 後 gs[6] 卡 3、到不了 actor 迴圈 res3@0x0075 → 怪物 HP 不被扣)。
  **非 opcode 缺失(last_unimpl=0)**,是跨 res3/res18/res4 的互動狀態機 + 「opendw 無法
  獨立跑一場完整戰鬥 dump 逐回合 char_data」雙重 oracle 限制。
- → **終戰本身的戰鬥閉環沿用 docs/42 既有卡點,本任務不重複攻(誠實記錄,不謊稱可打完)**。

### 3.3 結局訊息/段落是什麼

- **決戰/結局對話**:攻略 §5.20 註明手冊段落 **131/132/135**(Namtar 現身)、**120/134**(龍后)、
  **137/138**(Irkalla 與自由之劍重生);段落書已完整 bundle(`assets/bundle/paragraphs/`)。
- **勝利後結局畫面**:**不在任何 level event script 的 emit 中**。掃全 40 關 level script,
  area 27 最深的敘事 emit 止於「軍團決戰平原」(tile 0x23-0x26);**無「你劈死了 Namtar /
  歐西納自由了 / 結局」這類勝利 emit**。
  → 推論:**結局畫面由「戰勝 Namtar combat」後的戰鬥流程 / DRAGON.COM 主控觸發**(非 tile
  event script),與終戰戰鬥閉環綁在同一卡點。**逆不出獨立結局事件 script → 不臆造**(鐵則)。
  攻略結局文字僅「**勝利後請享受最後勝利的快樂吧! ✛**」,無獨立結局段落編號。

---

## 4. gate 依賴圖(需要什麼才能推進到結局)

```
                    [game_state 進度旗標(level event,op_9b 設 / op_9d 測)]
                                      │
  波卡城開局 ──gsB9.1──► 序盤各區 ──gsB9.4(15 區設「已造訪」)──► 中後期
       │                                                            │
       │                          [物品 gate(set 點不在 level event → 逆不出 set)]
       │                          市民證/朝聖者袍/眼鏡/龍寶石 = gs00.4/gs01.4… (TEST-only)
       ▼                                                            ▼
  area 18 瑪根 ◄──gsB9.4 測──┐                          area 32 龍谷 ──gsAA.1/gsB9.6──► Dragon Gem
   ├ tile 0x10 Irkalla 信徒門(op_9d gate)                          │
   ├ tile 0x0D 付命門(op_9d×2 + op_8c → Yes:op_6B NULL,有繞道)    │
   ├ Well of Souls 靈魂之泉(tile 0x12)                            │
   └ Namtar's Pit 納達之坑(tile 0x0A)                            │
       │                                                            │
       │           [char_data 祝福旗標 flags[85]:op_5F 設 / op_61 測]
       │           Irkalla 0x80 + Universal God 0x10 → 自由之劍一擊 100 HP
       │                                                            │
       └────────────────────────┬───────────────────────────────────┘
                                 ▼
                  area 27 尼塞山腹(Depths of Nisir)
                   ├ gsB9.2(25/29/30 設)測 → 終局段門
                   ├ gsB9.7(7/16 設)測 → tile 0x26 軍團段
                   ├ tile 0x12/0x22 gsA7.0/A7.1「已見」抑制
                   └ tile 0x18/0x19 op_8A encounter = 終戰 Namtar
                                 │
                                 ▼  ★ 卡點:全戰鬥閉環(docs/42,遊戲層 context)
                          [結局:勝利後觸發,不在 level script → 逆不出]
```

**推進到結局所需(grounded)**:
1. 跨區連通到 area 18 / 27 / 32(docs/54:38/40 可達,主線地表 15/16)。
2. game_state 進度旗標(gsB9.x)由序盤→後期事件**自動累積**(gate 邏輯已跑通)。
3. 物品 gate(市民證/朝聖者袍/眼鏡/龍寶石/自由之劍)—— **flag 被測得到,set 來源在
   inventory/共享 script,逆不出 set 點**(記錄,非臆造)。
4. char_data 祝福旗標(Irkalla/神)—— op_5F/61 已實作,戰鬥/共享 script 設。
5. **終戰 Namtar combat + 結局觸發 —— 卡全戰鬥閉環(docs/42)**,本任務不謊稱已打通。

---

## 5. 驗證(headless 截圖 + 回歸)

### 5.1 結局/gate 事件 → VM → 繁中渲染(`render_endgame`)

`docs/screenshots/endgame/`(320×200 indexed PPM→PNG,黑底白字 + 火龍之戰標題):

| 檔 | area/tile | 英文鍵 | 繁中 |
|---|---|---|---|
| a27_buck.png | 27 / 0x22 | Buck Ironhead, Namtar's top general… | 納達麾下的首席將軍——鐵頭巴克… |
| a27_mystalvision.png | 27 / 0x12 | At the center of the Solarium… Mystalvision… | 在日光殿的中央…太陽高階祭司密斯塔維恩… |
| a27_hell.png | 27 / 0x06 | Arrrggh! A Pit!! | 啊啊啊!是陷坑!! |
| a18_pit.png | 18 / 0x0A | …the very pit that spawned Namtar. | …從孕育出納達的那座深淵中升起。 |
| a18_irkalla.png | 18 / 0x10 | …Only worshippers of Irkalla may pass. | …唯有伊爾卡拉的信徒方可通行。 |
| a18_priceoflife.png | 18 / 0x0D | …demand the price of life… Do you consent? | …索求生命為代價…你同意嗎? |

→ **結局 + quest gate 事件確定能觸發 + 跑 + 繁中顯示**(CJK atlas 24×24,字形清晰無缺字)。

### 5.2 回歸

- **ctest 21/21 PASS**(含 vm_selftest、render_sweep 154-case、verify_combat*、verify_areaswitch、
  verify_city_entry、verify_i18n、smoke_app)。無回歸。
- **未改 VM source**(`src/vm/interpreter.{hpp,cpp}`、`vm_state.hpp`)→ diff_trace 逐指令對拍
  邏輯不受影響(本輪只加 2 個 standalone 觀測工具 + 渲染資產 + 資料檔)。

---

## 6. 改動檔案

- **新增工具**:
  - `opendw_remake/tools/verify/trace_quest_gates.cpp`(逆出 flag 依賴:op_9b/9d/50/4e/4f
    解碼成 (gs_byte,bit) + encounter 偵測)。
  - `opendw_remake/tools/verify/render_endgame.cpp`(area/tile 事件 → VM → i18n → CJK 渲染 PPM)。
  - 二者已註冊 `CMakeLists.txt`(非 ctest;grounding/觀測 + 截圖)。
- **資料/資產**:
  - `opendw_remake/assets/i18n/zh-TW/events.tsv`:+20 條(area 18 瑪根全段 + area 33 菲巴斯地牢
    + area 26/32 結局相關;area 27 結局段先前已譯齊)。
  - `opendw_remake/assets/fonts/cjk24.atlas`:重生(1878→2017 glyph,補新譯文 18 字:伊爾庫…)。
  - `opendw_remake/docs/screenshots/endgame/*.png`:6 張結局/gate 繁中截圖。
- **docs**:本檔(docs/55)。
- **未改 opendw;未改 VM source;DRAGON.COM / 圖檔未入庫。**

## 7. 卡點清單(精確,不臆造)

1. **終戰 Namtar 全戰鬥閉環**:結算公式已真值化,卡 per-character 動作指派狀態機 +
   無獨立戰鬥 oracle(docs/42 §14)。**沿用既有卡點,未攻**。
2. **結局畫面 script**:勝利後結局不在 level event script → 由戰鬥流程/主控觸發,**逆不出
   獨立 script**;與卡點 1 綁定。攻略結局亦無獨立段落編號。
3. **物品 gate set 來源**:市民證/朝聖者袍/眼鏡/龍寶石等 flag 被 op_9d 測得到,但 **set 點
   不在 level event script**(在 inventory/共享 resource)→ 逆不出 set,**記錄不臆造**。
4. **op_6B(area 18 tile 0x0D 付命門 Yes 分支)/ op_8D(area 33 暗語)**:opendw NULL,無 oracle;
   有合法繞道(tile 0x10 Irkalla 門)/ area 33 為隔離分量。**未硬補(屬另案)**。
5. **area 6/33 隔離分量**:docs/54 已記,與本任務正交。
