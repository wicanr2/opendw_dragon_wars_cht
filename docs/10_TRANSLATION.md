# Dragon Wars 翻譯對照表

> ⚠️ **品質警語(2026-06-10),使用前必讀**:
> 1. **「未知文字」雜訊條目已全數刪除(D1)** —— 原約 50 條來自 `_deprecated/20_ALL_TEXT_FROM_DATA1.txt` 暴力萃取的亂碼(如 `'l .ias'tdamu`、`JTs6plk,fd`),已移除。表中嵌入的損壞 section 統計表(`總計 3926 / ~150`…)亦一併清除。
> 2. **section 0x08/0x15 等並非文字表**;本表「來源 0x08/0x15」的分節僅沿用舊假設,實際文字在 script bytecode / res。
> 3. **譯名已對齊 `../CONTEXT.md`(D2)**:`Purgatory=波卡城/罪惡之城`(非「煉獄」)、`Namtar=納達`(非「納姆塔」)、`Nergal=奈羅`(≠Namtar)、`High/Low Magic=高級/初級魔法`。中英雙語條目皆保留;`slums of Purgatory` 保留「波卡城的貧民窟」語意。
> 4. 「技能名稱」表中把 `Purgatory/Wolf/Spider/Pikeman` 連同 sprite 編號當技能,屬誤植,請改參考 `26_MONSTERS_AND_SPRITES.md`。
> 乾淨可信的真實遊戲文字以 `ALL_TEXT_FROM_SCRIPTS.txt` 為準。

> 覆蓋率:刪除雜訊後約 250 條有意義條目

## 概述

| ID | 英文 | 中文 | 來源 | 狀態 |
|----|------|------|------|------|

## 主選單 (Main Menu) — Section 0x00

| ONTINUE_AN_OLD_GAME | 'ontinue an old game | 載入舊遊戲 | 0x00 | ✅ |
| S_NEW_NAME_BE | 's new name be? | 的新名字是？ | 0x00 | ✅ |
| MENU_FOREVER | (name) will be gone forever. Have mercy. | （名字）將永遠消失。請大發慈悲。 | 0x00 | ✅ |
| MENU_BEGIN_GAME | Begin the game | 開始遊戲 | 0x00 | ✅ |
| MENU_BYE | Bye bye, (name). | 再見，（名字）。 | 0x00 | ✅ |
| MENU_CREATE | Create character | 建立角色 | 0x00 | ✅ |
| MENU_PARTY | Current party... | 目前隊伍... | 0x00 | ✅ |
| DO_YOU_STILL_WISH_TO_STAR | Do you still wish to start a new game? | 你確定要開始新遊戲嗎？ | 0x00 | ✅ |
| MENU_QUIT | Do you wish to quit the game? | 您希望離開遊戲嗎？ | 0x00 | ✅ |
| MENU_SAVE | Do you wish to save your game? | 您希望儲存遊戲嗎？ | 0x00 | ✅ |
| DO_YOU_WISH_TO | Do you wish to.. | 你是否要… | 0x00 | ✅ |
| MENU_BEGIN | Do you wish to..\n\nBegin a new game\nContinue an old game | 您希望..\n\n開始新遊戲\n繼續舊遊戲 | 0x00 | ✅ |
| MENU_DELETE | Do you wish to..\n\nDelete (name) | 您希望..\n\n刪除（名字） | 0x00 | ✅ |
| MENU_LOADING | Loading... | 載入中… | 0x00 | ✅ |
| MENU_GENDER | Male or Female? | 男性或女性？ | 0x00 | ✅ |
| MENU_NAME_NEW | Name your new character. | 為您的新角色命名。 | 0x00 | ✅ |
| MENU_RENAME | Rename (name) | 重新命名（名字） | 0x00 | ✅ |
| MENU_START_WARN | Starting a new game will destroy your last saved game. Do you still wish to start a new game? | 開始新遊戲會摧毀您最後儲存的遊戲。您仍然希望開始新遊戲嗎？ | 0x00 | ✅ |
| MENU_PAUSED | The game is paused | 遊戲已暫停 | 0x00 | ✅ |
| MENU_VIEW | View (name) | 查看（名字） | 0x00 | ✅ |
| MENU_NAME | What will (name)'s new name be? | （名字）的新名字是什麼？ | 0x00 | ✅ |
| YOU_ARE_ABOUT_TO_DELETE | You are about to delete | 你即將刪除 | 0x00 | ✅ |
| MENU_DELETE_WARN | You are about to delete (name). What has (name) done to deserve such a fate?? | 您即將刪除（名字）。（名字）做了什麼以至於落得如此下場？？ | 0x00 | ✅ |
| MENU_MUST_HAVE | You must have someone in the party to begin the game!! | 您的隊伍中必須有人才能開始遊戲！！ | 0x00 | ✅ |
| YOU_STILL_HAVE | You still have | 你還有 | 0x00 | ✅ |
| MENU_POINTS | You still have (N) points left to distribute, do you wish to go back and distribute them? | 您還有（N）點可以分配，您希望回去分配它們嗎？ | 0x00 | ✅ |
| MENU_SAVED | Your game is saved. | 遊戲已儲存。 | 0x00 | ✅ |

## 狀態文字 (Status Text) — Section 0x02

| T_HAVE_ENOUGH_GOLD | 't have enough gold. | 金額不足。 | 0x02 | ✅ |
| STATUS_DEAD_FULL | That character is dead. | 該角色已死亡。 | 0x02 | ✅ |
| STATUS_OUT_OF_COMMISSION | That character is out of commission. | 該角色已無法行動。 | 0x02 | ✅ |
| STATUS_CHAINED | chained | 被鏈住 | 0x02 | ✅ |
| STATUS_DEAD | dead | 死亡 | 0x02 | ✅ |
| STATUS_POISONED | poisoned | 中毒 | 0x02 | ✅ |
| STATUS_STUNNED | stunned | 昏迷 | 0x02 | ✅ |

