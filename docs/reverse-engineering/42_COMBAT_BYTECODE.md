# 戰鬥結算改用原版 bytecode — 調查與 gap 分析

> 日期:2026-06-14
> 目標:讓戰鬥結算數值 = 原版真值(跑原版 res-script bytecode),取代 combat.cpp 的乾淨室 placeholder。
> 結論:**戰鬥腳本可在 remake VM 跑到「戰鬥選單(Fight/Run)的鍵盤等待」**;結算數值路徑(怪物生成、屬性、RNG、乘除)已用原版 bytecode 執行。離「完整一場戰鬥」尚差 1 個互動子系統(`wait_for_event`)+ 選單分支後的剩餘 opcode。

## 1. 戰鬥腳本位置(oracle 行號)

- **進入**:`op_8A`(engine.c:4876)→ `trigger_random_encounter`(engine.c:4818)。**只設圖形/動畫狀態**(`byte_4F0F`=怪物id、載入 sprite 資源、`init_monster_animation`、`byte_4F2B`=0xFF),**不寫任何戰鬥數值**到 game_state / char_data。
- **戰鬥腳本本體**:**DATA1 resource 3**(`assets/bundle/scripts/3.bin`,解壓 5390 bytes)。由 `script 1` 經 `op_58 load_resource res:0x03 offset:0x0000` 進入(對拍確認)。doc/script.md 亦標「Section 3, 1706 (encounter)」(1706 = 0x6AA)。
- **戰鬥用到的資源**:res 0x02、0x03(自身)、0x06(戰鬥訊息字串)、0x12(戰鬥選單,1375B,docs/24:136)、0x16。已全數抽進 `assets/bundle/scripts/`(自包含,執行期不需 DATA1)。
- **結算位置(關鍵)**:opendw C **沒有**戰鬥結算;命中/傷害/HP 全在 res3 bytecode,經以下 primitive:
  - `op_5D/5E`(get/set char_data,engine.c:2568/2601)讀寫角色/怪物 HP 等屬性。
  - `op_4D`(PRNG,engine.c:2210)亂數。
  - `op_33/34/35/36`(乘/除法,engine.c:1700+/6520/6539)傷害骰/縮放運算。
  - `op_5F/60/61`(角色 bit 屬性 set/clear/test)狀態旗標(dead/stunned…)。

## 2. 餵進 remake VM 的結果

工具:`tools/verify/probe_combat_script.cpp`(`probe_combat_script <bundle> 3`)。
設定:char_data=預設隊伍、game_state[0x1F]=人數、[6]=當前角色、[0x0A+i]=record selector、`headless_encounter=true`。

| 里程碑 | 步數 | 停在 | 原因 |
|---|---|---|---|
| 初始 | 554 | op_64 @0x6ad | **誤判**:`ax` 未在 dispatch 清零 → op_09 多吃 1 byte,把 op_5E 的 property operand `0x64` 當成 opcode。 |
| 修 dispatch `ax=op` | 794 | op_18 @0x571 | 真實缺的 opcode(opendw 有,remake 缺)。 |
| 實作 op_18 | 2314 | op_8A @0x1ea | 遭遇觸發(圖形);headless 下記錄怪物 id 後續跑。 |
| op_8A headless 續跑 | 2316 | op_58 @0x1f | 缺 bundle 資源(res 0x12 戰鬥選單)。 |
| 抽 res 2/6/18/22 進 bundle | 2445 | op_34 @0x165 | 缺乘/除法子系統。 |
| 實作 op_33/34/35/36 + 11C0..11CC | **2556** | **op_89 @0xf8** | **戰鬥選單(Fight/Run)的鍵盤等待**(`wait_for_event`)。 |

跑通的真實戰鬥數值運算(2556 步內):op_4D ×19(RNG)、op_5D ×6 / op_5E ×8(char data 讀寫)、op_34 ×1(乘法)、op_18 ×12。即**遭遇初始化 + 怪物生成的數值已用原版 bytecode 算出**。

## 3. 本次實作(逐指令對齊 opendw,byte-for-byte verified)

- **VM dispatch bug 修復(最關鍵)**:remake 的 run / run_script 迴圈漏了 opendw run_script(engine.c:6446)每次 dispatch 的 `cpu.ax = op_code; cpu.bx = cpu.ax`(opcode 進 al,高位清 0)。多個 opcode(op_09 等)的 byte/word 模式判定 `byte_3AE1 != (ax>>8)` 依賴此清零;漏設會 desync。修正後既有 7/7 ctest 全綠(證明與 opendw 一致)。
- **op_18**(engine.c:1276):`data[(gs[op1]|gs[op1+1]<<8)+op2] = r2`(byte/word)。
- **乘/除法子系統**(engine.c:1693/6520/6539):新增 `w11C0..w11CC` 工作區 + `mul16`/`div16`/`compute_division_vars`/`save_gamestate_vars`/`divide_and_save_results`,以及 **op_33/34/35/36**。結果回存 game_state[0x37..0x3C]。
- **op_8A headless 模式**:`headless_encounter` 旗標(預設 false 維持原 halt);true 時略過圖形載入(render leaf,沿用 batch10 中性化手法),只記錄怪物 id,讓結算路徑繼續。
- 全部加入 `vm_selftest`(op_18/33/34/35/36 共 5 項,手算 oracle 值對拍,PASS)。

## 4. 離「完整一場戰鬥」還差什麼(gap)

1. **`wait_for_event` / op_89 互動子系統**(engine.c:~2860 `wait_for_event`、4977 `wait_event`):戰鬥選單(Fight/Quickly fight/Run/Advance)的鍵盤等待 + key→address 跳轉表。深度綁定 UI 輸入層(ui_draw_string / mouse / timers / draw_rect / escape_string_table)。需移植輸入分派或在 headless 注入「選 Fight」的鍵值並跳到對應 address。**這是下一個主要工項。**
2. **選單分支後的剩餘 opcode**:跳過 op_89 後,Fight 分支的攻擊迴圈(res3 `call 0x0fac`/`0x0afa` 等子程式)可能再用到其他目前未實作 opcode。需逐一補(同 batch 手法)。
3. **op_5F/60/61 在戰鬥路徑的覆蓋**:目前 2556 步內未觸發(0 次),Fight 分支才會用到狀態旗標;已實作,待選單打通後驗證。

## 5. 哪些 opcode 是「無 oracle」(不可逐指令對齊)

opendw `targets[]`(engine.c:583)中為 `NULL` 的 opcode = 從未被逆向:
`0x02 0x1B 0x1E 0x20 0x29 0x2C 0x37 0x64 0x65 0x67 0x68 0x6B 0x6E 0x70 0x79 0x7E 0x7F 0x8E 0x8F 0x9C 0x9F`(及 0xA0+,多為字串/資料 byte)。
**經查戰鬥腳本 res3 的實際執行路徑(非 linear disasm)目前未觸發這些**(先前「卡在 op_64」是 dispatch bug 造成的誤判,0x64 實為 op_5E 的 property operand,非 opcode)。若 Fight 分支日後觸發某個 NULL opcode,則該 opcode 須從原始 COM 二進位逆向(dos/dragon.asm 僅含開機/視訊段,未及 op-dispatch 表,無現成參考)。

## 6. 現況對戰鬥結算數值的意義

- `src/game/combat.cpp` 的結算**仍是乾淨室 placeholder**(誠實標示不變)。本次**尚未**把它換成「跑 res3 bytecode」——因為完整一場戰鬥還卡在 op_89 互動 + 後續分支。
- 但已證明 **路徑可行(A 大致成立)**:結算所需 primitive(char_data/RNG/乘除)都已就緒且 byte-for-byte 對齊,戰鬥腳本能跑 2556 步到選單。打通 op_89 + Fight 分支剩餘 opcode 後,即可用原版數值取代 placeholder 並寫 `verify_combat_script`(逐回合 char_data 對拍 opendw)。

