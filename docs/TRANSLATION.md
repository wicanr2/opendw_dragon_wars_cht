# Dragon Wars 翻譯對照表

## 概述

本檔案包含從 DATA1 提取的所有遊戲文字，以及中文翻譯。
這些文字使用 5-bit 壓縮編碼，透過 `extract_string()` 函式提取。

**注意**：此檔案可在重新實作時直接嵌入程式碼，無需再由 DATA1 取得。

---

## 1. 主選單 (Main Menu)

| ID | 英文 | 中文 |
|----|------|------|
| MENU_BEGIN | Do you wish to..\n\nBegin a new game\nContinue an old game | 您希望..\n\n開始新遊戲\n繼續舊遊戲 |
| MENU_START_WARN | Starting a new game will destroy your last saved game. Do you still wish to start a new game? | 開始新遊戲會摧毀您最後儲存的遊戲。您仍然希望開始新遊戲嗎？ |
| MENU_PARTY | Current party... | 目前隊伍... |
| MENU_CREATE | Create character | 建立角色 |
| MENU_BEGIN_GAME | Begin the game | 開始遊戲 |
| MENU_DELETE | Do you wish to..\n\nDelete (name) | 您希望..\n\n刪除（名字） |
| MENU_RENAME | Rename (name) | 重新命名（名字） |
| MENU_VIEW | View (name) | 查看（名字） |
| MENU_NAME | What will (name)'s new name be? | （名字）的新名字是什麼？ |
| MENU_DELETE_WARN | You are about to delete (name). What has (name) done to deserve such a fate?? | 您即將刪除（名字）。（名字）做了什麼以至於落得如此下場？？ |
| MENU_FOREVER | (name) will be gone forever. Have mercy. | （名字）將永遠消失。請大發慈悲。 |
| MENU_BYE | Bye bye, (name). | 再見，（名字）。 |
| MENU_NAME_NEW | Name your new character. | 為您的新角色命名。 |
| MENU_GENDER | Male or Female? | 男性或女性？ |
| MENU_POINTS | You still have (N) points left to distribute, do you wish to go back and distribute them? | 您還有（N）點可以分配，您希望回去分配它們嗎？ |
| MENU_MUST_HAVE | You must have someone in the party to begin the game!! | 您的隊伍中必須有人才能開始遊戲！！ |
| MENU_LOADING | Loading... | 載入中… |
| MENU_QUIT | Do you wish to quit the game? | 您希望離開遊戲嗎？ |
| MENU_SAVE | Do you wish to save your game? | 您希望儲存遊戲嗎？ |
| MENU_SAVED | Your game is saved. | 遊戲已儲存。 |
| MENU_PAUSED | The game is paused | 遊戲已暫停 |

---

## 2. 狀態文字 (Status Text)

| ID | 英文 | 中文 |
|----|------|------|
| STATUS_CHAINED | chained | 被鏈住 |
| STATUS_POISONED | poisoned | 中毒 |
| STATUS_STUNNED | stunned | 昏迷 |
| STATUS_DEAD | dead | 死亡 |
| STATUS_OUT_OF_COMMISSION | That character is out of commission. | 該角色已無法行動。 |
| STATUS_DEAD | That character is dead. | 該角色已死亡。 |

---

## 3. 戰鬥文字 (Combat Text)

| ID | 英文 | 中文 |
|----|------|------|
| COMBAT_STILL_FACE | You still face | 您仍然面對 |
| COMBAT_GAINED_LEVEL | has gained a level! | 升級了！ |
| COMBAT_PARTY_ADVANCES | The party advances. | 隊伍前進。 |
| COMBAT_RETREATS | retreats back! | 撤退！ |
| COMBAT_OUT_OF_RANGE | is out of range. | 超出範圍。 |
| COMBAT_FAILS_DAMAGE | fails to do any damage. | 無法造成傷害。 |
| COMBAT_ATTACK_BLOCKED | the attack is attack is blocked! | 攻擊被擋住了！ |
| COMBAT_TRIED_RUN | tried to run away! | 試圖逃跑！ |
| COMBAT_DODGES | dodges! | 閃避！ |
| COMBAT_CALLS_HELP | calls for more help | 呼叫更多幫助 |
| COMBAT_FOR_COMBAT | for combat. | 為了戰鬥。 |
| COMBAT_TIME_FOR | time\s\ for | 時間\用於 |
| COMBAT_POINT_DAMAGE | point\s\ of damage | 點傷害 |
| COMBAT_BLOCKS | blocks! | 擋住！ |

