# OpenDW 原始文件摘要

**建立日期**：2026-06-09
**來源**：`/home/anr2/tmp/longcat/opendw/doc/`
**用途**：提供 opendw 原始文件的關鍵資訊，供中文化專案參考

---

## 一、遊戲資源格式 (resources.md)

### 1.1 DATA1 資源索引

| 資源編號 | 大小 | 壓縮 | 用途 | 備註 |
|----------|------|------|------|------|
| 0 | 1148 | 否 | 初始遊戲腳本 | 包含主選單、角色建立 |
| 7 | 5632 | 否 | 角色資料 | 3584 bytes 玩家記錄 + 256 bytes 遊戲狀態 + 0x700 bytes D760 |
| 29 | 未知 | 是 | 標題畫面 | 320×200 圖片 |
| 31 | 2177 | 是 | 怪物字串資料 | 包含怪物名稱 |
| 71 | 5846 | 是 | Purgatory 關卡 | 第一個關卡 |
| 110 | 11860 | 是 | Castle wall（視埠） | 城堡牆壁 |
| 111 | 7050 | 是 | Sky portion（視埠） | 天空部分 |
| 112 | 5054 | 是 | Red clay road portion（視埠） | 紅土路部分 |
| 116 | 936 | 是 | Water puddle（視埠） | 水坑 |
| 261 | 12452 | 是 | Scream（PCM 音訊） | 音效資源 |

### 1.2 DRAGON.COM 內嵌資源

| 資源編號 | 大小 | 用途 |
|----------|------|------|
| 0 | 2560 | 底部磚塊（訊息視窗） |
| 1 | 128 | 左下磚塊（訊息） |
| 2 | 128 | 右下磚塊（訊息） |
| 3 | 1280 | 頂部磚塊（訊息） |
| 4 | 576 | 右側角色邊框 |
| 5 | 1536 | 角色橫幅（Dragon Wars） |
| 6 | 1152 | 左側綠色支柱 |
| 7 | 64 | 左側磚塊支柱連接器 |
| 8 | 64 | 右側磚塊支柱連接器 |
| 9 | 2880 | 右側綠色支柱 |
| 10 | 364 | 北方向箭頭 |
| 11 | 364 | 東方向箭頭 |
| 12 | 364 | 南方向箭頭 |
| 13 | 364 | 西方向箭頭 |

### 1.3 資源 7（角色資料）結構

```
偏移 0x000-0x7FF：3584 bytes（7 個角色記錄 × 512 bytes）
偏移 0x800-0x8FF：256 bytes（遊戲狀態資料）
偏移 0x900-0xFFF：0x700 bytes（載入到 D760）
```

**單個角色記錄結構（512 bytes）**：
- 名稱：12 bytes
- 屬性：strength/dexterity/intel/spirit
- 生命值/昏迷值/法力值
- 技能：27 bytes
- 法術：9 bytes
- 物品：12 個物品 × 16 bytes
- 狀態/gender/level/xp/gold

---

## 二、遊戲腳本格式 (script.md)

### 2.1 虛擬 CPU

遊戲使用一個类似 65C816 的虛擬 CPU，最多 256 個 opcodes。
- 可在 8-bit 和 16-bit 模式間切換
- 每個 opcode 可接受可變數量參數
- 在 `engine.c` 中實作

### 2.2 已知腳本位置

**DATA1 Section 0（初始遊戲腳本）**：
- 偏移 1103：45 bytes
- 偏移 237：未知用途
- 偏移 246：顯示玩家名稱
- 偏移 1137：11 bytes

**其他腳本**：
- Section 71，偏移 3735：Purgatory 遊戲開始
- Section 71，偏移 5764：Purgatory 關卡腳本
- Section 71，偏移 3689：Purgatory 關卡腳本
- Section 22，偏移 68：未知用途
- Section 3，偏移 1706：遭遇戰腳本

### 2.3 Opcode 格式

