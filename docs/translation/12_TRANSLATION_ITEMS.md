# 物品翻譯對照表

> ⚠️ **已執行 D4(2026-06-10):虛構物品區已刪除**。原 §7.3 消耗品(potion/elixir/phoenix down…)、§7.4 配件(ring/amulet/necklace…)、
> §7.5 特殊物品的臆測項、§8「待確認物品」、以及 §9 物品 ID 表中對應的消耗品/配件/任務物品條目,
> 均為 LLM 依 RPG 慣例臆測,Dragon Wars(1989)未驗證,已整區移除。
> 本檔現只保留 **與 script/res 對得上的武器與防具**(以 `ALL_TEXT_FROM_SCRIPTS.txt` 為準),以及真實出現的物品相關訊息字串。
>
> **物品完整表待重建**:比照怪物從 res31 重建的做法,物品完整清單應從萃取資料(資源記錄 + script bytecode 內嵌字串)重建,而非臆測。

## 概述

本檔案包含從 DATA1 / script 提取的物品名稱與類型,以及中文翻譯。

**資料來源**：
- `ALL_TEXT_FROM_SCRIPTS.txt`(乾淨的內嵌 script 字串)
- res 解碼結果(資源記錄)

---

## 1. 武器類型(script 已驗證)

| 英文 | 中文 | 備註 |
|------|------|------|
| 2 handed sword | 雙手劍 | 需要雙手裝備的武器 |
| handed sword | 單手劍 | 可單手裝備的武器 |
| thrown weapon | 投擲武器 | 可投擲的武器 |
| full shield | 全盾 | 大型盾牌 |
| bow | 弓 | 遠程武器,需要箭矢 |
| arrow | 箭 | 弓的彈藥 |
| dagger | 匕首 | 輕型武器 |
| mace | 錘 | 鈍器 |
| staff | 法杖 | 法師武器 |
| axe | 斧 | 可單手或雙手使用 |
| spear | 長矛 | 長柄武器 |

---

## 2. 防具類型(script 已驗證)

| 英文 | 中文 | 備註 |
|------|------|------|
| cloth armor | 布甲 | 最基礎的防具 |
| leather armor | 皮甲 | 皮革製成的防具 |
| cuir bouilli armor | 硬化皮革甲 | 經過硬化的皮革防具(cuir bouilli 為法語「硬化皮革」) |
| brigandine armor | 布里甘丁甲 | 用小金屬片加固的防具 |
| scale armor | 鱗甲 | 用金屬鱗片製成的防具 |
| chain armor | 鎖子甲 | 用金屬環編織的防具 |
| plate and chain armor | 板甲和鎖子甲 | 混合型防具 |
| full plate armor | 全身板甲 | 最高級的防具 |
| pair of boots | 靴子 | 足部防具 |
| mage gloves | 法師手套 | 法師專用手套 |
| gauntlets | 護手 | 金屬製手套 |
| helmet | 頭盔 | 保護頭部 |

---

## 3. 特殊物品(script 已驗證)

| 英文 | 中文 | 備註 |
|------|------|------|
| Armor of Light | 光明甲 | 遊戲中出現的特殊名稱物品(script 內出現) |

> 註:Intelligence(智力)為物品需求屬性,非物品本身,見 `../CONTEXT.md` 屬性表。

---

## 4. 物品相關訊息文字(script 已驗證)

| 英文 | 中文 | 備註 |
|------|------|------|
| The item has no use here. | 該物品在這裡沒有用處。 | 使用失敗訊息 |
| Nothing happens. | 沒有任何事發生。 | 使用失敗訊息 |
| That didn't do any good. | 那沒有用處。 | 使用失敗訊息 |
| can't carry any more. | 無法攜帶更多。 | 物品欄已滿 |
| Who will get loot? | 誰來分配戰利品？ | 戰利品分配 |
| Which item... | 哪個物品… | 物品選擇 |
| You would have to have a | 您必須擁有 | 需求提示 |
| general overview | 一般概述 | 物品概述 |
| tries to learn | 嘗試學習 | 學習技能訊息 |

> 譯名須對齊 `../CONTEXT.md`(item=物品、weapon=武器、loot=戰利品、Discard=丟棄、Trade=交易、equip=裝備、deequip/unequip=卸下、reload/Load weapon=裝填)。

---

## 5. 物品 ID 對照表(僅含已驗證武器/防具)

| 物品 ID | 英文 | 中文 | 類型 |
|---------|------|------|------|
| ITEM_SWORD | handed sword | 單手劍 | 武器 |
| ITEM_2H_SWORD | 2 handed sword | 雙手劍 | 武器 |
| ITEM_BOW | bow | 弓 | 武器 |
| ITEM_ARROW | arrow | 箭 | 彈藥 |
| ITEM_DAGGER | dagger | 匕首 | 武器 |
| ITEM_MACE | mace | 錘 | 武器 |
| ITEM_STAFF | staff | 法杖 | 武器 |
| ITEM_AXE | axe | 斧 | 武器 |
| ITEM_SPEAR | spear | 長矛 | 武器 |
| ITEM_THROWN | thrown weapon | 投擲武器 | 武器 |
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
| ITEM_GAUNTLETS | gauntlets | 護手 | 防具 |
| ITEM_HELMET | helmet | 頭盔 | 防具 |
| ITEM_FULL_SHIELD | full shield | 全盾 | 防具 |
| ITEM_ARMOR_OF_LIGHT | Armor of Light | 光明甲 | 特殊防具 |

---

## 6. 待辦事項

- [ ] 從 res 資源記錄 + script bytecode 內嵌字串重建**完整**物品清單(比照怪物從 res31 重建)
- [ ] 確認物品類型翻譯一致性,對齊 `../CONTEXT.md`
- [ ] 與 `10_TRANSLATION.md` 中的物品名稱保持同步

---

## 7. 參考資料

- `docs/ALL_TEXT_FROM_SCRIPTS.txt`(乾淨 script 字串)
- `docs/26_MONSTERS_AND_SPRITES.md`(res 解碼重建的範例方法)
- `../CONTEXT.md`(術語/譯名標準)