## 對話/劇情 (Dialogue) — Section 0x03

| DIALOG_YOU_MAY_ONLY | "You may only enter once!" | "您只能進入一次！" | 0x03 | ✅ |
| DIALOG_ASK_VOLUNTEERS | Ask for volunteers | 請求志願者 | 0x03 | ✅ |
| DIALOG_COME_BACK | Come back when you are ready to face the challenge of combat! | 當您準備好面對戰鬥挑戰時再回來！ | 0x03 | ✅ |
| DIALOG_ENTER_ARENA | Do you wish to enter the arena? | 您希望進入競技場嗎？ | 0x03 | ✅ |
| DIALOG_LISTEN_RUMORS | Listen for rumors | 聆聽傳聞 | 0x03 | ✅ |
| PCUDPPHPHYFPAMAPERIENCE_P | PcudpphphyfpamAperience points | 經驗值 | 0x03 | ✅ |
| DIALOG_SAIL_FOR | Set sail for... | 啟航前往... | 0x03 | ✅ |
| DIALOG_SEVERAL_GLADIATORS | Several gladiators bearing recent battle scars block your way. | 幾個帶著最近戰鬥傷痕的角鬥士擋住了您的去路。 | 0x03 | ✅ |
| DIALOG_DONT_BUY | Sorry, but I don't want to buy that. | 抱歉，我不想買那個。 | 0x03 | ✅ |
| DIALOG_WELCOME | Welcome | 歡迎 | 0x03 | ✅ |
| DIALOG_WHO_HEALING | Who needs healing? | 誰需要治療？ | 0x03 | ✅ |
| DIALOG_WHO_ENTER | Who will enter? | 誰要進入？ | 0x03 | ✅ |
| DIALOG_WHO_LOOT | Who will get loot? | 誰來分配戰利品？ | 0x03 | ✅ |
| DIALOG_WHO_UNLOCK | Who will unlock the chest? | 誰來解鎖寶箱？ | 0x03 | ✅ |
| DIALOG_NO_GOLD | You don't have enough gold. | 您沒有足夠的金幣。 | 0x03 | ✅ |

## 物品/角色 (Item/Character) — Section 0x05

| ITEM_DISCARD | Discard the | 丟棄 | 0x05 | ✅ |
| DO_YOU_WISH_TO_TAKE_THEM | Do you wish to take them? | 你要拿走它們嗎？ | 0x05 | ✅ |
| ITEM_LOAD_WEAPON | Load weapon | 裝填武器 | 0x05 | ✅ |
| ITEM_NEW_WEAPON | New weapon | 新武器 | 0x05 | ✅ |
| ITEM_TARGET | Target... | 目標... | 0x05 | ✅ |
| ITEM_NO_USE | The item has no use here. | 該物品在這裡沒有用處。 | 0x05 | ✅ |
| ITEM_CANNOT_TRANSFER | This item cannot be transferred. | 該物品無法轉移。 | 0x05 | ✅ |
| ITEM_USE_ITEM | Use item | 使用物品 | 0x05 | ✅ |
| WHICH_CHARACTER | Which character? | 哪一名角色？ | 0x05 | ✅ |
| ITEM_EQUIP_WEAPON | You must equip a weapon that uses the | 您必須裝備使用該 | 0x05 | ✅ |
| ITEM_MUST_RELOAD | You must reload! | 您必須重新裝填！ | 0x05 | ✅ |
| ITEM_CANNOT_LOAD | can't be loaded into your weapon! | 無法裝入您的武器！ | 0x05 | ✅ |
| ITEM_CANNOT_CARRY | can't carry any more. | 無法攜帶更多。 | 0x05 | ✅ |
| ITEM_CANNOT_USE | can't use the | 無法使用 | 0x05 | ✅ |
| ITEM_HAS_NO_ITEMS | has no items. | 沒有物品。 | 0x05 | ✅ |
| ITEM_NOT_WEAPON | is not a weapon! | 不是武器！ | 0x05 | ✅ |
| ITEM_USES | uses... | 使用... | 0x05 | ✅ |

## 技能名稱 (Skill Names) — Section 0x05/0x15

> ⚠️ 下表原把 `Purgatory`(地名,波卡城)與 `Fanatic/Wolf/Spider/Pikeman`(怪物 sprite 編號)誤列為技能,已標註。
> 怪物正解見 `26_MONSTERS_AND_SPRITES.md`;Purgatory 正譯為「波卡城/罪惡之城」(非「煉獄」)。

| Sword | 劍術 | DATA1 Section 0x05 | 0x05/0x15 | ✅ |
| Bow | 弓術 | DATA1 Section 0x05 | 0x05/0x15 | ✅ |
| Fistfighting | 拳鬥 | DATA1 Section 0x15 | 0x05/0x15 | ✅ |
| ~~Purgatory~~(誤植:地名,非技能) | 波卡城/罪惡之城 | 71 (0x71) 關卡 | — | ⚠️ |
| ~~Fanatic~~(誤植:怪物 sprite) | 狂熱者 | 222 (0xDE) sprite | 見 26 | ⚠️ |
| ~~Wolf~~(誤植:怪物 sprite) | 野狼 | 168 (0xA8) sprite | 見 26 | ⚠️ |
| Shield | 盾術 | DATA1 Section 0x05 | 0x05/0x15 | ✅ |
| ~~Spider~~(誤植:怪物 sprite) | 蜘蛛 | 196 (0xC4) sprite | 見 26 | ⚠️ |
| ~~Pikeman~~(誤植:怪物 sprite) | 長矛兵 | 210 (0xD2) sprite | 見 26 | ⚠️ |

