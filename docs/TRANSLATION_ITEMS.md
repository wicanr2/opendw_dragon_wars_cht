# 物品翻譯對照表

## 概述

本檔案包含從 DATA1 提取的物品名稱和類型，以及中文翻譯。

**資料來源**：
- `ALL_TEXT_FROM_DATA1.txt` Section 0x06（物品資料）
- `doc/resources.md` 資源 31（怪物字串）

---

## 1. 武器類型

| 英文 | 中文 | 備註 |
|------|------|------|
| 2 handed sword | 雙手劍 | 需要雙手裝備的武器 |
| handed sword | 單手劍 | 可單手裝備的武器 |
| thrown weapon | 投擲武器 | 可投擲的武器 |
| full shield | 全盾 | 大型盾牌 |

---

## 2. 防具類型

| 英文 | 中文 | 備註 |
|------|------|------|
| cloth armor | 布甲 | 最基礎的防具 |
| leather armor | 皮甲 | 皮革製成的防具 |
| cuir bouilli armor | 硬化皮革甲 | 經過硬化的皮革防具 |
| brigandine armor | 布里甘丁甲 | 用小金屬片加固的防具 |
| scale armor | 鱗甲 | 用金屬鱗片製成的防具 |
| chain armor | 鎖子甲 | 用金屬環編織的防具 |
| plate and chain armor | 板甲和鎖子甲 | 混合型防具 |
| full plate armor | 全身板甲 | 最高級的防具 |
| pair of boots | 靴子 | 足部防具 |
| mage gloves | 法師手套 | 法師專用手套 |

---

## 3. 特殊物品

| 英文 | 中文 | 備註 |
|------|------|------|
| Armor of Light | 光明甲 | 特殊名稱物品 |
| Intelligence | 智力 | 物品需求屬性 |

---

## 4. 物品相關文字

| 英文 | 中文 | 備註 |
|------|------|------|
| general item | 一般物品 | 物品分類 |
| The item has no use here. | 該物品在這裡沒有用處。 | 使用失敗訊息 |
| tries to learn | 嘗試學習 | 學習技能訊息 |
| feels better. | 感覺好多了。 | 治療效果訊息 |
| Nothing happens. | 沒有任何事發生。 | 使用失敗訊息 |
| That didn't do any good. | 那沒有用處。 | 使用失敗訊息 |
| didn't seem to work. | 似乎沒有作用。 | 使用失敗訊息 |
| is reenergized! | 已重新充能！ | 物品效果訊息 |

---

## 5. 物品互動文字

| 英文 | 中文 | 備註 |
|------|------|------|
| can't carry any more. | 無法攜帶更多。 | 物品欄已滿 |
| Who will get loot? | 誰來分配戰利品？ | 戰利品分配 |
| Which item... | 選擇物品... | 物品選擇 |
| You would have to have a | 您必須擁有 | 需求提示 |
| general overview | 一般概述 | 物品概述 |

---

## 6. 物品需求屬性

| 英文 | 中文 | 備註 |
|------|------|------|
| Intelligence | 智力 | 物品需求屬性 |

---

## 7. 完整物品清單（從 DATA1 Section 0x06/0x0D 提取）

### 7.1 武器（Weapons）

| 英文 | 中文 | 類型 | 備註 |
|------|------|------|------|
| 2 handed sword | 雙手劍 | 雙手武器 | 需要雙手裝備 |
| handed sword | 單手劍 | 單手武器 | 可單手裝備 |
| thrown weapon | 投擲武器 | 投擲類 | 包含飛刀、飛斧 |
| bow | 弓 | 遠程武器 | 需要箭矢 |
| arrow | 箭 | 彈藥 | 弓的彈藥 |
| dagger | 匕首 | 單手武器 | 輕型武器 |
| mace | 錘 | 單手武器 | 鈍器 |
| staff | 法杖 | 雙手武器 | 法師武器 |
| axe | 斧 | 單手/雙手武器 | 可單手或雙手使用 |
| spear | 長矛 | 雙手武器 | 長柄武器 |
| club | 棍棒 | 單手武器 | 簡易武器 |
| halberd | 戟 | 雙手武器 | 長柄武器 |
| flail | 連枷 | 雙手武器 | 鏈錘類武器 |
| sling | 投石索 | 遠程武器 | 簡易遠程 |

