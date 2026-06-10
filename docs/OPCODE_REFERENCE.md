# Dragon Wars Virtual CPU — Opcode Reference
# Dragon Wars 虛擬 CPU — Opcode 參考手冊

> **Version / 版本**: 1.0.0 (2026-06-10)
> **Source / 來源**: Derived from `docs/25_OPCODE_INTERPRETATION.md` (community reverse-engineering analysis)
> **Status / 狀態**: Community research document — not an official Interplay publication

---

## License & Attribution / 授權與出處

This document is a community reverse-engineering reference based on:

本文件為社群逆向工程整理，基於下列來源：

- **opendw** — open-source reimplementation of Dragon Wars by Devin Smith, licensed under the **BSD License**.
  Repository: <https://github.com/demondevin/opendw>
- **Dragon Wars (1989)** — original game by **Rebecca Heineman / Burger Becky** at Interplay Productions.
  All trademarks and copyrights belong to their respective owners.
- **Analysis / 分析**: Community disassembly of the DOS COM binary (`dragon.asm`, 3721 lines) and
  opendw source (`src/lib/engine.c`), cross-referenced with `opendw/doc/script.md`.

**Purpose / 用途**: Historical preservation, CRPG research, and fan localisation.
This document does **not** reproduce game assets. Use for research and non-commercial preservation only.

本文件僅供研究與保存目的，不重製任何遊戲資產。

---

## Introduction / 簡介

**Dragon Wars** (1989, Interplay Productions) is a classic CRPG originally developed for the Apple IIgs
and ported to DOS, Amiga, and C64. Most of its game logic — dialogues, encounters, map triggers, UI —
is implemented in a **custom bytecode scripting language** executed by a virtual CPU inside the game engine.

**Dragon Wars**（1989，Interplay Productions）是一款經典 CRPG，最初為 Apple IIgs 開發，後移植至
DOS、Amiga 與 C64。遊戲大部分邏輯（對話、遭遇、地圖觸發、UI）由一套**自訂 bytecode 腳本語言**驅動，
在遊戲引擎內的虛擬 CPU 上執行。

The **opendw** project (Devin Smith) reimplements this engine in C, making the opcode dispatch table
(`targets[]` in `engine.c`, line 583) directly inspectable. This reference documents all 256 opcodes
based on that table and community disassembly of the original DOS COM binary.

**opendw** 專案（Devin Smith）以 C 語言重新實作此引擎，使 opcode 分派表（`engine.c` 第 583 行的
`targets[]`）可直接檢視。本參考手冊基於該表與原始 DOS COM 二進位反組譯結果，記錄全部 256 個 opcode。

### Who Should Use This / 適用對象

- CRPG enthusiasts and historians wanting to understand Dragon Wars internals
- Developers contributing to or forking the opendw reimplementation
- Fan localisation teams (especially CJK / Chinese localisation)

---

## Statistics Summary / 統計摘要

| Item / 項目 | Count / 數值 |
|---|---|
| Total opcodes / 總 opcode 數 | 256 (0x00–0xFF) |
| Implemented in opendw / 已實作 | 139 |
| Unimplemented (NULL) / 未實作（NULL） | 117 |
| Valid NULL (0x00–0x9F range, reasonable ASM addr) / 有效 NULL（位址合理）| ~22 |
| Likely data artifacts (0xA0–0xFF) / 疑似原始碼殘留（0xA0–0xFF）| ~95 |
| Category C (text/UI) unimplemented — localisation priority / 分類 C 未實作（中文化優先）| 4 (0x79, 0x7E, 0x7F, 0x8F) |

> **Key Finding / 重要發現**: The 96 NULL entries at 0xA0–0xFF have ASM addresses with patterns
> characteristic of x86 machine-code byte pairs (e.g. `0x8A06`, `0xE80E`, `0xA23A`) — completely
> unlike the valid engine code segment addresses (0x3B00–0x4AF6). These are likely artifacts from
> the original DOS COM file's data segment mis-interpreted as opcode addresses. **Do not attempt to
> implement these.**
>
> 0xA0–0xFF 的 96 個 NULL 項 ASM 位址呈現 x86 機器碼位元組對特徵，與低位索引的有效程式碼段位址
> 完全不同，疑似原始 COM 檔案反組譯過程中的資料段誤植。**不建議嘗試實作這些 opcode。**

---

## Virtual CPU Architecture / 虛擬 CPU 架構

The virtual CPU is not based on any standard architecture, though it shares concepts with the 65C816
(the game was originally authored on the Apple IIgs). Key characteristics:

虛擬 CPU 並非基於任何標準架構，但與 65C816 有概念上的相似（遊戲最初在 Apple IIgs 上開發）。
主要特性：

- **Byte/Word mode switch / 位元組/字組模式切換**: `byte_3AE1 = 0` → 8-bit mode; `= 0xFF` → 16-bit mode.
  Many opcodes behave differently depending on this flag.
  許多 opcode 的行為依此旗標而不同。
- **Variable-length instructions / 可變長度指令**: Arguments are 0, 1, or 2 bytes depending on the
  current byte/word mode and the opcode. Writing a disassembler is non-trivial.
  引數長度為 0、1 或 2 個位元組，依模式與 opcode 而定，反組譯器撰寫困難。
- **Script resources / 腳本資源**: Scripts are stored as resource sections in DATA1. The engine
  loads and executes them; `op_57`/`op_58` switch between scripts; `op_5A` terminates.
  腳本以資源節區形式儲存於 DATA1，`op_57`/`op_58` 切換腳本，`op_5A` 結束。

### Registers / 暫存器

| Variable / 變數 | Role / 用途 |
|---|---|
| `byte_3AE1` | Mode flag: 0 = 8-bit mode, 0xFF = 16-bit mode / 模式旗標 |
| `word_3AE2` | Primary accumulator (analogous to AX) / 主運算暫存器（類 AX） |
| `word_3AE4` | Secondary register — loop counter / offset (analogous to CX) / 次要暫存器（類 CX，迴圈計數器/偏移） |
| `word_3AE6` | Flags register: CF bit0, ZF bit6, SF bit7 / 旗標暫存器 |
| `word_3AE8` | Current resource index / 當前資源索引 |
| `word_3AEA` | Resource index copy / 資源索引副本 |
| `word_3ADB` | Scratch address for script jumps / 腳本跳轉暫存位址 |
| `running_script` | Pointer to current resource / 當前資源指標 |
| `word_3ADF` | Data resource pointer / 資料資源指標 |
| `cpu.pc` | Program counter / 程式計數器 |
| `cpu.base_pc` | Script base address / 腳本基底位址 |
| `cpu.sp` | Stack pointer / 堆疊指標 |
| `cpu.stack[]` | 256-byte stack / 256 位元組堆疊 |
| `game_state.unknown[]` | Game state array (~256 bytes) / 遊戲狀態陣列（約 256 位元組） |