## 7. 重現

```bash
# 建置 + 跑 probe(docker dwsdl)
docker run --rm -v "$PWD/opendw_remake":/app -w /app dwsdl bash -c '
  mkdir -p /tmp/rb && cd /tmp/rb && cmake /app && cmake --build . -j$(nproc)
  ./probe_combat_script /app/assets/bundle 3 300000'
```

---

## 8. 更新(2026-06-14,第二輪:打通 op_89 + 攻擊迴圈)

### 新進展:戰鬥腳本已可跑完整攻擊迴圈(real bytecode)
- 修「op_89 卡點」並沿 Fight 分支推進,步數從 2556 →(注入 F/A 鍵後)**跑滿 300000 步無 halt、無缺 opcode**——即攻擊解算迴圈已在 remake VM 上以原版 bytecode 執行。

### op_89(wait_event)語意拆解(oracle 行號)
`wait_event`(engine.c:4792)→ `wait_for_event`(4368)→ `handle_key_event`(4328):
- **VM 狀態(須複刻)**:讀 2-byte flags;表起點 = flags 後 pc;每筆 3 byte `[key][addr_lo][addr_hi]`(0xFF 結尾、0x00 catch-all、0x01 數字鍵→設 `gs[6]`、0x81 skip);命中 → `bx = base[di+1]|base[di+2]<<8`;`pc = base+bx`;`word_3AE2 = key`。
- **UI leaf(headless 中性化,不影響分支)**:`ui_draw_string` / mouse / timers / `draw_player_status_panel` / escape-string 繪製 / `poll_mouse`。
- **headless 注入**:`VmState.headless_key`(單鍵)+ `headless_keys`(序列,逐個 op_89 取用)。鍵為「大寫字母 | 0x80」(對照 get_key_from_buffer 大寫化),例 Fight='F'|0x80=0xC6、Attack='A'|0x80=0xC1、Run='R'|0x80=0xD2。

### 戰鬥選單結構(res 18,實測)
- 主選單 op_89 @res18:0xf8。key→addr 表:`F(0xC6)→0x0108`、`R(0xD2)→0x02cf`…
- Fight(0x108)分支後再一個 op_89(動作選單):`A(0xC1)→0x03bc`、`D(0xC4)→0x02c5`、`C(0xC3)→0x0517`(Attack/Dodge/Cast,對齊 CONTEXT「攻擊/閃避/施法」)。注入 A 進入攻擊解算。

### 本輪逐指令對齊實作(byte-for-byte,皆入 vm_selftest)
- **op_89**:headless 鍵注入 + key→addr 表跳轉(對照 wait_event/handle_key_event)。
- **op_17**(store_data_into_resource,engine.c:1247):寫入「gs 指定 index 的資源」`res[gs[op+2]].bytes[(gs[op]|gs[op+1]<<8)+r4]=r2`。為此加**持久資源快取** `VmState.res_cache`(對照 `resource_get_by_index → allocations[idx]`,engine.c:176),op_0F(讀)/op_17(寫)共用,寫入持久。
- **op_63**(set_char_data_word,engine.c:2761)、**op_69**(engine.c:2846):角色擴充狀態塊 `data_CA4C`(加 `VmState.char_ext[4096]`)+ `unknown_4456[]` per-char offset 表(tables.c:295)。op_63 在 `char_ext==0`(戰鬥首回合)走清-carry 分支(對照 0x444C);`!=0` 的 opendw 未實作分支(0x4430)標記 last_unimpl 不臆造。
- 另抽戰鬥用資源進 bundle:res 4(248B)。

### 仍差什麼(下一步)
- **怪物 combatant 設置**:目前攻擊迴圈空轉、HP 不變,因 **headless 入口從 script3 offset 0 直跑、怪物未被設成參戰角色**(`gs[0x1F]` 仍只 4 名隊員;`gs[0x5A]`=怪物資料資源索引未被正確設定)。opendw 中怪物參戰資料由「地圖層遭遇進入 + op_8A 載入怪物資源 + 設定階段(res3 sub 0x4f1 經 op_0F 讀 `gs[0x5A]` 指定資源)」建立。
- 因此 **`verify_combat_script` 逐回合 HP 對拍尚未達成**:需先讓 headless op_8A 忠實載入怪物資料(目前只記 id、略過圖形),並提供正確的遭遇進入 context(`gs[0x5A]` 等)。
- 一旦怪物參戰,攻擊迴圈即會對怪物 HP 做扣減(命中/傷害已走 op_4D/op_33-36 等原版算術);屆時再寫 verify_combat_script 並考慮移除 combat.cpp placeholder 標示。

### combat.cpp placeholder 狀態
- **仍維持乾淨室標示,不可移除** —— 因為一場戰鬥尚未真正跑出怪物 HP 變化(空轉),戰鬥數值尚未經原版 bytecode 驗證為 oracle 真值。誠實標示不變。

### 重現(本輪)
```bash
# probe 注入 F(Fight)+A(Attack):跑滿步數、無缺 opcode(攻擊迴圈執行中)
./probe_combat_script /app/assets/bundle 3 300000
# → ran 300000 steps; halted=0 ...(headless_keys={0xC6,0xC1})
```

---

## 9. 更新(2026-06-14,第三輪:怪物 roster 機制逆向 + verify_combat_script)

### 目標與結果
目標:讓怪物成為參戰角色、攻擊迴圈實扣 HP、達成 `verify_combat_script` 逐回合 HP 對拍 oracle byte-identical。
結果:**未達成 byte-identical HP**(走 fallback)。根因:「怪物 roster pipeline 未完整逆向」+「無可獨立執行的 opendw 可對拍一場完整戰鬥」。已 commit:確定性執行守護 `verify_combat_script`(ctest)+ 本輪逆向發現。

### 怪物→參戰角色(roster)機制(oracle 行號)
- `gs[0x1F]` = **參戰人數**(戰鬥期含怪物);opendw C **不寫**(engine.c:2538 只讀),由 res3 bytecode 設。res3 @0x0013 `op_19 0x1f,0x20`(備份 party 數到 gs[0x20])、@0x00BE 還原 → 戰鬥期 gs[0x1F] 會被擴張含怪物。
- **遭遇表**:res3 data @**0x04C6** = `{0x03D6, 0x0412, 0x044E, 0x048A, 0, …}`(對應 monster_info.cpp `script_data[]`),每項 = res3 內某怪物群定義 offset。
- **setup 鏈**:res3 主流程 `call 0x04F1`(遭遇 setup:`op_0F` 從 `resource_idx(gs[0x5A])->bytes` + `gs[0x58/0x59]` offset + `op_4D` RNG 走訪)+ `call 0x06B5`(`op_0D 0x04C6`:由遭遇 id 查 0x4C6 表 → 怪物群定義)。`gs[0x5A]` = 關卡資源 `(gs[4]+0x1E)` 之 index(engine.c:5452,綁 map/level 載入)。
- **怪物資料來源**:res31(2177B;= monster_info.cpp 解析對象)。本輪已抽進 bundle(`scripts/31.bin`)。

### 為何空轉(實測根因)
注入 F→A 後,迴圈在 **res4 @0x491 的 op_89(每角色動作提示)** 無限重提示:因 **roster 為空**(gs[0x1F] 仍只 4 名隊員、怪物未進 char_data 槽),攻擊執行子程式(res3 `call 0x0FAC`)無有效目標,回合無法完成 → 動作提示反覆重入。手動把怪物塞進 char_data slot4 + gs[0x1F]=5 **仍空轉**:因 bytecode 自身的 roster 簿記(gs[0x6A..0x6F]、gs[0x72]/gs[0x75] 兩側「尚有可動者」旗標、gs[0x8B..0x8F];由 sub 0x0669 清零、由遭遇 setup 填)未被正確建立。