| Opcode | 參數 | 說明 |
|--------|------|------|
| 00 | 無 | 切換到 16-bit 模式 |
| 01 | 無 | 切換到 8-bit 模式 |
| 02 | -- | 未知 |
| 03 | 無 | 彈出堆疊，重置資源 |
| 04 | 無 | 推入資源索引到堆疊 |
| 09 | 1 (B\|W) | 載入 word_3AE2 |
| 10 | 2 B | 載入 gamestate |
| 46 | 無 | 有符號跳躍 |
| 58 | 1 B | 載入 gamestate，檢查零/符號旗標 |
| 66 | 1 B | 載入 gamestate，檢查零/符號旗標 |

**完整 opcode 列表**：見 ANALYSIS.md 第 77-117 行

---

## 三、怪物資料 (monsters.txt)

### 3.1 怪物 sprite 圖形資源編號

> ⚠️ **修正(2026-06-10)**:下表的 168/196/200/210/222 是怪物 **sprite 圖形資源編號**,**不是怪物名稱清單**。
> 真正的怪物名稱在 **resource 31**(壓縮,偏移表 + 5-bit 解碼),共 20+ 隻。
> 正解見 `26_MONSTERS_AND_SPRITES.md`。下表「怪物名稱」欄僅為 opendw 原始 `monsters.txt` 對該 sprite 的標註,僅供對照。

| sprite 資源編號 | monsters.txt 標註 | 備註 |
|----------|----------|------|
| 168 (0xA8) | Wolf | 野狗？(sprite 圖編號) |
| 196 (0xC4) | Spider / Rock Spiders | 岩石蜘蛛(sprite 圖編號) |
| 200 (0xC8) | Innocent Man | 人類敵人(sprite 圖編號) |
| 210 (0xD2) | Pikeman | 使用長矛的敵人(sprite 圖編號) |
| 222 (0xDE) | Fanatic / Loon | 狂熱者(sprite 圖編號) |

### 3.2 怪物字串資源

資源 31（2177 bytes，壓縮）包含怪物相關字串資料。

---

## 四、關卡資料 (levels.md)

### 4.1 關卡格式

關卡檔案結構：
1. **前 4 bytes**：載入到 gamestate [0x21-0x24]
2. **資源清單**：可變長度，讀取直到 > 0x80
   - 每個 byte & 0x7F += 0x6E 得到資源編號
3. **快取資源**：成對設置
4. **關卡標題偏移**：0x16CF

### 4.2 已知關卡

| 資源編號 | 關卡名稱 | 備註 |
|----------|----------|------|
| 71 (0x71) | Purgatory | 第一個關卡 |
| 110 | Castle wall | 城堡牆壁 |
| 111 | Sky portion | 天空部分 |
| 112 | Red clay road portion | 紅土路部分 |
| 116 | Water puddle | 水坑 |

### 4.3 Purgatory 關卡結構

前 4 bytes：0x22, 0x22, 0x1c, 0x64

資源清單（& 0x7F += 0x6E）：
- 0x00 → 0x6E
- 0x01 → 0x6F
- 0x02 → 0x70
- 0x06 → 0x74
- 0x09 → 0x77
- 0x05 → 0x73
- 0x07 → 0x75
- 0x93 → 0x101（實際資源 0x93 & 0x7F = 0x13，+ 0x6E = 0x81）

---

## 五、按鍵處理 (keypress.txt)

### 5.1 按鍵綁定

- **Q 鍵**：退出遊戲（在 game loop 中處理）
- 按鍵處理在 `engine.c:3417` 附近

### 5.2 按鍵處理流程

```
op_89 → 設定按鍵代碼
op_58 → 載入按鍵回應腳本
op_06 → 設定迴圈計數器（9）
op_12 → 儲存遊戲狀態資料（按鍵代碼）
op_0D → 載入
op_3D → 運算
op_44 → 檢查
op_49 → 迴圈（對每個角色）
```

### 5.3 遭遇戰觸發 (op_7C)

```
op_30 → 運算
op_7C (0x1E1) → 觸發遭遇
op_01 → 清除遮罩（8-bit 運算）
op_66 → 載入遊戲狀態並檢查是否為 0
op_46 → 檢查是否有符號
op_10 → 載入 gamestate
```

---

## 六、視埠系統 (viewport.md)

