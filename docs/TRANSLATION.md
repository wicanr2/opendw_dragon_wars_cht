# Dragon Wars 翻譯對照表

## 概述

本檔案包含從 DATA1 提取的所有遊戲文字，以及中文翻譯。
這些文字使用 5-bit 壓縮編碼，透過 `extract_string()` 函式提取。

**注意**：此檔案可在重新實作時直接嵌入程式碼，再由 DATA1 取得。

**翻譯進度**：
- 總文字串：3926 個
- 已翻譯：~150 個（3.8%）
- 目標：80%+ 覆蓋率

**資源索引**：
- Section 0x00（主選單）：241 個文字
- Section 0x03（對話）：859 個文字
- Section 0x13（對話選項）：640 個文字
- Section 0x06（戰鬥）：481 個文字
- Section 0x0D（物品描述）：228 個文字
- Section 0x15（技能名稱）：15 個文字

**重要**：
- Section 0x07 是角色資料（CHARACTER_DATA），**不是物品名稱**
- Section 0x15 只有 15 個文字，技能名稱可能在其他 section

---

## 1. 主選單 (Main Menu) — Section 0x00

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| MENU_BEGIN | Do you wish to..\n\nBegin a new game\nContinue an old game | 您希望..\n\n開始新遊戲\n繼續舊遊戲 | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_START_WARN | Starting a new game will destroy your last saved game. Do you still wish to start a new game? | 開始新遊戲會摧毀您最後儲存的遊戲。您仍然希望開始新遊戲嗎？ | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_PARTY | Current party... | 目前隊伍... | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_CREATE | Create character | 建立角色 | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_BEGIN_GAME | Begin the game | 開始遊戲 | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_DELETE | Do you wish to..\n\nDelete (name) | 您希望..\n\n刪除（名字） | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_RENAME | Rename (name) | 重新命名（名字） | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_VIEW | View (name) | 查看（名字） | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_NAME | What will (name)'s new name be? | （名字）的新名字是什麼？ | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_DELETE_WARN | You are about to delete (name). What has (name) done to deserve such a fate?? | 您即將刪除（名字）。（名字）做了什麼以至於落得如此下場？？ | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_FOREVER | (name) will be gone forever. Have mercy. | （名字）將永遠消失。請大發慈悲。 | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_BYE | Bye bye, (name). | 再見，（名字）。 | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_NAME_NEW | Name your new character. | 為您的新角色命名。 | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_GENDER | Male or Female? | 男性或女性？ | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_POINTS | You still have (N) points left to distribute, do you wish to go back and distribute them? | 您還有（N）點可以分配，您希望回去分配它們嗎？ | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_MUST_HAVE | You must have someone in the party to begin the game!! | 您的隊伍中必須有人才能開始遊戲！！ | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_LOADING | Loading... | 載入中… | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_QUIT | Do you wish to quit the game? | 您希望離開遊戲嗎？ | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_SAVE | Do you wish to save your game? | 您希望儲存遊戲嗎？ | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_SAVED | Your game is saved. | 遊戲已儲存。 | DATA1 Section 0x00 | ✅ 已翻譯 |
| MENU_PAUSED | The game is paused | 遊戲已暫停 | DATA1 Section 0x00 | ✅ 已翻譯 |

---

## 2. 狀態文字 (Status Text) — Section 0x02

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| STATUS_CHAINED | chained | 被鏈住 | DATA1 Section 0x02 | ✅ 已翻譯 |
| STATUS_POISONED | poisoned | 中毒 | DATA1 Section 0x02 | ✅ 已翻譯 |
| STATUS_STUNNED | stunned | 昏迷 | DATA1 Section 0x02 | ✅ 已翻譯 |
| STATUS_DEAD | dead | 死亡 | DATA1 Section 0x02 | ✅ 已翻譯 |
| STATUS_OUT_OF_COMMISSION | That character is out of commission. | 該角色已無法行動。 | DATA1 Section 0x02 | ✅ 已翻譯 |
| STATUS_DEAD_FULL | That character is dead. | 該角色已死亡。 | DATA1 Section 0x02 | ✅ 已翻譯 |

---