### 還差什麼(精確 gap)
1. **完整逆向遭遇→roster pipeline**:res3@0x4C6 遭遇表項 → 怪物群定義格式 → RNG 決定數量/種類 → 寫 res31 怪物屬性進 char_data 怪物槽 + 設 gs[0x1F]/選擇子 gs[0x0A+i]/roster 簿記。**opendw 自身只部分 RE**(monster_info.cpp `sub_6B5` 留 TODO),需從 res31 格式 + bytecode 走訪逐步補齊。
2. **op_51**(`op_51 0x04ea`,sub 0x071b 用於 roster 初始化)等 roster 路徑 opcode 尚未實作(本輪未觸發到 halt,因更早就在 res4 動作提示空轉)。
3. **對拍 oracle 的根本限制**:opendw 需完整遊戲狀態(map/level/save)才能跑戰鬥,**無法獨立跑一場戰鬥輸出逐回合 char_data** 供 byte-diff。即使 roster 補齊,byte-identical 對拍仍需先有「可獨立執行並 dump char_data 的 oracle 路徑」(在 opendw 加 headless 戰鬥入口 + instrumentation)。

### 本輪交付
- `verify_combat_script`(ctest):固定 seed+隊伍+鍵跑 res3 bytecode 到攻擊迴圈,驗 **指令軌跡兩次執行 byte-identical + 全程無缺 opcode**(守護戰鬥 bytecode 路徑不回歸)。**明確不驗 HP-vs-oracle**(roster gap)。
- 抽 res31(2177B)、res4(248B)戰鬥資源進 bundle(自包含)。

### combat.cpp placeholder 狀態
- **仍維持乾淨室標示,不可移除**。一場戰鬥仍未跑出怪物 HP 變化(roster 空 → 空轉),戰鬥數值未經原版 bytecode 驗證為 oracle 真值。嚴守鐵則:未跑通+對拍前不謊稱 oracle。

---

## 10. 更新(第四輪:roster 推進 + 從 bytecode 反推 to-hit/傷害公式)

### roster setup 機制(逐項對 res3 bytecode / engine.c)
- **遭遇資料資源** = `gs[0x5A]`(= 關卡資源 `(gs[4]+0x1E)` 的 index,engine.c:5452 `load_level_resources`);實測等同 res31。**walker 入口**:`gs[0x58/0x59]` = `res31->bytes[2..3]`(initial_offset;對照 monster_info.cpp 0x4F6)。
- **怪物群模板載入**:res3 sub `0x04F1`(經 `op_0F` 從 `resource_idx(gs[0x5A])->bytes` + `gs[0x58]` offset + `op_4D` RNG 走訪)把怪物群寫進 `gs[0x28..0x36]`(13 byte 模板)、`gs[0x27]` = 群數。**已驗證**:給對 context 後 `gs[0x28..0x36]` 確實填入真資料(例 `01 00 0c 00 43 a2 0c 28 c5 14 …`),非全 0。
- **怪物群定義表** @res3 `0x04C6` = `{0x03D6, 0x0412, 0x044E, 0x048A, 0…}`(每項 = res3 內怪物群定義 offset;對照 monster_info.cpp `script_data[]`)。`sub 0x06B5`(`op_0D 0x04C6` 查表)取群定義。
- **仍卡點**:怪物未進 char_data 槽 + `gs[0x1F]`(參戰數)未增 → 戰鬥流程在「怪物登場 announce 迴圈」(res3 0x0029/0x010e,`call 0x06B5`)空轉,**未到 actor 迴圈(0x0075)**。announce 迴圈不終止的精確 gs 種子尚未完全對齊(需更多 gs[0x41]/群 index 簿記)。本輪未讓一回合真扣 HP。

### 本輪實作:op_51(roster 路徑必需)
- **op_51**(argmax over data,engine.c:2277 @0x418B):讀 2-byte operand(data 偏移),掃 `data[di+bl]`(bl 自 r4 遞減至 0),取**最大 byte**→r2、其 index→r4 低位。actor 迴圈 `sub 0x071b`(`op_51 0x04ea`)用它找「行動值最高的下一個 actor」。逐指令對齊,入 vm_selftest(PASS)。

### 從 bytecode 反推的 to-hit / 傷害公式(op 層級,oracle res3 行號)
> 這是「實機讀不到、bytecode 才看得到」的部分。**weapon 路徑被 op_68 擋住**(見下);**fistfighting 路徑大致可讀**。

**傷害主流程** `0x0D4F test gs[0x66]; jns 0x0D68`(`gs[0x66]` = 武器/徒手旗標):
- **武器路徑(0x0D68,gs[0x66]≥0)**:`op_68 0x08`→`gs[0x7c]`;`op_68 0x02 & 0x1f`→`gs[0x5d]`(骰數);若非零 `op_68 0x01 & 0x3f`→r4,`char_data[0x0c]`(STR),`op_31 gs[0x5d]`(STR rcr-減);最後 **`op_36 0x05`(÷5)**。→ **公式 = f(武器骰 op_68 欄位, STR) ÷ 5**。
- **徒手路徑(0x0D54,gs[0x66]<0=0xFF)**:`char_data[0x27]`(**Fistfighting 技能**,player.c 0x27)`min(.,8)` → `op_0D 0x0EC2` 查表(骰描述)→ `char_data[0x0c]`(STR)→ `op_36 0x05`(÷5)。
  - **徒手骰表 @res3 0x0EC2** = `00 01 21 41 22 42 23 24 33 34 36 37`(高 nibble=骰數、低 nibble=骰面的遊戲骰式編碼)。
- **骰擲子程式(`load_resource res:0x03 off:0x06EC`)**:`r2>>5` 取骰式 index → `op_0D 0x0713` 查骰值表;`r2 & 0x1f`→r4(骰數);迴圈 r4 次:`r2=0x0f`(or 表值)`op_4D`(RNG)`+1` `op_2F gs[0x5d]`(rcr-加)累加。
  - **骰值表 @res3 0x0713** = `04 06 08 0a 0c 14 1e 64 …`(= d4/d6/d8/d10/d12/d20/d30/d100)。

### op_68 是 oracle 不可解的硬阻(weapon 傷害)
- **op_68 在 opendw `targets[]` 為 NULL**(engine.c:583 區,handler @0x450A,**未逆向**);disasm.cpp 僅標 `1 arg`;`dos/dragon.asm` 不及該位址;`doc/script.md` 無。res3 用 op_68 **22 次**(命中/傷害/AI 路徑),武器傷害核心(0x0D68 的 `op_68 0x08/0x02/0x01`)即靠它讀「武器記錄的位元欄位」。**無任何可逐指令對齊的 oracle** → 須從原始 COM 反組譯 op-dispatch 表(現無素材)。

### 與 DOS 校準 / combat.cpp 是否一致
- 校準錨點(docs/43 §9):徒手 Str10 → {3,4,6}、傷害 `max(3, floor(1.5×raw))`、`×3/2`、AV=DV=Dex/4、AC 壓命中不減傷。
- bytecode 顯示的徒手路徑用 **Fistfighting 技能 + STR + 骰表 + `op_36 0x05`(÷5)**,與校準的 `×3/2`/`max(3)` **結構不同**(bytecode 是「骰 ÷5」而非「×1.5 取下限 3」)。**尚不能判定等價**:因徒手路徑的最終命中/傷害仍經 op_68(0x0E83 等)與 roster 數值,未端到端跑出數字驗證。
- **結論:不可升級標示**。weapon 傷害被 op_68 擋住、徒手傷害未端到端跑出數字對校準,故 combat.cpp **維持「手冊/實機校準」標示不變**(嚴守鐵則:未經 bytecode 跑通+對拍不謊稱 oracle)。