---

## 4. 法術文字 (Spell Text)

| ID | 英文 | 中文 |
|----|------|------|
| SPELL_LESSER_HEAL | Lesser Heal | 次級治療 |
| SPELL_MAGE_LIGHT | Mage Light | 法師之光 |
| SPELL_FIRE_LIGHT | Fire Light | 火焰之光 |
| SPELL_ELVARS_FIRE | Elvar's Fire | 艾爾瓦之火 |
| SPELL_CLOAK_ARCANE | Cloak Arcane | 隱匿奧秘 |
| SPELL_GREATER_HEALING | Greater Healing | 高級治療 |
| SPELL_CREATE_WALL | Create Wall | 創造牆壁 |
| SPELL_SOFTEN_STONE | Soften Stone | 軟化石頭 |
| SPELL_FIRE_STORM | Fire Storm | 火焰風暴 |
| SPELL_BATTLE_POWER | Battle Power | 戰鬥之力 |
| SPELL_MAJOR_HEALING | Major Healing | 主要治療 |
| SPELL_NOT_ENOUGH_POWER | doesn't have enough spell power. | 法力不足。 |
| SPELL_ENOUGH_POWER | have enough spell power. | 法力充足。 |
| SPELL_NO_SPELLS | has no spells. | 沒有法術。 |
| SPELL_BEYOND_HELP | is beyond our help. | 超出了我們的幫助範圍。 |
| SPELL_PERFECT_HEALTH | is in perfect health. | 健康狀態完美。 |
| SPELL_HEALED | is healed. | 已治癒。 |

---

## 5. 物品文字 (Item Text)

| ID | 英文 | 中文 |
|----|------|------|
| ITEM_TARGET | Target... | 目標... |
| ITEM_NO_USE | The item has no use here. | 該物品在這裡沒有用處。 |
| ITEM_CANNOT_USE | can't use the | 無法使用 |
| ITEM_CANNOT_CARRY | can't carry any more. | 無法攜帶更多。 |
| ITEM_HAS_NO_ITEMS | has no items. | 沒有物品。 |
| ITEM_CANNOT_TRANSFER | This item cannot be transferred. | 該物品無法轉移。 |
| ITEM_DISCARD | Discard the | 丟棄 |
| ITEM_EQUIP_WEAPON | You must equip a weapon that uses the | 您必須裝備使用該 |
| ITEM_NOT_WEAPON | is not a weapon! | 不是武器！ |
| ITEM_CANNOT_LOAD | can't be loaded into your weapon! | 無法裝入您的武器！ |
| ITEM_MUST_RELOAD | You must reload! | 您必須重新裝填！ |
| ITEM_USES | uses... | 使用... |
| ITEM_NEW_WEAPON | New weapon | 新武器 |
| ITEM_LOAD_WEAPON | Load weapon | 裝填武器 |
| ITEM_USE_ITEM | Use item | 使用物品 |
| ITEM_LEATHER_ARMOR | leather armor | 皮甲 |
| ITEM_PLATE_CHAIN_ARMOR | plate and chain armor | 板甲和鎖子甲 |

---

## 6. 對話文字 (Dialogue Text) — 從 DATA1 提取