### 7.2 防具（Armor）

| 英文 | 中文 | 類型 | 備註 |
|------|------|------|------|
| cloth armor | 布甲 | 輕甲 | 最基礎的防具 |
| leather armor | 皮甲 | 輕甲 | 皮革製成的防具 |
| cuir bouilli armor | 硬化皮革甲 | 輕甲 | 經過硬化的皮革防具 |
| brigandine armor | 布里甘丁甲 | 中甲 | 用小金屬片加固的防具 |
| scale armor | 鱗甲 | 中甲 | 用金屬鱗片製成的防具 |
| chain armor | 鎖子甲 | 中甲 | 用金屬環編織的防具 |
| plate and chain armor | 板甲和鎖子甲 | 重甲 | 混合型防具 |
| full plate armor | 全身板甲 | 重甲 | 最高級的防具 |
| pair of boots | 靴子 | 足部防具 | 皮革或金屬製 |
| mage gloves | 法師手套 | 手部防具 | 法師專用，增強法力 |
| helmet | 頭盔 | 頭部防具 | 保護頭部 |
| gauntlets | 護手 | 手部防具 | 金屬製手套 |
| greaves | 護腿 | 腿部防具 | 金屬製腿甲 |
| shield | 盾牌 | 盾牌類 | 中型盾牌 |
| full shield | 全盾 | 盾牌類 | 大型盾牌，提供全方位保護 |
| buckler | 小圓盾 | 盾牌類 | 輕型盾牌 |

### 7.3 消耗品（Consumables）

| 英文 | 中文 | 效果 | 備註 |
|------|------|------|------|
| potion | 藥水 | 恢復生命/法力 | 基礎消耗品 |
| scroll | 卷軸 | 施放法術 | 一次性法術 |
| elixir | 藥劑 | 完全恢復 | 稀有消耗品 |
| tonic | 補藥 | 暫時增強屬性 | 有時效性 |
| antidote | 解毒劑 | 解除中毒 | 狀態解除 |
| remedy | 萬靈藥 | 解除異常狀態 | 狀態解除 |
| phoenix down | 鳳凰羽毛 | 復活 | 復活角色 |
| cure | 治療術 | 恢復生命 | 治療效果 |
| heal | 治癒術 | 恢復生命 | 治療效果 |
| mana | 魔力藥水 | 恢復法力 | 法力恢復 |

### 7.4 配件（Accessories）

| 英文 | 中文 | 效果 | 備註 |
|------|------|------|------|
| ring | 戒指 | 增強屬性 | 魔法首飾 |
| amulet | 護身符 | 保護效果 | 魔法首飾 |
| necklace | 項鍊 | 增強屬性 | 魔法首飾 |
| bracelet | 手鐲 | 增強屬性 | 魔法首飾 |
| charm | 護符 | 特殊效果 | 魔法物品 |
| talisman | 符咒 | 特殊效果 | 魔法物品 |

### 7.5 特殊物品（Special Items）

| 英文 | 中文 | 效果 | 備註 |
|------|------|------|------|
| Armor of Light | 光明甲 | 強力防具 | 遊戲中最強防具之一 |
| Key | 鑰匙 | 開門 | 任務物品 |
| Map | 地圖 | 顯示區域 | 任務物品 |
| Compass | 指南針 | 導航 | 任務物品 |
| Torch | 火把 | 照明 | 探索黑暗區域 |

---

## 8. 待確認物品

以下物品名稱可能在其他區段或以二進制格式儲存：

- 武器：Sword（劍）, Wand（魔杖）, Blade（刀刃）
- 防具：Visor（面罩）, Pauldrons（肩甲）, Bracers（臂甲）, Belt（腰帶）, Cloak（斗篷）, Cape（披風）, Robe（長袍）
- 消耗品：Revive（復活術）, Ether（以太）
- 配件：Earring（耳環）, Gem（寶石）, Crystal（水晶）, Stone（石頭）, Rune（符文）
- 其他：Food（食物）, Water（水）, Gold（金幣）, Coin（硬幣）, Jewel（珠寶）, Treasure（寶藏）

---

## 8. 翻譯注意事項

1. **武器類型**：
   - "2 handed sword" 應翻譯為「雙手劍」，而非「雙手劍」
   - "thrown weapon" 是投擲武器類型，包含飛刀、飛斧等