## 3. 戰鬥文字 (Combat Text) — Section 0x06

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| COMBAT_STILL_FACE | You still face | 您仍然面對 | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_GAINED_LEVEL | has gained a level! | 升級了！ | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_PARTY_ADVANCES | The party advances. | 隊伍前進。 | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_RETREATS | retreats back! | 撤退！ | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_OUT_OF_RANGE | is out of range. | 超出範圍。 | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_FAILS_DAMAGE | fails to do any damage. | 無法造成傷害。 | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_ATTACK_BLOCKED | the attack is attack is blocked! | 攻擊被擋住了！ | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_TRIED_RUN | tried to run away! | 試圖逃跑！ | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_DODGES | dodges! | 閃避！ | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_CALLS_HELP | calls for more help | 呼叫更多幫助 | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_FOR_COMBAT | for combat. | 為了戰鬥。 | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_TIME_FOR | time\s\ for | 時間\用於 | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_POINT_DAMAGE | point\s\ of damage | 點傷害 | DATA1 Section 0x06 | ✅ 已翻譯 |
| COMBAT_BLOCKS | blocks! | 擋住！ | DATA1 Section 0x06 | ✅ 已翻譯 |

---

## 4. 法術文字 (Spell Text) — Section 0x08

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| SPELL_LESSER_HEAL | Lesser Heal | 次級治療 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_MAGE_LIGHT | Mage Light | 法師之光 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_FIRE_LIGHT | Fire Light | 火焰之光 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_ELVARS_FIRE | Elvar's Fire | 艾爾瓦之火 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_CLOAK_ARCANE | Cloak Arcane | 隱匿奧秘 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_GREATER_HEALING | Greater Healing | 高級治療 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_CREATE_WALL | Create Wall | 創造牆壁 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_SOFTEN_STONE | Soften Stone | 軟化石頭 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_FIRE_STORM | Fire Storm | 火焰風暴 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_BATTLE_POWER | Battle Power | 戰鬥之力 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_MAJOR_HEALING | Major Healing | 主要治療 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_NOT_ENOUGH_POWER | doesn't have enough spell power. | 法力不足。 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_ENOUGH_POWER | have enough spell power. | 法力充足。 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_NO_SPELLS | has no spells. | 沒有法術。 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_BEYOND_HELP | is beyond our help. | 超出了我們的幫助範圍。 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_PERFECT_HEALTH | is in perfect health. | 健康狀態完美。 | DATA1 Section 0x08 | ✅ 已翻譯 |
| SPELL_HEALED | is healed. | 已治癒。 | DATA1 Section 0x08 | ✅ 已翻譯 |

---

## 5. 物品文字 (Item Text) — Section 0x05

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| ITEM_TARGET | Target... | 目標... | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_NO_USE | The item has no use here. | 該物品在這裡沒有用處。 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_CANNOT_USE | can't use the | 無法使用 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_CANNOT_CARRY | can't carry any more. | 無法攜帶更多。 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_HAS_NO_ITEMS | has no items. | 沒有物品。 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_CANNOT_TRANSFER | This item cannot be transferred. | 該物品無法轉移。 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_DISCARD | Discard the | 丟棄 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_EQUIP_WEAPON | You must equip a weapon that uses the | 您必須裝備使用該 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_NOT_WEAPON | is not a weapon! | 不是武器！ | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_CANNOT_LOAD | can't be loaded into your weapon! | 無法裝入您的武器！ | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_MUST_RELOAD | You must reload! | 您必須重新裝填！ | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_USES | uses... | 使用... | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_NEW_WEAPON | New weapon | 新武器 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_LOAD_WEAPON | Load weapon | 裝填武器 | DATA1 Section 0x05 | ✅ 已翻譯 |
| ITEM_USE_ITEM | Use item | 使用物品 | DATA1 Section 0x05 | ✅ 已翻譯 |

---

## 6. 物品名稱 (Item Names) — Section 0x06/0x0D

**注意**：
- **Section 0x07 是角色資料（CHARACTER_DATA）**，包含 512 位元組的個人記錄，**不是物品名稱**
- **物品名稱儲存於 Section 0x06（物品資料）和 Section 0x0D（物品描述）**

