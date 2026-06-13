# 規劃:Read Paragraph 段落內嵌顯示功能

> **日期**：2026-06-10（規劃）/ 2026-06-13（已實作,自包含驗證通過）
> **目標**：重製版遇到「Read paragraph N」時,直接在遊戲內顯示手冊段落文字,免去翻實體手冊。
> **狀態**：✅ 已實作(自包含,無 DATA1)。段落號 N 來源逐指令對拍 opendw oracle;
> 段落書 bundle `assets/bundle/paragraphs/<locale>/paragraphs.tsv`(147 段,自 `docs/34`);
> VM op_58/59 對齊 opendw byte-stack + 新增 op_81(print_number)使 N 進輸出流;
> app `run_event` 攔「Read paragraph 」+ N → 顯示段落繁中原文(回退「Read paragraph N」);
> CJK atlas 擴至 1653 字涵蓋全段落;7 個觸發點 N=27/90/146/134/108/53/65 與地點吻合。
> **未做**:長段落 scrollable overlay(目前 FP 訊息區僅約 3 行,長段落底部截斷)。
> 截圖:`opendw_remake/docs/screenshots/r8_read_paragraph_zh.png`(Magic College + 段落 146)。

---

## 一、背景與動機

Dragon Wars 原版用「段落防拷」機制:遊戲只顯示 `Read paragraph 137`,玩家必須翻**實體手冊**查第 137 段。這是 1989 年防盜版設計,也順便節省記憶體(劇情文字不放遊戲裡)。

重製版的痛點:沒有實體手冊就玩不下去。**本功能讓段落文字直接顯示在遊戲內**,並可中英對照。

---

## ✅ 對齊驗證(2026-06-10,關鍵)

**手冊段落號 = 遊戲「Read paragraph N」的 N,已實證對齊。**

- 玩家截圖:畫面「Magan Underworld」+「Read paragraph **137**」。
- 轉寫的 `34_READ_PARAGRAPHS.md` 段落 137:「…你發現瑪根(Magan)地底世界之後——艾卡拉(Irkalla)被一條施過魔法的銀鍊綁住。」
- 內容與遊戲觸發點完全吻合 → 段落資料庫只要用「段落 N」直接查表即可,不需額外對齊轉換。

（附帶:該截圖的坐姿女性即**艾卡拉 Irkalla**,瑪根地底世界的黑暗之后,Amiga 版場景插圖。）

---

## 二、技術調查結果(已確認)

| 項目 | 結果 |
|------|------|
| 觸發字串 | `"Read paragraph "`(含尾空格)位於 **DATA1 section 0x08, offset 0x2d9** |
| 輸出 opcode | `op_78 (set_msg)` 輸出字串,其後腳本算出段落號 N,經 `op_81`(數字輸出,讀 word_3AE2)接在後面 |
| 段落號來源 | 腳本依當前地點/事件計算 N 寫入 word_3AE2 |
| 段落文字來源 | **不在遊戲檔內**,在印刷手冊(這正是防拷重點) |

→ 段落號是動態算出的,文字在手冊。重製版要做的是:**抓到 N → 從段落資料庫查文字 → 顯示**。

---

## 三、設計

### 3.1 攔截點(deep module:窄介面)

不逐條追 bytecode。改在**文字輸出邊界**攔截:訊息組裝完成、即將繪到螢幕時,偵測內容是否符合 `^Read paragraph (\d+)`。

- 在 `engine.c` 的訊息輸出路徑(`set_msg` / `append_string` / `ui_draw_string` 之一)加一個 hook:
  - 累積目前這條 message 的純文字。
  - 比對是否為 `Read paragraph N`。
  - 命中 → 解析 N,呼叫 `paragraph_show(N)`,並依設定決定是否**抑制**原本的「Read paragraph N」字樣。
- 優點:與腳本 bytecode 解耦,不管哪個腳本、哪個地點觸發都能攔到;只認最終文字。

### 3.2 段落資料庫

新增外部資料檔(不動原始 DATA1),build 時打包:

