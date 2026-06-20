# 67 — 主線關鍵物品/進度旗標的「取得端」逆向(walking-engine 物品給予機制)

> 日期:2026-06-20
> 對象:`opendw_remake/`(C++20/SDL2 重製《火龍之戰》Dragon Wars, Interplay 1989)
> 方法:**靜態反組譯(DRAGON.COM)+ 動態 bytecode trace(remake VM 全 40 關)+ opendw oracle 對照**。
>   grounded;逆得出才標真值,逆不出精確記錄卡點(opcode/位址/原因),不臆造。
> 接續:docs/gameplay/55(§2.2/§2.4 把「物品 gate set 來源逆不出」列為卡點)、docs/assessment/48(缺口 C
>   quest 體系)、docs/reverse-engineering/42(戰鬥)、OPCODE_REFERENCE.md。
> 新增工具:`tools/verify/trace_item_grants.cpp`(掃全 40 關 level script 的 op_5F/60/61/63/64/65/67/68/69/8C/8D)。
> 新增 RE:DRAGON.COM op_64/op_65/op_67 反組譯(opendw `targets[]`=NULL,本文首次逆出語意)。

---

## 0. 結論摘要(先讀這段)

docs/gameplay/55 §2.2 把「市民證 / 朝聖者之袍 / 眼鏡 / 龍寶石」這類物品 gate 記為「**set 點不在 level
event script,逆不出 set 來源**」。本輪把這句話**精確化並大部分解開**:

1. **「取得端」確實不在 level(tile)script,但它沒有消失 —— 它在 op_58 載入的共享/事件 script 裡。** 全 40 關
   tile script 動態 trace:**op_64(給物品)0 次、op_5F(設祝福旗標)0 次、op_69(寫物品記錄)0 次、零 halt**
   (含對所有 op_8C 注入 Yes 走 Yes 分支)。但對 bundle 的 16 個共享/事件 script 做反組譯掃描:**op_64
   /op_5F/op_69/op_67 大量出現**(script 3/6/8/11/31… = 對話/商店/祝福/法術 script)。
2. **取得物品的 VM 原語 = op_64(DRAGON.COM 0x446E,opendw NULL,本文反組譯逆出)**:在當前角色 12 格物品欄
   找第一個空格 → 從資料資源 `word_3ADF`(物品模板表)複製 **23 byte** 物品記錄進去。**這就是「給隊伍一件物品」。**
   配套:op_65(0x44B8)= 持有檢查、op_67(0x44CB)= 刪除物品並壓縮欄位。
3. **設祝福旗標 = op_5F(0x4372,已實作)**:對當前角色 `char record + 0x55`(=flags[85])OR 一個 bitmask
   → 0x80 Irkalla、0x10 永恆之神、0x20 Enkidu。op_61(0x43A6)測。**這些確認在共享 script(如 script 6 的
   祝福/法術段)被呼叫,非 tile script。**(flags[85] 的 0x55 偏移以 fraterrisus 手冊為準,見 §4 對 opendw 標籤誤植的澄清。)
4. **真正逆不出的剩餘部分 = 互動式 walking-engine 指令迴圈**(玩家在地板物品格按 TAKE → 開啟物品交換 UI →
   觸發哪一段 op_58 給物品):opendw 自身**未重製 TAKE/USE/商店交換的互動指令迴圈**,故無 oracle 路徑;且要把
   「tile 觸發 → 哪個共享 script tag + 哪個物品模板 offset」對應釘死,需端到端跑完整主線(連通受 docs/assessment/48
   缺口 A 阻)。

**一句話**:取得端的**機制層全部逆出**(op_64 給物品、op_5F 設祝福、op_67 刪物品,定址鏈完整);卡的是
**「哪個地點的哪個互動,呼叫哪段共享 script、複製哪個物品模板」這層 binding**,以及承載它的**互動式 TAKE 指令迴圈
opendw 未重製**。

---

## 1. 兩種「取得端」機制(逆出)

Dragon Wars 的角色資料分兩塊,取得端各走一塊:

