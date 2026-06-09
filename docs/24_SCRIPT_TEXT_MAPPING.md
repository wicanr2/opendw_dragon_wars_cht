# 遊戲腳本與 DATA1 文字對應

> **日期**：2026-06-09
> **來源**：`/home/anr2/tmp/longcat/opendw/script/*.scr`、`docs/ALL_TEXT_FROM_DATA1.txt`
> **目的**：建立遊戲腳本指令與 DATA1 區段文字的交叉對照表

---

## 腳本檔案總覽

| 檔案 | 大小 | 用途 | 載入資源 |
|------|------|----------|----------|
| `encounter.scr` | 2,511 bytes | 遭遇事件處理 | 0x03（主要對話） |
| `init.cpp` | 1,903 bytes | 初始化腳本 | 0x00（遊戲腳本） |
| `script0.scr` | 13,204 bytes | 主選單 + 遊戲初始化 | 0x03（主要對話） |
| `script01.scr` | 2,542 bytes | 主遊戲迴圈 | 0x03（主要對話） |
| `script03.scr` | 65,900 bytes | **主要遊戲邏輯** | 0x12、0x13、0x03、0x08 |
| `script06.scr` | 21,324 bytes | 法术 / 戰鬥系統 | 0x03、0x06 |
| `script11.scr` | 5,965 bytes | 角色管理（經驗/升級） | 0x0B、0x03 |
| `script12.scr` | 8,956 bytes | 物品使用/裝備 | 0x03、0x06 |
| `script13.scr` | 15,291 bytes | 角色狀態畫面 | 0x03、0x14 |
| `script15.scr` | 8,174 bytes | 法術系統 | 0x03、0x06 |
| `script18.scr` | 10,130 bytes | 隨機遭遇 | 0x03、0x11 |
| `script19.scr` | 5,431 bytes | 競技場 | 0x03、0x08 |
| `script22.scr` | 815 bytes | 死亡處理 | 0x16 |
| `script71.scr` | 10,982 bytes | 世界地圖移動 | 0x01 |

---

## 主要對話資源（Section 0x03）

### 位置
- **偏移**：`0x099C`
- **大小**：5,390 bytes
- **文字數**：859 條

### 用途
幾乎所有遊戲內文字都集中在這裡：
- 主選單（開始新遊戲、繼續遊戲）
- 戰鬥訊息（攻擊、傷害、升級）
- 法術文字（法術名稱、法力不足）
- NPC 對話（酒保、商店）
- 角色管理（刪除角色、改名）

### 腳本引用
| 指令 | 用途 | 範例 |
|------|------|------|
| `load_resource res: 0x03, offset: XXXX` | 載入區段 0x03 內文字 | `set_msg` 顯示 |
| `set_msg $("...")` | 顯示文字（含解碼） | 主選單、戰鬥訊息 |
| `draw_and_set` | 繪製 UI + 顯示文字 | 對話框 |
| `wait_event` | 等待玩家輸入 | 選項按鈕 |

### 關鍵文字片段（依遊戲流程）

#### 主選單（script0.scr）
```
"Do you wish to..\r\rBegin a new game\rContinue an old game"
"Starting a new game will destroy your last saved game..."
"Current party..."
```

#### 戰鬥系統（script03.scr、script06.scr）
```
" doesn't have enough spell power."
" gets the "
"'s status."
"'s statistics.\rStr:"
```

#### 法術系統（script15.scr）
```
 magic\r    （德魯伊/太陽/雜項法術）
```

#### 角色畫面（script13.scr）
```
"View...\r\r"
"General overview\r"
"Abilities\r"
"Low / High / Druid / Sun / Misc"
```

---

## 物品/技能資源（Section 0x06）

### 位置
- **偏移**：`0x209C`
- **大小**：3,453 bytes
- **文字數**：481 條

### 用途
物品名稱、武器/防具類型、技能需求。

### 關鍵文字
```
"2 handed sword"
"cloth armor / leather armor / chain armor / full plate armor"
"Intelligence"
"tries to learn"
"Armor of Light"
```

### 腳本引用
- `script12.scr`：物品使用/裝備時載入
- `script06.scr`：法術使用時載入

---

## 物品交易資源（Section 0x0B）

### 位置
- **偏移**：`0x5004`
- **大小**：877 bytes
- **文字數**：137 條

### 用途
商店交易相關文字。

### 關鍵文字
```
"can't carry any more"
"Who will get loot?"
"Which item..."
```

### 腳本引用
- `script11.scr`：`load_resource res: 0x0b, offset: XXXX`

---

## 戰鬥選單資源（Section 0x12）

### 位置
- **偏移**：`0x87A8`
- **大小**：1,375 bytes
- **文字數**：215 條

### 用途
戰鬥中的選單選項。

### 關鍵文字
```
"Will the party:\r\rFight\rQuickly fight"
"Advance ahead"
```

### 腳本引用
- `script03.scr`：`load_resource res: 0x12, offset: XXXX`

---

## 對話選項資源（Section 0x13）

### 位置
- **偏移**：`0x8D07`
- **大小**：1,394 bytes
- **文字數**：640 條

### 用途
對話系統中的選項按鈕文字。

### 關鍵文字
```
"Do you wish to enter the arena?"
"Come back when you are ready to face the challenge of combat!"
"Several gladiators bearing recent battle scars..."
```

