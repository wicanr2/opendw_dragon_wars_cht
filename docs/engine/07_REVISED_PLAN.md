# OpenDW 火龍之戰中文化 — 修正版計畫 (v2)

> **日期**：2026-06-10
> **作者**：L.CY 工作分身
> **前提**：已 review 先前模型產出（docs 01–06、10–14、20–31、40–60）
> **核心結論**：先前「DATA1 文字萃取」方法錯誤，導致 `20_ALL_TEXT_FROM_DATA1.txt` 多為雜訊，這是「句子總覺得不完整」的真正原因。本計畫修正萃取方法，並重排後續工作。

---

## 一、Review 結論：先前成果的真實狀態

| 產出 | 先前評價 | 實際狀態 | 證據 |
|------|----------|----------|------|
| `20_ALL_TEXT_FROM_DATA1.txt`（3,926 條） | 「優質成果」 | ❌ **多為雜訊**，不可用 | 同一句以滑動一格方式重複出現，夾雜大量亂碼 |
| `05_VERIFICATION_REPORT.md` section 統計 | 17/17 完整 | ❌ section 0x08–0x16「上百條 text」是假象 | 全 bit-offset 暴力掃描 0x08，最高分仍是 `Uiep npcapgb` 等亂碼 |
| `ALL_TEXT_FROM_SCRIPTS.txt`（~400 行） | 次要 | ✅ **唯一乾淨的真實遊戲文字** | script0：`Do you wish to.. / Begin a new game` 完全正確 |
| 反組譯還原（52 函式 / 143 opcode） | 完成 | ✅ 可信，保留 | dragon.asm / engine.c 對照一致 |
| 中文手冊 44 張掃描圖 | 已萃取 | ⚠️ 只有**圖**，OCR 全是空模板 | `31_CHINESE_MANUAL_TEXT.md` 全為「(待 OCR)」 |
| CJK 渲染 / SDL2 實作 | 0% | ❌ 未開始（與評估一致） | — |

### 1.1 根因：萃取方法錯誤

Dragon Wars 字串採 **5-bit 變長壓縮**（`alphabet[]` 表 + escape 0x1E 大寫切換 + escape 0xAF/0xDC 變數代入）。解碼器 `extract_string` 的回傳值是**下一條字串的起始 offset**。

先前萃取腳本（已是 throwaway，僅留 `verify_extraction.py` 驗證器）的做法是：**對每個 byte offset 都嘗試解碼一次**。因為 5-bit 打包下，從錯誤的 byte / bit 位置起解必然產生亂碼或半句，於是：

- 真正的句子被切成數十條半句（`wish to..` / `new game` / `egin a new game`…）。
- 中間混入大量「從錯位起解」的亂碼（`g.e kpatdg udc`）。
- 「3,926 條」嚴重灌水，真實句子數遠少於此且被淹沒。

→ 使用者「句子總覺得不完整」的直覺正確，但問題不是**漏抓**，而是**抓出來的是噪音**。

### 1.2 文字真正存放在哪裡（已驗證）

| 類別 | 位置 | 解碼方式 | 量級 |
|------|------|----------|------|
| **內嵌腳本字串**（對話、UI、提示、選單）— 主體 | script section（0x00 / 0x03 / 0x06 …）的 bytecode 中 | opcode `op_77 draw_and_set`、`op_78 set_msg`、`op_7B ui_header` 之後**緊接**一段 byte 對齊的 5-bit 字串 | 數百～上千條 |
| **資源字串**（怪物名 res 31、物品/法術名…） | 壓縮資源 section（>0x17）；`op_7A extract_string` 從載入後的資源指定 offset 解 | 需先 `resource_load` 解壓再解字串 | 數十～數百 |
| **Read Paragraph 段落** | **印刷手冊**（防拷設計，本來就不在 DATA1） | 中文手冊掃描圖 → OCR | 視手冊 |
| **硬編碼字串** | `dragon.com` / `engine.c`（`chained`/`poisoned`/`Loading...` 等） | 直接改原始碼 | < 20 |