| 區段 | 內容 | 取得原語 | 測試原語 |
|---|---|---|---|
| **char record**(`data_C960`,512B/員)`+0x55`=flags[85] | 祝福/詛咒旗標、屬性 | **op_5F**(設 bit)、op_60(清 bit) | **op_61** test |
| **char ext / 物品欄**(`data_CA4C`,23B×12 格/員) | 12 格物品記錄(裝備/任務物品) | **op_64**(找空格+塞 23B)、op_69(寫單欄) | **op_65**、op_68(讀欄)、op_72 的 0x80 分支(持有 gate) |

### 1.1 op_64 = GIVE_ITEM(DRAGON.COM 0x446E;opendw NULL → 本文反組譯逆出)

dispatch 表 `[0x3960+0x64*2]=0x446E`(已對拍 OPCODE_REFERENCE 全表位址)。反組譯(file offset 0x436E):

```
446E  call 0x4AC6              ; 進入點 housekeeping(與 op_67 共用)
4471  xor dx,dx                ; dx = 槽索引 slot = 0
4473  mov bx,dx; shl bx,1       ; bx = slot*2(進 unknown_4456 字表)
4477  al=[0x3866]; di=ax        ; al = gs[6](當前角色 index)
447E  ax=0xCA4C                ; char_ext 基底
4481  add ah,[di+0x386A]        ; + 角色頁(selector<<8;0x386A = gs base of 0x0A 區)
4485  add ax,[bx+0x4456]        ; + 槽偏移 unknown_4456[slot](= slot*23)
4489  di=ax
448B  cmp byte[di+0x0B],0       ; 槽內物品「名字第 12 byte」(offset 0x0B)== 0 → 空格
4490  je  0x449A               ; 空格 → 去填
4492  inc dx; cmp dx,0x0C       ; 下一格;12 格(0..0x0B)都滿?
4496  jb  0x4473               ;   還有就繼續找
4498  jmp 0x44B5              ;   全滿 → 放棄(不給)
449A  cx=0x17                  ; 23 byte
449D  es=[0x3ADF]             ; word_3ADF = 物品模板資料資源
44A1  bx=[0x3AE2]             ; word_3AE2 = 模板表內物品 offset(由前序 op 設)
44A5  al=es:[bx]; [di]=al      ; copy 1 byte
44AA  inc bx; inc di; loop     ; 複製 23 byte 物品記錄進空槽
44AE  [0x3867]=dl              ; gs[7] = 剛填入的 slot index
44B2  call 0x4AC0; jmp 0x3ACB  ; 收尾,回 dispatch 迴圈
```

**語意 = 把 `word_3ADF`(物品模板表)中 offset `word_3AE2` 處的 23-byte 物品記錄,加到當前角色第一個空物品欄。**
即「給隊伍一件物品」的 VM 原語。模板表 = DATA1 物品定義區(remake 的 `assets/bundle/items/items.bin` 是其
curated 子集 7 件,含 Dragon Stone @DATA1 0x5369)。**無 operand**(來源 offset 來自 word_3AE2,呼叫端先設)。

### 1.2 op_65 / op_67(同區段,本文一併逆出)

- **op_65(0x44B8)= 持有檢查**:`al=[word_3AE2]` → call 0x4754(物品簽章比對子程式)→ jb 分支。
  命中 → call 0x4AC0(`or [0x3ae6],0x40` = 設 word_3AE6 bit 0x40);起始 call 0x4AC6(`and [0x3ae6],0xbf`
  = 清 bit 0x40)。即「隊伍是否持有某物品 → 設 zero-flag 供 jz/jnz gate」。
- **op_67(0x44CB)= 刪除物品+壓縮**:從 gs[7] 指定槽起,把後面每格往前搬 23 byte(`[di]←[bx]`,bx=di+23),
  到第 0x0B 格後把末槽清 0。= 「移除一件物品並把欄位壓實」(交物品給 NPC / 消耗任務物品)。

#### 0x4754 簽章比對子程式(完整反組譯;op_65 的核心,本文逆出)