### 本輪交付
- 實作 op_51(roster argmax)+ vm_selftest(PASS);ctest 維持全綠(17 項)。
- 反推並記錄 to-hit/傷害公式結構(op 層級 + res3 骰表/骰值表 byte 值)。
- 精確標定硬阻 op_68(weapon 傷害,oracle 不可解)與 roster announce 迴圈卡點。

### 下一步
1. **roster 完成**:對齊 announce 迴圈所需 gs 種子(gs[0x41] 群 index、gs[0x47/0x48] 等),讓怪物進 char_data 槽 + gs[0x1F] 增 → 到 actor 迴圈。
2. **op_68 反推**:從原始 dragon.com 反組譯 0x450A(需新素材);或由 op_68 在 22 處的「operand → 取哪段位元」用法 + DOS 校準值**反推語意**(風險:非逐指令對齊,須標為「推斷」)。
3. 端到端跑出徒手一回合傷害數字 → 對 DOS 校準 {3,4,6};一致才可考慮升級標示。

---

## 11. 更新(第五輪:完整反推徒手傷害公式 → 端到端跑數字 → 對拍 bytecode)

### 達成:徒手傷害公式 = 原版真值(bytecode 反推 + 端到端執行驗證)
**`徒手傷害 = 傷害骰(descriptor) + floor(STR/5)`,無 ×3/2、無 floor(3)。**

### 關鍵前置修復:自我修改碼(self-modifying code)
- res3 骰子子程式(0x06EC)用**自我修改碼**設骰面:`0x06F6 word_3ADF[0x0701]=r2`(骰面值)patch 掉 `0x0700 op_09` 的 immediate operand(offset 0x0701)。
- **opendw**:`running_script->bytes` 與 `word_3ADF->bytes` 都來自 `resource_get_by_index`(engine.c:899),當 `word_3AE8==word_3AEA`(script_res==data_res)是**同一份 buffer** → 自改生效。
- **remake bug**:script / data_bytes 為分離 vector,op_14/15/16/18/1C 寫 data_bytes 不影響 fetch 的 script → 自改失效(op_4D 拿到 placeholder 0x0f)。
- **修復**:新增 `VmState::wdata(idx,v)` —— 寫 data_bytes,且 `script_res==data_res` 時同步寫 script。op_14/15/16/18/1C 全改走 wdata。回歸 17/17 不破。

### 完整算術序列(res3,oracle 行號)
1. **骰子 descriptor**(0x0D54-0x0D5E):`idx = min(char_data[0x27]=Fistfighting, 7)`;`descriptor = table_0EC2[idx]`(byte)。
   - **descriptor 表 @0x0EC2** = `00 01 21 41 22 42 23 24 …`。解碼:`sides = table_0713[descriptor>>5]`、**dice = (descriptor & 0x1f) + 1**。
   - **骰面表 @0x0713** = `04 06 08 0a 0c 14 1e 64`(d4/d6/d8/d10/d12/d20/d30/d100)。
   - Fist=0 → descriptor `0x00` → **1d4**(端到端驗 [1,4])。Fist 1→2d4、2→2d6、3→2d8、4→3d6 …(min(Fist,7) 封頂)。
2. **骰擲**(0x06EC,入參 r2=descriptor):`sides` 經自改 patch 進 0x0700;loop `(descriptor&0x1f)+1` 次:`r2=sides; op_4D(→[0,sides)); r2++(→[1,sides]); op_2F gs[0x5d](累加)`。
3. **STR 修正**(0x0D63→0x0D7F→0x0D81):`r2 = char_data[0x0c]=STR; op_36 0x05(÷5); word_3ADF[0x0dae]=r2` —— 即把 **floor(STR/5)** patch 進 0x0DAD `op_30` 的 immediate。
4. **合成**(0x0D9E-0x0DBF,每次命中):`r2 = 骰和(gs[0x5d]); op_30 patched(+floor(STR/5)); op_2F gs[0x61](跨命中累加)`;最終 `gs[0x5d] = gs[0x61]`(0x0DD2)。

### 端到端跑出的數字(跑 res3 bytecode,非手算)
| 輸入 | bytecode 輸出範圍 | = 1d4 + floor(STR/5) |
|---|---|---|
| STR 4, Fist 0 | [1,4] | 1d4 + 0 |
| STR 5–9, Fist 0 | [2,5] | 1d4 + 1 |
| STR 10–14, Fist 0 | **[3,6] = {3,4,5,6}** | 1d4 + 2 |
| STR 20–24, Fist 0 | [5,8] | 1d4 + 4 |
| STR 25, Fist 0 | [6,9] | 1d4 + 5 |

### ÷5 vs ×3/2 矛盾的真相
- **真相 = `+floor(STR/5)`(加法)**,**不是 ×3/2**。op_36 0x05 = STR÷5,結果**加進**傷害骰和(0x0DAD op_30),全程**無乘 3 除 2**。
- DOS §9 推的 `×3/2 + floor(3) → {3,4,6} 無 5`,是 **53 筆小樣本的近似**:真值 1d4+2 = {3,4,5,6} **含 5**;bytecode **證偽了「無 5」**。`×3/2` 與 op_33~36 的關聯是巧合擬合,非真機制(op_33~36 用於別處如初始化乘累加,非徒手傷害)。

### to-hit(bytecode 讀出,門檻鏈待續)
- **命中骰 = 1d16+3**(0x0F73:`r2=0x10; op_4D(→[0,16)); op_30 0x03(+3)` → [3,18];roll==3 自動命中)。
- 門檻側比較鏈(攻擊者 AV/裝甲 char_data[0x59] − 目標 AC table_0372[gs[0x84]])**尚未端到端驗證** → combat.cpp 命中骰式暫保留 2d10、門檻 base+dv+ac,不動未驗證行為。

### combat.cpp 標示升級
- **徒手傷害:升級為「bytecode 反推 + 端到端驗證」** —— `str_damage_bonus = STR/5`(取代 best-fit STR/16);傷害 = 骰 + floor(STR/5),移除 ×3/2 與 floor(3)。
- **武器傷害:維持 best-fit**(op_68 原版未逆向,擋住武器骰欄位讀取)。
- **to-hit:維持暫定**(骰式真值 1d16+3 已記,門檻鏈待驗)。

### 對拍交付
- `verify_combat_script` 新增**徒手傷害對拍 res3 bytecode**:跑 res3 0x0D54 路徑取分布,驗 STR10→[3,6] 含 5、STR20→[5,8]、STR4→[1,4](無 floor3)。= combat.cpp 公式 ⇄ bytecode 雙向確認。
- `verify_combat` D 案例更新:從 DOS best-fit {3,4,6} 改為 **bytecode 真值 {3,4,5,6}**(含 5)。

---

## 12. 更新(第六輪:完整反推 to-hit 門檻鏈 → 端到端跑 bytecode 驗證)

### 達成:to-hit 命中判定 = 原版真值(bytecode 反推 + 端到端執行驗證)
**`HIT ⟺ roll ≤ 13 + AV − (DV+AC)`,roll = 1d16+3 ∈ [3,18]。**`roll==3` 恆命中、`roll==18` 恆失手。**roll-under 系統**。