---

## Category Legend / 分類圖例

| Category / 分類 | Description / 說明 |
|---|---|
| **A** | Graphics / rendering (viewport, sprites, minimap) / 圖形/繪圖（視口、精靈、小地圖）|
| **B** | Audio / sound effects / 音效 |
| **C** | Text / UI display — **localisation priority** / 文字/UI 顯示（**中文化高優先**） |
| **D** | Game logic / control flow (data manipulation, jumps, character attributes) / 遊戲邏輯/流程（資料操作、跳轉、角色屬性） |
| **E** | Undecipherable — invalid/out-of-range ASM address / 無法判讀（ASM 位址無效或超出範圍） |

---

## Complete 256-Opcode Table / 完整 256-Opcode 對照表

Column key / 欄位說明:
- **Index** / **索引**: Opcode value (0x00–0xFF)
- **ASM Addr** / **ASM 位址**: Corresponding address in original DOS COM disassembly (`dragon.asm`)
- **Status** / **狀態**: ✅ implemented in opendw / ❌ not implemented (NULL)
- **Function / Semantic** / **函式名/語意**: opendw C function name or inferred semantics (inferred = not confirmed)
- **English Description** / **英文說明**: Operational meaning
- **中文說明**: 操作語意
- **Cat** / **分類**: Category A–E

> Rows marked ❌ are `NULL` in `engine.c targets[]`. Semantics for unimplemented entries are
> inferred from position between neighbouring implemented opcodes and disassembly context.
> 標記 ❌ 的項目在 `targets[]` 中為 NULL；未實作 opcode 的語意為推測，依鄰近已實作 opcode 與反組譯上下文判斷。

### 0x00–0x0F: Mode & Register Load / 模式切換與暫存器載入

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x00 | 0x3B18 | ✅ | `set_word_mode` | Switch to 16-bit mode (byte_3AE1 = 0xFF) | 切換 16-bit 模式（byte_3AE1=0xFF） | D |
| 0x01 | 0x3B0E | ✅ | `set_byte_mode` | Switch to 8-bit mode (byte_3AE1 = 0) | 切換 8-bit 模式（byte_3AE1=0） | D |
| 0x02 | 0x3B1F | ❌ | *(inferred)* mode-switch variant | (inferred) Mode-switch variant or clear high byte of word_3AE2 | 推測：模式切換變體，或清除 word_3AE2 高位元組 | D |
| 0x03 | 0x3B2F | ✅ | `op_03` | Pop 1 byte from stack → AX; reset resource pointer (word_3AEA) | 彈出堆疊 1 byte→AX，重設資源指標（word_3AEA） | D |
| 0x04 | 0x3B2A | ✅ | `op_04` | Push low byte of word_3AE8 onto stack | 推入 word_3AE8 低位元組到堆疊 | D |
| 0x05 | 0x3B3D | ✅ | `op_05` | game_state[arg] → word_3AE4 | game_state[arg]→word_3AE4 | D |
| 0x06 | 0x3B4A | ✅ | `op_06` | 1-byte immediate → word_3AE4 (loop counter) | 1-byte 立即數→word_3AE4（迴圈計數器） | D |
| 0x07 | 0x3B52 | ✅ | `op_07` | High byte of AX → word_3AE4 | AX 高位元組→word_3AE4 | D |
| 0x08 | 0x3B59 | ✅ | `op_08` | word_3AE4 → game_state[arg] | game_state[arg]←word_3AE4 | D |
| 0x09 | 0x3B67 | ✅ | `set_word3AE2_arg` | Load byte/word immediate from instruction stream → word_3AE2 | 從指令流載入 byte/word 立即數→word_3AE2 | D |
| 0x0A | 0x3B7A | ✅ | `load_word3AE2_gamestate` | word_3AE2 ← game_state[arg] | word_3AE2←game_state[arg] | D |
| 0x0B | 0x3B8C | ✅ | `op_0B` | word_3AE2 ← game_state[arg + word_3AE4] | word_3AE2←game_state[arg+word_3AE4] | D |
| 0x0C | 0x3BA2 | ✅ | `op_0C` | word_3AE2 ← word_3ADF->bytes[arg16] | word_3AE2←word_3ADF->bytes[arg16] | D |
| 0x0D | 0x3BB7 | ✅ | `op_0D` | word_3AE2 ← word_3ADF->bytes[arg16 + word_3AE4] | word_3AE2←word_3ADF->bytes[arg16+word_3AE4] | D |
| 0x0E | 0x3BD0 | ✅ | `op_0E` | word_3AE2 ← word_3ADF->bytes[game_state[arg] + word_3AE4] (indirect) | word_3AE2←word_3ADF->bytes[game_state[arg]+word_3AE4]（間接） | D |
| 0x0F | 0x3BED | ✅ | `op_extract_resource_data` | Decompress data from resource → word_3AE2 | 從資源解壓縮資料→word_3AE2 | D |