```
4754  xor ah,ah; push es; push ax          ; ax = item id(al,來自 word_3AE2 低位)
4758  di=6
475b  bl=[0x38ba]; bh=0; bx<<=1
4763  es=[bx+0x13c9]                        ; 段表選「物品定義資源」(由 [0x38ba] 索引)
4767  di=es:[di]                            ; di = es:[6] = 指標表 base
476a  pop ax; ax<<=1; di+=ax               ; di = ptab_base + item_id*2
476f  di=es:[di]                            ; di = 模板 offset(資源內)
4772  bl=[0x3867]; bh=0; bx<<=1             ; gs[7] = 當前物品槽
477a  al=[0x3866]; ah=0; si=ax             ; gs[6] = 當前角色
4781  ax=0xCA4C; ah+=[si+0x386a]; ax+=[bx+0x4456]; bx=ax  ; bx = char_ext 槽 base(同 op_64/68/69 定址)
478e  cx=0
4791  inc cx; inc di; inc bx                ; 逐 byte(template es:di vs slot [bx])
4794  cmp cx,7; je 0x4791                   ; **跳過 byte index 7**(數量/充能,因實例而異)
479a  al=es:[di]; cmp al,[bx]; jne 0x47b4   ; 不符 → stc(未命中)
47a1  cmp cx,0x16; jae 0x47b1               ; cx>=22 → clc(命中,上限)
47a6  cmp cx,0xb; jb 0x4791                 ; cx<11 → 下一 byte(type/id header)
47ac  test [bx],0x80; jne 0x4791            ; bytes 11+ = 名稱:高位元 set → 還有字,續比
47b1  clc; pop es; ret                      ; 名稱結尾且全符 → 命中
47b4  stc; pop es; ret                      ; 未命中
```

**語意**:比對「物品模板(資源內 offset)」vs「當前角色 gs[7] 槽的 23-byte 記錄」。bytes 1-6/8-10 = type/id
header 須全等(byte 7 跳過);bytes 11+ = 物品名(高位元 0x80 標「還有字」、清表結尾);全符回 carry-clear。
**這是 Dragon Wars / Bard's Tale 系列「7-bit 名稱 + header」物品識別的通用簽章比對**。remake `op65_has_item`
逐行對映此邏輯;模板資源 binding 的 byte-faithful 邊界見上方 note 與 §6。

> 三者(op_64/65/67)+ op_68(讀欄)+ op_69(寫欄)構成完整 CRUD:**給 / 查 / 刪 / 讀 / 改物品**。
> opendw 把 0x64/0x65/0x67 標 NULL(未實作)。**remake 已於 2026-06-20 全數接上 op_64(GIVE_ITEM)、
> op_65(HAS_ITEM,含 0x4754 簽章比對子程式)、op_67(REMOVE_ITEM)** —— 三者皆依 DRAGON.COM 反組譯為
> 真值、`tests/vm_selftest.cpp` 逐指令自證、ctest 34/34。物品 CRUD(給/查/刪)原語**不再 halt**。
> **唯一殘留**:op_65 的「模板資源 binding」—— 0x4754 從 [0x38ba]→[0x13c9] 段表選物品定義資源,remake
> 改用當前 word_3ADF(比對迴圈與 es:[6]+id*2 deref 結構 byte-exact;binding 視 script 載入態,bundle 目前
> 帶 curated items.bin 而非 raw DATA1 物品資源)。見 §6 與下方 0x4754 反組譯。

### 1.3 op_5F = 設祝福旗標(0x4372,已實作)

`op_5F or_char_data`:`get_bit_mask(word_3AE2)` 讀 1-byte operand 當 **record 內 byte 偏移 bx**、word_3AE2
當 **bit 號**(轉成 mask)→ `char record[(selector<<8) + bx] |= mask`。當 bx=0x55 時即寫 flags[85]:

| bit/mask | 旗標(攻略/手冊 44 §1) |
|---|---|
| `0x80` | 受 Irkalla 伊爾卡拉祝福 |
| `0x10` | 永恆之神(Universal God)祝福(+3 全屬性) |
| `0x20` | Enkidu 祝福(習得德魯伊魔法) |