| ID | 英文 | 中文 |
|----|------|------|
| DIALOG_WHO_ENTER | Who will enter? | 誰要進入？ |
| DIALOG_WHO_HEALING | Who needs healing? | 誰需要治療？ |
| DIALOG_WHO_LOOT | Who will get loot? | 誰來分配戰利品？ |
| DIALOG_WHO_UNLOCK | Who will unlock the chest? | 誰來解鎖寶箱？ |
| DIALOG_ASK_VOLUNTEERS | Ask for volunteers | 請求志願者 |
| DIALOG_LISTEN_RUMORS | Listen for rumors | 聆聽傳聞 |
| DIALOG_WELCOME | Welcome | 歡迎 |
| DIALOG_SAIL_FOR | Set sail for... | 啟航前往... |
| DIALOG_TAKE_STAIRS | There are /a\de\scending stairs here. Do you wish to take them? | 這裡有樓梯。您希望走嗎？ |
| DIALOG_CHEST_FOUND | You find an opened chest here. | 您找到了一個打開的寶箱。 |
| DIALOG_LOCKED_CHEST | You have found a locked chest. | 您找到了一個鎖著的寶箱。 |
| DIALOG_NO_GOLD | You don't have enough gold. | 您沒有足夠的金幣。 |
| DIALOG_DONT_BUY | Sorry, but I don't want to buy that. | 抱歉，我不想買那個。 |
| DIALOG_ANYONE_USE | Anyone can use it. | 任何人都可以使用。 |
| DIALOG_HAVE_TO_HAVE | You would have to have a | 您必須擁有 |
| DIALOG_TO_USE | to use it. | 才能使用它。 |
| DIALOG_GETS_THE | gets the | 獲得 |
| DIALOG_POOL_GOLD | Pool gold | 集合金幣 |
| DIALOG_SHARE_GOLD | Share gold | 分享金幣 |
| DIALOG_TRADE_GOLD | Trade gold | 交易金幣 |
| DIALOG_HOW_MUCH_GOLD | How much gold does | 多少金幣 |
| DIALOG_NOT_THAT_MUCH | doesn't have that much gold. | 沒有那麼多金幣。 |
| DIALOG_WHO_GIVE_GOLD | Who does (name) want to give the gold to? | （名字）想把金幣給誰？ |
| DIALOG_CHAINS_ENCUMBER | Your chains encumber you. | 您的鎖鏈妨礙了您。 |
| DIALOG_GENERAL_OVERVIEW | General overview | 一般概述 |
| DIALOG_ABILITIES | Abilities | 能力 |
| DIALOG_STATISTICS | statistics. | 統計。 |
| DIALOG_STATUS | status. | 狀態。 |
| DIALOG_GOLD_DO_YOU_WISH | gold, do you wish to... | 金幣，您希望... |
| DIALOG_DISCARD | Discard the | 丟棄 |
| DIALOG_NEXT_MENU | next menu | 下一個選單 |
| DIALOG_SUBTRACT_1 | subtract 1 | 減1 |
| DIALOG_AT_MENU | at menu | 在選單 |
| DIALOG_FULL_HEALING | Full healing | 完全治療 |
| DIALOG_PARTIAL_HEALING | Partial healing | 部分治療 |
| DIALOG_HEALING_DO_YOU_WISH | How much healing do you wish? | 您希望治療多少？ |
| DIALOG_WILL_COST | That will cost | 那將花費 |
| DIALOG_IN_GOLD_PAY | in gold, pay? | 金幣，支付？ |
| DIALOG_IM_SORRY_BUT | I'm sorry but | 我很抱歉但 |
| DIALOG_IS_BEYOND_HELP | is beyond our help. | 超出了我們的幫助範圍。 |
| DIALOG_IS_PERFECT_HEALTH | is in perfect health. | 健康狀態完美。 |
| DIALOG_NO_ONE_WANTS | No one wants to join up. | 沒有人想加入。 |
| DIALOG_PARTY_HAS_7 | The party has 7 characters already. | 隊伍已有7個角色。 |
| DIALOG_THIS_BRAVE_SOUL | This brave soul wishes | 這位勇敢的靈魂希望 |
| DIALOG_THESE_BRAVE_SOULS | These brave souls wish | 這些勇敢的靈魂希望 |
| DIALOG_BARKEEP_SAYS | The barkeep says, "I hear | 酒保說，"我聽說 |
| DIALOG_ALAS | Alas, your brave party has met its match! Your current adventure is over. | 唉，您的勇敢隊伍遇到了對手！您目前的冒險結束了。 |
| DIALOG_CANNOT_DISMISS | You cannot dismiss the last party member. | 您不能解僱最後一個隊伍成員。 |
| DIALOG_ENTER_ARENA | Do you wish to enter the arena? | 您希望進入競技場嗎？ |
| DIALOG_SEVERAL_GLADIATORS | Several gladiators bearing recent battle scars block your way. | 幾個帶著最近戰鬥傷痕的角鬥士擋住了您的去路。 |
| DIALOG_YOU_MAY_ONLY | "You may only enter once!" | "您只能進入一次！" |
| DIALOG_COME_BACK | Come back when you are ready to face the challenge of combat! | 當您準備好面對戰鬥挑戰時再回來！ |
| DIALOG_EXCELLENT | "Excellent!" says the guard, "And I see that you | "太好了！"守衛說，"我看到您 |
| DIALOG_VIEW_PARTY | View the party | 查看隊伍 |
| DIALOG_VIEWING_PARTY | Viewing current party. | 查看目前隊伍。 |
| DIALOG_USE_COMMANDS | Use these commands? | 使用這些指令？ |
| DIALOG_DEEQUIP | Do you wish to deequip your | 您希望卸下您的 |
| DIALOG_ATTACK_STYLE | Attack style... | 攻擊風格... |
| DIALOG_ATTACK_BLOW | Attack blow | 攻擊打擊 |
| DIALOG_MIGHTY_BLOW | Mighty blow | 強力打擊 |
| DIALOG_DISARM_ENEMY | Disarm enemy | 解除敵人武裝 |
| DIALOG_WILL_PARTY | Will the party: | 隊伍將： |
| DIALOG_FIGHT | Fight | 戰鬥 |
| DIALOG_QUICKLY_FIGHT | Quickly fight | 快速戰鬥 |
| DIALOG_USE_ITEM | Use item | 使用物品 |
| DIALOG_NEW_WEAPON | New weapon | 新武器 |
| DIALOG_LOAD_WEAPON | Load weapon | 裝填武器 |
| DIALOG_CANNOT_LEARN | cannot learn | 無法學習 |