### 6.1 武器類型

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| 2 handed sword | 雙手劍 | DATA1 Section 0x06 | ✅ 已翻譯 |
| handed sword | 單手劍 | DATA1 Section 0x06 | ✅ 已翻譯 |
| thrown weapon | 投擲武器 | DATA1 Section 0x06 | ✅ 已翻譯 |
| bow | 弓 | DATA1 Section 0x06 | ✅ 已翻譯 |
| arrow | 箭 | DATA1 Section 0x06 | ✅ 已翻譯 |
| dagger | 匕首 | DATA1 Section 0x06 | ✅ 已翻譯 |
| mace | 錘 | DATA1 Section 0x06 | ✅ 已翻譯 |
| staff | 法杖 | DATA1 Section 0x06 | ✅ 已翻譯 |
| axe | 斧 | DATA1 Section 0x06 | ✅ 已翻譯 |
| spear | 矛 | DATA1 Section 0x06 | ✅ 已翻譯 |

### 6.2 防具類型

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| full shield | 全盾 | DATA1 Section 0x06 | ✅ 已翻譯 |
| cloth armor | 布甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| leather armor | 皮甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| cuir bouilli armor | 硬化皮革甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| brigandine armor | 布里甘丁甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| scale armor | 鱗甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| chain armor | 鎖子甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| plate and chain armor | 板甲和鎖子甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| full plate armor | 全身板甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| pair of boots | 靴子 | DATA1 Section 0x06 | ✅ 已翻譯 |
| mage gloves | 法師手套 | DATA1 Section 0x06 | ✅ 已翻譯 |
| helmet | 頭盔 | DATA1 Section 0x06 | ✅ 已翻譯 |
| gauntlets | 護手 | DATA1 Section 0x06 | ✅ 已翻譯 |

### 6.3 特殊物品

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| Armor of Light | 光明甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| Intelligence | 智力 | DATA1 Section 0x06 | ✅ 已翻譯 |

---

## 7. 技能名稱 (Skill Names) — Section 0x05/0x15

**注意**：
- **Section 0x15 只有 15 個文字**，包含部分技能名稱
- **完整技能名稱可能在其他 section**（如 Section 0x05 物品/技能相關、Section 0x0E 技能描述）

### 7.1 Lore 類技能（知識技能）

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| Arcane Lore | 奧秘知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Cave Lore | 洞穴知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Forest Lore | 森林知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Mountain Lore | 山脈知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Town Lore | 城鎮知識 | DATA1 Section 0x15 | ✅ 已翻譯 |

### 7.2 戰鬥技能

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| Fistfighting | 拳鬥 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Thrown weapons | 投擲武器 | DATA1 Section 0x05 | ✅ 已翻譯 |
| Sword | 劍術 | DATA1 Section 0x05 | ✅ 已翻譯 |
| Bow | 弓術 | DATA1 Section 0x05 | ✅ 已翻譯 |
| Shield | 盾術 | DATA1 Section 0x05 | ✅ 已翻譯 |

---

## 8. 怪物名稱 (Monster Names) — Resource 31

**來源**：`doc/monsters.txt`、資源 31（怪物字串資料）

| 英文 | 中文 | 資源編號 | 狀態 |
|------|------|----------|------|
| Wolf | 狼 | 168 (0xA8) | ✅ 已翻譯 |
| Spider | 蜘蛛 | 196 (0xC4) | ✅ 已翻譯 |
| Innocent Man | 無辜者 | 200 (0xC8) | ✅ 已翻譯 |
| Pikeman | 長矛兵 | 210 (0xD2) | ✅ 已翻譯 |
| Fanatic | 狂熱者 | 222 (0xDE) | ✅ 已翻譯 |

---

## 9. 關卡名稱 (Level Names) — 從 levels.md

**來源**：`doc/levels.md`

| 英文 | 中文 | 資源編號 | 狀態 |
|------|------|----------|------|
| Purgatory | 煉獄 | 71 (0x71) | ✅ 已翻譯 |
| Castle wall | 城堡牆壁 | 110 | ✅ 已翻譯 |
| Sky portion | 天空部分 | 111 | ✅ 已翻譯 |
| Red clay road portion | 紅土路部分 | 112 | ✅ 已翻譯 |
| Water puddle | 水坑 | 116 | ✅ 已翻譯 |

---

## 10. 對話文字 (Dialogue Text) — Section 0x03

**注意**：Section 0x03 有 859 個文字，以下只列出部分。