op_60 清、op_61 測(test_player_property,設 sf/zf/cf 供 jnz/jz 分支)。

---

## 2. 動態證據:取得端不在 level(tile)script

工具 `trace_item_grants`(`tools/verify/trace_item_grants.cpp`):對全 40 關每個 tile 的事件 script,從
`script_pc` 動態跑 VM,記錄所有 op_5F/60/61/63/64/65/67/68/69/8C/8D,並對 op_5F/60/61 解碼 (record_offset, bit)。

**結果(全 40 關,含 `--yes` 注入走 Yes 分支兩種模式皆同)**:

```
op_8C prompt_no_yes : 61~62 次     (Y/N 提示:樓梯/暗門/付命門…)
op_8D read_string   : 1 次          (area 33 tile 0x14 說暗語)
op_5F (設祝福)       : 0 次
op_60/61            : 0 次
op_63               : 0 次
op_64 (給物品)       : 0 次   ← 關鍵
op_65/67            : 0 次
op_68/69 (物品讀寫)  : 0 次
op_64/65/67 HALT     : 0 格   (= tile script 從沒跑到這些未實作 opcode)
```

**解讀**:level tile script **只負責**(a)進度旗標(op_9b/9d/50,docs/gameplay/55)、(b)敘述 emit、
(c)Y/N 與字串輸入 gate、(d)換場 / 座標。**它從不直接給物品或設祝福**。物品格的 tile script 只 emit 描述文字
(52_MAINLINE_EVENT_STRINGS.md 有 15 處 "items are scattered…" / "In a closed trunk, you find… pilgrims wear"
這類 flavor,**全無「you receive X / added to inventory」動詞**)。

---

## 3. 取得端在哪:op_58 共享/事件 script

對 bundle 的 16 個 script(`assets/bundle/scripts/*.bin`,= op_58 載入的 `event_script_tags`
[0,1,3,5,8,9,10,11,17,19] + 戰鬥/商店 script)做 `dwdisasm.py` 反組譯掃描:

| script | 出現的取得端 opcode(反組譯) | 推測角色 |
|---|---|---|
| **6** | op_5F ×2(@0x021C/0x0A61)、op_64 ×4、op_69、op_67 | 菲巴斯/祝福+法術+給物品(@0x0212 "succeeds!"→op_68 0x07→**op_5F**→"fails!"…"is reenergized!" = 祝福/施法套用段,實證) |
| **3** | op_5F ×2、op_64 ×2、op_69 ×3、op_67、op_60 | 大型對話/商店 script(5390B,最大) |
| **11** | op_64 ×4、op_65 | 給物品密集(商店買 / NPC 給) |
| **31** | op_69 ×4、op_5F、op_60 ×3、op_65 | 魔法學院七測驗(眼鏡 Spectacles 來源候選) |
| **8** | op_5F ×2、op_69 | 黃泥蟾蜍/拉娜碎片相關 |
| **2** | op_69、op_60 | 戰鬥 res2 |
| **10/17/18** | op_64 / op_60 / op_5F 各 1 | 對話/祝福 |
| **0** | op_69 ×2(@0x021A/0x0455) | 通用初始化/共享 |

> ⚠️ 線性反組譯在 data/字串區會 desync(如 script 31 @0x000C 那個 op_64 落在 data 區,為假陽性);但
> **聚類本身是真的**:重物品/祝福邏輯集中在 script 3/6/11/31。script 6 @0x0210 段(op_68 0x07 → op_5F →
> "succeeds!/fails!/is reenergized!")結構完整、語意自洽,是**祝福/施法套用的確證**。

**機制鏈(逆出)**:
```
玩家踩物品格 / 與 NPC 對話 / 進商店
   │  tile script(op_71/op_72)emit 描述 + op_8C/op_89 給選項
   ▼  玩家選擇 → op_58 載入對應共享/對話 script(tag ∈ event_script_tags)
   ▼  共享 script:op_09 設 word_3AE2(物品模板 offset)→ op_64(找空格+塞 23B)= 給物品
                  或 op_09 設 bit + operand 設 record offset 0x55 → op_5F = 設祝福旗標
   ▼  之後 tile script 用 op_9d 測進度旗標 / op_72 0x80 分支或 op_65 測「是否持有」= gate
```