---

## 6a. 對話文字 (從遊戲截圖確認)

以下文字從遊戲截圖 `/home/anr2/tmp/longcat/org_dialogue/` 中確認：

**dragon_026.png** — 對話場景：
- 需要確認對話內容

**dragon_007.png** — 對話場景：
- 需要確認對話內容

**images.jpeg** — "Read Paragraph" 介面：
- 需要從中文手冊提取

**maxresdefault.jpg** — 遊戲場景：
- 需要確認對話內容

### Read Paragraph（讀取段落）

根據遊戲設計，"Read Paragraph" 是遊戲中的一個重要功能，用於顯示故事情節和對話。
這部分文字主要從遊戲手冊（`珍066-火龍之戰.rar`）中提取。

**需要從手冊提取的範圍**：
- 每個章節的劇情描述
- 角色對話
- 物品描述
- 任務提示

---

## 6b. 物品名稱 (從 DATA1 提取)

**重要修正**：
- **Section 0x07 是角色資料（CHARACTER_DATA）**，包含 512 位元組的個人記錄，**不是物品名稱**
- **物品名稱儲存於 Section 0x06（物品資料）和 Section 0x0D（物品描述）**
- **角色名稱儲存於 Section 0x07（角色資料）和 Section 0x01（角色初始化）**
- 以下是從截圖和文件中確認的物品名稱：

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| leather armor | 皮甲 | 截圖 | ✅ 已翻譯 |
| plate and chain armor | 板甲和鎖子甲 | 截圖 | ✅ 已翻譯 |
| cloth armor | 布甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| cuir bouilli armor | 硬化皮革甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| brigandine armor | 布里甘丁甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| scale armor | 鱗甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| chain armor | 鎖子甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| full plate armor | 全身板甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| pair of boots | 靴子 | DATA1 Section 0x06 | ✅ 已翻譯 |
| mage gloves | 法師手套 | DATA1 Section 0x06 | ✅ 已翻譯 |
| Armor of Light | 光明甲 | DATA1 Section 0x06 | ✅ 已翻譯 |
| (更多物品名稱需要從 DATA1 Section 0x06 提取) | | | |

---

## 6c. 技能名稱 (從 DATA1 Section 0x15 提取)

**注意**：
- **Section 0x15 只有 15 個文字**，包含部分技能名稱
- **完整技能名稱可能在其他 section**（如 Section 0x05 物品/技能相關、Section 0x0E 技能描述）
- 以下是從 DATA1 提取的技能名稱：

| 英文 | 中文 | 來源 | 狀態 |
|------|------|------|------|
| Arcane Lore | 奧秘知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Cave Lore | 洞穴知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Forest Lore | 森林知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Mountain Lore | 山脈知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Town Lore | 城鎮知識 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Fistfighting | 拳鬥 | DATA1 Section 0x15 | ✅ 已翻譯 |
| Thrown weapons | 投擲武器 | DATA1 Section 0x05 | ✅ 已翻譯 |
| (更多技能名稱需要從 DATA1 Section 0x05/0x0E 提取) | | | |

---

## 7. UI 文字 (從 dragon.com 提取)