### 0x10–0x1F: Data Store / 資料寫入

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x10 | 0x3C10 | ✅ | `op_10` | word_3AE2 ← word_3ADF[game_state[arg1] + arg2] | word_3AE2←word_3ADF[game_state[arg1]+arg2] | D |
| 0x11 | 0x3C2D | ✅ | `op_11` | game_state[arg] ← AH (high byte of AX) | game_state[arg]←AH | D |
| 0x12 | 0x3C59 | ✅ | `op_12` | game_state[arg] ← word_3AE2 | game_state[arg]←word_3AE2 | D |
| 0x13 | 0x3C72 | ✅ | `set_gamestate_offset` | game_state[arg + word_3AE4] ← word_3AE2 | game_state[arg+word_3AE4]←word_3AE2 | D |
| 0x14 | 0x3C8F | ✅ | `op_14` | word_3ADF->bytes[arg16] ← word_3AE2 | word_3ADF->bytes[arg16]←word_3AE2 | D |
| 0x15 | 0x3CAB | ✅ | `op_15` | word_3ADF->bytes[arg16 + word_3AE4] ← word_3AE2 | word_3ADF->bytes[arg16+word_3AE4]←word_3AE2 | D |
| 0x16 | 0x3CCB | ✅ | `op_16` | word_3ADF->bytes[game_state[arg] + word_3AE4] ← word_3AE2 | word_3ADF->bytes[game_state[arg]+word_3AE4]←word_3AE2 | D |
| 0x17 | 0x3CEF | ✅ | `store_data_into_resource` | Write word_3AE2 to resource byte | word_3AE2 寫入資源位元組 | D |
| 0x18 | 0x3D19 | ✅ | `op_18` | word_3ADF->bytes[game_state[arg1] + arg2] ← word_3AE2 | word_3ADF->bytes[game_state[arg1]+arg2]←word_3AE2 | D |
| 0x19 | 0x3D3D | ✅ | `op_19` | Copy game_state[src] → game_state[dst] | 複製 game_state[src]→game_state[dst] | D |
| 0x1A | 0x3D5A | ✅ | `op_1A` | game_state[arg] ← immediate (1 or 2 bytes) | game_state[arg]←立即數（1 或 2 byte） | D |
| 0x1B | 0x3D73 | ❌ | *(inferred)* immediate write variant | (inferred) Another game_state immediate-write addressing variant | 推測：另一種 game_state 立即數寫入變體 | D |
| 0x1C | 0x3D92 | ✅ | `op_1C` | word_3ADF->bytes[arg16] ← immediate byte/word | word_3ADF->bytes[arg16]←立即數 byte/word | D |
| 0x1D | 0x4ACC | ✅ | `op_1D` | memcpy 0x700 bytes to/from data_D760 | memcpy 0x700 bytes 到/從 data_D760 | D |
| 0x1E | 0x01B2 | ❌ | *(inferred)* jump/reset to init script | (inferred) ASM 0x01B2 = COM startup region; likely "restart script / jump to init code" | 推測：ASM 0x01B2=COM 啟動區，可能為「重啟腳本/跳轉到初始化程式碼」 | E |
| 0x1F | 0x4AF6 | ✅ | `op_1F` | Load DATA1 (stub, incomplete) | 載入 DATA1（stub，未完整） | D |

### 0x20–0x2F: Arithmetic — Increment / Decrement / Shift / 算術：遞增/遞減/移位

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x20 | 0x0000 | ❌ | *(invalid addr)* NOP / unused | Invalid address 0x0000; likely NOP or padding | 位址 0x0000 無效；可能是 NOP 或填充 | E |
| 0x21 | 0x3DAE | ✅ | `op_21` | word_3AE4 ← (word_3AE2 & 0xFF) | word_3AE4←(word_3AE2 & 0xFF) | D |
| 0x22 | 0x3DB7 | ✅ | `op_22` | word_3AE2 ← word_3AE4 | word_3AE2←word_3AE4 | D |
| 0x23 | 0x3DC0 | ✅ | `op_23` | Increment game_state[arg] (with carry) | 遞增 game_state[arg]（帶進位） | D |
| 0x24 | 0x3DD7 | ✅ | `op_24` | Increment word_3AE2 (with mask) | 遞增 word_3AE2（帶掩碼） | D |
| 0x25 | 0x3DE5 | ✅ | `inc_byte_word_3AE4` | Increment low byte of word_3AE4 | 遞增 word_3AE4 低位元組 | D |
| 0x26 | 0x3DEC | ✅ | `op_26` | Decrement game_state[arg] | 遞減 game_state[arg] | D |
| 0x27 | 0x3E06 | ✅ | `op_27` | Decrement word_3AE2 (with mask) | 遞減 word_3AE2（帶掩碼） | D |
| 0x28 | 0x3E14 | ✅ | `op_28` | Decrement low byte of word_3AE4 | 遞減 word_3AE4 低位元組 | D |
| 0x29 | 0x3E1B | ❌ | *(inferred)* DEC high byte or SHL variant | (inferred) Decrement high byte of word_3AE2 or word_3AE4 | 推測：遞減 word_3AE2 或 word_3AE4 高位元組 | D |
| 0x2A | 0x3E36 | ✅ | `op_2A` | SHL word_3AE2 by 1 | word_3AE2 左移1位（SHL） | D |
| 0x2B | 0x3E45 | ✅ | `op_2B` | SHL low byte of word_3AE4 by 1 | word_3AE4 低位元組左移1位 | D |
| 0x2C | 0x3E4C | ❌ | *(inferred)* SHL/SHR variant | (inferred) SHL high byte of word_3AE4, or SHR of word_3AE2/word_3AE4 | 推測：word_3AE4 高位元組左移，或 word_3AE2/word_3AE4 右移 | D |
| 0x2D | 0x3E67 | ✅ | `op_right_shift` | SHR word_3AE2 by 1 | word_3AE2 右移1位（SHR） | D |
| 0x2E | 0x3E6E | ✅ | `op_2E` | SHR low byte of word_3AE4 by 1 | word_3AE4 低位元組右移1位 | D |
| 0x2F | 0x3E75 | ✅ | `op_2F` | RCR + word_3AE2 += game_state[arg] (rotate carry right + add) | 進位旗標右移＋word_3AE2 加 game_state[arg]（RCR+加法） | D |

### 0x30–0x3F: Arithmetic — Add / Sub / Mul / Div / Bitwise / 算術：加減乘除/位元運算

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x30 | 0x3E9D | ✅ | `op_30` | RCR + word_3AE2 += immediate | 進位旗標右移＋word_3AE2 加立即數 | D |
| 0x31 | 0x3EC1 | ✅ | `op_31` | RCR + word_3AE2 -= game_state[arg] | 進位旗標右移＋word_3AE2 減 game_state[arg] | D |
| 0x32 | 0x3EEB | ✅ | `op_32` | RCR + word_3AE2 -= immediate | 進位旗標右移＋word_3AE2 減立即數 | D |
| 0x33 | 0x3F11 | ✅ | `op_33` | Multiply: word_3AE2 × game_state[arg]; result → game_state[55–58] | 乘法（word_3AE2 × game_state[arg]），結果存 game_state[55–58] | D |
| 0x34 | 0x3F4D | ✅ | `op_34` | Multiply: word_3AE2 × immediate | 乘法（word_3AE2 × 立即數） | D |
| 0x35 | 0x3F66 | ✅ | `op_35` | Divide: game_state[arg] ÷ word_3AE2 | 除法（game_state[arg] ÷ word_3AE2） | D |
| 0x36 | 0x3F8C | ✅ | `op_36` | Divide: word_3AE2 ÷ immediate | 除法（word_3AE2 ÷ 立即數） | D |
| 0x37 | 0x3FAD | ❌ | *(inferred)* mul/div variant or modulo | (inferred) Multiply/divide variant or modulo: word_3AE2 % game_state[arg] | 推測：乘除法變體或 modulo 運算（word_3AE2 % arg）| D |
| 0x38 | 0x3FBC | ✅ | `op_38` | word_3AE2 AND immediate/game_state[arg] | word_3AE2 AND 立即數/game_state[arg] | D |
| 0x39 | 0x3FD4 | ✅ | `op_39` | word_3AE2 OR game_state[arg] | word_3AE2 OR game_state[arg] | D |
| 0x3A | 0x3FEA | ✅ | `op_3A` | word_3AE2 OR immediate | word_3AE2 OR 立即數 | D |
| 0x3B | 0x4002 | ✅ | `op_3B` | word_3AE2 XOR game_state[arg] | word_3AE2 XOR game_state[arg] | D |
| 0x3C | 0x4018 | ✅ | `op_3C` | word_3AE2 XOR immediate | word_3AE2 XOR 立即數 | D |
| 0x3D | 0x4030 | ✅ | `op_3D` | Compare word_3AE2 vs game_state[arg]; set flags | 比較 word_3AE2 vs game_state[arg]，設旗標 | D |
| 0x3E | 0x4051 | ✅ | `op_3E` | Compare word_3AE2 vs immediate; set flags | 比較 word_3AE2 vs 立即數，設旗標 | D |
| 0x3F | 0x4067 | ✅ | `op_3F` | Compare word_3AE4 vs game_state[arg]; set flags | 比較 word_3AE4 vs game_state[arg]，設旗標 | D |