---

## 4. flags[85] = 0x55 的對齊澄清(opendw 標籤誤植)

opendw `player.c` 的 `struct player_record` 把 `[0x55..0x58]` 標成 `unsigned int gold`,**與本文「0x55=祝福
旗標」表面衝突**。釐清:**以 fraterrisus《Dragon Wars Hex Editing Guide》(docs/reverse-engineering/44 §1)為準**
—— 該手冊把 gold 放 `[81]`、flags 放 `[85]`(=0x55)。opendw 的 struct 是 Devin Smith 早期推測佈局,
**該欄標籤不可靠**(opendw 本身未實作祝福/金幣邏輯,沒對拍過)。op_5F/61 的 record offset 由呼叫端 operand 帶入,
**程式上對 0x55 寫 bitmask 完全成立**;0x55 是 byte 偏移,gold 是否真的 4-byte 也未經 opendw 驗證。
→ **採 fraterrisus:[85]=0x55=祝福旗標**,與 docs/gameplay/55 §2.1 一致。

---

## 5. 逐物品/旗標:取得端逆出多少(真值 vs 受阻)

| 物品/旗標 | 取得原語(逆出) | 在哪設(逆出程度) | 狀態 |
|---|---|---|---|
| **角色祝福 flags[85]**(Irkalla 0x80 / 永恆神 0x10 / Enkidu 0x20) | **op_5F**(record+0x55 OR mask) | 共享 script(script 6 等)的祝福/施法段;**機制+定址全逆出**,但「哪個地點觸發哪個 op_5F」需端到端跑 | **機制真值;binding 受阻** |
| **一般任務物品**(Skull、朝聖者之袍、市民證、眼鏡 Spectacles、龍寶石 Dragon Gem…) | **op_64**(找空格+複製 23B 模板) | 共享 script(3/6/11/31…)在對話/商店/測驗分支;**機制+定址+模板來源(word_3ADF)全逆出** | **機制真值;個別物品的 script-tag×模板-offset binding 受阻** |
| **物品消耗/上交**(交 Skull 給鐵匠、交碎片) | **op_67**(刪+壓縮) | 同上 | **機制真值;binding 受阻** |
| **持有 gate**(過橋驗市民證、進 Nisir 驗朝聖者袍) | **op_65** / **op_72 的 0x80 分支**(0x4754 簽章比對) | op_72 在 tile script 跑得到(已實作);op_65 在共享 script | **機制真值** |
| **進度旗標 gsB9.x**(主線「已造訪/已見」) | op_9b set / op_9d/50 test | level tile script(docs/gameplay/55 已逆出,135 flag/313 op) | **真值(既有)** |
| **自由之劍鑄造鏈**(Skull→Forge→Inferno/Apsu 淬煉) | 多步:op_64 給劍 + op_67 消耗 Skull + op_5F 祝福 | 共享 script + 段落書 137/138 敘事;**單一 op 機制逆出**,整鏈順序未端到端跑 | **原語真值;劇情鏈 binding 受阻** |
| **水中呼吸藥水 / 英雄魂**(area 22) | op_64 給 + 可能 op_5F | 共享 script | **機制真值;binding 受阻** |
| **Dragon Gem 龍寶石**(area 32) | op_64 給(模板 = items.bin "Dragon Stone" 候選,DATA1 0x5369) | 共享 script;龍谷 tile 32 只有 op_9d gate + encounter(docs/gameplay/55) | **機制真值;觸發點 binding 受阻** |
| **Irkalla 解咒**(area 18) | 推測 op_5F(對 Irkalla NPC 或隊伍設旗標)+ 進度旗標 | area 18 tile 多為 op_8c/op_9d gate(本輪 trace);解咒實體在共享/段落 | **gate 真值;解咒給予 binding 受阻** |

---