**關鍵**：內嵌腳本字串**無法靠靜態掃描 DATA1 得到** —— 必須跟著 bytecode 走到 `op_77/78/7B` 才知道字串起點與 byte 對齊。這就是 `disasm.cpp` 產出乾淨、而暴力掃描產出垃圾的根本差異。

---

## 二、修正後的策略：以「解碼器即真相」為基礎

不再自寫靜態掃描器猜結構。改用 **opendw 本身已驗證可正確解碼的程式** 來枚舉全部文字。兩條互補路徑：

### 路徑 A（主力）：擴充反組譯式萃取
擴充 `disasm.cpp`，對**所有** script-bearing section 線性掃描 `op_77 / op_78 / op_7B`，在每個命中點解碼內嵌字串，輸出 `(section, offset, text)`。
- 涵蓋率高、可重跑、ID 穩定（section+offset 當翻譯主鍵）。
- 風險：byte/word mode 切換使純線性反組譯不完美（見 `doc/script.md`）；某個 `0x78` 可能是資料而非 opcode → 產生少量誤判。以「解出來是否為乾淨文字」過濾。

### 路徑 B（校驗）：執行期插樁
在 `extract_string`（`engine.c:6207`）加 log，印出每次解碼的 `(來源資源, offset, text)`，實際跑遊戲 / 驅動腳本引擎，蒐集真正被顯示的字串。
- 補 A 的漏網（動態才會走到的分支），並反向剔除 A 的誤判。
- 需在 docker 內 build + 跑（符合 [HARD] docker 規則）。

> 兩路交集 = 高信心的完整文字清單；差集 = 待人工確認的邊界案例。

### 資源字串與手冊
- 資源字串：用既有 `resextract` 解出資源 section（自動解壓），再對 `op_7A` 引用的 offset 套同一解碼器。`monster_info.cpp` 已是此方向的雛形，先驗證它對 res 31 的輸出。
- Read Paragraph：對 44 張中文手冊掃描圖跑 OCR（docker + tesseract `chi_tra`），人工校對進 `31_CHINESE_MANUAL_TEXT.md`。英文手冊 PDF 同樣 OCR 作對照。

---

## 三、分階段計畫

### Phase 0 — 萃取管線重建（先解決「不完整」痛點）
**目標**：產出一份乾淨、去重、有穩定 ID 的可翻譯文字總表，取代 `20_ALL_TEXT_FROM_DATA1.txt`。

- [ ] P0-1 在 docker 內 build opendw_dragon_wars_cht（cmake + SDL2），確認 `disasm` / `resextract` / `monster_info` 可跑。
- [ ] P0-2 擴充 `disasm.cpp`：線性掃全 script section 的 `op_77/78/7B`，輸出 `docs/strings/inline_strings.tsv`（欄位：section, offset, raw_len, text）。
- [ ] P0-3 `extract_string` 加 log（路徑 B），跑標題→開新角色→進遊戲→一場戰鬥，蒐集 `runtime_strings.tsv`。
- [ ] P0-4 比對 A∩B / A∖B / B∖A，產出 `docs/strings/GAME_TEXT_MASTER.tsv`（去重、標信心）。
- [ ] P0-5 解 res 31 等資源字串，輸出 `resource_strings.tsv`。
- [ ] **驗收**：MASTER 表內每條都是可讀英文；總數量級合理（非數千條噪音）；隨機抽 20 條對照遊戲截圖 `org_dialogue/` 正確。

### Phase 1 — 中文手冊與對照
- [ ] P1-1 docker tesseract OCR 中文手冊 44 張 → 校對進 `31_CHINESE_MANUAL_TEXT.md`。
- [ ] P1-2 英文手冊 PDF OCR，建立「英文原文 ↔ Read Paragraph 編號」對照。
- [ ] P1-3 補抓缺頁 `2F3_SCAN1239_022.jpg`（0 byte）。

