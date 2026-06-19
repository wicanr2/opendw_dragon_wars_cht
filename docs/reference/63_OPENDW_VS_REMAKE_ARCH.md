# 63 — opendw(C 反組譯)vs opendw_remake(C++20/SDL2)架構差異分析

本文比較同一款遊戲《火龍之戰》(Dragon Wars, Interplay 1989/90)的兩套程式碼:

- **opendw** — Devin Smith 的 C 反組譯,對應 Rebecca Heineman 1989 的 16-bit x86 real-mode 組語原版。位置 `../../opendw/`(本專案唯讀依賴)。
- **opendw_remake** — 本專案以 C++20 + SDL2 從頭乾淨重寫的執行環境,內建繁中/英/日三語、自包含資產 bundle。位置 `opendw_remake/`。

兩者不是分支關係,而是「**參考實作(oracle)↔ 重寫**」的關係。下面所有結論都附原始檔案/行號或 docs 引文。報告中的數字以原始碼實測為準,並在差異處標出 docs 的舊快照口徑。

---

## 1. 定位與角色:oracle ↔ 重寫

| | opendw | opendw_remake |
|---|---|---|
| 本質 | 把 16-bit x86 組語**逐位址翻成 C** 的 port | 理解後的現代乾淨重寫 |
| 自我定位 | README:「an attempt to **port** this game to modern environments using the C language」 | README:「不是把組語再翻一次,而是**理解後的現代重寫**」 |
| 證據鏈 | `ANALYSIS.md` 的 ASM↔C 逐位址對照表 + `targets[].src_offset` 保留原 ASM 位址 | 差異測試 / golden 對拍 / round-trip,把 opendw 當正確性基準 |
| 在本專案的角色 | **行為 oracle**(唯讀,不修改) | 交付物(可玩、可中文化、可維護) |

關鍵澄清:**「behavior oracle / diff_trace / golden」這套詞彙是 remake 端引入的方法論,不是 opendw 自身的用語。** 對 opendw 全樹 grep `oracle|golden|diff_trace` 零命中;opendw 把自己當「逐位址翻譯的 port」,用 unit test(`src/tests/`)驗證。是 remake 反過來把 opendw 當參考實作,才產生 oracle 關係。報告中不要把兩邊的方法論混為一談。

remake 的 oracle 流程有兩條主軸(`docs/engine/REWRITE_READINESS.md`):

- **diff_trace 逐指令對拍**:同一段 bytecode 丟進 opendw 與 remake VM,逐指令比對 `(pc, op, r2, r4, flags, mode)` 完全一致(`trace_remake` + `vm_selftest`)。
- **golden 畫面對拍**:viewport / sprite / minimap 像素與 opendw 輸出 byte-for-byte 比對。

策略總綱:「純資料 opcode 用 diff_trace;畫面用 golden;無 oracle 的用 ASM 實作 + 人工驗並標註。」

---

## 2. 架構對比

### 2.1 opendw:單體 C + 全域 state + indirect-call VM

- **巨型 engine.c**(6637 行):約 800+ 個依位址命名的全域變數、`struct virtual_cpu`、256-entry opcode 跳表、~278 個 opcode handler、`run_script` dispatch loop,全擠在一個檔。
- **全域 game state 未結構化**:核心是一塊 256-byte scratch,`struct game_state { unsigned char unknown[256]; }`(`state.h:37-57`),存取靠 `set_game_state(offset, value)` / `get_game_state(offset)`,欄位語意只用註解標 offset。另有約 812 處 `word_XXXX` / `data_XXXX` / `counter_XXXX` 散落 engine.c(如 `engine.c:45 counter_104D`、`engine.c:187 word_5038`)。
- **VM = 軟體模擬 16-bit x86,indirect call through function-pointer table**:`struct op_call_table { void (*func)(); const char *src_offset; }`(`engine.c:578-581`),表 256 槽、139 非 NULL。dispatch loop `run_script`(`engine.c:6413`):

  ```c
  op_code = *cpu.pc++;                       // es lodsb
  cpu.ax = op_code; cpu.bx = cpu.ax;        // xor ah,ah
  void (*callfunc)(void) = targets[op_code].func;
  if (callfunc != NULL) { callfunc(); if (op_code == 0x5A) done = 1; }
  else { printf("unhandled op code ... terminate"); exit(1); }   // engine.c:6451-6463
  ```

  x86 慣例直接搬進 C:`lodsb`、`op_5A`=ret、`bit_extract` 用 `rcl al,1` 模擬旋轉進位。