## 戰鬥 (Combat) — Section 0x06

| T_RECHARGE_THAT | 't recharge that. | 無法充值那個物品。 | 0x06 | ✅ |
| NOTHING_HAPPENS | Nothing happens. | 什麼都沒發生。 | 0x06 | ✅ |
| THAT_DIDN_T_DO_ANY_GOOD | That didn't do any good. | 那沒有起任何作用。 | 0x06 | ✅ |
| COMBAT_PARTY_ADVANCES | The party advances. | 隊伍前進。 | 0x06 | ✅ |
| YOU_CAN_T_RECHARGE_THAT | You can't recharge that. | 你無法充值那個物品。 | 0x06 | ✅ |
| COMBAT_STILL_FACE | You still face | 您仍然面對 | 0x06 | ✅ |
| COMBAT_BLOCKS | blocks! | 擋住！ | 0x06 | ✅ |
| COMBAT_CALLS_HELP | calls for more help | 呼叫更多幫助 | 0x06 | ✅ |
| COMBAT_DODGES | dodges! | 閃避！ | 0x06 | ✅ |
| COMBAT_FAILS_DAMAGE | fails to do any damage. | 無法造成傷害。 | 0x06 | ✅ |
| COMBAT_FOR_COMBAT | for combat. | 為了戰鬥。 | 0x06 | ✅ |
| COMBAT_GAINED_LEVEL | has gained a level! | 升級了！ | 0x06 | ✅ |
| COMBAT_OUT_OF_RANGE | is out of range. | 超出範圍。 | 0x06 | ✅ |
| COMBAT_POINT_DAMAGE | point\s\ of damage | 點傷害 | 0x06 | ✅ |
| COMBAT_RETREATS | retreats back! | 撤退！ | 0x06 | ✅ |
| COMBAT_ATTACK_BLOCKED | the attack is attack is blocked! | 攻擊被擋住了！ | 0x06 | ✅ |
| COMBAT_TIME_FOR | time\s\ for | 時間\用於 | 0x06 | ✅ |
| COMBAT_TRIED_RUN | tried to run away! | 試圖逃跑！ | 0x06 | ✅ |

## 物品描述 (Item Desc) — Section 0x06/0x0D

| dagger | 匕首 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| bow | 弓 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| axe | 斧 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| Intelligence | 智力 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| staff | 法杖 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| spear | 矛 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| arrow | 箭 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| gauntlets | 護手 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| mace | 錘 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |
| helmet | 頭盔 | DATA1 Section 0x06 | 0x06/0x0D | ✅ |

## 角色資料 (Character Data) — Section 0x07


## 法術 (Spells) — Section 0x08

| SPELL_BATTLE_POWER | Battle Power | 戰鬥之力 | 0x08 | ✅ |
| SPELL_CLOAK_ARCANE | Cloak Arcane | 隱匿奧秘 | 0x08 | ✅ |
| SPELL_CREATE_WALL | Create Wall | 創造牆壁 | 0x08 | ✅ |
| SPELL_ELVARS_FIRE | Elvar's Fire | 艾爾瓦之火 | 0x08 | ✅ |
| SPELL_FIRE_LIGHT | Fire Light | 火焰之光 | 0x08 | ✅ |
| SPELL_FIRE_STORM | Fire Storm | 火焰風暴 | 0x08 | ✅ |
| SPELL_GREATER_HEALING | Greater Healing | 高級治療 | 0x08 | ✅ |
| SPELL_LESSER_HEAL | Lesser Heal | 次級治療 | 0x08 | ✅ |
| SPELL_MAGE_LIGHT | Mage Light | 法師之光 | 0x08 | ✅ |
| SPELL_MAJOR_HEALING | Major Healing | 主要治療 | 0x08 | ✅ |
| PIRATE_S_COVE | Pirate's Cove | 海賊灣 | 0x08 | ✅ |
| RUSTIC_AND_AFTER_MANY_DAY | Rustic and after many days of travel you arrive at the northern shore. | 經過多日旅行，你抵達了北方海岸。 | 0x08 | ✅ |
| SPELL_SOFTEN_STONE | Soften Stone | 軟化石頭 | 0x08 | ✅ |
| SUNKEN_RUINS | Sunken ruins | 沉沒的遺跡 | 0x08 | ✅ |
| YOU_CLIMB_ABORD_SHIP_AND_ | You climb abord ship and sail for the island of Rustic and after many days of travel you arrive at the northern shore. | 你登上船，航向拉斯提克島，經過多日的旅行，抵達了北方海岸。 | 0x08 | ✅ |
| YOU_SAIL_BACK_TO_SMUGGLER | You sail back to Smuggler's cove without incident. | 你平安地駛回走私者海灣。 | 0x08 | ✅ |
| SPELL_NOT_ENOUGH_POWER | doesn't have enough spell power. | 法力不足。 | 0x08 | ✅ |
| SPELL_NO_SPELLS | has no spells. | 沒有法術。 | 0x08 | ✅ |
| SPELL_ENOUGH_POWER | have enough spell power. | 法力充足。 | 0x08 | ✅ |
| SPELL_BEYOND_HELP | is beyond our help. | 超出了我們的幫助範圍。 | 0x08 | ✅ |
| SPELL_HEALED | is healed. | 已治癒。 | 0x08 | ✅ |
| SPELL_PERFECT_HEALTH | is in perfect health. | 健康狀態完美。 | 0x08 | ✅ |

