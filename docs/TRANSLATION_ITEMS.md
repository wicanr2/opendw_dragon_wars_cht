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

## 7. 待確認物品

以下物品名稱可能在其他區段或以二進制格式儲存：

- 武器：Sword, Axe, Bow, Arrow, Staff, Dagger, Mace, Spear, Crossbow, Club, Wand, Blade, Halberd, Flail, Sling
- 防具：Helmet, Gauntlets, Greaves, Shield, Buckler, Helm, Visor, Pauldrons, Bracers, Belt, Cloak, Cape, Robe
- 消耗品：Potion, Scroll, Elixir, Tonic, Antidote, Remedy, Phoenix Down, Revive, Cure, Heal, Mana, Ether
- 配件：Ring, Amulet, Necklace, Bracelet, Earring, Charm, Talisman, Gem, Crystal, Stone, Rune
- 其他：Key, Map, Compass, Torch, Rope, Food, Water, Gold, Coin, Gem, Jewel, Treasure

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

## 9. 待辦事項

- [ ] 找出完整物品名稱清單（可能在其他區段）
- [ ] 確認物品類型翻譯一致性
- [ ] 建立物品 ID 對照表
- [ ] 從遊戲截圖確認物品名稱
- [ ] 從中文手冊提取物品名稱

---

## 10. 参考资料

- `docs/ALL_TEXT_FROM_DATA1.txt` Section 0x06
- `doc/resources.md`