### 0x40–0x4F: Conditionals / Jumps / Flow Control / 條件/跳轉/流程控制

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x40 | 0x4074 | ✅ | `op_40` | Compare word_3AE4 vs immediate; set flags | 比較 word_3AE4 vs 立即數，設旗標 | D |
| 0x41 | 0x407C | ✅ | `op_41` | JC: jump to immediate address if CF=0 | JC：若 CF=0 則跳轉到立即數位址 | D |
| 0x42 | 0x4085 | ✅ | `op_42` | JNC: jump if CF≠0 | JNC：若 CF≠0 則跳轉 | D |
| 0x43 | 0x408E | ✅ | `op_check_flags_jmp` | Jump if (flags & 0x41) == 1 | 若 (flags & 0x41)==1 則跳轉 | D |
| 0x44 | 0x4099 | ✅ | `op_jz` | JZ: jump if ZF≠0 | JZ：若 ZF≠0 則跳轉 | D |
| 0x45 | 0x40A3 | ✅ | `op_jnz` | JNZ: jump if ZF=0 | JNZ：若 ZF=0 則跳轉 | D |
| 0x46 | 0x40AF | ✅ | `op_js` | JS: jump if SF≠0 | JS：若 SF≠0 則跳轉 | D |
| 0x47 | 0x40B8 | ✅ | `op_47` | JNS: jump if SF=0 | JNS：若 SF=0 則跳轉 | D |
| 0x48 | 0x40ED | ✅ | `op_48` | If game_state[arg] < 0x80, set 0x80 bit and sign flag | 若 game_state[arg] < 0x80，設 0x80 bit 並設 sign flag | D |
| 0x49 | 0x4106 | ✅ | `loop` | LOOP: word_3AE4--; jump if result ≠ 0xFF | LOOP：word_3AE4-- 若非 0xFF 則跳轉 | D |
| 0x4A | 0x4113 | ✅ | `op_4A` | word_3AE4++; jump if ≠ arg | word_3AE4++ 若 != arg 則跳轉 | D |
| 0x4B | 0x4122 | ✅ | `op_stc` | STC: set carry flag | STC：設定 carry flag | D |
| 0x4C | 0x412A | ✅ | `op_clc` | CLC: clear carry flag | CLC：清除 carry flag | D |
| 0x4D | 0x4132 | ✅ | `op_prng` | Pseudo-random number generator → word_3AE2 | 偽隨機數產生器→word_3AE2 | D |
| 0x4E | 0x414B | ✅ | `op_4E` | Set bit in game_state (OR bitmask) | 設定 game_state 某 bit（OR bitmask） | D |
| 0x4F | 0x4155 | ✅ | `op_4F` | Clear bit in game_state (AND ~bitmask) | 清除 game_state 某 bit（AND ~bitmask） | D |

### 0x50–0x5F: Bit Test / Call / Return / Resource / 位元測試/呼叫/返回/資源

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x50 | 0x4161 | ✅ | `op_50` | TEST bit in game_state; set flags | TEST game_state 某 bit，設旗標 | D |
| 0x51 | 0x418B | ✅ | `op_51` | Find maximum value in word_3ADF[arg] → word_3AE2 | 在 word_3ADF[arg] 中找最大值→word_3AE2 | D |
| 0x52 | 0x41B9 | ✅ | `op_52` | JMP: unconditional jump (no return address saved) | JMP（無條件跳轉，不存返回位址） | D |
| 0x53 | 0x41C0 | ✅ | `op_53` | CALL: jump and push return address | CALL（跳轉並推入返回位址） | D |
| 0x54 | 0x41E1 | ✅ | `op_54` | RET: pop return address and jump | RET（彈出返回位址並跳轉） | D |
| 0x55 | 0x41E5 | ✅ | `op_peek_and_pop` | Peek word, pop byte → word_3AE2 | peek word, pop byte→word_3AE2 | D |
| 0x56 | 0x41FD | ✅ | `op_56` | Push word_3AE2 (byte or word mode) | 推入 word_3AE2（byte 或 word 模式） | D |
| 0x57 | 0x4215 | ✅ | `op_57` | Load new resource and switch script | 載入新資源並切換腳本 | D |
| 0x58 | 0x4239 | ✅ | `op_58` | Load tagged resource, switch script execution | 載入 tag 資源，切換腳本執行 | D |
| 0x59 | 0x41C8 | ✅ | `op_59` | Restore resource index from stack, pop return address | 從堆疊恢復資源索引並彈出返回位址 | D |
| 0x5A | 0x3AEE | ✅ | `op_5A` | End script (restore stack/resource state, engine exits) | 結束腳本（恢復堆疊/資源狀態，engine 退出） | D |
| 0x5B | 0x427A | ✅ | `op_5B_unused` | Get map tile data from game_state[0,1] coordinates | 取 game_state[0,1] 座標，get_map_tile_data | D |
| 0x5C | 0x4295 | ✅ | `op_5C` (op_for_call) | Execute script for each party member | 對隊伍每個角色執行腳本 | D |
| 0x5D | 0x42D8 | ✅ | `get_character_data` | Read character attribute → word_3AE2 | 從角色資料讀取屬性→word_3AE2 | D |
| 0x5E | 0x4322 | ✅ | `set_character_data` | Write word_3AE2 to character attribute | 將 word_3AE2 寫入角色屬性 | D |
| 0x5F | 0x4372 | ✅ | `op_or_char_data` (op_or_with_char_data) | OR bitmask into character attribute | 角色屬性 OR bitmask | D |

