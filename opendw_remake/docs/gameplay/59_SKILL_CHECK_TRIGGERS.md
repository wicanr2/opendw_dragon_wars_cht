# 59 — 非戰鬥技能檢定觸發點(逆向結果)

聚焦「非戰鬥技能(Climb / Hide / Pickpocket / 各 Lore / Tracking / Swim / Bureaucracy /
Merchant…)的實際檢定觸發點是否能從原版 level script 逆出」。Lockpick / Bandage 的
檢定已於 #127 / #135 處理(見 `progression.cpp`),本文件處理其餘技能。

結論先講:**這些技能的檢定觸發點無法從可追蹤的 level event bytecode 逆出 → 受阻**。
下面是支持此結論的證據。

## 真值層級(誠實標示)

| 項目 | 真值層級 |
|---|---|
| 技能欄位置(player record offset 0x20-0x3A) | 可識別(byte 真值)— `opendw/src/lib/player.c` `struct skill_info` |
| op_5D = 唯一「依 offset 讀角色屬性」的 opcode | 可識別(byte 真值)— `get_character_data @engine.c:2568`,dispatch 表 0x5D |
| **非戰鬥技能檢定的「觸發點」(哪格、哪條件下讀 skill 欄)** | **受阻** — 見下方掃描證據 |
| 檢定門檻 / 成功率公式 | 受阻 — roll(op_4D PRNG)+ compare + 跨資源結算未反編 |

## 逆向方法(同 item2 陷阱的 VM-tracing 路徑)

1. 角色技能 = 角色 record offset 0x20-0x3A。讀取這些欄位的**唯一** opcode 是
   op_5D(`get_character_data`):`r2 = char_data[(selector<<8) + property_offset]`。
   level script 若要「讀技能值並據以分支」,必然出現 op_5D 且其 operand(property
   offset)落在 0x20-0x3A。
2. 在 VM 加診斷 hook `CharReadObserver`(`interpreter.{hpp,cpp}`,純觀測、不改 VM
   行為):op_5D 每次讀屬性時回呼 `(prop_offset, value, op_pc)`。
3. 工具 `tools/verify/detect_skill_checks.cpp`:逐關(全 40 關)逐特殊事件格
   (tile>1)→ `script_pc` → 跑 VM,攔截 op_5D,記錄哪格在讀哪個 skill 欄。
   接 `BundleProvider` 讓 op_58 跨資源 call 能跟進共用子 script;並注入選單鍵
   (A–Z | 0x80、Y)讓「玩家選動作後才檢定」的互動分支也能走到。

## 掃描證據(decisive)

全 40 關、599 個特殊事件格,在以下三種逐步放寬的條件下跑 VM:

| 條件 | op_5D 觸發次數 | 讀到 skill 欄(0x20-0x3A)次數 |
|---|---|---|
| 基本(headless) | 0 | 0 |
| + op_58 provider(跟進跨資源子 script) | 0 | 0 |
| + 注入選單鍵(走互動分支) | 0 | 0 |

即使跟進 op_58 共用子 script、並注入選單選擇,**op_5D 在所有事件格 script 中從未被
執行到**,自然也沒有任何「讀 skill 欄」的命中。

事件格 script 的終止點分佈(最後一次掃描):
- op_0x00(`last_unimpl=0`,即正常 op_5A 返回 / pc 越界):566 格 — 多數事件格是
  純劇情 / 描述,跑完即返回,從不碰角色屬性。
- op_0x6B:27 格 — opendw 未反編的 opcode(decompilation 本身不完整)。
- op_0x6C / op_0x7F / op_0x8D:各數格 — 同為未反編 opcode。

注:`grep` 數 0x5D byte 會在某些關出現非 0,但那是**字串 / operand 內的資料 byte**,
不是 op_5D opcode 位置;唯一可信的判定是動態(VM 真正執行到),而動態結果為 0。

## 解讀:技能檢定觸發點不在可追蹤 bytecode

非戰鬥技能(Climb / Hide / Pickpocket / Tracking / Swim / Lore…)的檢定**不是**由
per-tile level event script 透過 op_5D 驅動。最可能位於原版 DRAGON.COM 的
**硬編碼 walking-engine**(移動 / 地形特徵 / 互動指令的 handler)——而 opendw 的反編譯
**並未涵蓋**這部分(`engine.c` 只反出 VM 直譯器與一批 opcode;op_6B/6C/7F/8D 等本身就
未反編,亦無任何 roll-vs-skill 的判定段被反出)。

依專案硬性規則「技能檢定的觸發點 / 門檻必須來自原版,逆不出就誠實標受阻,不臆造」,
故本任務對「Climb / Hide / Pickpocket / Tracking / Swim / 各 Lore / Bureaucracy /
Merchant」的觸發點一律 **標記受阻**,不接線、不臆造檢定點。

對照已完成項:#127 / #135 的 Lockpick / Bandage 檢定其**門檻**(d20 difficulty)在
`progression.hpp` 已明確標 `[remake]` 量化,亦非從原版 bytecode 逆出,只是那兩個技能
有明確的玩家動作入口(開鎖 / 包紮指令)可掛 remake 設計檢定。其餘技能連入口都無法從
bytecode 定位,故維持受阻。

## 受阻記錄(逆向到哪一步)

- 已建立 VM-tracing 掃描器(`detect_skill_checks`)+ op_5D 觀測 hook,並驗證:全 40 關
  599 事件格、含 op_58 跟進 + 互動鍵注入,op_5D **零觸發**。
- 阻點:技能檢定觸發點在原版未反編的硬編碼 engine 區,無 C 碼可逐指令對拍;繼續往下
  需要反組譯原始 DRAGON.COM 的 walking-engine(超出現有 opendw oracle 範圍)。

## 產物

- `src/vm/interpreter.{hpp,cpp}`:新增 `CharReadObserver` 診斷 hook(op_5D 觀測,
  純診斷,不改 VM 行為;ctest 30 項全綠不變)。
- `tools/verify/detect_skill_checks.cpp`:技能檢定掃描器(非 ctest gate,供逆向用)。
- 無功能接線(觸發點受阻 → 不臆造)。