### 腳本引用
- `script19.scr`：競技場對話
- `script03.scr`：一般對話

---

## 商店/交易資源（Section 0x14）

### 位置
- **偏移**：`0x9279`
- **大小**：903 bytes
- **文字數**：86 條

### 用途
商店交易、購買物品。

### 關鍵文字
```
"Skill     Amount Cost"
```

### 腳本引用
- `script13.scr`：角色畫面中顯示商店資訊

---

## 技能名稱資源（Section 0x15）

### 位置
- **偏移**：`0x9600`
- **大小**：257 bytes
- **文字數**：15 條

### 用途
技能名稱列表。

### 關鍵文字
```
"Mountain Lore"
"Fistfighting"
"Thrown weapons"
"Arcane Lore / Cave Lore / Forest Lore / Town Lore"
```

### 腳本引用
- `script15.scr`：法術系統

---

## 死亡訊息資源（Section 0x16）

### 位置
- **偏移**：`0x9701`
- **大小**：78 bytes
- **文字數**：30 條

### 用途
隊伍滅亡時的訊息。

### 關鍵文字
```
"Alas, your brave party has met its match! Your current adventure is over."
```

### 腳本引用
- `script22.scr`：`load_resource res: 0x16, offset: 0x0000`

---

## 治療/復活資源（Section 0x11）

### 位置
- **偏移**：`0x8642`
- **大小**：358 bytes
- **文字數**：71 條

### 用途
治療師對話。

### 關鍵文字
```
"Who needs healing?"
"I'm sorry but"
"is beyond our help."
"is in perfect health."
"What service would you like performed on"
"Full healing / Partial healing"
"How much healing do you wish?"
"That will cost ... in gold, pay?"
```

### 腳本引用
- `script18.scr`：`load_resource res: 0x0b, offset: XXXX`（商店）
- `script03.scr`：治療場景

---

## 法術系統資源（Section 0x06 + Section 0x15）

### 法術名稱（Section 0x15）
- "Druid magic"
- "Sun magic"
- "Misc magic"

### 法術效果（Section 0x06）
- 各法術的消耗/效果描述

### 腳本引用
- `script15.scr`：法術系統邏輯
- `script06.scr`：戰鬥中施法

---

## 競技場資源（Section 0x08 + Section 0x03）

### 位置
- Section 0x08：偏移 `0x4419`，767 bytes，134 條文字
- Section 0x03：競技場相關對話

### 關鍵文字（Section 0x03）
```
"Do you wish to enter the arena?"
"Come back when you are ready to face the challenge of combat!"
"Several gladiators bearing recent battle scars..."
```

### 腳本引用
- `script19.scr`：競技場邏輯

---

## 資源載入模式

### 典型模式
```asm
# 載入區段資源
load_resource res: 0x03, offset: 0x0000

# 顯示解碼文字
set_msg $("...")

# 等待玩家選擇
wait_event ..., 'Y', 0x..., 'N', 0x..., 0xff
```

### 解碼方式
1. `load_resource` 載入指定區段
2. `extract_string` 使用 5-bit 字母解碼
3. `set_msg` 顯示解碼後的文字
4. 特殊字元：`\r` = 換行，`\r\r` = 空行

### 跳轉結構
```asm
# 根據玩家選擇跳轉
wait_event 0x0000, 'B', 0x0045, 'C', 0x02df, 0xff
# 'B' -> 0x0045 (Begin new game)
# 'C' -> 0x02df (Continue)
# 0xff -> 結束
```

---

## 中文化重點

### 高優先（P0）
- **區段 0x03**：幾乎所有可見文字
- **區段 0x16**：死亡訊息（简短，易處理）
- **區段 0x15**：技能名稱（15 條，短）

### 中優先（P1）
- **區段 0x11**：治療對話
- **區段 0x12**：戰鬥選單
- **區段 0x13**：對話選項
- **區段 0x0B**：商店交易

### 低優先（P2）
- **區段 0x06**：物品/技能（481 條，量大）
- **區段 0x14**：商店表格
- **區段 0x08**：競技場

### 技術挑戰
1. **指標區段**（0x00–0x0F）：可直接修改
2. **壓縮區段**（0x1D–0xFF）：需要重新壓縮
3. **指標表**：每個區段有 2-byte 索引，需維持正確性
4. **字元編碼**：需處理 5-bit 字母表 + 大小寫 escape codes

---

## 區段大小限制

| 區段 | 目前大小 | 最大可用 | 狀態 |
|------|----------|----------|------|
| 0x03 | 5,390 | 65,535 | 可擴充 |
| 0x06 | 3,453 | 65,535 | 可擴充 |
| 0x0B | 877 | 65,535 | 可擴充 |
| 0x11 | 358 | 65,535 | 可擴充 |
| 0x12 | 1,375 | 65,535 | 可擴充 |
| 0x13 | 1,394 | 65,535 | 可擴充 |
| 0x14 | 903 | 65,535 | 可擴充 |
| 0x15 | 257 | 65,535 | 可擴充 |
| 0x16 | 78 | 65,535 | 可擴充 |

### 注意
- 區段 0x10（8,192 bytes 全 0xFF）可作為擴充緩衝區
- 區段 0x18–0x1C（0xFFFE = 空）也可重新配置

---

*檔案產生日期：2026-06-09*
*工具：script/*.scr, docs/ALL_TEXT_FROM_DATA1.txt*