### 0x60–0x6F: Character Data / Map / Movement / 角色資料/地圖/移動

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x60 | 0x438B | ✅ | `op_and_char_data` | Character attribute AND ~bitmask | 角色屬性 AND ~bitmask | D |
| 0x61 | 0x43A6 | ✅ | `test_player_property` | TEST character attribute bit; set flags | TEST 角色屬性某 bit，設旗標 | D |
| 0x62 | 0x43BF | ✅ | `op_scan_for_char` | Scan characters for attribute value >= arg; set flags | 掃描角色找屬性值 >= arg，設旗標 | D |
| 0x63 | 0x43F7 | ✅ | `op_set_char_data_word` | Set character word attribute (data_CA4C) | 設定角色 word 屬性（data_CA4C） | D |
| 0x64 | 0x446E | ❌ | *(inferred)* get_char_data_word | (inferred) Read character data_CA4C word — complement of op_63 | 推測：讀取角色 data_CA4C（get_char_data_word），與 op_63 互補 | D |
| 0x65 | 0x44B8 | ❌ | *(inferred)* clear/test char data | (inferred) Clear or test character data_CA4C | 推測：清除或測試角色 data_CA4C | D |
| 0x66 | 0x40C1 | ✅ | `op_66` | Load game_state[arg]; set ZF/SF flags (like TEST) | game_state[arg] 載入，設 ZF/SF 旗標（類似 TEST） | D |
| 0x67 | 0x44CB | ❌ | *(inferred)* char attr / bit clear | (inferred) Character attribute / data_CA4C bit-clear operation | 推測：角色屬性/data_CA4C 相關，可能是清除某 bit | D |
| 0x68 | 0x450A | ❌ | *(inferred)* char data / map coord | (inferred) Character data operation or map coordinate pre-op | 推測：角色資料操作或地圖座標相關前置操作 | D |
| 0x69 | 0x453F | ✅ | `op_69` | Write character data_CA4C | 寫入角色 data_CA4C 資料 | D |
| 0x6A | 0x4573 | ✅ | `op_6A` | Check if game_state[0,1] is within 4-byte rectangle; set sign flag | 檢查 game_state[0,1] 是否在 4-byte 矩形範圍內，設 sign flag | D |
| 0x6B | 0x45A1 | ❌ | *(inferred)* coord range check or move | (inferred) Another coordinate range check, or movement pre-step | 推測：另一種座標範圍檢查，或移動玩家的前置動作 | D |
| 0x6C | 0x45A8 | ✅ | `op_6C` | Move player according to game_state[3] facing direction | 依 game_state[3] 方向移動玩家 | D |
| 0x6D | 0x45F0 | ✅ | `op_6D` | Process minimap commands | 處理小地圖命令（process_minimap_commands） | A |
| 0x6E | 0x45FA | ❌ | *(inferred)* minimap aux command | (inferred) Minimap-related auxiliary instruction (input or draw) | 推測：小地圖繪製或輸入相關輔助指令 | A |
| 0x6F | 0x4607 | ✅ | `op_6F` | Move player and store map tile data → game_state | 移動玩家並儲存地圖 tile 資料到 game_state | D |

### 0x70–0x7F: Map Script / Text Display / 地圖腳本/文字顯示

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x70 | 0x4632 | ❌ | *(inferred)* level script variant | (inferred) Level script execution variant, possibly run_level_script init | 推測：關卡腳本執行相關指令（run_level_script 變體） | D |
| 0x71 | 0x465B | ✅ | `op_71` | Execute level script (map tile trigger) | 執行關卡腳本（地圖 tile 觸發） | D |
| 0x72 | 0x46B6 | ✅ | `op_72_unused` | Map tile script execution (partially incomplete) | 地圖 tile 腳本執行（多處未完整） | D |
| 0x73 | 0x47B7 | ✅ | `op_73` | game_state[0x3E] ← game_state[0x3F] | game_state[0x3E]←game_state[0x3F] | D |
| 0x74 | 0x47C0 | ✅ | `op_74` | Draw rectangle (draw_rectangle, 4-byte args x,y,w,h) | 繪製矩形框（draw_rectangle，4 byte 引數 x,y,w,h） | A |
| 0x75 | 0x47D1 | ✅ | `op_75` | Full-screen redraw (ui_draw_full) | 全螢幕重繪（ui_draw_full） | A |
| 0x76 | 0x47D9 | ✅ | `op_76` | Draw pattern (draw_pattern & draw_rect) | 繪製圖案（draw_pattern & draw_rect） | A |
| 0x77 | 0x47E3 | ✅ | `op_77` | Draw pattern then set_msg (draw + text output) | draw_pattern 後 set_msg（繪製圖案並輸出訊息） | C |
| 0x78 | 0x47EC | ✅ | `set_msg` | Extract compressed string from script stream; call handle_byte_callback | 從腳本流提取壓縮字串並呼叫 handle_byte_callback | C |
| 0x79 | 0x47FA | ❌ | *(inferred)* set_msg with params ⭐ L10N KEY | (inferred) Parameterised set_msg variant — colour/position/flag string output; possibly `draw_string_at_position` or `set_msg_with_flags` | 推測：set_msg 帶參數變體，可能帶顏色/位置/旗標的字串輸出 | C |
| 0x7A | 0x4801 | ✅ | `op_7A` | Extract compressed string from word_3ADF resource stream | 從 word_3ADF 資源流提取壓縮字串 | C |
| 0x7B | 0x482D | ✅ | `read_header_bytes` | Set UI header (set_ui_header) | 設定 UI header（set_ui_header） | C |
| 0x7C | 0x4817 | ✅ | `op_7C` | Random encounter, set UI header | 隨機遭遇，設定 UI header | C |
| 0x7D | 0x483B | ✅ | `op_7D` | Output character name (write_character_name) | 輸出角色名稱（write_character_name） | C |
| 0x7E | 0x4845 | ❌ | *(inferred)* formatted string / char name variant ⭐ L10N KEY | (inferred) Formatted string output or character name variant with padding/alignment; possibly `append_spaces` prefix | 推測：帶格式字串輸出，或輸出角色名稱的另一種格式（補空格對齊） | C |
| 0x7F | 0x486D | ❌ | *(inferred)* cursor set / string end ⭐ L10N KEY | (inferred) Set cursor position or string-end control variant (like advance_cursor without newline) | 推測：設定游標位置或字串結束控制（類 advance_cursor 但不換行） | C |