### 完整判定式(res3 to-hit 子程式 @0x0F73,op 層級)
```
0x0F73  r2 = 0x10                          ; 16
0x0F75  op_4D                              ; r2 = RNG[0,16)
0x0F76  op_30 0x03                         ; r2 += 3  → roll ∈ [3,18]  (= 1d16+3)
0x0F78  cmp r2, 0x03 ; jz 0x0FA8(clc=HIT)  ; roll==3 → 恆命中
0x0F7D  cmp r2, 0x12 ; jc 0x0FAA(stc=MISS) ; roll==18(0x12)→ 恆失手
0x0F82  gs[0x7b] = r2                       ; 存 roll
0x0F84  r4 = gs[0x84] ; op_0D 0x0372 → gs[0x7a]   ; 目標 defense(= DV+AC)
0x0F8B  r2 = char_data[0x59]                ; 攻擊者 armor(實測對門檻無影響)
0x0F8D  op_30 0x80 ; op_2F gs[0x79]         ; + 0x80 + AV(gs[0x79])
0x0F94  op_30 0x0d                          ; + 0x0d(=13)
        (op_31 gs[0x7a]:− 目標 defense;op_32 0x80:− 0x80)
0x0FA3  cmp r2, gs[0x7b] ; jnc 0x0FAA(MISS) ; 若(13+AV−def) >= roll → 命中側比較
0x0FA8  clc=HIT  /  0x0FAA  stc=MISS
```
- 化簡(0x80 加後再減抵消):**門檻 = 13 + AV − def**;`roll ≤ 門檻 → HIT`。

### 端到端跑出的驗證(掃 AV / def,跑 res3 bytecode)
| AV | def | bytecode 門檻(最大命中 roll) | = 13 + AV − def(夾 [3,17]) |
|---|---|---|---|
| 0 | 0 | 13 | 13 |
| 1 | 0 | 14 | 14 |
| 3 | 0 | 16 | 16 |
| 5 | 0 | 17(夾,18 恆失手) | 18→17 |
| 5 | 5 | 13 | 13 |
| 5 | 10 | 8 | 8 |
| 5 | 15 | 3(夾,3 恆命中) | 3 |
- **char_data[0x59](armor)對命中門檻無影響**(掃 0/5/10/50/100 門檻不變)。

### 與 DOS / SDA 交叉檢查
- 命中率 = (門檻 − 3 + 1)/16(roll∈[3,18] 共 16 值,roll≤門檻 命中)。
- DOS §9:無甲近戰命中率 **73–80%**、AV **3–6**。代入:AV=5、def≈3–5 → 門檻 13–15 → 命中率 11–13/16 = **69–81%**,**與 DOS 完全相符**。
- SDA「AV vs DV」結構吻合:門檻單調隨 AV↑ / def↓。

### combat.cpp 標示升級 → 是
- **to-hit 升級為「bytecode 反推 + 端到端驗證」**:`roll = 1d16+3`;`HIT ⟺ roll ≤ kToHitBase(13) + AV − (DV+AC)`;roll3 恆命中、roll18 恆失手。取代先前暫定 2d10 / roll-over。
- AC 仍在命中側(DOS §9)、不減傷 —— bytecode 證實(armor 0x59 不影響門檻、def=DV+AC 壓低門檻)。
- **武器傷害維持 best-fit**(op_68 原版未逆向);**徒手傷害已是 bytecode 真值**(§11)。

### 對拍交付
- `verify_combat_script` 新增 **to-hit 對拍 res3 bytecode**:跑 0x0F73 掃 AV/def,驗 bytecode 門檻 == `13+AV−def`(夾 [3,17]),5 case 全 PASS。
- `verify_combat` E 案例更新:從 roll-over「AC 抬高 need」改為 **roll-under「AC 降低門檻」**(bytecode 真值)。

### 至此戰鬥結算的 oracle 狀態
| 項目 | 狀態 |
|---|---|
| RNG(op_4D) | bytecode 移植,對拍 oracle 演算法 |
| to-hit(1d16+3,門檻 13+AV−def) | **bytecode 反推 + 端到端驗證** ✅ |
| 徒手傷害(dice + STR/5) | **bytecode 反推 + 端到端驗證** ✅ |
| 武器傷害 | best-fit(op_68 原版 NULL,無 oracle)⚠ → **§13:op_68 已反組譯,骰來源/解碼升級 bytecode 確認;STR bonus 仍 best-fit** |
| 怪物 roster pipeline | 部分逆向,未端到端(§9)⚠ |

---

## 13. 更新(第七輪:反組譯原始 DRAGON.COM op_68 handler → 武器傷害真值缺口收斂)

> 日期:2026-06-14
> 方法:opendw `targets[]` 把 op_68 標 NULL(handler @0x450A,從未逆向)。直接從原始
> 16-bit DRAGON.COM 反組譯(docker ndisasm,COM 載入 @CS:0x100,file offset = addr − 0x100)。
> **DRAGON.COM 不入庫**;只入庫結論。

### op_68 handler 定位(已確認)
- VM dispatch loop @0x3ACF:`es lodsb; xor ah,ah; mov bx,ax; shl bx,1; jmp [bx+0x3960]`。
  跳表 base = `0x3960`;op_68 → `[0x3960 + 0x68*2]` = `[0x3A30]` = **0x450A**(= opendw `targets[]` 標註值,完全吻合)。
- handler 範圍 0x450A..0x453F(下一 op_69 起點)。

### op_68 反組譯(file offset 0x440A)
```
0x450A  bl=[0x3867]; bh=0; bx<<=1        ; 物品槽 index(= gs[7])× 2(字表)
0x4512  al=[0x3866]; di=ax               ; 角色 index(= gs[6])
0x4519  ax=0xCA4C; ah += [di+0x386a]     ; char_ext 基底 + 角色頁(selector<<8)
0x4520  ax += [bx+0x4456]                ; + unknown_4456[slot](= slot*23,23B/item)
0x4524  di=ax; lodsb; di += ax           ; + operand(物品記錄內 byte offset)
0x452C  ax=[di]; [0x3ae2]=al             ; word_3AE2 低位 = char_ext[di]
0x4531  if [0x3ae1]!=0: [0x3ae3]=ah      ; word 模式才取高位
```
- **op_68 = op_69(@0x453F)的「讀」孿生**(op_69 同定址、`mov [di],al` 寫)。opendw 自身已逆出
  op_69(engine.c:2846)→ 二者定址法相互佐證:`char_ext[(sel<<8) + unknown_4456[slot] + operand]`,
  `sel = gs[gs[6]+0x0A]`、`slot = gs[7]`、stride 23、區段 = `data_CA4C`(opendw 拆成獨立 4096B 陣列;
  原始 binary 中 0xCA4C = 0xC960 + 0xEC,在同一資料段內,即 char record 內偏移 +0xEC 的物品/裝備區)。
- **unknown_4456 表**(@0x4456,binary 實讀)= `{0,23,46,69,92,115,138,161,184,207,230,253,...}` = **slot × 23**。
  佐證 docs/44 §2「裝備 23B/件」+ op_64(@0x446F)裝備邏輯:掃 12 槽 × 23B、找 byte[+0xB]==0 空槽寫入。

### 武器傷害邏輯(op 層級,res3 0x0D68,**op_68 反推後可完整讀懂**)
```
0x0D68  op_68 0x08 → gs[0x7c]            ; 主傷害骰 descriptor = 裝備記錄 byte[8]
0x0D6C  op_68 0x02; &0x1f → gs[0x5d]     ; byte[2] 低 5 bit
0x0D72  test; jz 0x0D81                  ; byte[2]&0x1f==0 → 跳過 op_36(÷STR)
0x0D76  op_68 0x01; &0x3f; op_21; load STR(0x0c); op_31 gs[0x5d]   ; (byte[2]!=0 才走)
0x0D7F  op_36 (÷ STR/5,patch 0x0DAE)     ; ← byte[2]==0 時被 0x0D73 jz 跳過
0x0DA1  load_resource 0x03:0x06ec        ; 用 gs[0x7c]=descriptor 擲骰(與徒手共用)
0x0DA5  r2 = gs[0x5d](骰和); 0x0DA7 op_30 0x00; 0x0DAD op_30 <0x0DAE 自改值>
```
- **descriptor 解碼**(同 0x06EC):sides = `{4,6,8,10,12,20,30,100}[d>>5]`、count = `(d & 0x1f)+1`。

