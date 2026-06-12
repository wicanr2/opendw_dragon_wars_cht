# ADR 0001 — Asset Bundle 與 ResourceProvider:讓 resource 脫離 DATA1、可編輯/可替換

> **狀態**：已採納(2026-06-12)
> **決策者**：L.CY
> **相關**：`opendw_remake/ARCHITECTURE.md` §5(自包含資產)

## 背景

remake 目前執行期直接從原始 `DATA1/DATA2` 解碼資源(`archive.load`)。這帶來兩個限制:
1. **要修改某個 resource(對話、sprite、tileset)很困難** —— 字串是 5-bit 變長壓縮且嵌在 bytecode、sprite 是 encounter 格式、全部包在壓縮 section 裡,改一個就要重打包整個 DATA1 並維持 offset。
2. 執行期**依賴原始磁碟檔**。

需求:讓資源變成**可獨立編輯、可替換**的自有資產(例:翻譯對話、替換 sprite,未來塞 **X68000 / PC-9801** 的高彩美術),且執行期不需 DATA1。

## 決策

### 1. 引入 asset bundle + ResourceProvider 抽象
```
DATA1/DATA2 ──(tools/extract,一次性,用已驗證 R0 解碼)──► assets/bundle/
ResourceProvider(介面)
  ├─ Data1Provider   讀原始檔(萃取來源 + 對拍 oracle)
  └─ BundleProvider  讀 bundle(remake 執行期用,不需 DATA1)
```
remake 執行期走 **BundleProvider**;DATA1 僅用於「萃取一次」與「驗證」。

### 2. 每種 resource 用適合的可編輯格式
| 類型 | bundle 格式 | 編輯方式 |
|------|------------|----------|
| 對話 / 文字 | **外部字串表,鍵 = `resource:offset`** | 改 TSV,**不動 bytecode**(避開 5-bit 變長重編打亂 offset) |
| bytecode(腳本) | 原樣二進位 | VM 直接跑;改字串走字串表 |
| sprite | **indexed `.spr`(自有簡易格式)+ 對應 `.png`(編輯用)** | 改 PNG → 工具轉回 `.spr` |
| tileset / map | tileset→PNG;map→可讀格式 | 同上 |

- **字串不重編 5-bit**:bytecode 維持原樣;VM 的字串輸出 opcode 改成「以 `resource:offset` 查字串表」取字(英文或中文)。這也正是 i18n 機制(R3 已 demo)。
- **sprite 走 PNG/indexed**:remake 的 sprite 層改成「載 bundle sprite」而非「從 DATA1 解 encounter 格式」。

### 3. Sprite 替換(X68000 / PC-9801)
- 由 **sprite manifest**(`ID → 檔案 + 來源平台 + 色盤 + scale`)驅動;換平台美術 = 換檔案。
- **分層**:
  - *資料層(本決策即提供)*:bundle + manifest,換檔即換圖。
  - *渲染層(後續 opt-in)*:X68000(65536 色)/PC-9801(16/4096 色)美術超出 DOS 16 色 320×200,需獨立的 **「remaster 渲染模式」**(更高色深 / framebuffer / per-sprite native 解析度)。manifest 預留 `palette`/`scale` 欄位。

### 4. 正確性
- bundle 的非編輯資源,執行期載入須與 `Data1Provider` 的解碼輸出 **byte-for-byte 一致**(沿用 R0 對拍)。編輯過的資源不對拍(那是刻意改動)。

## 後果

**正面**:對話可直接翻譯(改 TSV)、sprite/tileset 可用繪圖軟體改、未來可塞 X68000/PC-9801 美術、執行期脫離 DATA1、利於 modding 與保存。
**代價**:多一層 extract + bundle 格式維護;remaster 高彩渲染是另一筆獨立工作。
**不做的事**:不重編 5-bit 字串回 DATA1(改走外部字串表);不在本決策內做高彩渲染(僅預留 manifest 欄位)。

## 實作順序
1. bundle 格式 + `tools/extract`(先 sprite → `.spr`+PNG + manifest,字串 → TSV by offset)。
2. remake `BundleProvider` + sprite 載入(走 bundle,不碰 DATA1)。
3. 驗證「換 PNG 就換圖」+ bundle 載入 == DATA1 解碼(未編輯者)。
4. (後續)remaster 渲染模式吃高彩 sprite。