### 0x80–0x8F: Text / Input / Audio / 文字/輸入/音效

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x80 | 0x487F | ✅ | `advance_cursor` | Advance cursor and pad with spaces (1-byte arg) | 推進游標並填補空格（1 byte 引數） | C |
| 0x81 | 0x48C5 | ✅ | `op_81` | Convert word_3AE2 to decimal string and output | word_3AE2 轉數字字串並輸出 | C |
| 0x82 | 0x48D2 | ✅ | `op_82` | Read 4-byte big number from game_state and output | 從 game_state 4 bytes 讀取大數並輸出 | C |
| 0x83 | 0x48EE | ✅ | `op_83` | Output raw byte value of word_3AE2 directly | 直接輸出 word_3AE2 的 byte 值 | C |
| 0x84 | 0x4907 | ✅ | `op_84` | Allocate game_memory | 分配 game_memory | D |
| 0x85 | 0x4920 | ✅ | `op_85` | Release resource (resource_index_release) | 釋放資源（resource_index_release） | D |
| 0x86 | 0x493E | ✅ | `load_word3AE2_resource` | Load resource → word_3AE2 index | 載入資源→word_3AE2 索引 | D |
| 0x87 | 0x4955 | ✅ | `op_87` | Write resource to disk | 將資源寫入磁碟 | D |
| 0x88 | 0x496D | ✅ | `op_wait_escape` | Wait for Escape key, then continue | 等待 Escape 鍵後繼續 | C |
| 0x89 | 0x4977 | ✅ | `wait_event` | Wait for event (2-byte flag + key mapping table until 0xFF) | 等待事件（2 byte flag + 按鍵對照表，直到 0xFF） | C |
| 0x8A | 0x498E | ✅ | `op_8A` | Trigger random encounter | 觸發隨機遭遇（trigger_random_encounter） | D |
| 0x8B | 0x499B | ✅ | `op_8B` | Refresh viewport | 刷新視口（refresh_viewport） | A |
| 0x8C | 0x49A5 | ✅ | `prompt_no_yes` | Display (N)o/(Y)es prompt; wait for keypress; set flags | 顯示 (N)o/(Y)es 提示，等待按鍵，設旗標 | C |
| 0x8D | 0x49D3 | ✅ | `op_read_string` | Read keyboard string input | 讀取鍵盤字串輸入（read_string_input） | C |
| 0x8E | 0x0000 | ❌ | *(invalid addr)* NOP / unused | Invalid address 0x0000; likely NOP or unused | 位址 0x0000，無效；可能是 NOP 或未使用 | E |
| 0x8F | 0x49DD | ❌ | *(inferred)* read_string variant ⭐ L10N KEY | (inferred) read_string variant with flag, or combined read+display operation | 推測：字串讀取變體（帶旗標的 read_string，或讀取後立即顯示） | C |

### 0x90–0x9F: UI / Misc Logic / UI/雜項邏輯

| Index | ASM Addr | Status | Function/Semantic | English Description | 中文說明 | Cat |
|---|---|---|---|---|---|---|
| 0x90 | 0x49E7 | ✅ | `op_sound_effect` | Play sound effect (1-byte arg) | 播放音效（1 byte 引數） | B |
| 0x91 | 0x49F3 | ✅ | `op_91` | Draw player status panel | 繪製玩家狀態面板（draw_player_status_panel） | A |
| 0x92 | 0x49FD | ✅ | `op_92` | Draw status panel and wait for UI interaction (with timer polling) | 繪製狀態面板並等待 UI 互動（含計時器輪詢） | A |
| 0x93 | 0x4A67 | ✅ | `op_93` | Push low byte of word_3AE4 onto stack | 推入 word_3AE4 低位元組到堆疊 | D |
| 0x94 | 0x4A6D | ✅ | `op_94` | Pop from stack → low byte of word_3AE4 | 從堆疊彈出→word_3AE4 低位元組 | D |
| 0x95 | 0x4894 | ✅ | `op_95` | Set cursor x,y position (relative to draw_rect offset) | 設定游標 x,y 位置（相對 draw_rect 偏移） | C |
| 0x96 | 0x48B5 | ✅ | `op_96` | Output string and pad to column width (append_spaces) | 輸出字串並填補到欄位寬度（append_spaces） | C |
| 0x97 | 0x42FB | ✅ | `op_97` | Load from character data (offset + word_3AE4) → word_3AE2 | 從角色資料（偏移+word_3AE4）載入→word_3AE2 | D |
| 0x98 | 0x4348 | ✅ | `op_98` | Write word_3AE2 to character data (offset + word_3AE4) | word_3AE2 寫入角色資料（偏移+word_3AE4） | D |
| 0x99 | 0x40E7 | ✅ | `op_99` | TEST word_3AE2 against itself; set ZF/SF flags | TEST word_3AE2 自身，設 ZF/SF 旗標 | D |
| 0x9A | 0x3C42 | ✅ | `op_9A` | game_state[arg] ← 0xFF (clear flag) | game_state[arg]←0xFF（清除 flag） | D |
| 0x9B | 0x416B | ✅ | `op_9B` | Set bit in game_state (OR bitmask) | 設定 game_state 某 bit（OR bitmask） | D |
| 0x9C | 0x4175 | ❌ | *(inferred)* AND ~bitmask on game_state | (inferred) Clear game_state bit (AND ~bitmask); similar to op_4F | 推測：清除 game_state 某 bit（AND ~bitmask），與 op_4F 類似 | D |
| 0x9D | 0x4181 | ✅ | `op_9D` | TEST game_state bit; set flags | TEST game_state 某 bit，設旗標 | D |
| 0x9E | 0x492D | ✅ | `op_9E` | Get resource size → word_3AE2 | 取得資源大小→word_3AE2 | D |
| 0x9F | 0x4AF0 | ❌ | *(inferred)* DATA1 resource operation | (inferred) Another resource operation (between op_1D/0x4ACC and op_1F/0x4AF6); possibly a DATA1 read variant | 推測：另一種資源操作，可能是 DATA1 讀取相關 | D |

### 0xA0–0xFF: Data Artifacts (Not Valid Opcodes) / 資料殘留（非有效 Opcode）

All 96 entries in this range are `NULL` in `targets[]`. Their stored ASM addresses exhibit patterns
characteristic of **x86 machine-code byte pairs** (`0x8A06`, `0xE80E`, `0xA23A`, etc.), entirely
unlike the valid engine code segment addresses (0x3B00–0x4AF6).

此範圍 96 個項目全部為 NULL。儲存的 ASM 位址呈現 x86 機器碼位元組對特徵，
與低位索引的有效程式碼段位址完全不同。

**Conclusion / 結論**: These are `targets[]` array overflow/padding areas pointing into the original
DOS COM file's **data segment** (not code segment). They are **not** valid game script opcodes and
should not be implemented.