- **runtime 耦合**:opcode handler、渲染(ui.c)、資源(resource.c)、查表(tables.c 把原 dragon.com 固定資料整段搬成 6 張 256-entry C 陣列)彼此透過全域變數與直接呼叫纏在一起。未實作 opcode 直接 `exit(1)`。

### 2.2 opendw_remake:vertical-slice deep modules + 窄介面

按功能切,不按抽象層切(`ARCHITECTURE.md` §2,遵 `rules/70-deep-modules.md`)。實際 `src/` 行數實測:

| 模組 | 檔數 | 行數 | 職責(對外窄介面,內部隱藏複雜度) |
|---|---|---|---|
| `vm/` | 4 | 2603 | script 虛擬 CPU:`interpreter.cpp` 的 `kImpl[256]` 成員函數指標 dispatch、`vm_state`、`trace` |
| `render/` | 27 | 4177 | SDL2 渲染:320×200 indexed framebuffer、16 色盤、整數放大、8×8 ASCII、24×24 CJK、第一人稱 viewport 合成 |
| `game/` | 29 | 4539 | scene 狀態機:title / explore / combat / chargen / shop / recruit / 結局 |
| `resource/` | 12 | 822 | DATA1/DATA2 archive reader、解壓、5-bit text codec、自包含 bundle |
| `audio/` | 2 | 297 | 音訊邊界 |
| `i18n/` | 4 | 156 | 翻譯表、Read Paragraph DB、glyph cache |
| `main.cpp` | 1 | 4034 | scene 主迴圈/組裝 |

窄介面範例(`resource/archive.hpp`):「Deep module:對外只露『給我 resource N 的 bytes』,內部隱藏 768-byte header、section offset 累加、DATA1↔DATA2 fallback、壓縮判定。」對外只有 `Archive::open()` / `load(id)` / `load_raw(id)` 三個方法,把 opendw `resource.c` 散在全域的載入邏輯收進一個類。

### 2.3 具體模組/檔案對照

| opendw(C 反組譯) | opendw_remake(C++) | 對拍方式 |
|---|---|---|
| `engine.c` `run_script`(6413)+ `targets[256]` 跳表 | `vm/interpreter.cpp` `kImpl[256]` 成員函數指標表(`interpreter.cpp:1933`)+ `run()` | diff_trace 逐指令 |
| `engine.c` 各 `op_XX` handler | `interpreter.hpp` 逐 opcode `opNN_xxx()` 成員函數,**每個都標 opendw 行號/ASM 位址** | diff_trace |
| `ui.c` `draw_viewport`/`update_viewport`(4 象限解碼+鏡像) | `render/viewport.cpp`(「忠實 port 自 opendw `ui.c`,藍本 `golden_decode.c`,逐行對照」)+ `viewport_compose.cpp` | golden 像素(40 關 ×4 朝向 154 case) |
| `resource.c` `resource_load` + 768-byte header | `resource/archive.cpp` `Archive::load` | round-trip |
| `compress.c` `decompress_data1`(LZSS)/`extract_letter`(5-bit) | `resource/decompress.cpp` + `resource/text_codec.cpp` | round-trip |
| `player.c` `struct player_record`(512 B/角色,`data_C960`) | `game/party.cpp` + `game/equipment.cpp`(char_data 對映 `data_C960`) | byte-for-byte 欄位 |
| `tables.c` 6 張 256-entry 查表 + 字型 | `render/viewport_tables.hpp` / `render/font.cpp` | golden |
| `state.c` 256-byte `game_state` | `vm/vm_state.hpp`(具名存取 + 同 offset 語意) | diff_trace 比對 gs |