### 10.1 角色管理對話

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| DIALOG_WHO_ENTER | Who will enter? | 誰要進入？ | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_WHO_HEALING | Who needs healing? | 誰需要治療？ | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_WHO_LOOT | Who will get loot? | 誰來分配戰利品？ | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_WHO_UNLOCK | Who will unlock the chest? | 誰來解鎖寶箱？ | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_ASK_VOLUNTEERS | Ask for volunteers | 請求志願者 | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_LISTEN_RUMORS | Listen for rumors | 聆聽傳聞 | DATA1 Section 0x03 | ✅ 已翻譯 |

### 10.2 商店/交易對話

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| DIALOG_WELCOME | Welcome | 歡迎 | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_SAIL_FOR | Set sail for... | 啟航前往... | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_NO_GOLD | You don't have enough gold. | 您沒有足夠的金幣。 | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_DONT_BUY | Sorry, but I don't want to buy that. | 抱歉，我不想買那個。 | DATA1 Section 0x03 | ✅ 已翻譯 |

### 10.3 競技場對話

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| DIALOG_ENTER_ARENA | Do you wish to enter the arena? | 您希望進入競技場嗎？ | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_SEVERAL_GLADIATORS | Several gladiators bearing recent battle scars block your way. | 幾個帶著最近戰鬥傷痕的角鬥士擋住了您的去路。 | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_YOU_MAY_ONLY | "You may only enter once!" | "您只能進入一次！" | DATA1 Section 0x03 | ✅ 已翻譯 |
| DIALOG_COME_BACK | Come back when you are ready to face the challenge of combat! | 當您準備好面對戰鬥挑戰時再回來！ | DATA1 Section 0x03 | ✅ 已翻譯 |

---

## 11. 對話選項 (Dialogue Options) — Section 0x13

**注意**：Section 0x13 有 640 個文字，以下只列出部分。

### 11.1 行動選項

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| Fight | 戰鬥 | DATA1 Section 0x13 | ✅ 已翻譯 |
| Quickly fight | 快速戰鬥 | DATA1 Section 0x13 | ✅ 已翻譯 |
| Use item | 使用物品 | DATA1 Section 0x13 | ✅ 已翻譯 |
| New weapon | 新武器 | DATA1 Section 0x13 | ✅ 已翻譯 |
| Load weapon | 裝填武器 | DATA1 Section 0x13 | ✅ 已翻譯 |

### 11.2 攻擊選項

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| Attack style... | 攻擊風格... | DATA1 Section 0x13 | ✅ 已翻譯 |
| Attack blow | 攻擊打擊 | DATA1 Section 0x13 | ✅ 已翻譯 |
| Mighty blow | 強力打擊 | DATA1 Section 0x13 | ✅ 已翻譯 |
| Disarm enemy | 解除敵人武裝 | DATA1 Section 0x13 | ✅ 已翻譯 |

---

## 12. UI 文字 (UI Text) — 從 dragon.com 提取

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|
| UI_CONFIG_TITLE | Dragon Wars Configure Menu V1.1 | 龍之戰設定選單 V1.1 | dragon.com | ✅ 已翻譯 |
| UI_COPYRIGHT | Copyright 1989, 1990 Interplay | 版權 1989, 1990 Interplay | dragon.com | ✅ 已翻譯 |
| UI_CGA_RGB | A. CGA RGB monitor | A. CGA RGB 螢幕 | dragon.com | ✅ 已翻譯 |
| UI_CGA_COMP | B. CGA composite monitor | B. CGA 複合式螢幕 | dragon.com | ✅ 已翻譯 |
| UI_TANDY | C. Tandy 16 color | C. Tandy 16 色 | dragon.com | ✅ 已翻譯 |
| UI_EGA_VGA | D. EGA/VGA 16 color | D. EGA/VGA 16 色 | dragon.com | ✅ 已翻譯 |
| UI_MOUSE_ON | 1. Mouse On | 1. 滑鼠開啟 | dragon.com | ✅ 已翻譯 |
| UI_MOUSE_OFF | 2. Mouse Off | 2. 滑鼠關閉 | dragon.com | ✅ 已翻譯 |
| UI_SELECT_SCREEN | Select a screen format by typing its letter. | 輸入字母選擇螢幕格式。 | dragon.com | ✅ 已翻譯 |
| UI_PRESS_KEY | Press [KEY] to begin the game or press "S" to save configuration | 按 [KEY] 開始遊戲或按 "S" 儲存設定 | dragon.com | ✅ 已翻譯 |
|