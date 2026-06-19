# docs/ 文件審查報告 (Doc Audit)

> **審查日期**：2026-06-10
> **審查者**：L.CY 工作分身
> **依據**：本 session 已查證的「正確基準」(見下) + repo 根 `../CONTEXT.md`
> **原則**：保守、不破壞 — 作廢檔一律「加註指向正確來源」而非刪除;不確定者列入待裁決,不擅改。

---

## 正確基準 (作為事實)

1. `20_ALL_TEXT_FROM_DATA1.txt`(3926 條)= **雜訊**(逐 byte 暴力解 5-bit 壓縮的滑動重複 + 亂碼)。乾淨文字 = `ALL_TEXT_FROM_SCRIPTS.txt`(跟 bytecode op_77/78/7b 解出)。
2. section 0x08–0x16 **不是文字表**;「上百條 text」是暴力萃取假象。
3. 怪物名在 **res31**;`monsters.txt` 的 168/196/200/210/222 是 **sprite 圖編號,非名字**。正解見 `26_MONSTERS_AND_SPRITES.md`。
4. 官方譯名以 `../CONTEXT.md` 為準:Purgatory=波卡城/罪惡之城、Namtar=納達、Nergal=奈羅(≠Namtar)、Low/High Magic=初級/高級魔法。
5. 手冊:`30/31`(tesseract OCR 粗糙)已被 `33/34`(視覺精確轉寫)取代。
6. 現行計畫 = `07_REVISED_PLAN.md`(取代 `04_NEXT_PLAN.md`)。

---

## 分類總表

圖例:✅ 現行/良好 ｜ ✏️ 已修正(加註) ｜ 📦 已整併(留指標) ｜ ⚠️ 標作廢(加註指向正解)