remake VM 刻意保留 opendw 的暫存器語意(`interpreter.cpp:2085`:「每次 dispatch 都 `cpu.ax = op_code; cpu.bx = cpu.ax`」對照 opendw `run_script`),確保逐指令對拍成立。差別在:opendw 用全域裸函數 + 位址命名,remake 用 class 成員函數 + 語意命名 + sink callback 解耦(`MessageSink` / `SoundSink` 把 VM 與渲染/音訊邊界分離,`interpreter.hpp:25-44`),VM 不直接觸碰 SDL。

---

## 3. 保真層級(四標籤模型)

remake 誠實標示每個機制的真值來源(`docs/assessment/47_REMAKE_ASSESSMENT.md` 定義四標籤):**bytecode 真值**(逐指令/byte 對拍 opendw)、**best-fit**(bytecode 有矛盾或無完整 oracle,取最合理值)、**remake 設計**(乾淨室,grounded 手冊)、**受限 demo / 受阻**(僅 headless 路徑可達或無 oracle)。

### Tier A — byte-for-byte / 逐指令對拍真值

- **VM opcode**:`(pc, op, r2, r4, flags, mode)` 逐指令對拍一致(`vm_selftest`、`trace_remake`)。
- **第一人稱 viewport**:`render_sweep` 全 40 關 ×4 朝向 154 case viewport_memory PASS(byte-for-byte)。
- **sprite / minimap**:`verify_encounter_golden_spider` / `_wolf` 對 oracle PPM byte-for-byte。
- **戰鬥單次攻擊公式**:to-hit(`roll=1d16+3`)、徒手傷害(`骰+floor(STR/5)`)、武器主傷害骰(op_68)= bytecode 真值(`docs/reverse-engineering/42 §11/§12`)。
- **存讀檔**:`verify_save` byte-for-byte round-trip。

### Tier B — remake 設計(無 oracle,刻意設計,已標明)

- **combat_loop vs res3**:完整戰鬥迴圈(4 人 vs 怪群、多回合、勝負、XP +80)是 remake 設計的確定性編排(`game/combat_loop.cpp`)。其 header 明文界定:單次攻擊走 `resolve_attack`(bytecode 真值,不重算公式);XP +80 有據(`docs/reverse-engineering/43` DOS 實機);行動順序/目標選擇/逃跑是 remake 設計(SDA 只定性「DEX 高先攻」,無 oracle 確切實作)。
- **結局序列**:`docs/gameplay/56` §2.2 —「原版『勝利後結局畫面』由戰鬥流程/DRAGON.COM 主控觸發,不在任何 level event script 中…逆不出獨立結局 script。本序列以已 bundle 的真實素材組合,『組合與串接』= remake 設計。」
- **世界地圖連通慣例**:`docs/assessment/49` §C — wrap 邊界改 modular 環繞,連通數字為 remake 慣例(opendw 原版在此 `exit(1)`)。

### Tier C — 受阻(無獨立 oracle)

- **怪物逐回合 HP**:`docs/assessment/49` §D —「全戰鬥 VM 閉環(怪物 HP 真扣 + oracle 對拍)🔒…opendw 無『獨立跑一場戰鬥 dump 逐回合 HP』路徑」;`verify_combat_script` 只驗指令軌跡確定性、不驗 HP。
- **門 K-on-wall / 陷阱**:`docs/assessment/49` §C — 機制完備但需逐格跑 bytecode 才能實戰觸發(受阻),`docs/gameplay/57` 為其機制文件。
- **Phoebus / area 6/33 隔離**:`docs/gameplay/55` §5.4 — 與主線正交的隔離分量。
- **X68000 / Amiga `.PKH`**:DOS 反組譯之外的平台無 oracle;多版本素材抽取屬 remake 加值(見 §4),非真值對拍。

---

## 4. remake 的加值(opendw 完全沒有)