這些是 targets[] 陣列溢位/填充區域，對應到原始 DOS COM 檔案的資料段（非程式碼段）。
**不建議嘗試實作這些 opcode。**

| Range / 範圍 | Status | Notes / 備註 |
|---|---|---|
| 0xA0–0xFF (96 entries) | ❌ NULL | ASM addresses are data segment artifacts, not engine code / ASM 位址為資料段殘留，非引擎程式碼 |

---

## Unimplemented Opcode Analysis / 未實作 Opcode 判讀

The following are the ~22 "valid NULL" opcodes in the 0x00–0x9F range with reasonable ASM addresses.
All semantics are **inferred** from position between neighbouring implemented opcodes and disassembly context.

以下為 0x00–0x9F 範圍內約 22 個「有效 NULL」opcode，ASM 位址合理。
所有語意均為**推測**，依鄰近已實作 opcode 與反組譯上下文判斷。

### 0x02 — ASM: 0x3B1F

- **Position / 位置**: Between `set_word_mode` (0x3B18) and `op_04` (0x3B2A)
- **Inferred semantic / 推測語意**: Mode-switch variant distinct from `set_byte_mode` (0x3B0E); possibly clears high byte of word_3AE2 rather than byte_3AE1
- **Category / 分類**: D
- **Evidence / 依據**: dragon.asm does not cover this range / dragon.asm 未涵蓋此範圍

### 0x1B — ASM: 0x3D73

- **Position / 位置**: Between `op_1A` (0x3D5A, game_state←immediate) and `op_1C` (0x3D92, word_3ADF←immediate)
- **Inferred semantic / 推測語意**: Another game_state immediate-write addressing mode (possibly double-byte immediate write to game_state variant)
- **Category / 分類**: D

### 0x1E — ASM: 0x01B2

- **Position / 位置**: ASM 0x01B2 corresponds to the COM startup region (dragon.asm line 105–106: `mov dx, offset empty_string` area near `loc_1B5`)
- **Inferred semantic / 推測語意**: Possibly "jump/call to initialisation code" or "reset engine"; may be a stub address misplanted in targets[]
- **Category / 分類**: E (special — ASM address points outside the script engine region / ASM 位址指向非腳本引擎區域)

### 0x20 — ASM: 0x0000

- **Inferred semantic / 推測語意**: Invalid address; likely NOP, unused, or padding / 位址無效；可能是 NOP 或填充
- **Category / 分類**: E

### 0x29 — ASM: 0x3E1B

- **Position / 位置**: Between `op_28` (0x3E14, DEC word_3AE4 low byte) and `op_2A` (0x3E36, SHL word_3AE2)
- **Inferred semantic / 推測語意**: DEC high byte of word_3AE4, or another decrement variant
- **Neighbour pattern / 相鄰模式**: 0x28=DEC 3AE4-lo, 0x29=?, 0x2A=SHL 3AE2, 0x2B=SHL 3AE4-lo
- **Category / 分類**: D

### 0x2C — ASM: 0x3E4C

- **Position / 位置**: Between `op_2B` (0x3E45, SHL word_3AE4 low) and `op_right_shift` (0x3E67, SHR word_3AE2)
- **Inferred semantic / 推測語意**: SHR low byte of word_3AE4 (complement of 0x2B SHL); or another shift variant
- **Neighbour pattern / 相鄰模式**: 0x2B=SHL 3AE4-lo, 0x2C=?, 0x2D=SHR 3AE2, 0x2E=SHR 3AE4-lo
- **Category / 分類**: D

### 0x37 — ASM: 0x3FAD

- **Position / 位置**: Between `op_36` (0x3F8C, divide) and `op_38` (0x3FBC, AND)
- **Inferred semantic / 推測語意**: Multiply/divide variant or modulo operation: `word_3AE2 % game_state[arg]`
- **Neighbour pattern / 相鄰模式**: 0x35=DIV game_state, 0x36=DIV imm, 0x37=?, 0x38=AND — arithmetic group
- **Category / 分類**: D

### 0x64 — ASM: 0x446E

- **Position / 位置**: Between `op_set_char_data_word` (0x43F7) and NULL 0x65 (0x44B8)
- **Inferred semantic / 推測語意**: Read character data_CA4C word (`get_char_data_word`) — read complement of op_63 (write)
- **Category / 分類**: D

### 0x65 — ASM: 0x44B8

- **Position / 位置**: Between NULL 0x64 (0x446E) and op_66 (0x44CB → actual 0x40C1)
- **Inferred semantic / 推測語意**: Clear or test character data_CA4C
- **Category / 分類**: D

### 0x67 — ASM: 0x44CB

- **Position / 位置**: After op_66 area
- **Inferred semantic / 推測語意**: Character attribute bit-clear operation on data_CA4C
- **Category / 分類**: D

### 0x68 — ASM: 0x450A

- **Position / 位置**: Between NULL 0x67 (0x44CB) and op_69 (0x453F)
- **Inferred semantic / 推測語意**: Character data operation or map coordinate pre-operation
- **Category / 分類**: D

### 0x6B — ASM: 0x45A1

- **Position / 位置**: Between `op_6A` (0x4573, rectangle range check) and `op_6C` (0x45A8, move player)
- **Inferred semantic / 推測語意**: Another coordinate range check, or movement prerequisite action
- **Category / 分類**: D

### 0x6E — ASM: 0x45FA

- **Position / 位置**: Between `op_6D` (0x45F0, minimap commands) and `op_6F` (0x4607, move player)
- **Inferred semantic / 推測語意**: Minimap draw or input auxiliary instruction
- **Category / 分類**: A

### 0x70 — ASM: 0x4632

- **Position / 位置**: Between `op_6F` (0x4607) and `op_71` (0x465B)
- **Inferred semantic / 推測語意**: Level script execution variant (`run_level_script` variant or level data init)
- **Category / 分類**: D

### 0x79 — ASM: 0x47FA ⭐ Localisation Key / 中文化關鍵

- **Position / 位置**: Between `set_msg` (0x47EC, string output) and `op_7A` (0x4801, resource string output)
- **Inferred semantic / 推測語意**: Parameterised `set_msg` variant — possibly `draw_string_at_position` or `set_msg_with_flags`, accepting colour, position, or flag parameters
- **Category / 分類**: C
- **Localisation impact / 中文化影響**: If this opcode controls string display attributes, an unimplemented state could prevent certain dialogue/UI text from rendering correctly

### 0x7E — ASM: 0x4845 ⭐ Localisation Key / 中文化關鍵