## 招募 (Recruitment) — Section 0x09

| NO_ONE_WANTS_TO_JOIN_UP | No one wants to join up. | 沒有人想加入。 | 0x09 | ✅ |
| THE_PARTY_HAS_7_CHARACTER | The party has 7 characters already. | 隊伍已有 7 名角色。 | 0x09 | ✅ |
| THESE_BRAVE_SOULS_WISH | These brave souls wish | 這些勇者希望 | 0x09 | ✅ |
| THIS_BRAVE_SOUL_WISHES | This brave soul wishes | 這位勇者希望 | 0x09 | ✅ |

## 商店 (Shop) — Section 0x0A

| BUY_AN_ITEM | Buy an item | 購買物品 | 0x0A | ✅ |
| BUY_WHICH_ITEM | Buy which item? | 要購買哪個物品？ | 0x0A | ✅ |
| EXAMINE_WHICH_ITEM | Examine which item? | 要察看哪個物品？ | 0x0A | ✅ |
| SELL_AN_ITEM | Sell an item | 出售物品 | 0x0A | ✅ |
| SELL_WHICH_ITEM | Sell which item? | 要出售哪個物品？ | 0x0A | ✅ |

## 寶箱 (Chest) — Section 0x0B

| GWLIAGSMDFP_ABYWHO_WILL_U | Gwliagsmdfp abyWho will unlock the chest? | 誰要打開寶箱？ | 0x0B | ✅ |
| THE_CHEST_REMAINS_LOCKED | The chest remains locked! | 寶箱仍然上鎖！ | 0x0B | ✅ |
| WHICH_ITEM | Which item... | 哪個物品… | 0x0B | ✅ |
| YOU_FIND_AN_OPENED_CHEST_ | You find an opened chest here. | 你發現這裡有一個已打開的寶箱。 | 0x0B | ✅ |
| YOU_HAVE_FOUND_A_LOCKED_C | You have found a locked chest. | 你發現了一個上鎖的寶箱。 | 0x0B | ✅ |

## 技能/法術選擇 (Skill/Spell) — Section 0x0C

| WHICH_ATTRIBUTE | Which attribute... | 哪項屬性… | 0x0C | ✅ |
| WHICH_MAGIC | Which magic... | 哪種法術… | 0x0C | ✅ |
| WHICH_SKILL | Which skill... | 哪個技能… | 0x0C | ✅ |
| WHICH_SPELL | Which spell... | 哪個法術… | 0x0C | ✅ |
| WHICH_TYPE | Which type... | 哪種類型… | 0x0C | ✅ |

## 物品欄 (Inventory) — Section 0x0D

| ARE_YOU_SURE_YOU_WANT_TO_ | Are you sure you want to throw away the | 你確定要丟掉 | 0x0D | ✅ |
| CARRIED_ITEMS | Carried items | 攜帶物品 | 0x0D | ✅ |
| GENERAL_OVERVIEW | General overview | 總覽 | 0x0D | ✅ |
| MAGIC_SPELLS | Magic spells... | 法術… | 0x0D | ✅ |
| UNEQUIP_THE | Unequip the | 卸下 | 0x0D | ✅ |
| YOUR_CHAINS_ENCUMBER_YOU | Your chains encumber you. | 你的鎖鏈妨礙了你。 | 0x0D | ✅ |

## 法術名稱 (Spell Names) — Section 0x0E

| ARMOR_OF_LIGHT | Armor of Light | 光明之鎧 | 0x0E | ✅ |
| COLUMN_OF_FIRE | Column of Fire | 火柱 | 0x0E | ✅ |
| EARTH_SUMMON | Earth Summon | 召喚大地 | 0x0E | ✅ |
| INSECT_PLAGUE | Insect Plague | 蟲災 | 0x0E | ✅ |
| INVOKE_SPIRIT | Invoke Spirit | 招魂 | 0x0E | ✅ |
| MITHRAS_BLESS | Mithras' Bless | 米斯拉斯的祝福 | 0x0E | ✅ |
| MYSTIC_MIGHT | Mystic Might | 神秘之力 | 0x0E | ✅ |
| POOG_S_VORTEX | Poog's Vortex | 普格漩渦 | 0x0E | ✅ |
| RAGE_OF_MITHRAS | Rage of Mithras | 米斯拉斯之怒 | 0x0E | ✅ |
| REVEAL_GLAMOUR | Reveal Glamour | 揭破幻象 | 0x0E | ✅ |
| SALA_S_SWIFT | Sala's Swift | 莎拉之迅 | 0x0E | ✅ |
| SUMMON_SALAMANDER | Summon Salamander | 召喚火蠑螈 | 0x0E | ✅ |
| VORN_S_GUARD | Vorn's Guard | 沃恩之衛 | 0x0E | ✅ |
| WATER_SUMMON | Water Summon | 召喚水源 | 0x0E | ✅ |
| WRATH_OF_MITHRAS | Wrath of Mithras | 米斯拉斯之憤 | 0x0E | ✅ |

## 遊戲控制 (Game Control) — Section 0x0F

| ARE_YOU_SURE_YOU_WISH_TO_ | Are you sure you wish to dismiss | 你確定要解雇 | 0x0F | ✅ |
| SAVING_GAME | Saving game... | 儲存中… | 0x0F | ✅ |
| YOU_CANNOT_DISMISS_THE_LA | You cannot dismiss the last party member. | 你不能解雇最後一名隊員。 | 0x0F | ✅ |

## 神殿 (Temple) — Section 0x11

