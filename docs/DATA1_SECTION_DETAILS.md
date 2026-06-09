# DATA1 Section 詳細分析

> **來源**：`docs/ALL_TEXT_FROM_DATA1.txt`（3,926 條文字，17 個 section）
> **分析日期**：2026-06-09
> **目標**：為每個 section 建立詳細的內容分類與結構描述

---

## Section 類型對照表

| Section | 類型 | 文字數 | 內容描述 | 萃取狀態 |
|---------|------|--------|----------|----------|
| 0x00 | SCRIPT | 241 | 遊戲邏輯 + 選單文字 | ✅ 完成 |
| 0x01 | MAP/SCENE | 4 | 場景載入觸發 | ✅ 完成 |
| 0x02 | MAP/SCENE | 20 | 場景載入觸發 | ✅ 完成 |
| 0x03 | SCRIPT | 859 | 主要遊戲對話、NPC 對話 | ✅ 完成 |
| 0x04 | MAP/SCENE | 10 | 場景載入觸發 | ✅ 完成 |
| 0x05 | SCRIPT | 62 | 戰鬥對話 | ✅ 完成 |
| 0x06 | SCRIPT | 481 | 城市/商店對話 | ✅ 完成 |
| 0x07 | CHARACTER | 34 | 角色資料 | ✅ 完成 |
| 0x08 | TEXT | 134 | 物品描述 | ✅ 完成 |
| 0x09 | TEXT | 112 | 法術描述 | ✅ 完成 |
| 0x0A | TEXT | 181 | 章節標題/結局 | ✅ 完成 |
| 0x0B | TEXT | 137 | 戰鬥系統文字 | ✅ 完成 |
| 0x0C | TEXT | 151 | 商店/道具店 | ✅ 完成 |
| 0x0D | TEXT | 228 | 酒保傳聞 | ✅ 完成 |
| 0x0E | TEXT | 59 | 戰鬥遭遇 | ✅ 完成 |
| 0x0F | TEXT | 156 | 結局/遊戲結束 | ✅ 完成 |
| 0x10 | ITEM_DATA | — | **物品二進位資料** | ⚠️ 無文字 |
| 0x11 | ITEM_TEXT | 71 | 物品名稱/治療服務 | ✅ 完成 |
| 0x12 | ITEM_SHOP | 215 | 道具店交易 | ✅ 完成 |
| 0x13 | SPELL_TEXT | 640 | 法術名稱/競技場 | ✅ 完成 |
| 0x14 | SPELL_SHOP | 86 | 法術商店 | ✅ 完成 |
| 0x15 | MONSTER_TEXT | 15 | 怪物名稱 | ✅ 完成 |
| 0x16 | GAME_OVER | 30 | 遊戲結束畫面 | ✅ 完成 |

---

## 詳細內容分類

### 選單文字（Section 0x00）
- 新遊戲/繼續遊戲
- 按鍵映射（方向鍵、功能鍵）
- 未知指令提示（"Unknown"）

### NPC 對話（Section 0x03）
- 城市居民對話
- 商店老板對話
- 任務提示
- 戰鬥中對話

### 物品描述（Section 0x08）
已辨識的物品類型：
- 武器：劍（sword）、雙手劍（2 handed sword）
- 防具：布甲（cloth armor）、皮甲（leather armor）、鎖子甲（chain armor）、板甲（full plate armor）
- 配件：靴子（pair of boots）、手套（mage gloves）、盾牌（full shield）
- 特殊物品：Armor of Light

### 法術描述（Section 0x09）
已辨識的法術：
- Elvar's Fire
- Column of Fire
- Greater Healing
- Major Healing
- Full Shield

### 章節標題（Section 0x0A）
- 遊戲章節標題
- 結局段落

### 戰鬥系統（Section 0x0B）
- 戰鬥選項（Fight、Quick Fight、Run）
- 攻擊模式（Normal Blow、Mighty Blow）
- 距離計算（40 呎法則）

### 商店交易（Section 0x0C）
- 購買/出售提示
- 金幣不足提示
- 物品選擇

### 酒保傳聞（Section 0x0D）
- 遊戲中的「Read Paragraph」場景線索
- 城市情報

### 遊戲結束（Section 0x0F, 0x16）
- "Alas, your brave party has met its match!"
- 勝利結局
- 感謝訊息（Humbaba 勝利）

---

## 物品/法術/怪物名稱清單

### 物品名稱（來自 sections 0x08, 0x11, 0x12）
```
full shield
2 handed sword
cloth armor
leather armor
cuir bouilli armor
brigandine armor
scale armor
chain armor
plate and chain armor
full plate armor
pair of boots
mage gloves
Armor of Light
```

### 法術名稱（來自 sections 0x09, 0x13, 0x14）
```
Elvar's Fire
Column of Fire
Greater Healing
Major Healing
Full healing
Partial healing
Heal
```

### 怪物名稱（來自 section 0x15）
需要進一步分析（OCR 品質影響）

---

## 待處理項目

### Section 0x10（ITEM_DATA）
- 此 section 為二進位結構資料，非文字
- 需要 `src/tools/section_dump.cpp` 來解析
- 結構猜測：
  ```
  struct item_record {
    uint16_t item_id;
    char name[20];      // null-terminated ASCII
    uint8_t type;       // 武器/防具/消耗品
    uint8_t stats[8];   // 屬性數值
  };
  ```

### Section 0x15（MONSTER_TEXT）
- 目前僅 15 條文字
- 可能需要從其他 section 補充怪物名稱

---

## 萃取工具狀態

### 可用工具
- `src/tools/resextract.cpp` — 萃取指定 section 為 binary
- `src/tools/strextract.cpp` — 文字萃取測試
- `src/tools/disasm.cpp` — 反組譯

### 待建工具
- `src/tools/section_dump.cpp` — section 結構傾印
- `src/tools/script_lint.cpp` — script 文字使用分析
- `tools/verify_extraction.py` — 萃取完整性驗證

---

*產生日期：2026-06-09*