- **Position / 位置**: Between `op_7D` (0x483B, character name output) and `advance_cursor` (0x487F) (skipping 0x7F)
- **Inferred semantic / 推測語意**: Formatted string output, or character-name output variant with space-padding for alignment; possibly an `append_spaces` prefix form
- **Category / 分類**: C
- **Localisation impact / 中文化影響**: Character name display path; may affect name alignment or multi-byte character rendering

### 0x7F — ASM: 0x486D ⭐ Localisation Key / 中文化關鍵

- **Position / 位置**: Between NULL 0x7E (0x4845) and `advance_cursor` (0x487F)
- **Inferred semantic / 推測語意**: Cursor position setter or string-end control variant (like advance_cursor but without newline); or "end string output and reset state"
- **Category / 分類**: C
- **Localisation impact / 中文化影響**: Cursor/text layout related; affects character position calculation for CJK glyph placement

### 0x8E — ASM: 0x0000

- **Inferred semantic / 推測語意**: Invalid address; likely NOP or unused / 位址無效；可能是 NOP 或未使用
- **Category / 分類**: E

### 0x8F — ASM: 0x49DD ⭐ Localisation Key / 中文化關鍵

- **Position / 位置**: Between `op_read_string` (0x49D3, keyboard input) and `op_sound_effect` (0x49E7)
- **Inferred semantic / 推測語意**: read_string variant with flags, or combined read+display operation (equivalent to read + display in one opcode)
- **Category / 分類**: C
- **Localisation impact / 中文化影響**: Player naming/input flow; unimplemented state may break naming functionality for non-ASCII characters

### 0x9C — ASM: 0x4175

- **Position / 位置**: Between `op_9B` (0x416B, OR bitmask) and `op_9D` (0x4181, TEST bitmask)
- **Inferred semantic / 推測語意**: AND ~bitmask — clear game_state bit; similar function to op_4F
- **Category / 分類**: D

### 0x9F — ASM: 0x4AF0

- **Position / 位置**: Between op_1D (0x4ACC) and op_1F (0x4AF6)
- **Inferred semantic / 推測語意**: Another resource operation; possibly a DATA1 read variant
- **Category / 分類**: D

---

## Localisation Priority Opcodes / 中文化關鍵 Opcode 清單

This section is for fan localisation teams, especially CJK localisation (Chinese/Japanese/Korean).
Implemented opcodes are listed as background context; unimplemented ones are marked ❌.

本節供同人中文化/本地化團隊參考。已實作 opcode 作為背景資訊；未實作者標記 ❌。

### Priority 1 — String Display Core Path / 第一優先（字串顯示路徑核心）

| Priority | Index | Status | Note — English | 備註 — 中文 |
|---|---|---|---|---|
| 1 | **0x79** | ❌ | `set_msg` parameterised variant — missing link in string output main path | set_msg 帶參數變體 — 字串輸出主路徑上的缺失環節 |
| 2 | **0x7E** | ❌ | Formatted string output or character name formatting — character display path | 字串輸出或角色名稱格式化 — 角色顯示路徑 |
| 3 | **0x7F** | ❌ | Cursor/string-end control — affects text layout | 游標/字串結束控制 — 影響文字佈局 |
| 4 | **0x8F** | ❌ | read_string variant — player naming/input flow | read_string 變體 — 玩家命名輸入流程 |

### Priority 2 — Implemented (Ensure Correct Behaviour) / 第二優先（已實作，確保功能完整）

| Priority | Index | Status | C Function | Description — English | 說明 — 中文 |
|---|---|---|---|---|---|
| 5 | 0x77 | ✅ | `op_77` | Draw pattern then output string — common in dialogue boxes | 繪製圖案後輸出字串，對話框常用 |
| 6 | 0x78 | ✅ | `set_msg` | Primary string output opcode | 主要字串輸出 opcode |
| 7 | 0x7B | ✅ | `read_header_bytes` | Set UI title header | UI 標題設定 |
| 8 | 0x7C | ✅ | `op_7C` | Encounter UI header | 遭遇 UI 標題 |
| 9 | 0x7D | ✅ | `op_7D` | Output character name | 角色名稱輸出 |
| 10 | 0x80 | ✅ | `advance_cursor` | Advance cursor | 游標推進 |
| 11 | 0x81 | ✅ | `op_81` | Numeric output | 數字輸出 |
| 12 | 0x83 | ✅ | `op_83` | Raw byte output | 原始 byte 輸出 |
| 13 | 0x88 | ✅ | `op_wait_escape` | Wait for player confirmation | 等待玩家確認 |
| 14 | 0x89 | ✅ | `wait_event` | Interactive wait | 互動等待 |
| 15 | 0x8C | ✅ | `prompt_no_yes` | Y/N selection prompt | Y/N 選擇提示 |
| 16 | 0x8D | ✅ | `op_read_string` | Keyboard input | 鍵盤輸入 |
| 17 | 0x95 | ✅ | `op_95` | Set cursor position | 游標位置設定 |
| 18 | 0x96 | ✅ | `op_96` | Pad with spaces to column width | 填補空格 |

---

## Undecipherable Opcodes / 無法判讀清單

Opcodes where the ASM address is invalid or falls outside the disassembly coverage:

ASM 位址無效或超出反組譯涵蓋範圍的 opcode：

| Index / 索引 | ASM Addr | Notes / 備註 |
|---|---|---|
| 0x1E | 0x01B2 | Points to COM startup region, outside script engine / 指向 COM 啟動區，非腳本引擎 |
| 0x20 | 0x0000 | Address 0x0000, invalid / 位址 0x0000，無效 |
| 0x8E | 0x0000 | Address 0x0000, invalid / 位址 0x0000，無效 |
| 0xA0–0xFF (96 entries) | various | ASM addresses show x86 machine-code byte-pair patterns; data segment artifacts / ASM 位址呈現 x86 機器碼位元組對特徵，資料段殘留 |

---

## References / 引用來源

- `src/lib/engine.c` — opendw main implementation; `targets[]` dispatch table at line 583 / 主要實作，targets[] 第 583 行
- `docs/dragon.asm` — partial disassembly of original DOS COM binary (3721 lines, covering select ranges within 0x100–0x5C7B) / 部分反組譯（3721 行，涵蓋 0x100–0x5C7B 部分區段）
- `opendw/doc/script.md` — sparse opcode text description / opcode 文字說明（稀疏）
- `opendw/doc/keypress.txt` — keypress handling flow (relevant to op_89) / 按鍵處理流程（op_89 相關）
- `docs/25_OPCODE_INTERPRETATION.md` — source analysis report (this document's authoritative basis) / 來源分析報告（本文件的唯一事實來源）

---

*This document is part of the community Dragon Wars preservation and localisation project.*
*本文件為社群 Dragon Wars 保存與中文化專案的一部分。*