| HOW_MUCH_HEALING_DO_YOU_W | How much healing do you wish? | 你希望治療多少？ | 0x11 | ✅ |
| I_M_SORRY_BUT | I'm sorry but | 抱歉，但 | 0x11 | ✅ |
| THAT_WILL_COST | That will cost | 這將花費 | 0x11 | ✅ |
| WHAT_SERVICE_WOULD_YOU_LI | What service would you like performed on | 你希望對誰施行何種服務？ | 0x11 | ✅ |

## 戰鬥指令 (Combat Commands) — Section 0x12

| DAIW_BYATTACK_STYLE | 'daiw byAttack style... | 攻擊風格…… | 0x12 | ✅ |
| ATTACK_STYLE | Attack style... | 攻擊風格…… | 0x12 | ✅ |
| BLOCK_ATTACK | Block attack | 格擋攻擊 | 0x12 | ✅ |
| DO_YOU_WISH_TO_DEEQUIP_YO | Do you wish to deequip your | 你是否要卸下你的 | 0x12 | ✅ |
| DODGE_ENEMIES | Dodge enemies | 閃避敵人 | 0x12 | ✅ |
| FIRING_RATE | Firing rate | 射速 | 0x12 | ✅ |
| USE_THESE_COMMANDS | Use these commands? | 要使用這些指令嗎？ | 0x12 | ✅ |
| VIEW_THE_PARTY | View the party | 查看隊伍 | 0x12 | ✅ |
| VIEWING_CURRENT_PARTY | Viewing current party. | 目前隊伍一覽。 | 0x12 | ✅ |
| WILL_THE_PARTY | Will the party: | 隊伍要： | 0x12 | ✅ |

## 對話選項 (Dialogue Options) — Section 0x13