### 端到端驗證(跑 res3 bytecode,非手算;入 `verify_combat_script`)
| op_68 0x08(byte[8]) | byte[2] | bytecode 輸出 | = roll(descriptor) |
|---|---|---|---|
| 0x00 | 0 | [1,4] | 1d4 |
| 0x21 | 0 | [2,12] | 2d6 |
| 0x05 | 0 | [6,24] | 6d4 |
| 0xA3 | 0 | [4,80] | 4d20 |
| 任意 | =5 | [5,5] | **定值 5(覆寫骰擲)** |
| 任意 | =2 | [2,2] | **定值 2** |

### 結論
1. **op_68 0x08 = 主傷害骰來源(byte[8]),骰式解碼 = sides[d>>5]/(d&0x1f)+1** —— **bytecode 反推 + 端到端驗證,證據確鑿**。combat.cpp 武器骰(`primary_dmg`,fraterrisus bit[64-71]= byte[8])**結構正確,標示升級為 bytecode 確認**。
2. **byte[2]&0x1f != 0 → 傷害 = 定值(byte[2]&0x1f),覆寫骰擲** —— 端到端確認(= 遠程/特殊定傷武器路徑;**修正** §10「byte[2]=骰數」的舊假說)。
3. **武器傷害是否 +floor(STR/5):bytecode 證據矛盾,維持 best-fit(誠實鐵則)**:
   - 隔離執行:byte[2]==0 時 0x0D73 jz **跳過 op_36** → 武器路徑**不加 STR bonus**(1d4 STR20 → [1,4])。
   - 但 0x0DAD 加的是**自我修改位址 0x0DAE**的值;完整一場戰鬥中該值可能由前序攻擊者(徒手或 byte[2]!=0 武器)的 op_36 殘留 → **隔離分析無法判定真機殘留與否**(self-modifying-code 不確定性)。
   - DOS 實機(§43:Str14→3~4、Str21→6)+ SDA 均示武器傷害隨 STR 增 → **保留 +floor(STR/5)**,不依隔離 bytecode 逕刪(無完整戰鬥 oracle 可確認;刪除恐 regress DOS 校準)。
4. **remake 已實作 op_68**(`interpreter.cpp` op68_get_char_ext,op_69 讀孿生),戰鬥武器路徑不再撞未實作 opcode。

### combat.cpp 標示升級
- **武器主傷害骰來源 + 骰式解碼:best-fit → bytecode 反推 + 端到端驗證** ✅(op_68 0x08 = byte[8])。
- **武器 STR bonus:維持 best-fit**(self-modifying-code 矛盾,無完整戰鬥 oracle)⚠ —— 誠實標示。

### 至此戰鬥結算 oracle 狀態(更新)
| 項目 | 狀態 |
|---|---|
| RNG(op_4D) | bytecode 移植,對拍 oracle 演算法 |
| to-hit(1d16+3,門檻 13+AV−def) | bytecode 反推 + 端到端驗證 ✅ |
| 徒手傷害(dice + STR/5) | bytecode 反推 + 端到端驗證 ✅ |
| **武器主傷害骰來源/解碼(op_68 0x08=byte[8])** | **bytecode 反推 + 端到端驗證 ✅(本輪)** |
| 武器 STR bonus(+STR/5?) | best-fit(self-modifying-code 矛盾,無完整戰鬥 oracle)⚠ |
| 武器定傷(byte[2]!=0) | bytecode 反推 + 端到端驗證 ✅(本輪) |
| 怪物 roster pipeline | 部分逆向,未端到端 ⚠ |

---

## 13. 更新(第七輪:roster announce 迴圈精確診斷)

### 結論:roster **已建立**(怪物在群緩衝區),卡點 = res18↔res4 選單互動迴圈(非 roster 缺失)

本輪未改 remake source(僅 probe 調查),ctest 維持 17/17。把 announce/roster 卡點從「不明空轉」精確化到 **op 與 gs 層級**。

### announce 迴圈結構(res3 0x0029-0x003F)實為「怪物登場訊息 + 戰鬥選單」
- `0x0029 load_resource res:0x12(=res18)off:0x0000` → 解一隻怪名;`0x002D call 0x010e` 列出整群怪名;`0x0030 set_msg " appear."`;`0x0038 load res18 off:0x0097`(= **主戰鬥選單** "Will the party: Fight/Quickly fight/Run/Advance");`0x003C jc 0x0028`(分頁/重繪迴圈)。
- **res18 不是單純文字**:含 3 個 op_89 鍵盤等待選單(實測):
  - `@res18:0x00F8` 主選單:`F(0xC6)→0x0108`、`Q(0xD1)→0x0108`、`R(0xD2)→0x02CF`、`A(0xC1)→0x02EE`。
  - `@res18:0x01EA` 角色動作選單:`A(0xC1)→0x03BC`、`D(0xC4)→0x02C5`、`C(0xC3)→0x0517`、`R(0xD2)→…`。
  - `@res18:0x029B` 第三層選單。

### roster 已正確建立(關鍵發現)
- **怪物參戰資料寫進「群定義緩衝區」data[0x03D6],不是 char_data 角色槽**(slots 4-6 HP 恆 0、選擇子恆 0 是預期,非 bug)。
- setup 鏈確認運作:`call 0x04F1`(遭遇 setup,經 op_0F 走訪 res31 + op_4D)→ 怪物群模板填 gs[0x28..0x36](`01 00 0c 00 43 a2 0c 28 c5 14 …`,gs[0x27]=群數 1);`0x053B-0x05A0` 迴圈用 op_16/op_18 把 res31 怪物記錄(0x21 bytes)寫進 data[0x03D6]。
- **端到端驗證**:跑完整 announce + 餵 Fight/Attack 鍵後,data[0x03D6] = `2d 2d 12 42 40 0b 44 00 04 68 0a 12 …`(**真實怪物戰鬥屬性**,非全 0)。byte[0x0a] = 怪物 count(`op_10 0x41,0x0a` 讀、`op_18 0x41,0x0a` 寫,& 0x1f)。

### 精確卡點:res18↔res4 選單互動迴圈空轉(headless)
- 餵 `{Fight=0xC6, Attack=0xC1×N}` 後,流程**仍未抵達 actor 迴圈(res3 0x0075)**,而是在 **res18 @0x04B4** 空轉:該處 `op_58 → res4 off:0x0000`(動作輸入資源),形成 res18(選單)↔res4(輸入)互動迴圈。
- 與 op_89 同類:這是**逐角色動作指派的 UI 互動迴圈**,headless 無法滿足其「全角色已選定動作」的推進條件(需要 res4 的逐角色輸入狀態 + 分頁推進,非單純鍵注入)。
- op_89 在此流程執行 2 次(主選單 + 角色動作選單),但角色動作選單迴圈不收斂。

### 還差什麼(比上輪更具體)
1. **res4(動作輸入資源)的逐角色狀態機**:res18 主選單選 Fight 後,進入「為每個參戰角色指派動作(Attack/Defend/Cast)」的迴圈,經 op_58 反覆載入 res4。需逆向 res4 的逐角色推進條件(哪個 gs counter 標記「此角色已選」、何時全員完成 → 跳出進 res3 actor 迴圈 0x0075)。
2. **headless 動作注入**:類似 op_89 headless_keys,但需對「角色動作選單」逐角色餵鍵 + 滿足完成條件。目前 headless_keys 序列會被選單迴圈無限消耗。
3. 一旦進 actor 迴圈(0x0075):op_51 找行動者 → to-hit(0x0F73,已驗)→ 傷害(0x0D54,已驗)→ 對 data[0x03D6] 群緩衝區的怪物 HP 結算。**結算公式都已 bytecode 真值化,只差驅動到那一步**。

### 武器 STR bonus 定論狀態
- **未定論**:因完整戰鬥仍未跑到 actor 迴圈執行武器攻擊(0x0D68 op_68 路徑),武器 STR bonus 的自改碼殘留行為無法在完整戰鬥中觀察。**維持 best-fit 標示不變**(嚴守鐵則)。