### 6.1 視埠規格

- 視埠是 320×200 framebuffer 中的一個 **80×136 字元**區域
- 每 row = 80 字元 = 640 pixels
- 每 column = 136 字元 = 1088 pixels
- 視埠資料儲存在 `viewport_memory`（10880 bytes = 136 × 80）

### 6.2 中文化影響

若改用 24×24 字元：
- 視埠將變為 **13×8 字元**（320÷24≈13, 200÷24≈8）
- 需要重新設計 UI 佈局
- 所有 8×8 cell 座標需要轉換

---

## 七、中斷系統 (interrupts.md)

見 `/home/anr2/tmp/longcat/opendw/doc/interrupts.md` 完整文件。

---

## 八、樣式指南 (style.md)

見 `/home/anr2/tmp/longcat/opendw/doc/style.md` 完整文件。

---

## 九、参考资料 (references.md)

見 `/home/anr2/tmp/longcat/opendw/doc/references.md` 完整文件。

---

## 十、中文化關鍵資訊

### 10.1 需要利用的關鍵資源

1. **資源 31**：怪物字串資料（2177 bytes，壓縮）
   - 應包含完整怪物名稱列表
   - 解壓後可提取所有怪物名稱

2. **資源 7**：角色資料（5632 bytes，未壓縮）
   - 用於理解角色結構
   - 驗證翻譯後的角色名稱長度

3. **資源 71**：Purgatory 關卡（5846 bytes，壓縮）
   - 包含關卡對話和腳本
   - 可提取關卡相關文字

### 10.2 腳本位置（用於提取文字）

| Section | 偏移 | 用途 |
|---------|------|------|
| 0 | 1103 | 初始腳本 |
| 0 | 237 | 未知 |
| 0 | 246 | 玩家名稱顯示 |
| 0 | 1137 | 未知 |
| 71 | 3735 | Purgatory 開始 |
| 71 | 5764 | Purgatory 腳本 |
| 22 | 68 | 未知 |
| 3 | 1706 | 遭遇戰 |

### 10.3 按鍵綁定映射

按鍵處理在 `engine.c` 中，使用以下 opcodes：
- `op_89`：設定按鍵代碼
- `op_58`：載入按鍵回應腳本
- `op_12`：儲存按鍵代碼到遊戲狀態

---

## 十一、待辦事項

- [ ] 解壓資源 31 提取完整怪物名稱
- [ ] 分析資源 7 的角色結構
- [ ] 從資源 71 提取關卡對話
- [ ] 建立完整的按鍵綁定映射表
- [ ] 分析視埠繪製邏輯
- [ ] 確認所有腳本位置的用途

---

## 十二、檔案路徑

### 原始 opendw 文件
```
/home/anr2/tmp/longcat/opendw/
├── doc/
│   ├── script.md           # 遊戲腳本格式
│   ├── resources.md        # 資源格式
│   ├── monsters.txt        # 怪物資料
│   ├── levels.md           # 關卡格式
│   ├── keypress.txt        # 按鍵處理
│   ├── viewport.md         # 視埠系統
│   ├── interrupts.md       # 中斷系統
│   ├── style.md            # 樣式指南
│   └── references.md       # 参考资料
├── src/
│   ├── engine.c            # 遊戲引擎（虛擬 CPU）
│   ├── ui.c                # UI 系統
│   ├── resource.c          # 資源管理
│   └── ...
└── data/
    └── DATA1               # 遊戲資源檔
```

### 本專案文件
```
/home/anr2/tmp/longcat/opendw_dragon_wars_cht/
├── docs/
│   ├── PLAN.md             # 中文化計畫
│   ├── ANALYSIS.md         # 反組譯分析
│   ├── TRANSLATION.md      # 翻譯對照表
│   ├── SKILL.md            # 技能記錄
│   ├── SDL2_IMPLEMENTATION.md  # SDL2 實作計畫
│   ├── DATA1_RESOURCE_INDEX.md  # DATA1 資源索引
│   └── ORIGINAL_DOCS_SUMMARY.md # 本檔案
└── src/
    └── ...
```

---

**檔案結束**