| SCRIPT13_EXCELLENT | "Excellent!" says the guard, "And I see that you | "太好了！"守衛說，"我看到您 | 0x13 | ✅ |
| SCRIPT13_YOU_MAY_ONLY | "You may only enter once!" | "您只能進入一次！" | 0x13 | ✅ |
| OBC_ABYYOU_MAY_CHOOSE | 'obc abyYou may choose | 你可以選擇 | 0x13 | ✅ |
| SCRIPT0_FOREVER | (name) will be gone forever. Have mercy. | （名字）將永遠消失。請大發慈悲。 | 0x13 | ✅ |
| UI_MOUSE_ON | 1. Mouse On | 1. 滑鼠開啟 | 0x13 | ✅ |
| UI_MOUSE_OFF | 2. Mouse Off | 2. 滑鼠關閉 | 0x13 | ✅ |
| UI_CGA_RGB | A. CGA RGB monitor | A. CGA RGB 螢幕 | 0x13 | ✅ |
| SCRIPT06_ANYONE_USE | Anyone can use it. | 任何人都可以使用。 | 0x13 | ✅ |
| UI_CGA_COMP | B. CGA composite monitor | B. CGA 複合式螢幕 | 0x13 | ✅ |
| BEAST_FROM_THE_PIT | Beast From The Pit. | 深淵之獸。 | 0x13 | ✅ |
| SCRIPT0_BEGIN_GAME | Begin the game | 開始遊戲 | 0x13 | ✅ |
| SCRIPT0_BYE | Bye bye, (name). | 再見，（名字）。 | 0x13 | ✅ |
| UI_TANDY | C. Tandy 16 color | C. Tandy 16 色 | 0x13 | ✅ |
| SCRIPT13_COME_BACK | Come back when you are ready to face the challenge of combat! | 當您準備好面對戰鬥挑戰時再回來！ | 0x13 | ✅ |
| UI_COPYRIGHT | Copyright 1989, 1990 Interplay | 版權 1989, 1990 Interplay | 0x13 | ✅ |
| SCRIPT0_CREATE | Create character | 建立角色 | 0x13 | ✅ |
| SCRIPT0_PARTY | Current party... | 目前隊伍... | 0x13 | ✅ |
| UI_EGA_VGA | D. EGA/VGA 16 color | D. EGA/VGA 16 色 | 0x13 | ✅ |
| DO_YOU_DESCRIBE_YOURSELVE | Do you describe yourselves as thieves, beggars, or tramps? | 你們認為自己是小偷、乞丐還是流浪漢？ | 0x13 | ✅ |
| SCRIPT13_ENTER_ARENA | Do you wish to enter the arena? | 您希望進入競技場嗎？ | 0x13 | ✅ |
| SCRIPT0_BEGIN_NEW | Do you wish to..\n\nBegin a new game\nContinue an old game | 您希望..\n\n開始新遊戲\n繼續舊遊戲 | 0x13 | ✅ |
| SCRIPT0_DELETE | Do you wish to..\n\nDelete (name) | 您希望..\n\n刪除（名字） | 0x13 | ✅ |
| UI_CONFIG_TITLE | Dragon Wars Configure Menu V1.1 | 龍之戰設定選單 V1.1 | 0x13 | ✅ |
| UI_DRIVE_ERR | Drive error. | 磁碟錯誤。 | 0x13 | ✅ |
| SCRIPT03_EACH_MEMBER | Each member gets | 每位成員獲得 | 0x13 | ✅ |
| UI_FATAL | Fatal error : Out of memory.$ | 嚴重錯誤：記憶體不足。$ | 0x13 | ✅ |
| FROM_THE_PIT | From The Pit. | 來自深淵。 | 0x13 | ✅ |
| UI_SAVED | Game state saved. | 遊戲進度已儲存。 | 0x13 | ✅ |
| SCRIPT0_INTERPLAY | Interplay | _interp | 0x13 | ✅ |
| SCRIPT0_IS | Is | 是 | 0x13 | ✅ |
| SCRIPT0_LOADING | Loading... | 載入中… | 0x13 | ✅ |
| SCRIPT0_GENDER | Male or Female? | 男性或女性？ | 0x13 | ✅ |
| SCRIPT0_NAME_NEW | Name your new character. | 為您的新角色命名。 | 0x13 | ✅ |
| NO_ONE_ESCAPES_PURGATORY_ | No one escapes Purgatory alive, and few know the luxury to die in bed within her walls. | 沒有人能活著逃出波卡城，極少人能在她的城牆內安詳地死在床上。 | 0x13 | ✅ |
| UI_MOUSE_HELP | Press 1 or 2 for enabling/disabling mouse support. | 按 1 或 2 啟用/停用滑鼠支援。 | 0x13 | ✅ |
| UI_PRESS_KEY | Press [KEY] to begin the game or press "S" to save configuration | 按 [KEY] 開始遊戲或按 "S" 儲存設定 | 0x13 | ✅ |
| PURGATORY_ALIVE_AND_FEW_K | Purgatory alive, and few know the luxury to die in bed within her walls. | 活著逃出波卡城，極少人能在城牆內安詳死去。 | 0x13 | ✅ |
| SCRIPT0_RENAME | Rename (name) | 重新命名（名字） | 0x13 | ✅ |
| UI_SAVING | Saving game state. | 儲存遊戲進度。 | 0x13 | ✅ |
| UI_SELECT_SCREEN | Select a screen format by typing its letter. | 輸入字母選擇螢幕格式。 | 0x13 | ✅ |
| SCRIPT13_SEVERAL_GLADIATORS | Several gladiators bearing recent battle scars block your way. | 幾個帶著最近戰鬥傷痕的角鬥士擋住了您的去路。 | 0x13 | ✅ |
| SCRIPT06_SORRY_DONT_BUY | Sorry, but I don't want to buy that. | 抱歉，我不想買那個。 | 0x13 | ✅ |
| SCRIPT0_START_WARN | Starting a new game will destroy your last saved game. Do you still wish to start a new game? | 開始新遊戲會摧毀您最後儲存的遊戲。您仍然希望開始新遊戲嗎？ | 0x13 | ✅ |
| STRIPPED_OF_ALL_POSSESSIO | Stripped of all possessions and wealth, you've been dropped naked and defenseless into the slums of Purgatory by order of Namtar, the Beast From The Pit. | 你被深淵之獸納達下令剝奪所有財產和財富，赤身裸體、毫無防備地被丟進波卡城的貧民窟。 | 0x13 | ✅ |
| SCRIPT06_TARGET | Target... | 目標... | 0x13 | ✅ |
| SCRIPT03_PARTY_ADVANCES | The party advances. | 隊伍前進。 | 0x13 | ✅ |
| THIS_IS_THE_MAIN_GATE_TO_ | This is the main gate to Purgatory -- the gate through which you were unceremoniously dumped following your arrival in port. Guards are posted here. | 這是波卡城的大門——你被粗魯地丟進這座城市時就是通過這道門。這裡有守衛駐守。 | 0x13 | ✅ |
| THOSE_WORDS_HARDLY_FINISH | Those words hardly finish echoing in your ears as the gate slams shut behind you and the roar of the crowd raises to a cresendo. | 這些話在你耳中迴盪未已，身後的門已猛然關上，群眾的歡呼聲達到了高潮。 | 0x13 | ✅ |
| SCRIPT0_VIEW | View (name) | 查看（名字） | 0x13 | ✅ |
| SCRIPT0_NAME | What will (name)'s new name be? | （名字）的新名字是什麼？ | 0x13 | ✅ |
| WHO_WILL_CHOOSE_AN_ITEM | Who will choose an item? | 誰要選擇物品？ | 0x13 | ✅ |
| UI_WRITE_PROT | Write protected. | 防寫保護。 | 0x13 | ✅ |
| SCRIPT0_DELETE_WARN | You are about to delete (name). What has (name) done to deserve such a fate?? | 您即將刪除（名字）。（名字）做了什麼以至於落得如此下場？？ | 0x13 | ✅ |
| YOU_MAY_CHOOSE | You may choose | 你可以選擇 | 0x13 | ✅ |
| SCRIPT0_MUST_HAVE | You must have someone in the party to begin the game!! | 您的隊伍中必須有人才能開始遊戲！！ | 0x13 | ✅ |
| YOU_MUST_THEN_PROVE_YOURS | You must then prove yourselves to me! Defeat the dreaded Humbaba and you will know honor in my court. You will find him at the northeast corner of my realm. | 那麼你們必須向我證明自己！擊敗可怕的胡姆巴巴，你們將在我的宮廷中獲得榮譽。他就在我領土的東北角。 | 0x13 | ✅ |
| SCRIPT03_STILL_FACE | You still face | 您仍然面對 | 0x13 | ✅ |
| SCRIPT0_POINTS | You still have (N) points left to distribute, do you wish to go back and distribute them? | 您還有（N）點可以分配，您希望回去分配它們嗎？ | 0x13 | ✅ |
| YOU_WILL_FIND_HIM_AT_THE_ | You will find him at the northeast corner of my realm. | 你將在我領土的東北角找到他。 | 0x13 | ✅ |
| SCRIPT06_HAVE_TO_HAVE | You would have to have a | 您必須擁有 | 0x13 | ✅ |
| YOU_RE_FREE_TO_GO_YOUR_OW | You're free to go your own way in the city, but the guards will happily kick your spleen up through your teeth if you want to rush the gate. | 你可以在城裡自由行動，但如果你想硬闖大門，守衛會很樂意把你的脾臟從牙齒裡踢出來。 | 0x13 | ✅ |
| SCRIPT03_APPEAR | appear. | 出現了。 | 0x13 | ✅ |
| SCRIPT06_CANNOT_CARRY | can't carry any more. | 無法攜帶更多。 | 0x13 | ✅ |
| SCRIPT03_CHARGES | charges ahead! | 衝鋒向前！ | 0x13 | ✅ |
| SCRIPT03_DEEQUIPS | deequips the | 卸下了 | 0x13 | ✅ |
| SCRIPT06_DONT_HAVE_GOLD | doesn't have that much gold. | 沒有那麼多金幣。 | 0x13 | ✅ |
| SCRIPT03_EQUIPS | equips the | 裝備了 | 0x13 | ✅ |
| SCRIPT03_EXP | experience points | 經驗值 | 0x13 | ✅ |
| SCRIPT03_GOLD | gold | 金幣 | 0x13 | ✅ |
| SCRIPT03_GAINED_LEVEL | has gained a level! | 升級了！ | 0x13 | ✅ |
| SCRIPT03_HITS | hits | 命中 | 0x13 | ✅ |
| SCRIPT03_OUT_OF_RANGE | is out of range. | 超出範圍。 | 0x13 | ✅ |
| SCRIPT06_LEATHER_ARMOR | leather armor | 皮甲 | 0x13 | ✅ |
| UI_ESC_EXIT | or press ESC to return to MS-DOS. | 或按 ESC 返回 MS-DOS。 | 0x13 | ✅ |
| SCRIPT06_PLATE_CHAIN | plate and chain armor | 板甲和鎖子甲 | 0x13 | ✅ |
| SCRIPT03_POINT_DAMAGE | point\s\ of damage | 點傷害 | 0x13 | ✅ |
| SCRIPT03_RELOADS | reloads | 重新裝填 | 0x13 | ✅ |
| SCRIPT03_RETREATS | retreats back! | 撤退！ | 0x13 | ✅ |
| SCRIPT03_RUNS_AWAY | runs away! | 逃跑了！ | 0x13 | ✅ |
| SCRIPT03_TIME_FOR | time for | 時間用於 | 0x13 | ✅ |
| SCRIPT06_TO_USE | to use it. | 才能使用它。 | 0x13 | ✅ |