## 5b. 給物品「常式」逆出(2026-06-20,攻略反推法 — 連通修復 + op_64 實作後重查)

連通修復(40/40)+ op_64/65/67 實作後,**用攻略反推法**(如找菲巴斯位置)重查共享 script,逆出**給物品的
集中常式 = `scripts/11.bin`**(寶箱/物品處理 script):

```
11.bin @0x0012 (give-item routine):
  load_resource res:0x0B off:0x3F   ; 載物品定義資源
  for_call over party                ; 對隊伍迴圈
    w3AE4 = gs[0xD9]                  ; gs[0xD9] = 物品資源 id(呼叫端設)
    data_res = res(w3AE4)
    w3AE2 = gs[0xD7]                  ; gs[0xD7] = 物品模板 offset(呼叫端設)★
    op_64                            ; GIVE_ITEM(從 word_3ADF[w3AE2] 複製 23B)
    jnz full                         ; 背包滿 → " can't carry any more."
    emit: <charname>" gets the "<item>  ; 給物品訊息
```

**關鍵 binding 變數**:呼叫端在 `op_58 res11@0` 前設 **`gs[0xD7]` = 物品模板 offset**(+ `gs[0xD9]` = 物品資源),
即決定「給哪件物品」。op_64 散布在 scripts 11(×4)/3(×2)/31/6;op_5F(設祝福旗標)在 scripts 18/3(×2)/6/8。

**攻略反推可行性(回答「能不能像找菲巴斯一樣反推」)= 可行**:攻略 38 各區「事件/圖例表」明寫哪地點給哪物品
(翠玉之眼@海盜竊穴、骷髏→矮人鑄爐、龍寶石@龍谷…);要釘死 binding = 找各 op_58 res11 呼叫端 + 其 `gs[0xD7]`
設值 → 對物品資源解出物品名 → 對攻略地點。**機制已逆出,剩「逐呼叫端 gs[0xD7] 列舉」的工**(屬內容工,非阻斷)。

**已落地**:`scripts/11.bin` 的玩家可見訊息(`" gets the "` 給物品、`" can't carry any more."` 背包滿、寶箱訊息)
本輪補繁中(events.tsv)→ 物品取得/開寶箱回饋顯示繁中。

## 5c. 端到端驗證(2026-06-20)— 發現真正的最後缺口:event↔party 角色狀態未同步

實際追「開寶箱 → op_64 給物品 → 進隊伍背包」全鏈,**verify 到精確斷點**:

1. **寶箱 tile 走 `op_58 res11@0x000f`(上鎖入口)**,先 emit「上了鎖的箱子」,**op_64 給物品在 0x0012 入口**
   (解鎖後才走)。即上鎖寶箱要先解鎖才給物品(K 解鎖互動);無鎖物品/NPC 給予走 0x0012/0x0009。
2. **致命斷點:`run_event`(main.cpp:797)設 VM state 時只載 `game_state`,完全沒載/寫回 party 的 `char_data`
   /`char_ext`**。→ op_64 把物品寫進 VM **暫態 char_ext(預設全 0)**,事件一結束就丟,**沒進隊伍背包/存檔**。
3. **建模落差**:原版 `data_CA4C = data_C960 + 0xEC`(char_ext 背包 = 512B record 內偏移 0xEC 起的後半;
   stats 前 236B + 背包 12 槽×23B = 512),**兩者重疊**;remake VM 用**兩個分開的陣列**,故即使同步 char_data 也
   不會帶到背包。隊伍背包真值存於 512B record `[236 + slot*23]`(= `--demo-items` 注入用的佈局,c.inventory())。

**即:給物品 MECHANISM 完整(op_64/65/67 + script 11 + 旁白繁中全到位、vm_selftest 自證),但 END-TO-END
持久化未通** —— `run_event` 缺「事件前載 party 背包 → char_ext、事件後 char_ext → party 背包」這段同步。
此即 §6 列的「event→char 持久化」gap 的具體形,同時影響**物品給予**與**祝福旗標(op_5F)**(祝福也寫進
暫態 char_data,同樣丟失)。

