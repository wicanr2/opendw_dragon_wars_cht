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
