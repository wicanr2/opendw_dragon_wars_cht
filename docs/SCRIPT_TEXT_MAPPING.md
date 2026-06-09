# 遊戲腳本文字映射

> **來源**：`/home/anr2/tmp/longcat/opendw/script/*.scr` 分析
> **分析日期**：2026-06-09
> **目標**：建立 script 與 DATA1 文字的對應關係

---

## 文字顯示相關 Opcodes

### 主要文字顯示 Opcode

| Opcode | 名稱 | 參數 | 說明 |
|--------|------|------|------|
| `extract_string` | 萃取字串 | 無 | 從資源萃取出 5-bit 壓縮字串並顯示 |
| `write_character_name` | 顯示角色名字 | 無 | 從當前角色資料顯示名字 |
| `load_resource` | 載入資源 | res: 0xNN, offset: 0xNNNN | 載入指定資源 |
| `ui_draw_string` | 繪製字串 | 無 | 在螢幕繪製字串 |
| `refresh_viewport` | 更新視窗 | 無 | 更新遊戲視窗 |
| `wait_event` | 等待事件 | 多個 | 等待按鍵/事件（含選單文字） |

### 資源載入格式
```
load_resource res: 0xNN, offset: 0xNNNN
```
- `res` = DATA1 section 索引
- `offset` = section 內偏移位址

---

## 各腳本使用文字資源統計

### script01.scr（主遊戲迴圈）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x0002 | res: 0x03 | 場景資源 |
| 0x0036 | res: 0x16 | **遊戲結束畫面** |
| 0x00A1 | res: 0x01 | 地圖資源 |
| 0x00C2 | res: 0x0d | **酒保傳聞** |
| 0x00C9 | res: 0x0f | **結局畫面** |

**功能**：主遊戲迴圈，處理移動、轉向、進入場景

### script03.scr（主要對話/事件）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x001F, 0x0029, 0x0045 | res: 0x12 | **道具店交易** |
| 0x0038, 0x0061 | res: 0x12, offset: 0x0097 | 道具店 |
| 0x006C, 0x00D0 | res: 0x03, offset: 0x08b6 | NPC 對話 |
| 0x00D5 | res: 0x16 | **遊戲結束** |
| 0x0137 | res: 0x03, offset: 0x014b | 對話 |
| 0x015C | extract_string | 文字顯示 |
| 0x01F2 | write_character_name | 角色名字 |
| 0x06D7 | res: 0x03, offset: 0x06e8 | 對話 |

**功能**：主要 NPC 對話、事件觸發

### script06.scr（城市/商店）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x00A6 | res: 0x06, offset: 0x00e2 | 城市事件 |
| 0x0109 | res: 0x06, offset: 0x0110 | 城市事件 |
| 0x019C, 0x01BD | res: 0x03 | 對話 |
| 0x05F3, 0x05FC, 0x0623 | res: 0x03 | 對話 |
| 0x067C, 0x06A5, 0x06D1, 0x0733 | res: 0x03 | 對話 |

**功能**：城市事件、商店交易

### script0.scr（角色管理）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x017D | res: 0x0d | **酒保傳聞** |
| 0x027F | res: 0x14 | **法術商店** |
| 0x02C7 | res: 0x14, offset: 0x000a | 法術商店 |

**功能**：角色管理、法術商店

### script11.scr（治療服務）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x0012, 0x0066, 0x007B, 0x013B | res: 0x0b, offset: 0x003f | **戰鬥系統/治療** |
| 0x00F0 | extract_string | 文字顯示 |

**功能**：治療服務、生命值恢復

### script12.scr（戰鬥遭遇）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x0190 | extract_string | 文字顯示 |
| 0x024D | extract_string | 文字顯示 |
| 0x02C9, 0x034F | res: 0x0e | **戰鬥遭遇** |

**功能**：戰鬥觸發、遭遇處理

### script13.scr（競技場）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x0088 | res: 0x03, offset: 0x08f4 | 競技場對話 |
| 0x01BD, 0x027D, 0x0284, 0x0448, 0x046E | res: 0x02 | 地圖資源 |
| 0x031A | res: 0x05, offset: 0x008b | 戰鬥對話 |

**功能**：競技場戰鬥

