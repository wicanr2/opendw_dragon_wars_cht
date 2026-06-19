# 為什麼原始火龍之戰要拆 DATA1 / DATA2?(1989 硬體環境下的設計推理)

> 從本專案實際觀察到的檔案結構,推理 Interplay Dragon Wars(1989)把資料拆成
> `DATA1` / `DATA2` 兩檔的原因。屬工程推理,非官方文件。

## 觀察到的事實

| 事實 | 來源 |
|------|------|
| 5.25" 版有 `DISK01.IMA` / `DISK02.IMA`,各 **360KB**(368640 bytes) | 原始 `Dragon Wars (1990).zip` |
| 3.5" 版單片 **720KB** 即裝得下全部 | 同上 |
| 資料量:DATA1 296KB + DATA2 352KB + DRAGON.COM 56KB ≈ **650KB** | 取出的檔案 |
| 兩檔都有完整 384 筆 16-bit header;某資源在某檔標 `0xFFFF` = 不在此檔 | `resource.c` + 本專案 DATA2 bug 修正 |
| DATA1 = 低編號(0x00–0x16:初始 script、角色資料、壓縮字串/怪物資料、常用 UI 圖) | 萃取分析 |
| DATA2 = 高編號(0x17+:地圖視埠、怪物 sprite、PCM 音效) | 萃取分析 |
| section >0x17 為 LZSS/Huffman 壓縮 | `compress.c` |

## 推理

### 1. 最硬的約束:軟碟容量
**650KB 塞不進一片 360KB 的 5.25" 軟碟。** 資料必須跨兩片:DATA1(296KB)+ COM 放 Disk 1,DATA2(352KB)放 Disk 2。**DATA1/DATA2 的切分,本質上對應兩片實體磁碟。** 3.5"(720KB)容得下時就一片,但邏輯切分沿用(同一套引擎/格式)。

### 2. `0xFFFF` 機制 = 多卷資源目錄
兩檔各有完整 header,`0xFFFF` 代表「不在這片,去另一片找」。這是一個橫跨兩片磁碟的**統一資源 ID 空間 + 卷目錄**:遊戲用同一套 resource ID,不管實體在哪片。為多磁碟而生的索引設計。
> (本專案踩過的雷:原 `resource.c` 只讀 data1,沒處理 `0xFFFF→去 data2`,導致近半怪物 sprite 讀錯。修正後才知道這正是卷目錄機制。)

### 3. 依「存取模式」分卷,減少換片
1989 年很多人**直接從軟碟跑**(沒硬碟或太貴太小)。把「開機/共用」(初始 script、UI、角色)放 Disk 1,「各關卡大塊媒體」(地圖、怪物 sprite、遭遇音效)放 Disk 2,可**最小化換片**:在城裡跑多讀 Disk 1,進新區域才需 Disk 2。

### 4. RAM + 壓縮
DOS real mode 僅 640KB、64KB 分段,650KB 不可能全載入。所以**按需載入**(resource manager 128 slot,載入/釋放)+ **壓縮**(section >0x17)。壓縮讓每片裝更多、也省 RAM。DATA2 幾乎全是壓縮圖形/音效 —— 「大塊媒體壓起來放第二片」。

## 一句話

> **DATA1/DATA2 = 「把 650KB 的遊戲塞進 1989 年 360KB 軟碟」的解法**:跨兩片磁碟、用 `0xFFFF` 卷目錄統一 ID、按存取頻率分卷減少換片、再加壓縮與按需載入對付 640KB RAM。

## 與本 remake 的對照(反向)

remake 把兩者**合成一個 asset bundle**(見 [`adr/0001`](adr/0001-asset-bundle-and-resource-provider.md))。現代儲存沒有軟碟容量/換片/640KB 約束,所以:
- 當年**拆**是為了**塞得下**(硬體妥協)。
- 現在**合**是為了**改得動**(可編輯/可替換,對話=改 TSV、sprite=改 PNG)。