| ID | 英文 | 中文 |
|----|------|------|
| UI_CONFIG_TITLE | Dragon Wars Configure Menu V1.1 | 龍之戰設定選單 V1.1 |
| UI_COPYRIGHT | Copyright 1989, 1990 Interplay | 版權 1989, 1990 Interplay |
| UI_CGA_RGB | A. CGA RGB monitor | A. CGA RGB 螢幕 |
| UI_CGA_COMP | B. CGA composite monitor | B. CGA 複合式螢幕 |
| UI_TANDY | C. Tandy 16 color | C. Tandy 16 色 |
| UI_EGA_VGA | D. EGA/VGA 16 color | D. EGA/VGA 16 色 |
| UI_MOUSE_ON | 1. Mouse On | 1. 滑鼠開啟 |
| UI_MOUSE_OFF | 2. Mouse Off | 2. 滑鼠關閉 |
| UI_SELECT_SCREEN | Select a screen format by typing its letter. | 輸入字母選擇螢幕格式。 |
| UI_PRESS_KEY | Press [KEY] to begin the game or press "S" to save configuration | 按 [KEY] 開始遊戲或按 "S" 儲存設定 |
| UI_ESC_EXIT | or press ESC to return to MS-DOS. | 或按 ESC 返回 MS-DOS。 |
| UI_MOUSE_HELP | Press 1 or 2 for enabling/disabling mouse support. | 按 1 或 2 啟用/停用滑鼠支援。 |
| UI_SAVING | Saving game state. | 儲存遊戲進度。 |
| UI_SAVED | Game state saved. | 遊戲進度已儲存。 |
| UI_DRIVE_ERR | Drive error. | 磁碟錯誤。 |
| UI_WRITE_PROT | Write protected. | 防寫保護。 |
| UI_FATAL | Fatal error : Out of memory.$ | 嚴重錯誤：記憶體不足。$ |

---

## 8. 技能文字 (Skill Text)

| ID | 英文 | 中文 |
|----|------|------|
| SKILL_ARCANE_LORE | Arcane Lore | 奧秘知識 |
| SKILL_CAVE_LORE | Cave Lore | 洞穴知識 |
| SKILL_FOREST_LORE | Forest Lore | 森林知識 |
| SKILL_MOUNTAIN_LORE | Mountain Lore | 山脈知識 |
| SKILL_TOWN_LORE | Town Lore | 城鎮知識 |
| SKILL_FISTFIGHTING | Fistfighting | 拳鬥 |
| SKILL_SKILL_AMOUNT_COST | Skill     Amount Cost | 技能     數量 費用 |
| SKILL_AMOUNT_COST | Amount Cost | 數量 費用 |

---

## 9. 使用方式

在重新實作時，可以直接嵌入翻譯文字：

```c
// 定義翻譯字串
typedef struct {
    const char* id;
    const char* english;
    const char* chinese;
} TranslationEntry;

const TranslationEntry translations[] = {
    {"MENU_BEGIN", "Do you wish to..\n\nBegin a new game\nContinue an old game", "您希望..\n\n開始新遊戲\n繼續舊遊戲"},
    {"MENU_LOADING", "Loading...", "載入中…"},
    // ...
};
```

---

## 10. 技術細節

### 5-bit 編碼字母表
```
0xa0 = 空格
0xe1-0xfa = a-z, 0-9 (小寫字母和數字)
0xb0-0xb9 = 0-9 (備用數字)
0x41-0x5a = A-Z (大寫字母)
0x8d = 換行 (\n)
0xa8 = '
0xa9 = :
0xaf = /
0xdc = \
0xa3 = !
0xaa = (
0xbf = )
0xbc = ]
0xbe = -
0xba = +
0xbb = *
0xad = ,
0xa5 = .
```

### 解壓演算法
```python
def extract_string(data, byte_offset, max_len=500):
    be = BitExtractor(data, byte_offset)
    result = []
    for _ in range(max_len):
        letter = be.extract_letter()
        if letter == 0: break
        result.append(letter)
    return bytes(result), be.byte_offset
```

### 高位元處理
字元值可能包含 `0x80` 高位元，需要 `& 0x7F` 取得實際字元：
```c
char actual_char = alphabet_value & 0x7F;
```

---

## 11. 待辦事項

- [ ] 從中文手冊提取更多對話文字
- [ ] 實作中文顯示（24×24 點陣）
- [ ] 實作中文輸入