**修法(已界定,屬子系統工 + golden 風險)**:run_event 前後同步 VM char_ext/char_data ↔ party 512B records
(member i → record i,selector=i*2,背包在 [236+slot*23]);需設 gs[6]/gs[0x1F]/gs[0x0A+i] 角色 context。
風險:改變事件期間的角色 context 可能影響 flavor 事件分支;需 combat/save golden + 事件 trace 全驗。
**verify 工具(mainline_events/trace_quest_gates)用自有 VM setup、不經 run_event**,combat 走 party records、
save 不跑事件 → 風險主要在「遊戲內事件行為」層,需實測。

### ✅ 已實作並端到端驗證(2026-06-20)

`run_event`(main.cpp)補上 char 同步:**事件前**把 party 512B records 載進 `char_data`(member i→record i,
selector=i*2)、設 gs[6]=0/gs[0x1F]=人數、背包鏡射進 char_ext(char_ext[k]≡char_data[0xEC+k]);**事件後**
char_ext→char_data[0xEC+]、逐欄比對寫回 party(只有實際變動才重建 Party,flavor 事件零副作用)。

驗證:
- **verify_item_persist**(新 ctest):op_64 給物品 → char_ext → 同步 → 512B record 背包 [236..258] 持久,PASS。
- **ctest 35/35**(含 combat/save golden);實機 smoke test(探索+FP)exit 0、flavor 事件不誤改 char_data。
- 完整鏈:op_64→char_ext(vm_selftest)→ record[236+](verify_item_persist)→ 背包 UI/存檔(同 `--demo-items`
  位置、verify_save 涵蓋)。**給物品/祝福端到端持久化打通**(物品進背包、祝福旗標進 record)。

殘留:**互動觸發層**(上鎖寶箱要 K 解鎖、NPC 給予要對話選擇)仍走未反編的 walking-engine,headless 難自動
觸發 op_64;但「一旦事件跑到 op_64,物品就確實進背包並持久」已驗證。物品-地點逐一編目(攻略反推)為內容工。

---

## 6. 還卡在哪(精確,不臆造)

1. **互動式 walking-engine 指令迴圈(TAKE / USE / 商店交換)opendw 未重製**:玩家在地板物品格按 TAKE → 開
   物品交換 UI → 選擇 → 觸發給物品。**opendw 的主迴圈只做 bytecode 執行 + 移動 + wait_for_event,沒有 TAKE/USE/
   trade 的互動指令處理**(engine.c 查無對應 command dispatch)→ **無 oracle 路徑可對拍此互動層**。DRAGON.COM 內
   有(物品 CRUD 原語 op_64/65/67 已逆出),但**互動 UI driver(按鍵 → 選 NPC/選物品 → 呼叫共享 script)那段
   未反組譯**(屬 docs/assessment/48 缺口「尚未反編的 walking-engine」)。
2. **「地點/互動 → 共享 script tag → 物品模板 offset」三元 binding 未釘死**:已知 op_64 從 word_3ADF[word_3AE2]
   複製 23B,但**「龍寶石具體由哪個 script、設 word_3AE2= 哪個 offset」**需要:(a)連通到該地區(連通受缺口 A 阻,
   <10% 可達)、(b)走進該對話/商店分支、(c)dump word_3ADF/word_3AE2。當前 headless trace 進不到那層(tile
   script 不直接給物品,要再經 op_58 + 玩家選擇)。
3. **共享 script 反組譯只到「opcode 出現」層,未到「控制流還原」層**:dwdisasm 線性反組譯在 data 區 desync;要精確
   說「script 6 第幾個分支給 Skull」需逐分支動態 trace + 餵正確 game_state(尚未做)。
4. **op_64/65/67 在 remake/opendw 皆未實作**:要實作給物品/刪物品,需把 §1.1/§1.2 反組譯落成 handler(無 oracle
   對拍,只能靠反組譯自證 + 自測)。

---

## 7. 對 remake 的可行接法建議

> 全部屬「機制已逆出,可乾淨室實作」;binding 與互動 UI 仍受連通(缺口 A)與 oracle 缺路徑阻,屬後續。