| 加值 | 證據 |
|---|---|
| 繁中 + 英 + 日三語在地化 | 根 README;日文從 X68000 原版抽事件文字補入 `assets/i18n/ja/`(`docs/reverse-engineering/46`,events 13→212 / spells 57 / monster 23 byte-for-byte) |
| CJK 雙層渲染(ADR-0002) | 像素層維持 320×200 indexed framebuffer;**文字層改用 SDL2_ttf + host TTF(wqy-zenhei)在視窗原生高解析繪製,永不被縮放 → 恆銳利**(`docs/adr/0002`) |
| 多版本美術主題(F8 切換) | DOS / Amiga / X68000 從三版原磁碟抽素材(`docs/reference/61`) |
| VGA-256 主題 | 「原版沒有這個版本…用演算法(漸層 + 邊緣壓暗)把 flat 色塊擴成 256 色」(`docs/reference/62`)= 純 remake 加值 |
| 世界地圖重設計 / 半透明 UI | `docs/gameplay/59` 版面保真;訊息框 3/4 dither 半透明疊層 |
| 全域熱鍵 | `docs/engine/CONTROLS.md`:F4 語言循環、F8 theme 循環、F10 自動存檔離開、離開確認 |
| 音訊邊界 | `audio/` lib + `verify_audio` ctest(op_90 接 oracle 索引,Hz/ms = remake 設計) |
| 打包 / CI | 跨平台(Linux/macOS/Windows)、Docker 多階段、CPack `.tar.gz` 自包含 bundle、GitHub Actions CI(`docs/engine/50`)。**runtime 不依賴 `DRAGON.COM` / `DATA1` / `DATA2`** |
| 自動化測試 | ctest 34 項(實測)= `vm_selftest` + `render_sweep` + `smoke_app` + 31 個 `verify_*` |

對照 opendw:opendw 沒有在地化、沒有 CJK、單一 DOS 美術、無打包/CI 流程、驗證以少量 unit test 為主、runtime 直接讀 `dragon.com`。

---

## 5. opendw 有但 remake 尚未完整

- **res3 全戰鬥閉環**:`docs/assessment/49` §D — bytecode 路徑跑到 actor 迴圈前卡在 `res18↔res4` 逐角色動作指派狀態機(非 opcode 缺失,`last_unimpl=0`);怪物 HP 真扣 + oracle 對拍仍受阻(remake 用 `combat_loop` 確定性編排替代,但那不是原 bytecode 閉環)。
- **部分 NULL / 未逆 opcode**:opendw `targets[]` 有約 21 個從未逆向的 NULL opcode;remake 已逆出並實作 op_68,其餘約 20 個未補。另在 0x00–0x9F 範圍內,opendw 非 NULL 但 remake 未實作者約 23 個(多為 RPG 系統「動詞」:特殊攻擊、部分技能效果)。
- **原 runtime 完整覆蓋**:未實作集中在「RPG 系統的動詞」;受阻集中在跨區連通(wrap 樞紐 edge→area 對映,opendw 自身亦 `exit(1)`)與戰鬥真值閉環。

注:`docs/assessment/49` 為快照,shop / recruit / progression / ending 等項自快照後已補上對應 ctest(見 §6 測試清單),此處列「快照時點」的缺口。

---

## 6. 程式碼度量

| 指標 | opendw | opendw_remake |
|---|---|---|
| 語言 | C(反組譯) | C++20 + SDL2 |
| `src/lib` 行數 | ~11,098(含 6637 行 engine.c) | — |
| `src/` 行數 | — | 16,628(實測)+ main.cpp 4034 |
| 最大單檔 | engine.c 6637 行 | interpreter.cpp 2101 / main.cpp 4034 |
| 模組劃分 | 單體 + 若干輔助 .c | 6 個 vertical-slice deep modules |
| VM opcode 槽 | 256 槽,139 非 NULL | `kImpl[256]`,**126 實作**(實測) |
| 全域裸狀態 | 256-byte `game_state` + ~812 處 `word_/data_` 全域 | `vm_state` 封裝 + sink callback 解耦 |
| 自動化測試 | unit test(`src/tests/` 數支) | **ctest 34**(diff_trace + golden + round-trip + verify_*) |

opcode 數字 drift(報告口徑):remake `kImpl` 實測 **126** 實作;`docs/assessment/47` / `docs/assessment/49` 寫 117(舊快照);`docs/reverse-engineering/25` 的 139 是描述 **opendw `targets[]`** 的非 NULL 數(不同口徑)。原 256 槽中 0xA0–0xFF 約 96 個是反組譯時資料段誤植的 NULL artifact,`docs/reverse-engineering/OPCODE_REFERENCE.md` 明言「不建議嘗試實作這些 opcode」。