### 本輪交付
- 精確診斷:roster 已建(群緩衝區 data[0x03D6]),卡點 = res18↔res4 逐角色動作選單迴圈(op 與 gs 層級定位)。
- 未改 remake source;ctest 17/17 不破。下一步明確:逆向 res4 逐角色動作狀態機 + headless 動作注入。

### res4 = 目標選擇(target selection),含第 3 個 op_89
- res4(248B)是 **「Target...」目標選擇** 資源,流程:`gs[0x90]/gs[0x91]` = 目標槽;若 `gs[0x90]` 號誌位設(0xFF=自動/無需選)→ `0x0045 r2=gs[0x81]; clc; retf`(完成,回主流程);否則 `0x0055 draw "Target..."` + `0x007D wait_event`(**第 3 個 op_89**,目標鍵表)。
- **headless 卡在此 op_89**:目標未選定 → "Target..." 提示無限等待。
- 完整命中鏈所需 headless 互動:**主選單(Fight)→ 角色動作選單(Attack)→ res4 目標選單(選怪)**,3 層 op_89 + 各自的逐角色/逐目標推進狀態。

### 下一步(最小解鎖路徑)
1. headless 對 3 層 op_89 分別注入:Fight(0xC6)→ 每角色 Attack(0xC1)→ 每次 res4 目標(怪物 index 對應鍵)。
2. 或設 gs[0x90]/gs[0x91] 自動目標(號誌位)讓 res4 跳過 "Target..." prompt → 直接 retf。
3. 滿足角色動作選單的「全員已選」推進(res18↔res4 迴圈收斂)→ 進 res3 actor 迴圈 0x0075 → to-hit/傷害對 data[0x03D6] 怪物結算。
- **結算公式(to-hit 1d16+3 門檻、徒手 dice+STR/5)均已 bytecode 真值化並對拍**;只差驅動 3 層 UI 互動到 actor 迴圈。

---

## 14. 更新(第八輪:headless 驅動 3 層選單 — 確認非 opcode 缺失,卡在動作指派狀態機)

### 怎麼驅動 3 層選單
- op_89 headless_keys 序列 + **新增數字鍵(type 0x01)處理**:對照 wait_for_event 0x29EF,按鍵 '1'..'7'(0xB1..0xB7)→ index = key−0xB1;< gs[0x1F] → 命中該數字筆、設 gs[6]=index(選定隊員/目標)。入 vm_selftest(CC/DD,PASS)。
- 3 層 op_89 已定位(實機鍵表):
  - **主選單 @res18:0x00F8**:Fight `F`(0xC6)→0x0108、Run `R`(0xD2)、Advance `A`(0xC1)。
  - **角色動作選單 @res18:0x01EA**:Attack `A`(0xC1)→0x03BC、Defend `D`(0xC4)、Cast `C`(0xC3)。
  - **目標選擇 @res18:0x0491 + res4:0x007D**:Attack `A`(0xC1)→0x04AB;res4「Target...」數字鍵選怪。

### 抵達 actor 迴圈了嗎 → 否,但精確定位卡點(非 opcode 缺失)
- 餵 Fight → Attack×N:**處理完全部 4 名角色**(gs[6] 0→1→2→3),但 gs[6]=3 後**動作指派迴圈不終止、無限重提示**,未抵達 res3 actor 迴圈 0x0075。
- **關鍵:last_unimpl=0x00 全程無未實作 opcode** —— 卡點不是 opcode 缺失,而是 **per-character 動作指派狀態機不收斂**。
- 迴圈內容(已 trace 完整 cycle,皆已實作 op):action 選單(op_89)→ op_63(寫角色 char_ext 動作)→ op_58 進 res4 目標選擇 → res4 用 0x06b1/0x06b5 掃怪物群找目標 → 回 action 選單。怪物群緩衝 data[0x03D6] byte0a(count)=0x0a(怪確實在)。

### 卡點精確化(比上輪更深)
- 動作指派迴圈處理完 4 員後,「全員已指派 → 退出進 actor 迴圈」的終止條件未滿足:gs[6] 卡在 3、重複 action 選單。
- 推測根因:per-character 動作確認/推進依賴更廣的遊戲狀態(gs[0x35] 事件旗標、char_ext/gs 的逐角色「已選動作」標記、res3@0x08b6 的動作輸入 driver 計數器),這些由完整地圖層戰鬥進入流程建立,**headless 直跑 res3 offset 0 + 合成 context 無法完整重現**。
- **非單一 opcode 或鍵序列可解**:是跨 res3/res18/res4 的互動式動作指派狀態機,需逐欄逆向「每角色動作如何標記完成 + 全員完成偵測」。

### roster + 結算現況(再次確認)
- roster **已建立**(怪物在群緩衝 data[0x03D6],非 char_data 槽,為原版設計)。
- to-hit(1d16+3 門檻 13+AV−def)、徒手傷害(dice+STR/5)**均已 bytecode 真值化 + 端到端對拍**。
- **武器 STR bonus 仍未定論**:需 actor 迴圈實際執行武器攻擊(0x0D68)才能觀察自改碼殘留,目前未到 → **維持 best-fit 標示不變**(嚴守鐵則)。

### 本輪交付
- op_89 數字鍵(type 0x01)目標/隊員選擇 headless 處理(逐指令對照 wait_for_event 0x29EF;入 vm_selftest CC/DD)。
- 精確診斷:卡點 = per-character 動作指派狀態機不收斂(**非 opcode 缺失**,last_unimpl=0);怪物 roster 已建、結算公式已驗。
- ctest 17/17 不破。下一步:逆向 res3@0x08b6 動作輸入 driver 的「逐角色動作完成標記 + 全員完成偵測」。

---

## 15. 更新(第九輪:res3 全戰鬥閉環打通 — actor 迴圈 → to-hit → 傷害 → 怪物 HP 實扣)

> 日期:2026-06-17
> 結果:**閉環跑通**。headless 驅動完整一回合動作指派 → 抵達 res3 actor 迴圈(0x0075)→
>   執行 to-hit(0x0F73)+ 徒手傷害(0x0D54)+ 怪物 HP 扣減(0x07C7),**怪物群緩衝
>   data[0x03D6] HP byte 實際被扣**。全程無未實作 opcode。新增 `verify_combat_round` 守護。

### 收斂鏈(第八輪卡點解法 — 逆向 res18/res4 動作指派狀態機)
第八輪誤判「gs[6] 卡在 3」;真正的逐角色計數器是 **gs[0x67]**(非 gs[6]),收斂條件鏈:
- **res18 主選單 @0x00F8**:Fight `F`(0xC6)→ 0x0108 `gs[0x67]=0`(逐角色動作 index 歸零)。
- **逐角色迴圈 @0x0111**:設 gs[0x06]=gs[0x67] → 角色動作選單 @0x01EA Attack `A`(0xC1)→0x03BC
  → 武器檢查 → 攻擊風格選單 @0x0491 Attack blow `A`(0xC1)→0x04AB → 0x04AD `call 0x053B`
  (寫動作記錄 0x00=攻擊)→ 0x04B4 `op_58 res4`(目標選擇)→ **0x04B8 `jc 0x0111`**。
- **res4 目標 @0x0000**:`gs[0x92]` 目標模式;數字鍵('1'|0x80=0xB1)選怪 → 0x008F `op_32 0xB1; clc; retf`
  → res18 0x04B8 carry **clear** → 不跳 0x0111 → 0x04C0 `jmp 0x023E`:**`inc gs[0x67]`**(下一角色)。
- **全員完成偵測 @0x0240**:`w3AE4=gs[0x67]; check_gs 0x1F; jnc 0x0111`(gs[0x67] < gs[0x1F] → 下一角色;
  == → 落到 0x0247)。