```
data/paragraphs/
  paragraphs_zh.txt    # 繁中(官方手冊轉寫)
  paragraphs_en.txt    # 英文(英文手冊)
  paragraphs.idx       # 段落號 → (offset,len) 索引
```

格式(純文字,易維護、易校對):
```
@137
你正站在罪惡之城——波卡(Purgatory)的競技場大門前……
@138
...
```

- 來源優先序:**官方手冊轉寫**(`34_READ_PARAGRAPHS.md`,視覺轉寫中)> **軟體世界攻略**(交叉校對/補漏)> 英文手冊(`32_EN_MANUAL_TEXT.md`,英文對照用)。
- 提供一支小工具 `build_paragraphs`:把 `34_READ_PARAGRAPHS.md` 編成 `paragraphs_zh.txt` + `.idx`。

### 3.3 顯示 UI:段落檢視器(overlay)

- 觸發時彈出一個**段落檢視視窗**(可佔 viewport 或全螢幕半透明框)。
- 內容:該段中文(或中英對照),長段可**捲動 / 分頁**(段落常超過一畫面)。
- 操作:`空白/Enter` 翻頁,`Esc` 關閉回到遊戲。
- 依賴 **#2 的 CJK 24×24 渲染**(段落是大量中文)。在 #2 完成前,可先用英文段落驗證框架。

### 3.4 模式切換(config)

`--paragraphs` 選項:
| 值 | 行為 |
|----|------|
| `inline`(預設) | 直接顯示段落文字,取代「Read paragraph N」 |
| `bilingual` | 中英對照顯示 |
| `manual` | 維持原版只顯示「Read paragraph N」(懷舊/驗證用) |

語言:`--lang zh-TW`(預設,顯示中文段落)/ `en`(英文段落)。

---

## 四、實作步驟

1. **段落資料管線**:`34_READ_PARAGRAPHS.md`(轉寫中)→ `build_paragraphs` → `paragraphs_zh.txt + .idx`。先可不依賴遊戲即完成。
2. **攔截 hook**:在訊息輸出路徑加 `Read paragraph N` 偵測 + `paragraph_show(N)`。先用 stdout/log 驗證 N 抓得到。
3. **段落檢視器 UI**:文字框 + 捲動/分頁 + Esc。先英文、後接 #2 中文。
4. **config 開關**:`--paragraphs` / `--lang`。
5. **驗證**:遊戲走到會觸發段落的地點(如 Purgatory 競技場 = 段落 4 一類),確認彈出正確段落並可關閉續玩。

---

## 五、相依與風險

| 項目 | 說明 |
|------|------|
| 依賴 #2 CJK 渲染 | 中文段落需要 24×24 點陣;先以英文段落把框架做完 |
| 段落號 ↔ 文字對齊 | 需確認手冊段落編號與遊戲 N 完全一致(用已知觸發點交叉驗證,如 Purgatory 大門) |
| 段落文字完整性 | 官方手冊轉寫(agent 進行中)+ **軟體世界攻略**交叉補漏;缺號標記待補 |
| 長段落排版 | 中文段落長,需捲動/分頁與自動換行(CJK 不分詞,按字寬斷行) |
| 版權 | 段落文字為手冊版權內容;與本專案既有手冊掃描同性質,屬保存/中文化範疇 |

---

## 六、待使用者確認的決策

1. **預設模式**:`inline`(直接顯示中文段落)還是 `bilingual`(中英對照)?建議 `inline`、可切 `bilingual`。
2. **段落文字主來源**:以**官方手冊**為主、**軟體世界攻略**為輔校對 —— 是否同意?(軟體世界文字若更通順,可作主來源,但需確認與遊戲段落號對應)
3. **顯示位置**:viewport 內嵌 vs 全螢幕段落框?段落長,建議全螢幕可捲動框。

---

## 七、外部參考來源

- **軟體世界**(臺灣電腦雜誌)火龍之戰攻略 —— 使用者提供,作段落文字交叉校對來源。
- 官方臺灣中文手冊(珍066-火龍之戰)旅行指南 → `34_READ_PARAGRAPHS.md`。
- 英文手冊 → `32_EN_MANUAL_TEXT.md`(英文段落對照)。