1. **實作 op_64 / op_65 / op_67 handler**(§1.1/§1.2 反組譯為真值來源,無 oracle → 加 vm_selftest 逐指令自證):
   - op_64:`char_ext` 內當前角色 12 格找 `[slot*23 + 0x0B]==0` 的空格 → 從物品模板資源(word_3ADF)offset
     word_3AE2 複製 23B → 寫 gs[7]=slot。需先把物品模板表(DATA1 物品定義 / items.bin 全集)接成 word_3ADF
     可定址的資源。
   - op_67:從 gs[7] 槽起每格往前搬 23B、末槽清 0。
   - op_65:call 物品簽章比對(0x4754 邏輯:7-byte 簽章 + 0x0B..0x16 範圍比對,見 op_72 0x80 分支)。
2. **接共享/對話 script 執行**(P1-1):把 `event_script_tags` 的 16 個 script 真正跑起來(op_58 已實作),讓
   op_64/op_5F 在對話/商店分支被觸發 —— 這比硬寫 quest 體系更忠實(原版的「給物品」就是這些 script 在做)。
3. **物品 gate(op_72 0x80 分支 / op_65)做成可玩**:過橋驗市民證、進 Nisir 驗朝聖者袍等 = 「隊伍是否持有簽章
   X」→ op_72 0x80 分支已在 tile script 跑得到,補完 0x4754 簽章比對即成 gate。
4. **暫以 remake 設計層補互動 TAKE**(opendw 無此路徑,不強求對拍):地板物品格 emit 描述後給「拾取」動作 →
   呼叫 op_64-等價路徑,把物品塞進物品欄。數值正確性靠 op_64 反組譯自證,互動體驗靠 remake 自定。
5. **祝福**:op_5F 已實作,只差「在共享 script 的對應段被呼叫」—— 隨 P1-1(跑共享 script)一併解決;自由之劍
   「Irkalla 0x80 + 神 0x10 → 一擊 100 HP」可在戰鬥結算讀 flags[85] 套用(接 docs/reverse-engineering/42)。

---

## 8. 改動檔案 / 驗證

- **新增工具**:`opendw_remake/tools/verify/trace_item_grants.cpp`(已註冊 CMakeLists;非 ctest,屬 grounding/觀測)。
  - 全 40 關 tile script 掃 op_5F/60/61/63/64/65/67/68/69/8C/8D;對 5F/60/61 解碼 (record_offset, bit);
    偵測 op_64/65/67 halt。`--yes` 對 op_8C/op_89 注入 'Y' 走 Yes 分支。
  - 實測輸出見 §2(op_64/5F/69 = 0、零 halt)。
- **新增 RE**:DRAGON.COM op_64(0x446E)/op_65(0x44B8)/op_67(0x44CB)反組譯(capstone 16-bit,uv venv 容器;
  dispatch 表位址全對拍 OPCODE_REFERENCE)。**DRAGON.COM / 反組譯產物未入庫**(從 /tmp 取 + docker capstone,原檔不入庫)。
- **未改 VM source / opendw**:本輪純觀測 + 反組譯 + 文件;interpreter/vm_state 未動,既有 ctest 31/31 不受影響。
- **回歸**:新工具編譯通過(dwsdl 容器 cmake build);未改既有對拍路徑。

## 9. 卡點清單(精確)

1. **互動 TAKE/USE/trade 指令迴圈**:opendw 未重製 → 無 oracle 路徑;DRAGON.COM 的互動 UI driver 段未反組譯。
2. **物品/祝福 binding**:op_64 來源(word_3ADF×word_3AE2)機制逆出,但「哪地點→哪 script→哪模板 offset」需端到端
   跑(連通受 docs/assessment/48 缺口 A 阻)。
3. **共享 script 控制流還原**:dwdisasm 線性反組譯 desync,精確分支需逐分支動態 trace + 正確 game_state。
4. **op_64/65/67 無 oracle 對拍**:opendw NULL,實作只能靠反組譯自證 + 自測(同 op_6B/op_8D 的做法)。