- **確認 @0x0247**:`"Use these commands?"` → op_8C prompt → **`Y`(0xD9)→ 0x0265 clc retf**
  → 返回 res3 0x0069 → 0x006C 載 res3@0x08b6(combat-stat setup)→ 0x0075 **actor 迴圈**。

→ 第八輪缺的兩把鑰匙:**res4 目標數字鍵**(逐角色目標)+ **最終 `Y` 確認**。補上即收斂。

### 三個自我修改碼/持久性 bug(閉環真正卡住的根因,非狀態機本身)
驅動鍵補齊後,初期仍空轉。逐層追出 remake VM **資源 buffer 持久性與 opendw 不一致**
(opendw `resource_get_by_index` 回傳持久 allocation;remake 每次重載全新 copy → 自改/跨資源寫入丟失):

1. **run_script 同資源自改丟失**:res3 `for_call 0x0761`(op_5C)逐角色經 run_script 寫
   **黨側 initiative 陣列 data[0x04EA]**(`op_15 0x04EA`)。run_script 對同一資源(script_index==
   script_res)重載全新 copy、返回還原舊 copy → 寫入丟失 → gs[0x73](黨 initiative)恆 0
   → 黨永不行動(0x0AFA=0)。**修**:同資源 run_script 沿用 live s_.script、返回保留自改。
2. **跨資源 op_17 寫入丟失**:res18 用 `store_data_res`(op_17)把角色動作(0x00=攻擊)寫進
   **res3 @0x04CE**;res18 返回 res3 後該寫入須可見。原 res_bytes_by_index 對「非當前資源」
   載全新 copy 寫進 res_cache,但 op_59 返回時從 call frame 還原(丟棄 res_cache 寫入)
   → 動作記錄恆 0x0b(預設 advance),非 0x00(attack)→ 黨側不打怪。**修**:op_58 跨資源
   call 時把「被暫停的當前資源」live bytes 種進 res_cache;op_59 跨資源返回時優先從 res_cache
   還原(含子資源期間的 op_17 寫入)。
3. **op_58 葉子自改不可污染上層**:上述 res_cache 機制**僅對「跨資源」**生效;同資源葉子
   子程式(骰子 0x06EC、徒手/武器傷害 0x06EC 自改)維持「框內有效、返回丟棄」(§11 語意),
   否則 verify_combat_script 回歸。op_59 以 `cross_res = (child_res != parent_res)` 區分。

### 補實作 4 個 opcode(opendw 有 oracle,逐指令對齊)
抵達 actor 迴圈與回合清理過程依序撞到(皆 opendw engine.c 已實作、非 NULL):
- **op_91**(@engine.c:5934)`draw_player_status_panel()` — 純渲染 leaf,headless no-op。
- **op_92**(@5940)`draw_player_status_panel + ui_draw_string + 延遲計時輪詢` — UI/timer leaf;
  斷言 gs[0xDC]==0(對齊 oracle,否則 opendw 亦 abort),headless 取「無輸入」確定性分支 → no-op。
- **op_97**(@6051)`load_char_data`:r2 = char_data[base + operand + r4](base=selector<<8;
  = op_5D 同定址多加 r4)。**關鍵**:這個 opcode 之前未實作,使武器傷害 byte[2] 路徑在
  0x0D7B 早停 → 造成 §13「byte2!=0=定值」的**假象**(見下)。
- **op_98**(@6082)`store_char_data`:op_97 寫孿生 + `gs[gs[6]+0x18]=0`。

### 閉環證據(verify_combat_round,固定 seed,headless)
```
抵達 res3 actor 迴圈 0x0075;怪物群 count(byte0a&0x1f)=1
to-hit(0x0F73)=5 徒手傷害(0x0D54)=5 怪物HP扣減(0x07C7)=5 黨側攻擊(0x0AFA)=12
怪物 byte[0x09] @0x03df: 0x03 -> 0x02   ← 怪物 HP 群緩衝 byte 實扣
兩次固定 seed 軌跡 byte-identical(確定性)；全程 last_unimpl=0
```
- **怪物 HP 欄 = 群緩衝 data[0x03D6] 記錄 byte[0x09]**(扣減經 0x07C7 路徑;另有 HP 工作陣列
  data[0x0278+idx*2],由 0x07D3 `op_0D 0x0278`/`op_15 0x0278` 讀寫)。
- **不對拍具體 HP 數值**:opendw 無可獨立執行的完整戰鬥 oracle 逐回合 byte-diff(§9 限制);
  verify_combat_round 守護「閉環跑通 + 核心公式路徑被執行 + 怪物 HP 變化 + 確定性」,
  **不謊稱數值對拍**。公式數值真值由 verify_combat_script(隔離子程式對拍)守護(不變)。

### 武器 STR bonus 定論(受阻序列 B — 已解)
§13「byte[2]&0x1f != 0 → 傷害=定值」是 **op_97 未實作的假象**:武器路徑 0x0D7B `load_char_data`
(op_97)未實作 → 腳本早停,gs[0x5d] 停在 byte2 設定值 → 看似「定值」。實作 op_97 後端到端重跑:
- **武器純骰路徑(byte[2]&0x1f==0):無 STR bonus**。武器 1d4,STR5/STR25 端到端皆 [1,4];
  即使把 0x0DAE(STR-add 自改 immediate)pre-patch 成 5,結果仍 [1,4](**每次攻擊前被重設、
  非殘留**)。根因:0x0D73 `jz 0x0D81` 對 byte2==0 **跳過** op_36(÷STR)。
- **byte[2]&0x1f != 0:非定值**(byte2 作起始值疊加 STR/骰修正;端到端 byte2=5/2d6/STR20→[5,15]、
  byte2=2/1d4/STR10→[2,5])。verify_combat_script 的兩條 byte2 斷言已更正為此真值範圍。
- → **combat.cpp 標示升級**:武器傷害 `dmg_bonus = 0`(無 +floor(STR/5));徒手仍 `+floor(STR/5)`。
  取代第七輪「self-modifying-code 殘留不確定、保留 best-fit」的受阻標示。

### 本輪交付
- 收斂鏈逆向(gs[0x67] 逐角色計數 + `Y` 確認)→ headless 驅動到 actor 迴圈。
- 修 3 個資源持久性 bug(run_script 同資源自改 / op_58·op_59 跨資源 op_17 可見性 / 葉子隔離)。
- 補 op_91/92/97/98(逐指令對齊 opendw)。
- combat.cpp:武器 STR bonus 定論為**無**(端到端驗)→ 標示升級。
- 新增 `verify_combat_round` ctest(閉環守護);verify_combat_script byte2 斷言更正。
- **ctest 31/31 全綠**(原 30 + verify_combat_round)。

### 至此戰鬥結算 oracle 狀態(更新)
| 項目 | 狀態 |
|---|---|
| RNG(op_4D) | bytecode 移植,對拍 oracle 演算法 |
| to-hit(1d16+3,門檻 13+AV−def) | bytecode 反推 + 端到端驗證 ✅ |
| 徒手傷害(dice + floor(STR/5)) | bytecode 反推 + 端到端驗證 ✅ |
| 武器主傷害骰來源/解碼(op_68 0x08=byte[8]) | bytecode 反推 + 端到端驗證 ✅ |
| **武器 STR bonus = 無**(byte2==0 跳過 op_36) | **bytecode 端到端驗證 ✅(本輪定論,受阻序列 B 解)** |
| **全戰鬥閉環(actor 迴圈 → to-hit → 傷害 → 怪物 HP 實扣)** | **跑通 + 確定性守護 ✅(本輪)** |
| 怪物 HP 逐回合「具體數值」對拍 opendw | 受限:opendw 無可獨立執行完整戰鬥 oracle(§9)⚠ |