### Phase 2 — 翻譯（在乾淨 MASTER 表上進行）
- [ ] P2-1 依來源拆檔：`menu / dialogue / items / spells / monsters / ui / paragraphs`，統一 `| ID(section:offset) | 英文 | 中文 | 狀態 |`。
- [ ] P2-2 建 `glossary.md` 術語表（HP=生命值、Spell Power=法力…），先鎖 P0 級 UI/選單。
- [ ] P2-3 由 P0 高→P3 低逐批翻譯（見 §四優先序）。先前 `10–14_TRANSLATION_*.md` 可信部分回填，垃圾來源重來。

### Phase 3 — 中文顯示與回寫（沿用 06_IMPLEMENTATION_PLAN 但修正前置）
- [ ] CJK 24×24 點陣渲染層（文泉驛）、`vga_sdl` pixel scaling、混排。
- [ ] `patch_strings`：把中文 5-bit / 直接 Big5 寫回 DATA1，處理**變長字串**問題（見風險）。
- [ ] 硬編碼字串改原始碼。

> 文件修正（先前 review 指出）：統一 24×24 glyph 規格、刪除 11×11/22×22 矛盾描述、修正「物品名在 0x07 / 技能名在 0x15」等錯誤索引。

---

## 四、翻譯優先序（量級待 Phase 0 定案後校正）

| 優先 | 內容 | 來源 |
|------|------|------|
| P0 | UI / 選單 / Yes-No / 角色建立 | script0、硬編碼 |
| P0 | 主線對話、戰鬥訊息 | inline（0x00/0x03/0x06） |
| P1 | 物品 / 法術 / 技能名 | 資源字串 + player.c 型別表 |
| P1 | 怪物名 | res 31 |
| P2 | NPC 對話、隨機事件 | inline |
| P3 | Read Paragraph 段落 | 中文手冊 |

---

## 五、風險

| 風險 | 影響 | 緩解 |
|------|------|------|
| 線性反組譯誤判 opcode | inline 萃取有雜訊 | 路徑 B 執行期插樁交叉驗證 |
| 變長中文字串回寫 DATA1 | section 偏移錯亂、存檔不相容 | 字串表改間接定址 / 重建 section offset header；或外掛字串表不動原檔 |
| 24×24 與 8×8 混排排版 | UI 破版 | 先 16×16 保守版；`rect_dimensions` 重算 |
| tesseract 繁中辨識率 | 手冊 OCR 需大量校對 | 人工為主、OCR 為輔 |
| docker 內 SDL2 跑遊戲蒐 log | 環境成本 | 先用 `vga_null` headless 後端驅動腳本即可 |

---

## 六、與舊文件的處置

- **保留**：`02_ANALYSIS`、`23_SOURCE_CODE_MAP`、`dragon.asm`、`06_IMPLEMENTATION_PLAN`（Phase 3 部分）。
- **作廢/重做**：`20_ALL_TEXT_FROM_DATA1.txt`（噪音）、`05_VERIFICATION_REPORT` 的 section 統計、`22_DATA1_SECTION_DETAILS` 中「TEXT section 上百條」的結論。
- **修正**：`24_SCRIPT_TEXT_MAPPING` 中引用 `ALL_TEXT_FROM_DATA1` 的 offset 對照。

---

## 七、立即下一步（建議今日執行）

1. docker build 專案，驗證 `disasm` 可跑（P0-1）。
2. 擴充 `disasm.cpp` 全 section 掃 `op_77/78/7B`，先產出 inline 字串草表（P0-2）。
3. 拿草表對照 `org_dialogue/` 截圖抽驗，確認乾淨後，再決定是否需要路徑 B。

*（本檔取代 `04_NEXT_PLAN.md` 作為現行計畫；04 保留為歷史。）*
