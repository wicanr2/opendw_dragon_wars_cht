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
# → ran 2556 steps; halted=1 ... final_pc=0x00fb(op_89 戰鬥選單鍵盤等待)
```