| 檔案 | 分類 | 狀態 | 本次處理 |
|------|------|------|----------|
| `00_DOC_AUDIT.md`(本檔) | 報告 | ✅ | 新增 |
| `01_PLAN.md` | 規劃 | ✅ 保留 | 早期計畫,內含 11×11/22×22 字型尺寸矛盾(03 已指出),歷史價值保留;頂部加歷史性註記 |
| `02_ANALYSIS.md` | 分析 | ✅ 保留 | 反組譯 ASM↔C 對照,可信,不動 |
| `03_REVIEW_REPORT.md` | 審查報告 | ✏️ 修正 | 把「3926 條=優質成果/已完整萃取」結論標作廢,頂部加註指向 07 |
| `_deprecated/04_NEXT_PLAN.md` | 計畫(舊) | ⚠️ 作廢(已歸檔 D3) | 被 `07_REVISED_PLAN.md` 取代;頂部作廢註記保留;已 git mv 至 `_deprecated/` |
| `05_SDL2_IMPLEMENTATION.md` | 實作計畫 | ✅ 保留 | SDL2 取代 DOS 設定/音效,內容獨立有效,不動 |
| `_deprecated/05_VERIFICATION_REPORT.md` | 驗證報告 | ⚠️ 作廢(已歸檔 D3) | 「3926 條完整萃取/17 section 全 ✅」為暴力萃取假象;頂部作廢註記保留;已 git mv 至 `_deprecated/` |
| `06_IMPLEMENTATION_PLAN.md` | 實作計畫 | ✅ 保留 | 中文顯示路徑,引用舊結論但主體有效;頂部加註「萃取數據以 07 為準」 |
| `07_REVISED_PLAN.md` | 計畫(現行) | ✅ 良好 | 不動 |
| `08_READ_PARAGRAPH_FEATURE.md` | 規劃 | ✅ 良好 | 不動 |
| `10_TRANSLATION.md` | 翻譯主表 | ✏️ 修正(D1+D2 已執行) | 刪除 50 條「未知文字」雜訊 + 26 列損壞 section 統計表;Purgatory→波卡城、Namtar→納達、Humbaba→胡姆巴巴、Beast→深淵之獸(中英雙語皆留);誤植技能表已標註 |
| `11_TRANSLATION_DIALOGUE.md` | 翻譯對話 | ✏️ 修正(D2 已執行) | Purgatory(城)→波卡城、slums of Purgatory→波卡城的貧民窟、Namtar→納達、Humbaba→胡姆巴巴、Court of Miracles→奇蹟宮廷、Clopin→特魯伊弗 全文統一 |
| `12_TRANSLATION_ITEMS.md` | 翻譯物品 | ✏️ 修正(D4 已執行) | 刪除臆測消耗品/配件/特殊物/待確認區與對應 ID 列;只留已驗證武器/防具;頂部註明物品完整表待從 res/script 重建 |
| `13_TRANSLATION_SKILLS.md` | 翻譯技能 | ✅ 保留 | 內容尚可,加註資料來源(技能名實際在 res,非 0x15) |
| `14_TRANSLATION_MONSTERS.md` | 翻譯怪物 | ✏️ 修正 | 168/196=怪物名 為 sprite 編號誤用、整章虛構怪物清單 — 加警語指向 26 |
| `_deprecated/20_ALL_TEXT_FROM_DATA1.txt` | 資料(雜訊) | ⚠️ 作廢(已歸檔 D3) | 已 git mv 至 `_deprecated/`;指向 `ALL_TEXT_FROM_SCRIPTS.txt` |
| `ALL_TEXT_FROM_SCRIPTS.txt` | 資料(乾淨) | ✅ 良好 | 不動 |
| `21_DATA1_RESOURCE_INDEX.md` | 資源索引 | ✏️ 修正 | 沿用「3926 條/22 section 有文字」假數據;頂部加註,數字以 26/07 為準 |
| `22_DATA1_SECTION_DETAILS.md` | section 分析 | ✏️ 修正 | 「0x08–0x16 各有上百條 text」為假象;頂部加作廢註記 |
| `23_SOURCE_CODE_MAP.md` | 原始碼地圖 | ✅ 良好 | 不動 |
| `24_SCRIPT_TEXT_MAPPING.md` | 腳本映射 | ✅ 保留 | 引用 ALL_TEXT_FROM_DATA1 但映射邏輯有參考價值;頂部加註資料源 |
| `25_OPCODE_INTERPRETATION.md` | opcode 判讀 | ✅ 良好 | 不動 |
| `OPCODE_REFERENCE.md` | opcode 參考 | ✅ 良好 | 不動 |
| `26_MONSTERS_AND_SPRITES.md` | 怪物+sprite | ✅ 良好 | 不動(正解來源) |
| `_deprecated/30_CHINESE_MANUAL_INDEX.md` | 手冊索引 | ⚠️ 作廢(已歸檔 D3) | 被 33/34 取代;頂部作廢註記保留;已 git mv 至 `_deprecated/` |
| `_deprecated/31_CHINESE_MANUAL_TEXT.md` | 手冊 OCR | ⚠️ 作廢(已歸檔 D3) | tesseract OCR 粗糙;被 33/34 取代;已 git mv 至 `_deprecated/` |
| `32_EN_MANUAL_TEXT.md` | 英文手冊 OCR | ✅ 保留 | 英文手冊 OCR,仍是英文段落唯一來源,保留;加「OCR 粗糙待校」註 |
| `33_MANUAL_TRANSCRIPTION.md` | 手冊精確轉寫 | ✅ 良好 | 不動(取代 30/31) |
| `34_READ_PARAGRAPHS.md` | 段落精確轉寫 | ✅ 良好 | 不動(取代 30/31 的段落部分) |
| `35_SOFTWORLD_25.md` | 軟世攻略 25 | ✅ 良好 | 不動 |
| `36_SOFTWORLD_26.md` | 軟世攻略 26 | ✅ 良好 | 不動 |
| `37_SOFTWORLD_27.md` | 軟世攻略 27 | ✅ 良好 | 不動 |
| `40_ORIGINAL_DOCS_SUMMARY.md` | 原始文件摘要 | ✏️ 修正 | 怪物表沿用 168=Wolf(sprite 編號當名字);加註指向 26 |
| `41_TECHNICAL_DEBT.md` | 技術債 | ✅ 保留 | 重構清單,內容有效,不動 |
| `50_BUILD.md` | 建置 | ✅ 保留 | 不動(待 build 實際對齊時再檢) |
| `51_TEST_PLAN.md` | 測試 | ✅ 保留 | 不動 |
| `60_SKILL.md` | Skill 記錄 | ✏️ 修正 | 含「3926 條 5-bit 解壓成果」舊敘述;頂部加註萃取方法已修正,指向 07 |
| `99_INDEX.md` | 索引(舊) | ⚠️ 重建 | 原內容竟是 OpenDW DOS build 說明(非索引),完全文不對題;重寫為真索引;指向作廢檔的連結已改 `_deprecated/` |
| `README.md` | docs 索引 | ✏️ 修正 | 補上 00/33–37、修正閱讀順序、作廢檔連結改 `_deprecated/`、新增「作廢歸檔」區 |
| `_deprecated/README.md` | 歸檔說明 | ✅ 新增 | 列出 6 個作廢檔 + 改參考對象 |
| `_deprecated/verify_extraction.py` | 驗證腳本 | ⚠️ 作廢(已歸檔 D5) | 驗的是 3,926 雜訊基準,邏輯過時;頂部加作廢 docstring;已 git mv 至 `_deprecated/` |

