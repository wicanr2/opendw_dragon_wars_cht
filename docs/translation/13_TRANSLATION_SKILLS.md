# 技能翻譯對照表

> ℹ️ **註(2026-06-10)**:資料來源標示的「Section 0x15 = 技能資料」並不準確(0x15 的「文字數」來自暴力萃取雜訊)。
> 技能名稱實際隨角色/物品資料以 5-bit 編碼儲存。本檔的 Lore 類技能(Arcane/Cave/Forest/Mountain/Town Lore)+
> Fistfighting / Thrown weapons 名稱可信;§3「技能描述」多為推測。譯名以 `../CONTEXT.md` 為準。

## 概述

本檔案包含從 DATA1 提取的技能名稱和描述，以及中文翻譯。

**資料來源**：
- `ALL_TEXT_FROM_DATA1.txt` Section 0x15（技能資料）
- `doc/resources.md`

---

## 1. 技能名稱

### 1.1 Lore 類技能（知識技能）

| 英文 | 中文 | 備註 |
|------|------|------|
| Arcane Lore | 奧秘知識 | 魔法相關知識 |
| Cave Lore | 洞穴知識 | 洞穴探索知識 |
| Forest Lore | 森林知識 | 森林探索知識 |
| Mountain Lore | 山脈知識 | 山脈探索知識 |
| Town Lore | 城鎮知識 | 城鎮探索知識 |

### 1.2 戰鬥技能

| 英文 | 中文 | 備註 |
|------|------|------|
| Fistfighting | 拳鬥 | 近戰格斗技能 |

### 1.3 投擲技能

| 英文 | 中文 | 備註 |
|------|------|------|
| Thrown weapons | 投擲武器 | 投擲類武器技能 |

---

## 2. 技能相關文字

| 英文 | 中文 | 備註 |
|------|------|------|
| tries to learn | 嘗試學習 | 學習技能訊息 |
| cannot learn | 無法學習 | 學習失敗訊息 |
| Skill Amount Cost | 技能 數量 費用 | 技能學習介面 |
| Amount Cost | 數量 費用 | 技能學習介面 |

---

## 3. 技能描述（待確認）

以下技能描述可能在其他區段或以二進制格式儲存：

### 3.1 法術技能
- Mage Light（法師之光）
- Fire Light（火焰之光）
- Elvar's Fire（艾爾瓦之火）
- Cloak Arcane（隱匿奧秘）
- Create Wall（創造牆壁）
- Soften Stone（軟化石頭）
- Fire Storm（火焰風暴）
- Battle Power（戰鬥之力）
- Column of Fire（火柱）
- Major Healing（主要治療）
- Greater Healing（高級治療）
- Lesser Heal（次級治療）

### 3.2 戰鬥技能
- Fighting（戰鬥）
- Attack style（攻擊風格）
- Mighty blow（強力打擊）
- Disarm enemy（解除敵人武裝）

---

## 4. 技能類型說明

### 4.1 Lore 類技能
Lore 類技能提供環境知識，影響探索和發現：
- **Arcane Lore**：魔法相關知識，影響魔法物品和法術的發現
- **Cave Lore**：洞穴探索知識，影響地下區域的探索
- **Forest Lore**：森林探索知識，影響森林區域的探索
- **Mountain Lore**：山脈探索知識，影響山脈區域的探索
- **Town Lore**：城鎮探索知識，影響城鎮區域的探索

### 4.2 戰鬥技能
- **Fistfighting**：近戰格斗技能，影響徒手攻擊的傷害和命中率
- **Thrown weapons**：投擲武器技能，影響投擲武器的傷害和命中率

---

## 5. 翻譯注意事項

1. **Lore 技能**：
   - "Lore" 應翻譯為「知識」，而非「學識」或「學問」
   - "Arcane" 應翻譯為「奧秘」，而非「秘法」或「魔法」

2. **戰鬥技能**：
   - "Fistfighting" 應翻譯為「拳鬥」，而非「拳擊」或「格斗」
   - "Thrown weapons" 應翻譯為「投擲武器」，而非「投擲武器技能」

3. **技能名稱格式**：
   - 技能名稱應保持一致，避免混用「知識」和「學識」
   - 技能名稱應簡潔，避免過長

---

## 6. 待辦事項

- [ ] 找出完整技能名稱清單（可能在其他區段）
- [ ] 確認技能描述翻譯一致性
- [ ] 建立技能 ID 對照表
- [ ] 從遊戲截圖確認技能名稱
- [ ] 從中文手冊提取技能名稱

---

## 7. 参考资料

- `docs/ALL_TEXT_FROM_DATA1.txt` Section 0x15
- `doc/resources.md`
- `docs/TRANSLATION.md` Section 8
