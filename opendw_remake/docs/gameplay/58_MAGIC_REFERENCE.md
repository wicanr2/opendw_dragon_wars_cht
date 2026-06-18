# 58 — 法術效果參考表(fraterrisus 攻略對齊)

> 來源:GameFAQs《Dragon Wars Guide and Walkthrough (PC)》by fraterrisus, v3.0(2025-10-21),Magic 章節(原檔 `org_magic/`,**第三方含浮水印,不入庫**;本檔僅整理其數據事實 + 出處)。
> 用途:法術效果落地的權威依據(比官方手冊完整,含召喚生物屬性、攻擊判定公式)。真值層級:**攻略=約定俗成 + 反編佐證,非官方臺灣手冊**;落地時標 `remake 設計(grounded 攻略)`。
> 對齊 `src/game/spells.{hpp,cpp}`。

## 法術攻擊判定(Zap / Debuff 類)

- 每目標一次攻擊擲骰,類近戰但:**AV 用 INT**、武器技能位置換成**魔法技能 ranks**。防禦方用一般 DV。
- 擲骰 `1d16+2`,roll-under 命中條件:`roll ≤ 12 + 魔法技能 ranks + AV(INT) − 防禦方 DV`。
- 傷害只擲一次;**「miss」的目標吃半傷**。回報值=所有命中/未命中的平均。
- 變動消耗(var.):可選花幾點 Power;**上限 = 2 × 該技能 ranks**(Sun Magic 3 → 最高 6 點 Inferno)。持續「Nhr/pt」的「小時」非真實時間,只表「花越多 Power 持續越久」。

## 召喚生物(Summon;填補空隊伍槽,需 <7 人;持續 4 小時/點)

| 法術 | 生物 | HP | Dex | Armor | 武器(傷害骰) |
|---|---|---:|---:|---|---|
| Air Summon 召喚風元素 | Air Element | 12 | 12 | Air Armor 8 | Air Talons 1d10 |
| Earth Summon 召喚地元素 | Earth Elemnt | 15 | 14 | Earth Shield 10 | Hammers 1d20 |
| Water Summon 召喚水元素 | Water Elemnt | 25 | 15 | Water Wings 11 | Waves 1d20 |
| Fire Summon 召喚火元素 | Fire Element | 35 | 18 | Fire Shield 15 | Flames 2d20 |
| Beast Call 呼叫野獸 | Beast | 13 | 16 | Fur 7 | Claws 1d12 |
| Invoke Spirit 召喚精靈 | Spirit | 13 | 18 | Aura Shield 12 | Ice Hands 3d10 |
| Wood Spirit 樹木精靈 | Wood Spirit | 19 | 16 | Bark 9 | Splinters 1d12 |
| Summon Salamander 召喚火蜥蜴 | Salamander | 23 | 24 | Scales 10 | Claws 1d10 |

> AV/DV 由 Dex 推(同怪物 DEX/4 慣例);無法奪取其裝備。

## Low Magic 初級魔法(波卡城魔法店免費)

| 法術 | POW | 目標 | 範圍 | 類型 | 效果 |
|---|---:|---|---|---|---|
| Charm 魅惑 | 3 | 1 | — | Buff | 治 1–4 hp、整場 +1 AV |
| Lesser Heal 次級治療 | 2 | 1 | — | Heal | 治 1d4 hp |
| Luck 幸運 | 3 | 1 | — | Buff | 整場 +2 DV |
| Mage Fire 法師火焰 | 2 | 1 | 30' | Zap | 1d8 傷 |
| Disarm 解除武裝 | 4 | 1 | 30' | Debuff | 卸目標武裝(部分怪不可卸) |
| Mage Light 法師魔光 | var. | — | — | Misc | 光源,3hr/pt |

## High Magic 高級魔法

| 法術 | POW | 目標 | 範圍 | 類型 | 效果 |
|---|---:|---|---|---|---|
| Healing 治療 | 3 | 1 | — | Heal | 1d6 hp |
| Group Heal 群體治療 | 6 | party | — | Heal | 1d6 hp |
| Mystic Might 神祕之力 | 4 | 1 | — | Buff | 整場 +15 STR |
| Sala's Swift | 8 | 1 | — | Buff | 整場 +8 DEX |
| Vorn's Guard | 6 | party | — | Buff | 整場 +2 AC |
| Cloak Arcane 神祕斗篷 | var. | party | — | Buff | +2 AC,持續 1hr/pt |
| Fire Light | var. | 1 | 30' | Zap | 1d6 hp/pt |
| Ice Chill 冰寒 | var. | 1 | 50' | Zap | 1d4 hp/pt |
| Elvar's Fire | 6 | group | 30' | Zap | 2d6 hp |
| Poog's Vortex | 11 | group | 20' | Zap | 4d6 hp |
| Big Chill | 15 | all | 30' | Zap | 4d6 hp |
| Reveal Glamour | 2 | — | 40' | Misc | 破幻象(僅魔法學院測驗有用) |
| Sense Traps 感知陷阱 | var. | — | — | Misc | 無視陷阱,2hr/pt |
| Dazzle 眩目 | 3 | 1 | 30' | Debuff | 敵下回合 miss |
| Cowardice 怯懦 | 8 | group | 60' | Debuff | 敵群逃跑 |
| Air/Water/Earth/Fire Summon | var. | — | — | Summon | 見召喚表 |