### 統計(D1–D5 執行後)

- ✏️ 修正(加註/對齊/清理):**12**(01, 03, 06, 10, 11, 12, 13, 21, 22, 24, 32, 40, 60 + README — 含 README)
- ⚠️ 作廢並 git mv 至 `_deprecated/`:**6** 檔(04、05_VERIFICATION、20、30、31、verify_extraction.py)
- 重建:`99_INDEX.md`(原為文不對題的 DOS build 說明)
- 新增:`00_DOC_AUDIT.md`、`_deprecated/README.md`
- ✅ 保留/良好不動:其餘(02, 05_SDL2, 07, 08, 23, 25, 26, 33–37, 41, 50, 51, OPCODE_REFERENCE, ALL_TEXT_FROM_SCRIPTS.txt, dragon.asm)

> 註:整併(📦)未做硬合併 — 30/31 與 33/34 結構不同(索引/OCR vs 精確轉寫),改以「作廢 + 歸檔 `_deprecated/` + 指標」處理。

---

## 裁決執行紀錄 (D1–D5 已執行,2026-06-10)

> 使用者已於本輪裁決 D1–D5。以下為執行結果。**未 git commit**,僅改檔/`git mv`,留待 review。

### D1 — 刪除「未知文字」雜訊條目 ✅ 已執行
- `10_TRANSLATION.md`:刪除 **50 列**「中文欄=未知文字」的亂碼條目(如 `'l .ias'tdamu`、`JTs6plk,fd`),並一併刪除嵌入表中的**損壞 section 統計表 26 列**(`**總計**/**3926**`、`0xNN（…）|數|數`、`英文|中文|職業`…)。
- 真實有意義條目全數保留;頂部警語改為「已刪除」紀錄。

### D2 — 譯名對齊(中英雙語都保留)✅ 已執行
- 一律維持「English | 中文」雙語,不丟英文、不只留中文。
- `10`:`Purgatory(城)→波卡城`(NO_ONE_ESCAPES / PURGATORY_ALIVE / THIS_IS_THE_MAIN_GATE / IS_FREEDOM_FROM / THE_STONE_WALLS 5 條煉獄→波卡城)、`Beast From The Pit→深淵之獸`、`Humbaba→胡姆巴巴`、`Namtar→納達`(STRIPPED 條前一輪已改)。
- `11`:§9.1 開場 4 條(slums of Purgatory→**波卡城的貧民窟**、其餘城名→波卡城、深淵野獸納姆塔→深淵之獸納達);§9.3 `奇蹟法庭→奇蹟宮廷`、`亨巴巴→胡姆巴巴`、`特魯伊富→特魯伊弗`、`法庭→宮廷`;§11 注意事項改為「已統一」。
- `slums of Purgatory` 語意保留為「波卡城的貧民窟」,未把 slums 也改掉。

### D3 — 作廢檔移至 `_deprecated/` ✅ 已執行
- `git mv` 至 `docs/_deprecated/`:`04_NEXT_PLAN.md`、`05_VERIFICATION_REPORT.md`、`30_CHINESE_MANUAL_INDEX.md`、`31_CHINESE_MANUAL_TEXT.md`、`20_ALL_TEXT_FROM_DATA1.txt`。
- 各檔頂部原作廢註記保留。新增 `_deprecated/README.md` 說明。
- `README.md`、`99_INDEX.md`、本檔內指向這些檔的連結/路徑已改為 `_deprecated/…`;`32_EN_MANUAL_TEXT.md` 內一處對 31 的引用亦更新。

### D4 — 刪除 `12_TRANSLATION_ITEMS.md` 虛構物品 ✅ 已執行
- 刪除:§7.3 消耗品(potion/scroll/elixir/phoenix down/cure…)、§7.4 配件(ring/amulet/necklace/bracelet/charm/talisman)、§7.5 特殊物品的臆測項(Key/Map/Compass/Torch)、§8「待確認物品」、§9 物品 ID 表中對應的消耗品/配件/任務物品列。
- 保留:script/res 已驗證的武器(§1)、防具(§2)、Armor of Light(特殊,真實出現)、真實訊息字串、武器/防具 ID 列。
- 頂部註明:物品完整表待比照怪物從 res31 重建。

### D5 — `verify_extraction.py` 移至 `_deprecated/` ✅ 已執行
- `git mv` 至 `docs/_deprecated/verify_extraction.py`,頂部 docstring 加作廢說明;`_deprecated/README.md` 亦列出。