## 技能列表 (Skill List) — Section 0x14

| AMOUNT_COST | Amount Cost | 數量 費用 | 0x14 | ✅ |
| SKILL_AMOUNT_COST | Skill Amount Cost | 技能 數量 費用 | 0x14 | ✅ |

## 技能名稱 (Skill Names) — Section 0x15

| MOUNTAIN_LORE | Mountain Lore | 山地知識 | 0x15 | ✅ |
| THROWN_WEAPONS | Thrown weapons | 投擲武器 | 0x15 | ✅ |

## 遊戲結束 (Game Over) — Section 0x16

| ALAS_YOUR_BRAVE_PARTY_HAS | Alas, your brave party has met its match! Your current adventure is over. | 哀哉，你的勇敢隊伍遇到了勢均力敵的對手！你目前的冒險結束了。 | 0x16 | ✅ |

## 腳本文字 (Script Text) — Section SCRIPT

| T1016 | 's new name be? | 的新名字是？ | SCRIPT | ✅ |
| A_BREEZE_CRAWLS_IN_FROM_T | A breeze crawls in from the harbor, bearing a sickly stench. | 一陣微風從港灣吹來，帶著刺鼻的惡臭。 | SCRIPT | ✅ |
| A_GREAT_CHORUS_OF_VOICES_ | A great chorus of voices issues up from the west. | 一陣宏大的合唱聲從西方傳來。 | SCRIPT | ✅ |
| ADVANCE_AHEAD | Advance ahead | 向前推進 | SCRIPT | ✅ |
| AHEAD_LAY_ODD_WATERS | Ahead lay odd waters. | 前方是詭異的水域。 | SCRIPT | ✅ |
| T1001 | Are you sure you want to throw away the | 你確定要丟掉 | SCRIPT | ✅ |
| BEGIN_A_NEW_GAME | Begin a new game | 開始新遊戲 | SCRIPT | ✅ |
| T1002 | Block attack | 格擋攻擊 | SCRIPT | ✅ |
| T1003 | Buy an item | 購買物品 | SCRIPT | ✅ |
| T1004 | Buy which item? | 要購買哪個物品？ | SCRIPT | ✅ |
| BYE_BYE | Bye bye, | 再見， | SCRIPT | ✅ |
| T1005 | Carried items | 攜帶物品 | SCRIPT | ✅ |
| CAST_SPELL | Cast spell | 施法 | SCRIPT | ✅ |
| CONTINUE_AN_OLD_GAME | Continue an old game | 載入舊遊戲 | SCRIPT | ✅ |
| DO_YOU_FEEL_LUCKY | Do you feel lucky? | 你覺得幸運嗎？ | SCRIPT | ✅ |
| T1007 | Do you wish to deequip your | 你是否要卸下你的 | SCRIPT | ✅ |
| DO_YOU_WISH_TO_PRAY_TO_IR | Do you wish to pray to Irkalla? | 你要向伊爾卡拉祈禱嗎？ | SCRIPT | ✅ |
| T1006 | Dodge enemies | 閃避敵人 | SCRIPT | ✅ |
| EQUIP_THE | Equip the | 裝備 | SCRIPT | ✅ |
| T1008 | Examine which item? | 要察看哪個物品？ | SCRIPT | ✅ |
| T1009 | General overview | 一般概覽 | SCRIPT | ✅ |
| HAVE_MERCY | Have mercy. | 請大發慈悲。 | SCRIPT | ✅ |
| T1010 | Have mercy。 | 請大發慈悲。 | SCRIPT | ✅ |
| HOW_MUCH_GOLD_DOES | How much gold does | 多少金幣 | SCRIPT | ✅ |
| IRKALLA_IS | Irkalla is | 伊爾卡拉 | SCRIPT | ✅ |
| IS_FREEDOM_FROM_PURGATORY | Is freedom from Purgatory worth a long dive into what might be shallow water, then a desperate swim through the harbor? | 從波卡城脫身，值得你跳入可能很淺的水中，然後絕望地游過港灣嗎？ | SCRIPT | ✅ |
| T1011 | Magic spells... | 法術… | SCRIPT | ✅ |
| MALE_OR | Male or | 男性或 | SCRIPT | ✅ |
| T1012 | Nothing happens. | 什麼都沒發生。 | SCRIPT | ✅ |
| POOL_GOLD | Pool gold | 集中金幣 | SCRIPT | ✅ |
| POWER_FOR | Power for | 法力消耗 | SCRIPT | ✅ |
| QUICKLY_FIGHT | Quickly fight | 快速戰鬥 | SCRIPT | ✅ |
| T1013 | Saving game... | 儲存中… | SCRIPT | ✅ |
| T1014 | Sell an item | 出售物品 | SCRIPT | ✅ |
| T1015 | Sell which item? | 要出售哪個物品？ | SCRIPT | ✅ |
| SHARE_GOLD | Share gold | 分享金幣 | SCRIPT | ✅ |
| SORRY_BUT | Sorry but | 抱歉，但 | SCRIPT | ✅ |
| T1017 | That didn't do any good. | 那沒有起任何作用。 | SCRIPT | ✅ |
| T1018 | The chest remains locked! | 寶箱仍然上鎖！ | SCRIPT | ✅ |
| THE_CROWD_GROWS_WILD_WITH | The crowd grows wild with the hope of more victims. | 群眾因期待更多受害者而變得瘋狂。 | SCRIPT | ✅ |
| THE_GUARDS_TAKE_YOUR_GOLD | The guards take your gold as a penalty for losing the combat. | 守衛奪走你的金幣作為戰鬥失敗的懲罰。 | SCRIPT | ✅ |
| THE_PARTY_CLIMBS_THROUGH_ | The party climbs through the wall and dives into the cold waters of the sea. | 隊伍翻過城牆，跳入冰冷的海水中。 | SCRIPT | ✅ |
| THE_SEA_IS_COLD_AND_ROUGH | The sea is cold and rough. Only a good swimmer has a chance out here. | 大海寒冷而波濤洶湧。只有优秀的泳者才有機會在這裡生存。 | SCRIPT | ✅ |
| THE_STONE_WALLS_OF_PURGAT | The stone walls of Purgatory stand as a monument to shattered lives and broken dreams. | 波卡城的石牆是破碎生命與破碎夢想的紀念碑。 | SCRIPT | ✅ |
| THERE_S_A_GAP_IN_THE_CITY | There's a gap in the city wall. Far below, you see the water of the harbor through which you entered this dreaded isle. | 城牆上有個缺口。遠遠下方，你看到了你進入這座可怕島嶼時通過的港灣水域。 | SCRIPT | ✅ |
| TRADE_GOLD | Trade gold | 交易金幣 | SCRIPT | ✅ |
| TRADE_THE | Trade the | 交換 | SCRIPT | ✅ |
| T1019 | Trade the | 交易 | SCRIPT | ✅ |
| T1020 | Unequip the | 卸下 | SCRIPT | ✅ |
| T1021 | Use these commands? | 要使用這些指令嗎？ | SCRIPT | ✅ |
| T1023 | View the party | 查看隊伍 | SCRIPT | ✅ |
| T1022 | Viewing current party. | 目前隊伍一覽。 | SCRIPT | ✅ |
| WHAT_WILL | What will | 將要 | SCRIPT | ✅ |
| T1024 | What will | 請選擇 | SCRIPT | ✅ |
| T1025 | Which attribute... | 哪項屬性… | SCRIPT | ✅ |
| T1026 | Which item... | 哪個物品… | SCRIPT | ✅ |
| WHO_DOES | Who does | 誰要 | SCRIPT | ✅ |
| WHO_WILL_SACRIFICE_AN_ITE | Who will sacrifice an item? | 誰要犧牲物品？ | SCRIPT | ✅ |
| T1027 | You are about to delete | 你即將刪除 | SCRIPT | ✅ |
| T1028 | You can't recharge that. | 你無法充值那個物品。 | SCRIPT | ✅ |
| T1029 | You find an opened chest here. | 你發現這裡有一個已打開的寶箱。 | SCRIPT | ✅ |
| T1030 | You have found a locked chest. | 你發現了一個上鎖的寶箱。 | SCRIPT | ✅ |
| YOU_HEAR_THE_BLOODTHIRSTY | You hear the bloodthirsty howls of a great crowd from behind the wall to the north. | 你聽到北方牆後傳來一大群嗜血者的嚎叫。 | SCRIPT | ✅ |
| YOU_HEAR_THE_LUSTY_SHOUTS | You hear the lusty shouts of a large crowd coming from the east. | 你聽到東方傳來一大群人的歡呼聲。 | SCRIPT | ✅ |
| YOU_STAND_ABOVE_THE_DEAD_ | You stand above the dead remains of your opponents basking in glory as the crowd chants your names. | 你站在對手屍體上方，沐浴在榮耀中，群眾高喊著你的名字。 | SCRIPT | ✅ |
| T1031 | You still have | 你還有 | SCRIPT | ✅ |
| ZAP_THE | Zap! The | 啪！ | SCRIPT | ✅ |
| T1032 | Zap! The | 轟！ | SCRIPT | ✅ |