## Druid Magic 德魯伊魔法

| 法術 | POW | 目標 | 範圍 | 類型 | 效果 |
|---|---:|---|---|---|---|
| Greater Healing 高階治療 | 4 | 1 | — | Heal | 1d6 hp |
| Cure All 全體治癒 | 6 | party | — | Heal | 1d8 hp(最佳群補) |
| Scare 恐嚇 | 4 | party | 20' | Buff | 整場 +2 AV(命中對手群即生效,適用全體) |
| Death Curse 死亡詛咒 | 6 | 1 | 40' | Zap | 3d6 hp |
| Fire Blast | 12 | group | 30' | Zap | 4d6 hp |
| Insect Plague 蟲災 | 4 | group | 60' | Debuff | 整場 −2 AV、−2 DV |
| Whirl Wind 龍捲風 | 4 | group | 40' | Debuff | 把敵群推後 30' |
| Brambles 荊棘 | 5 | group | 60' | Debuff | 敵下回合 miss |
| Create Wall 建立石牆 | 5 | — | — | Misc | 修復泥神神廟用 |
| Soften Stone 軟化石頭 | 6 | — | — | Misc | 通過地牢障礙 |
| Beast Call / Wood Spirit / Invoke Spirit | var. | — | — | Summon | 見召喚表 |

## Sun Magic 太陽魔法

| 法術 | POW | 目標 | 範圍 | 類型 | 效果 |
|---|---:|---|---|---|---|
| Sun Light 陽光 | 3 | 1 | — | Heal | 1d6 hp |
| Heal 治療 | 4 | 1 | — | Heal | 1d8 hp |
| Major Healing 高階治療 | 6 | party | — | Heal | 1d6 hp |
| Holy Aim 神聖瞄準 | 5 | party | — | Buff | 整場 +2 AV |
| Battle Power 戰鬥之力 | 8 | party | — | Buff | 整場 +10 STR |
| Mithras' Bless | 5 | party | — | Buff | 整場 +3 DV |
| Armor of Light 光之護甲 | 6 | 1 | — | Buff | 整場 +2 DV(手冊寫 AC 是錯的) |
| Sun Stroke 中暑 | var. | 1 | 20' | Zap | 1d8 hp/pt |
| Rage of Mithras | var. | 1 | 70' | Zap | 1d6 hp/pt |
| Exorcism 驅魔 | 5 | group | 50' | Zap | 6d6 hp(僅不死) |
| Inferno 地獄火 | var. | all | 40' | Zap | 1d4 hp/pt(全場最佳 zap) |
| Wrath of Mithras | var. | group | 90' | Zap | 1d4 hp/pt(玩家學不到,怪會用) |
| Fire Storm 火焰風暴 | 20 | all | 60' | Zap | 6d6 hp |
| Charger 補給 | 8 | — | — | Misc | 為法術物品加 1 charge |
| Disarm Trap 解除陷阱 | var. | — | — | Misc | 無視陷阱,2hr/pt |
| Guidance 導引 | var. | — | — | Misc | UI 加指南針,3hr/pt |
| Radiance 光輝 | var. | — | 40' | Misc | 光源,2hr/pt(比 Mage Light 範圍長) |
| Column of Fire 火柱 | 5 | group | 40' | Debuff | 阻止敵群前進(一次) |
| Light Flash | 6 | group | 50' | Debuff | 敵下回合 miss(彩蛋) |
| Summon Salamander 召喚火蜥蜴 | var. | — | — | Summon | 見召喚表 |

## Miscellaneous Magic 雜項魔法(任何 Low Magic 持有者可學用)

| 法術 | POW | 目標 | 範圍 | 類型 | 效果 |
|---|---:|---|---|---|---|
| Zak's Speed | 10 | party | — | Buff | 整場 +15 DEX |
| Kill Ray 致命光線 | 15 | 1 | 50' | Zap | 10–80 hp |
| Prison 牢籠 | 8 | group | 60' | Buff/Debuff | 整場阻止目標前進 |

## 落地對照(spells.cpp）

- amount/dice 一律以本表為準回填(原 TODO/amount=0 者);變動消耗法術 amount 表「每點」量(如 Inferno 1d4/pt)。
- Zap/Debuff 走法術攻擊判定(INT-AV、1d16+2、miss 半傷);Heal/Buff 不需判定。
- 召喚 → 加臨時友方 combatant(屬性照召喚表)。
- Misc 工具類(Mage Light/Radiance/Guidance/Create Wall/Charger)= 探索態效果,多數需地圖/UI 對接(部分受阻於真實 .lvl 觸發點,誠實標示)。