### script15.scr（UI 顯示）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x0327 | ui_draw_string | **繪製字串** |
| 0x0328 | write_character_name | 角色名字 |

**功能**：UI 畫面繪製

### script18.scr（遭遇事件）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x0012 | extract_string | "You have just attracted some unwanted..." |
| 0x003D | res: 0x12, offset: 0x0051 | 道具店 |
| 0x00D0 | res: 0x03, offset: 0x06b1 | 對話 |
| 0x02F3 | res: 0x0c, offset: 0x0005 | 商店交易 |

**功能**：隨機遭遇事件

### script19.scr（物品/法術描述）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x001B | res: 0x08, offset: 0x0018 | **物品描述** |
| 0x0137 | res: 0x05, offset: 0x0012 | 戰鬥對話 |
| 0x0184 | res: 0x0b, offset: 0x0006 | 戰鬥系統 |
| 0x042A, 0x0555 | res: 0x08, offset: 0x0012~0x0015 | **物品描述** |

**功能**：物品描述顯示

### script71.scr（Purgatory 關卡）
| 偏移 | 資源 | 用途 |
|------|------|------|
| 0x0E8E | res: 0x01, offset: 0x0002 | 地圖 |
| 0x0EA0 | res: 0x13, offset: 0x0006 | 法術 |
| 0x0EAB | res: 0x08, offset: 0x0018 | **物品描述** |
| 0x0FC1 | res: 0x08, offset: 0x000f | **物品描述** |
| 0x1050 | res: 0x08, offset: 0x000c | **物品描述** |
| 0x105E, 0x1115, 0x1303 | res: 0x08, offset: 0x0018 | **物品描述** |
| 0x10A2 | res: 0x05, offset: 0x000f | 戰鬥對話 |
| 0x10A9 | res: 0x0c, offset: 0x0000 | 商店交易 |
| 0x112D | res: 0x13, offset: 0x0000 | 法術 |
| 0x1140, 0x1352 | res: 0x03, offset: 0x0000 | 對話 |
| 0x11E5 | res: 0x0b, offset: 0x0000 | 戰鬥系統 |
| 0x1338 | res: 0x08, offset: 0x0015 | **物品描述** |

**功能**：Purgatory 特殊關卡

---

## 文字顯示流程圖

### 1. 對話顯示
```
script03.scr / script06.scr / script18.scr
    ↓
load_resource res: 0xNN (載入 section)
    ↓
extract_string (解壓縮 5-bit 文字)
    ↓
ui_draw_string (繪製到螢幕)
```

### 2. 角色名字顯示
```
script0.scr / script03.scr / script13.scr
    ↓
write_character_name (從角色資料讀取)
    ↓
ui_draw_string
```

### 3. 遊戲結束畫面
```
script01.scr
    ↓
load_resource res: 0x16, offset: 0x0000
    ↓
顯示 "Alas, your brave party has met its match!"
```

### 4. 物品/法術描述
```
script19.scr
    ↓
load_resource res: 0x08, offset: 0xNNNN (物品描述)
load_resource res: 0x14, offset: 0xNNNN (法術描述)
    ↓
extract_string
    ↓
ui_draw_string
```

---

## 翻譯影響分析

### 直接可翻譯的文字
- `extract_string` 呼叫 → 從 DATA1 萃取的文字
- `write_character_name` → 角色名字（需在角色資料中修改）
- `ui_draw_string` → 直接繪製的字串
- `wait_event` 選單 → 選單選項文字

### 需特殊處理的文字
- `res: 0x0d`（酒保傳聞）→ 對應中文手冊「Read Paragraph」
- `res: 0x16`（遊戲結束）→ 固定訊息，可直譯
- `res: 0x0f`（結局）→ 對應中文手冊結局段落

### 不需翻譯的資源
- `res: 0x01, 0x02, 0x04, 0x05` → 地圖/場景資源
- `res: 0x07` → 角色樣板（二進位）

---

## 待辦事項

- [ ] 建立完整的 opcode 字典
- [ ] 交叉比對所有 `extract_string` 與 `ALL_TEXT_FROM_DATA1.txt`
- [ ] 建立文字索引（每條文字在哪些 script 中被使用）
- [ ] 測試中文顯示不破壞遊戲邏輯

---

*產生日期：2026-06-09*