ctest 數字 drift:原始碼 `grep -c add_test` = **34**;README/docs 內 19/27/32/33 為各時點舊快照,以 34 為現值。

測試/golden 機制:
- `vm_selftest` — 同段 bytecode 餵 opendw 與 remake VM,逐指令比對。
- `trace_remake` + `diff_trace.sh` — 純資料 opcode 逐指令對拍。
- `render_sweep` — 40 關 ×4 朝向 154 case viewport byte-for-byte。
- `verify_encounter_golden_*` — sprite 對 oracle byte-for-byte。
- `verify_save` — 存檔 round-trip。
- 其餘 `verify_*`(chargen/shop/recruit/progression/ending/spells/equipment/terrain/theme/vga256/wrap/i18n…)涵蓋各遊戲系統。

---

## 7. 方法論評述

**「反編當 oracle + 乾淨重寫」vs「直接改反編碼」的取捨。** opendw 是逐位址翻譯的 port:能跑、保真高,但全域裸狀態 + 位址命名 + 巨型 engine.c + `exit(1)` 行為,使「中文化/換美術/加音訊/跨平台打包」幾乎不可下手——任何改動都要在纏繞的 runtime 裡找 magic offset。若直接在 opendw 上改,等於背著反組譯的全部包袱。

remake 選擇把 opendw 當**唯讀的正確性基準**,在乾淨 C++ 重寫,並用差異測試把「重寫」從「賭一把」變成「可驗證的工程」(`REWRITE_READINESS.md`):每加一個 opcode、每寫一個模組,都能自動證明行為等同原版。代價是要先逆出 bytecode 語意、要維護對拍 harness;換來的是可維護、可在地化、可打包、可長期演進。

**deep modules 實踐落地。** `resource/archive.hpp`、`combat_loop.hpp` 等 header 都遵守「對外窄介面、內部隱藏複雜度」:archive 把 768-byte header / DATA1↔DATA2 fallback / 壓縮判定藏在 `load(id)` 後;combat_loop 把行動順序/目標選擇/回合結算藏在「建戰鬥 / 推進回合 / 查勝負」三個動作後。VM 用 sink callback 把渲染/音訊推到邊界,符合「adapters at the edges only」。

**誠實標示文化是這套方法論的關鍵。** 四標籤(bytecode 真值 / best-fit / remake 設計 / 受阻)+ 三級對拍(diff_trace 逐指令 / golden 畫面 / 無 oracle 則 ASM 實作 + 人工標註),讓「哪些是原版真值、哪些是我們的設計決策、哪些根本還拿不到 oracle」一目了然。`combat_loop.hpp` 開頭那段「真值來源界定(務必誠實)」就是範例:XP +80 標「有據」,行動順序標「remake 設計」,怪物 HP 閉環標「受阻」。這讓後續工程師不會把 remake 的設計決策誤當原版行為,也不會在受阻處浪費力氣硬逆。

---

## 附:三個最關鍵的架構差異(摘要)

1. **VM dispatch**:opendw `targets[256]` 全域裸函數指標 + `src_offset` 字串(`engine.c:578`)→ remake `kImpl[256]` class 成員函數指標 + 每 opcode 標 oracle 行號(`interpreter.hpp`),保留 x86 暫存器語意以維持逐指令對拍。
2. **狀態管理**:opendw 256-byte `game_state` + ~812 處位址命名全域,渲染/資源/VM 透過全域纏繞 → remake `vm_state` 封裝 + `MessageSink`/`SoundSink` 把 VM 與 SDL/audio 解耦,vertical-slice 模組各管各的。
3. **正確性策略**:opendw 逐位址翻譯 + 少量 unit test,未實作即 `exit(1)` → remake 把 opendw 當 oracle,34 項 ctest(diff_trace 逐指令 + golden 像素 + round-trip),並用四標籤誠實區分真值/設計/受阻。