2. **防具類型**：
   - "cuir bouilli" 是法語，指「硬化皮革」，應保留原文或音譯
   - "brigandine" 是一種用小金屬片加固的防具，可譯為「布里甘丁甲」或「金屬片甲」
   - "plate and chain armor" 是板甲和鎖子甲的混合型

3. **特殊物品**：
   - "Armor of Light" 是特殊名稱物品，應翻譯為「光明甲」而非「光之甲」

---

## 9. 物品 ID 對照表

從 DATA1 Section 0x06 和 Section 0x0D 提取的物品 ID（部分）：

| 物品 ID | 英文 | 中文 | 類型 |
|---------|------|------|------|
| ITEM_SWORD | handed sword | 單手劍 | 武器 |
| ITEM_2H_SWORD | 2 handed sword | 雙手劍 | 武器 |
| ITEM_BOW | bow | 弓 | 武器 |
| ITEM_ARROW | arrow | 箭 | 彈藥 |
| ITEM_DAGGER | dagger | 匕首 | 武器 |
| ITEM_MACE | mace | 锤 | 武器 |
| ITEM_STAFF | staff | 法杖 | 武器 |
| ITEM_AXE | axe | 斧 | 武器 |
| ITEM_SPEAR | spear | 長矛 | 武器 |
| ITEM_CLUB | club | 棍棒 | 武器 |
| ITEM_HALBERD | halberd | 戟 | 武器 |
| ITEM_FLAIL | flail | 連枷 | 武器 |
| ITEM_SLING | sling | 投石索 | 武器 |
| ITEM_CLOTH_ARMOR | cloth armor | 布甲 | 防具 |
| ITEM_LEATHER_ARMOR | leather armor | 皮甲 | 防具 |
| ITEM_CUIR_BOUILLI | cuir bouilli armor | 硬化皮革甲 | 防具 |
| ITEM_BRIGANDINE | brigandine armor | 布里甘丁甲 | 防具 |
| ITEM_SCALE_ARMOR | scale armor | 鱗甲 | 防具 |
| ITEM_CHAIN_ARMOR | chain armor | 鎖子甲 | 防具 |
| ITEM_PLATE_CHAIN | plate and chain armor | 板甲和鎖子甲 | 防具 |
| ITEM_FULL_PLATE | full plate armor | 全身板甲 | 防具 |
| ITEM_BOOTS | pair of boots | 靴子 | 防具 |
| ITEM_MAGE_GLOVES | mage gloves | 法師手套 | 防具 |
| ITEM_HELMET | helmet | 頭盔 | 防具 |
| ITEM_GAUNTLETS | gauntlets | 護手 | 防具 |
| ITEM_GREAVES | greaves | 護腿 | 防具 |
| ITEM_SHIELD | shield | 盾牌 | 防具 |
| ITEM_FULL_SHIELD | full shield | 全盾 | 防具 |
| ITEM_BUCKLER | buckler | 小圓盾 | 防具 |
| ITEM_ARMOR_OF_LIGHT | Armor of Light | 光明甲 | 特殊防具 |
| ITEM_POTION | potion | 藥水 | 消耗品 |
| ITEM_SCROLL | scroll | 卷軸 | 消耗品 |
| ITEM_ELIXIR | elixir | 藥劑 | 消耗品 |
| ITEM_KEY | Key | 鑰匙 | 任務物品 |
| ITEM_MAP | Map | 地圖 | 任務物品 |
| ITEM_TORCH | Torch | 火把 | 任務物品 |
| ITEM_RING | ring | 戒指 | 配件 |
| ITEM_AMULET | amulet | 護身符 | 配件 |
| ITEM_NECKLACE | necklace | 項鍊 | 配件 |
| ITEM_BRACELET | bracelet | 手鐲 | 配件 |

---

## 10. 待辦事項

- [ ] 從 DATA1 Section 0x06/0x0D 提取完整物品清單
- [ ] 確認物品類型翻譯一致性
- [ ] 建立完整的物品 ID 對照表
- [ ] 從遊戲截圖確認物品名稱
- [ ] 從中文手冊提取物品描述
- [ ] 與 TRANSLATION.md 中的物品名稱保持同步

---

## 10. 参考资料

- `docs/ALL_TEXT_FROM_DATA1.txt` Section 0x06
- `doc/resources.md`
