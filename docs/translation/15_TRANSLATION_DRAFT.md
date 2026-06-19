# 15_TRANSLATION_DRAFT.md — 火龍之戰繁體中文翻譯草表

> **版本**：v0.1（2026-06-10，乾淨重建）
> **來源**：`ALL_TEXT_FROM_SCRIPTS.txt`（主）、`CONTEXT.md`（術語）、`26_MONSTERS_AND_SPRITES.md`（怪物名）、`33_MANUAL_TRANSCRIPTION.md`（參考語境）
> **不使用**：`_deprecated/` 目錄下任何舊檔

---

## 翻譯原則

1. **術語優先**：所有專有名詞一律依 `CONTEXT.md`，不得自行改譯。
   - 例：Purgatory → 波卡城（地名）/ 罪惡之城（敘述）；Namtar → 納達；Nergal → 奈羅（≠ 納達）
2. **碎片組句**：遊戲文字以「片段 + 變數」在英語語序下拼接。各片段照樣翻，但若直接組合後中文不通順，**標 `⚠語序`** 並說明理想的中文組合方式。
3. **佔位符約定**：
   - 角色名 → `{名字}`
   - 數字/數值 → `{數字}`
   - 物品名 → `{物品}`
   - 怪物名 → `{怪物}`
   - 複數/複合 → 依語境判斷是否需要助詞調整
4. **不確定譯法**：標 `〔?〕`。需要上下文才能決定的譯法，列到檔末「待確認」節。
5. **空行/格式字串**：原文若為純格式字元（括號、換行符）則維持原樣，不翻。
6. **Interplay 公司名**：保留原文，不翻。

---

## 一、主選單 / 角色建立（script0）

> 遊戲開機進入的第一個畫面；包含主選單、隊伍管理、角色建立、命名確認流程。

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| Interplay | Interplay | script0 | 公司名，保留原文 |
| Do you wish to.. | 你想要… | script0 | 主選單提示語 |
| Begin a new game | 開始新遊戲 | script0 | |
| Continue an old game | 繼續舊遊戲 | script0 | |
| Starting a new game will destroy your last saved game. Do you still wish to start a new game? | 開始新遊戲將刪除上次的存檔。你確定要開始新遊戲嗎？ | script0 | |
| Current party... | 目前隊伍… | script0 | |
| Create character | 建立角色 | script0 | 依 CONTEXT.md：Create → 建立 |
| Begin the game | 開始遊戲 | script0 | |
| ) | ) | script0 | 格式字元，保留 |
| Do you wish to.. | 你想要… | script0 | 角色選擇選單（重複使用） |
| Delete {名字} | 刪除{名字} | script0 | ⚠語序：片段1，見下方組合說明 |
| Rename {名字} | 重新命名{名字} | script0 | ⚠語序：片段2，見下方組合說明 |
| View {名字} | 檢視{名字} | script0 | ⚠語序：片段3，見下方組合說明 |
| What will | {名字}的新名字要叫什麼？ | script0 | ⚠語序：此為片段，與下一行組合 |
| 's new name be? | （與上行合用） | script0 | ⚠語序：原文拆成「What will ␣[名字]␣ 's new name be?」，中文組合：「{名字}的新名字要叫什麼？」→ 變數在中文句中提前 |
| You are about to delete | 你即將刪除 | script0 | ⚠語序：片段，見下方組合說明 |
| . What has | 。{名字}做了什麼 | script0 | ⚠語序：片段 |
|  done to deserve such a fate?? | 竟要受此命運？？ | script0 | ⚠語序：三片段組合原文：「You are about to delete ␣{名字}␣. What has ␣{名字}␣ done to deserve such a fate??」→ 中文：「你即將刪除{名字}。{名字}做了什麼，竟要落得如此下場？？」|
| Delete {名字} forever. | 永遠刪除{名字}。 | script0 | 確認刪除按鈕文字 |
| Have mercy. | 留他一命。 | script0 | 取消刪除按鈕文字 |
| Bye bye, {名字} | 再見了，{名字} | script0 | 刪除後顯示 |
| Name your new character. | 為你的新角色命名。 | script0 | |
| Is | {名字}是 | script0 | ⚠語序：性別選擇片段，與下行組合 |
| Male or | 男性還是 | script0 | ⚠語序：片段 |
| Female? | 女性？ | script0 | ⚠語序：三片段組合原文：「Is ␣{名字}␣ Male or Female?」→ 中文：「{名字}是男性還是女性？」→ 變數在中文句中提前 |
| You still have {數字} points left to distribute, do you wish to go back and distribute them? | 你還有 {數字} 點可以分配，是否要返回分配？ | script0 | |
| You must have someone in the party to begin the game!! | 隊伍中至少要有一位角色才能開始遊戲！！ | script0 | |
| Loading... | 載入中… | script0 | 依 CONTEXT.md |

### ⚠語序說明（script0 碎片組句）

**組合句 A：Delete / Rename / View 選單**
- 原文結構：選單列表中，遊戲先顯示序號（`)`），再接 `Delete ` / `Rename ` / `View ` + `{名字}`，即「`1) Delete Alice`」。
- 中文呈現：`1) 刪除 Alice` / `1) 重新命名 Alice` / `1) 檢視 Alice`。
- **語序正常，無需調整**，但 opcode 實作時需確認「動詞 + 名字」的順序與中文習慣一致。

**組合句 B：「What will ___ 's new name be?」**
- 原文切成兩段：`What will ` + `{名字}` + `'s new name be?`
- 直接組合中文：`「要叫什麼」+ {名字} + `「的新名字」` → 不通順
- **理想組合**：`「{名字}的新名字要叫什麼？」`
- Remake 實作建議：把這兩個片段合併為單一字串 `「{名字}的新名字要叫什麼？」`，或調整 opcode 使變數插入位置前移。

**組合句 C：「You are about to delete ___ . What has ___ done to deserve such a fate??」**
- 原文切成三段：`You are about to delete ` + `{名字}` + `. What has ` + `{名字}` + ` done to deserve such a fate??`
- **理想組合**：`「你即將刪除{名字}。{名字}做了什麼，竟要落得如此下場？？」`
- 語序尚可，但「. What has」這個片段在中文句中需重寫為「。{名字}做了什麼」，變數需再次出現。

**組合句 D：「Is ___ Male or Female?」**
- 原文切成三段：`Is ` + `{名字}` + （換行）`Male or` + （換行）`Female?`
- 中文順序為「{名字}是男性還是女性？」，變數位置需從中間移到句首。
- Remake 實作建議：合併為「`{名字}是男性還是女性？`」。

---

## 二、戰鬥訊息（script03）

> 戰鬥解析結果、升級通知、移動/逃跑/攻擊訊息。

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| {怪物} appear. | {怪物}出現了。 | script03 | 遭遇戰開始；片段，前面有複數處理 |
| You still face {怪物}. | 你們仍面對{怪物}。 | script03 | 敵人尚未擊倒 |
| , | ， | script03 | 格式分隔，組句用 |
| a {怪物} | 一個{怪物} | script03 | 不定冠詞轉換為中文計量詞〔?〕依怪物種類而異 |
| an {怪物} | 一個{怪物} | script03 | 同上 |
| Each member gets {數字} experience points and {數字} gold for combat. | 每位成員獲得 {數字} 點經驗值與 {數字} 枚金幣。 | script03 | ⚠語序：原文片段切法：`Each member gets ` + `{數字}` + ` experience points ` + `and ` + `{數字}` + ` gold ` + `for combat.` |
| {名字} has gained a level! | {名字}升級了！ | script03 | |
| b. | b. | script03 | 格式字元（選項 b） |
| The party advances. | 隊伍前進。 | script03 | |
| {名字} runs away! | {名字}逃跑了！ | script03 | |
| {名字} deequips the {物品} | {名字}卸下了{物品} | script03 | 依 CONTEXT.md：deequip → 卸下 |
| {名字} reloads | {名字}重新裝填 | script03 | 依 CONTEXT.md：reload → 裝填 |
| {名字} equips the {物品} | {名字}裝備了{物品} | script03 | 依 CONTEXT.md：equip → 裝備 |
| {名字} charges ahead! | {名字}衝鋒向前！ | script03 | |
| {名字} retreats back! | {名字}後退撤離！ | script03 | |
| {名字} is out of range. | {名字}不在攻擊範圍內。 | script03 | |
| hits {名字} {數字} time for {數字} point of damage | 命中{名字} {數字}次，造成 {數字} 點傷害 | script03 | ⚠語序：此為攻擊命中片段，組合方式見下方說明 |
| , killing {名字} | ，擊殺了{名字} | script03 | 命中片段的結尾追加 |
| fails to do any damage. | 未能造成任何傷害。 | script03 | |
| the attack is blocked! | 攻擊被格擋了！ | script03 | |
| misses. | 未命中。 | script03 | |
| {名字} shoots {名字} | {名字}射擊{名字} | script03 | 遠程攻擊片段 |
| and | 並 | script03 | 連接片段 |
| \hitstuns {名字} {數字} time for {數字} point of damage | 擊暈了{名字} {數字}次，造成 {數字} 點傷害 | script03 | ⚠語序：\hitstuns 為特殊標記，依 CONTEXT.md：stun → 暈眩 |
| , stunning {名字} | ，使{名字}暈眩 | script03 | |
| fails to do any damage. | 未能造成任何傷害。 | script03 | 遠程版（重複） |
| the attack is blocked! | 攻擊被格擋了！ | script03 | 遠程版（重複） |
| misses. | 未命中。 | script03 | 遠程版（重複） |
| {名字} attacks {名字} | {名字}攻擊{名字} | script03 | 近戰攻擊 |
| and | 並 | script03 | 連接片段 |
| {名字} picks up {名字} weapon. | {名字}撿起了{名字}的武器。 | script03 | ⚠語序：「picks up __ weapon」→「撿起了__的武器」 |
| The {怪物} advances {數字}' feet. | {怪物}前進了 {數字} 呎。 | script03 | 怪物移動 |
| {名字} waits for you. | {名字}等待著你。 | script03 | |
| breathes | 吐息 | script03 | 龍/怪物吐息動作片段〔?〕完整句需上下文 |

### ⚠語序說明（script03 攻擊命中組合句）

**命中組合句（近戰）**：
- 原文結構推測：`{攻擊者}` + `attacks` + `{被攻擊者}` + ` and ` + `hits` + `{被攻擊者}` + ` {數字} time for {數字} point of damage` + （可選）`, killing {名字}`
- 中文理想：「{攻擊者}攻擊{被攻擊者}，命中 {數字} 次，造成 {數字} 點傷害（，擊殺了{名字}）。」
- `time` / `times`（單複數）：中文不需要，直接用「次」即可。
- `point of damage` / `points of damage`（單複數）：中文不需要，直接用「點傷害」。

---

## 三、法術/技能 UI（script06 + script12 + script13 + script15）

### 3-A 法術使用結果（script06）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| {名字} doesn't have enough spell power. | {名字}的法力不足。 | script06 | 依 CONTEXT.md：Power → 法力 |
| but it didn't seem to work. | 但似乎沒有效果。 | script06 | 片段，前接施法失敗描述 |
| Nothing happens. | 什麼事都沒發生。 | script06 | |
| That didn't do any good. | 那沒有任何幫助。 | script06 | |
| {名字} bandages {名字} and now {名字} feels better. | {名字}幫{名字}包紮，{名字}感覺好多了。 | script06 | ⚠語序：原文三個名字佔位，中文語序相同，但「now」不需翻出 |
| at the {數字} monster | 對 {數字} 隻怪物 | script06 | ⚠語序：法術攻擊片段，見下方組合說明 |
| and hits {數字} of them | 命中其中 {數字} 隻 | script06 | 片段 |
| for {數字} point of damage each | 各造成 {數字} 點傷害 | script06 | 片段 |
| , killing {數字} all of them. | ，消滅了全部 {數字} 隻。〔?〕 | script06 | ⚠語序：`killing {數字} all of them` 結構不標準，可能為`killing all of them`或`killing {數字} of them`，待確認 |
| , killing {數字} of them. | ，消滅了其中 {數字} 隻。 | script06 | 片段 |
| at the party | 對我方隊伍 | script06 | 敵方法術目標片段 |
| and hits {數字} of you | 命中你們其中 {數字} 人 | script06 | 片段 |
| for {數字} point of damage each | 各造成 {數字} 點傷害 | script06 | 片段（重複） |
| and the party is | 而隊伍 | script06 | ⚠語序：片段，後接狀態描述 |
| is | 〔角色〕 | script06 | ⚠語序：單人版，前接名字 |
| imprisoned! | 被監禁了！ | script06 | 狀態後綴 |
| and the party is | 而隊伍 | script06 | 片段（重複） |
| is | 〔角色〕 | script06 | 片段（重複） |
| pushed away! | 被推開了！ | script06 | 狀態後綴 |
| Which... | 選擇… | script06 | 選單提示 |
| Zap! The {物品} | 嗶！{物品} | script06 | 〔?〕法術裝備充能效果，「Zap!」為音效詞，譯法待確認 |
| You can't recharge that. | 那個無法充能。 | script06 | |
| but the spell couldn't reach! | 但法術施放距離不夠遠！ | script06 | 片段 |
| , but it had no effect at all! | ，但完全沒有任何效果！ | script06 | 片段 |
| and a {物品} | 以及一個{物品} | script06 | 片段，戰利品列表 |

### ⚠語序說明（script06 法術攻擊組合句）

**法術攻擊組合句**：
- 推測原文：`{施法者}` + `{法術名}` + ` at the {數字} monster` + ` and hits {數字} of them` + ` for {數字} point of damage each` + `, killing {數字} of them.`
- 中文理想：「{施法者}對 {數字} 隻怪物施展{法術名}，命中其中 {數字} 隻，各造成 {數字} 點傷害，消滅了其中 {數字} 隻。」
- **語序基本一致**，但中文需省略 `the`，並在句尾添加句號。

### 3-B 法術選單（script12）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| Which... | 選擇… | script12 | 選單提示（重複） |
| {名字} Magic | {名字}魔法 | script12 | 魔法類型標題，{名字}為 Low/High/Druid/Sun/Misc 等 |
| {名字} only has {數字} point of magic power. | {名字}只有 {數字} 點法力。 | script12 | |
| {名字} has no spells. | {名字}沒有任何法術。 | script12 | |
| Which type... | 選擇種類… | script12 | |
| Which spell... | 選擇法術… | script12 | |
| Power for {法術名} spell... | {法術名}的消耗法力… | script12 | ⚠語序：「Power for X spell」→「X 的消耗法力」，依 CONTEXT.md：Power 此處為法力消耗 |
| {數字} points maximum. | 最多 {數字} 點。 | script12 | |

### 3-C 角色狀態查看（script13）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| {名字}'s status. | {名字}的狀態。 | script13 | |
| View... | 查看… | script13 | 依 CONTEXT.md：View → 檢視；手冊版用「查看」，採手冊 |
| General overview | 一般狀態 | script13 | 依手冊第 10 頁 |
| Abilities | 能力 | script13 | 依手冊第 10 頁 |
| Low | 初級 | script13 | 魔法類型前綴，依 CONTEXT.md |
| High | 高級 | script13 | 魔法類型前綴，依 CONTEXT.md |
| Druid | 德魯伊 | script13 | 魔法類型前綴，依 CONTEXT.md |
| Sun | 太陽 | script13 | 魔法類型前綴，依 CONTEXT.md |
| Misc | 雜項 | script13 | 依 CONTEXT.md：Miscellaneous → 雜項 |
| {類型} magic | {類型}魔法 | script13 | 與上方前綴組合：「初級魔法」「高級魔法」等 |
| {名字}'s statistics. | {名字}的屬性。 | script13 | |
| Str: | 力量： | script13 | 依 CONTEXT.md |
| Dex: | 敏捷： | script13 | 依 CONTEXT.md |
| Int: | 智力： | script13 | 依 CONTEXT.md |
| Spr: | 精神： | script13 | 依 CONTEXT.md |
| Attack: | 攻擊： | script13 | 依 CONTEXT.md |
| Defense: | 防禦： | script13 | 依 CONTEXT.md |
| Level: | 等級： | script13 | 依 CONTEXT.md |
| AC: | 防禦等級： | script13 | 依 CONTEXT.md：AC → 防禦等級 |
| Health | 生命 | script13 | 依 CONTEXT.md |
| Stun | 暈眩值 | script13 | 依 CONTEXT.md |
| Power: | 法力： | script13 | 依 CONTEXT.md：Power → 法力（屬性語境） |
| Exp | 經驗 | script13 | 依 CONTEXT.md |
| Carried items | 攜帶物品 | script13 | |
| A) Gold | A) 金幣 | script13 | 依 CONTEXT.md |
| -- | -- | script13 | 格式字元 |
| # | # | script13 | 格式字元（物品序號） |
| {名字} has no in gold, do you wish to... | {名字}沒有金幣，你是否要… | script13 | ⚠語序：「has no in gold」結構疑為「has no gold」，「in」可能是片段分隔，待確認 |
| {名字} has {數字} gold, do you wish to... | {名字}有 {數字} 枚金幣，你想要… | script13 | ⚠語序：「has `` in `` gold」三片段組合，見下方說明 |
| Pool gold | 集中金幣 | script13 | 依 CONTEXT.md |
| Share gold | 平分金幣 | script13 | 依 CONTEXT.md |
| Trade gold | 交易金幣 | script13 | 依 CONTEXT.md |
| How much gold does {名字} want to trade? | {名字}想要交易多少金幣？ | script13 | ⚠語序：「How much gold does __ want to trade?」→ 主語後置為「{名字}想要交易多少金幣？」 |
| {名字} doesn't have that much gold. | {名字}沒有那麼多金幣。 | script13 | |
| Who does {名字} want to give the gold to? | {名字}想把金幣給誰？ | script13 | ⚠語序：「Who does __ want to give the gold to?」→「{名字}想把金幣給誰？」，主語前移 |
| Your chains encumber you. | 你身上的鎖鏈令你行動不便。 | script13 | |
| This item cannot be transferred. | 此物品無法轉移。 | script13 | |
| {物品}, do you wish to... | {物品}，你是否要… | script13 | 物品操作選單 |
| Trade the {物品} | 交易{物品} | script13 | 依 CONTEXT.md：Trade → 交易 |
| Discard the {物品} | 丟棄{物品} | script13 | 依 CONTEXT.md：Discard → 丟棄 |
| Equip the {物品} | 裝備{物品} | script13 | 依 CONTEXT.md：Equip → 裝備 |
| Unequip the {物品} | 卸下{物品} | script13 | 依 CONTEXT.md：Unequip → 卸下 |
| You must equip a weapon that uses the {物品} | 你必須裝備一件使用{物品}的武器 | script13 | 〔?〕「uses the」後接彈藥/輔助物品 |
| Are you sure you want to throw away the {物品} | 你確定要丟棄{物品}嗎 | script13 | |
| Who does {名字} want to give the {物品} | {名字}想把{物品}給誰 | script13 | ⚠語序：「Who does __ want to give the __」→「{名字}想把{物品}給誰」，主語前移 |
| Sorry but {名字} can't carry any more. | 抱歉，{名字}已無法攜帶更多物品。 | script13 | |
| {名字} now has the {物品}. | {名字}現在持有{物品}。 | script13 | |
| {名字}'s skills... | {名字}的技能… | script13 | |
| {名字}'s High | {名字}的高級 | script13 | 法術清單標題片段 |
| {名字}'s Druid | {名字}的德魯伊 | script13 | 法術清單標題片段 |
| {名字}'s Sun | {名字}的太陽 | script13 | 法術清單標題片段 |
| {名字}'s Miscellaneous | {名字}的雜項 | script13 | 法術清單標題片段 |
| {名字} Magic spells... | {名字}魔法法術… | script13 | ⚠語序：與上方組合為「{角色名}的高級魔法法術…」 |
| {名字} doesn't have any. | {名字}沒有任何法術。 | script13 | |

### ⚠語序說明（script13 金幣相關）

**金幣查詢組合句**：
- 原文片段：`{名字} has ` + `{數字}` + ` in` + ` gold, do you wish to...`（以及 `has ` + `no` + ` in` + ` gold` 的無金幣版本）
- `in` 可能是 `0` 片段（當數字為空）和 `gold` 之間的分隔，或者是「…in gold」（以金幣計）的用法。
- 中文統一為：「{名字}有 {數字} 枚金幣，你想要…」/ 「{名字}沒有金幣，你想要…」

### 3-D 系統/暫停（script15）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| Do you wish to quit the game? | 你確定要離開遊戲嗎？ | script15 | |
| Do you wish to save your game? | 你是否要儲存遊戲？ | script15 | |
| Saving game... | 儲存遊戲中… | script15 | 依 CONTEXT.md |
| Your game is saved. | 遊戲已儲存。 | script15 | |
| The game is paused | 遊戲已暫停 | script15 | 依 CONTEXT.md |

---

## 四、物品/戰利品（script11）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| {名字} gets the {物品} | {名字}拿到了{物品} | script11 | |
| {名字} can't carry any more. | {名字}已無法攜帶更多物品。 | script11 | |
| Who will get loot? | 誰要拿取戰利品？ | script11 | 依 CONTEXT.md：loot → 戰利品 |
| Which item... | 選擇物品… | script11 | 依 CONTEXT.md：item → 物品 |
| ) | ) | script11 | 格式字元 |
| Gold ({數字}) | 金幣（{數字}） | script11 | 依 CONTEXT.md：Gold → 金幣 |

---

## 五、戰鬥選單（script18）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| Will the party: | 隊伍要採取何種行動？ | script18 | 遭遇戰選單標題 |
| Fight | 戰鬥 | script18 | 依 CONTEXT.md |
| Quickly fight | 快速戰鬥 | script18 | 依 CONTEXT.md |
| Run | 逃跑 | script18 | 依 CONTEXT.md |
| Advance ahead | 前進 | script18 | 依 CONTEXT.md：Advance → 前進 |
| {名字}, choose: | {名字}，選擇行動： | script18 | 個人行動選單 |
| Attack | 攻擊 | script18 | 依 CONTEXT.md |
| Dodge enemies | 閃避敵人 | script18 | 依 CONTEXT.md |
| Block attack | 格擋攻擊 | script18 | 依 CONTEXT.md |
| Cast spell | 施法 | script18 | 依 CONTEXT.md |
| Use item | 使用物品 | script18 | 依 CONTEXT.md |
| New weapon | 換武器 | script18 | 〔?〕「New weapon」可能為「換持另一武器」 |
| Load weapon | 裝填武器 | script18 | 依 CONTEXT.md：Load weapon → 裝填 |
| Run | 逃跑 | script18 | （重複） |
| Move | 移動 | script18 | |
| View the party | 查看隊伍 | script18 | |
| Viewing current party. | 正在查看目前隊伍。 | script18 | |
| Use these commands? | 使用這些命令嗎？ | script18 | |
| {名字} moves... | {名字}移動… | script18 | |
| Ahead | 向前 | script18 | 方向選項 |
| Behind | 向後 | script18 | 方向選項 |
| Do you wish to deequip your {物品} | 你是否要卸下你的{物品} | script18 | 依 CONTEXT.md：deequip → 卸下 |
| The {物品} | {物品} | script18 | 片段，需上下文 |

---

## 六、競技場/劇情對話（script19 + script71）

### 6-A 競技場（script19）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| Do you wish to enter the arena? | 你是否要進入競技場？ | script19 | 依 CONTEXT.md：arena → 競技場 |
| Come back when you are ready to face the challenge of combat! | 等你準備好迎戰時再回來！ | script19 | |
| Several gladiators bearing recent battle scars block your way. "You may only enter once!" | 幾個身負近日傷疤的鬥士擋住了你的去路。「你們只能進場一次！」 | script19 | 依 CONTEXT.md：gladiator → 鬥士 |
| "Excellent!" says the guard, "And I see that you are in need of some more equipment." | 「太好了！」守衛說道，「我看你們似乎還需要一些裝備。」 | script19 | |
| You may choose {數字} items. Who will choose an item? | 你們可以選擇 {數字} 件物品。誰來選一件物品？ | script19 | |
| "Don't forget to equip your items", says the guard. | 「別忘了裝備你們的物品，」守衛說道。 | script19 | |
| all have brought your own equipment." | 都自備了裝備。」 | script19 | ⚠語序：片段，前接「我看你們」或「很好，你們」，待確認完整句 |
| "Let the combat commence!" | 「讓戰鬥開始吧！」 | script19 | |
| Those words hardly finish echoing in your ears as the gate slams shut behind you and the roar of the crowd raises to a cresendo. | 話音未落，身後的門轟然關上，看台上的喧囂聲浪已如排山倒海而來。 | script19 | cresendo 為 crescendo（漸強）的拼寫錯誤，原文保留 |
| This is the main gate to Purgatory -- the gate through which you were unceremoniously dumped following your arrival in port. Guards are posted here. | 這是通往波卡城的主城門——你們抵達港口後就是從這扇門被粗暴地丟進來的。門口有守衛駐守。 | script19 | 依 CONTEXT.md：Purgatory → 波卡城 |
| You're free to go your own way in the city, but the guards will happily kick your spleen up through your teeth if you want to rush the gate. | 你可以在城中自由行動，但若想硬闖城門，守衛會很樂意把你的脾臟踢進你喉嚨裡。 | script19 | 保留原文誇張語氣 |
| Stripped of all possessions and wealth, you've been dropped naked and defenseless into the slums of Purgatory by order of Namtar, the Beast From The Pit. | 你們被剝奪了所有財物，赤身裸體、手無寸鐵地被丟進波卡城的貧民窟——這是奉深淵之獸納達之命。 | script19 | 依 CONTEXT.md：Namtar → 納達；the Beast From The Pit → 深淵之獸 |
| No one escapes Purgatory alive, and few know the luxury to die in bed within her walls. | 沒有人能活著逃出波卡城，而能在城牆之內安然死於床榻者也寥寥無幾。 | script19 | |
| "It is an honor to have you amongst us, O mighty victors of the great Humbaba! Accept this gold as a token of our gratitude!" | 「能有你們加入我們，實乃榮幸，偉大的胡姆巴巴征服者！請接受這些金幣作為我們感謝的象徵！」 | script19 | 依 CONTEXT.md：Humbaba → 胡姆巴巴 |
| "Enjoy your meager existence amongst us!" | 「就在我們之中苟延殘喘吧！」 | script19 | |
| Clopin Trouillefou growls, "There are no honest men in the Court of Miracles. You will be punished unless a thief, beggar, or tramp!" | 克洛潘·特魯伊弗咆哮道：「奇蹟宮廷沒有誠實之人。除非你是小偷、乞丐或流浪漢，否則你將受到懲罰！」 | script19 | 依 CONTEXT.md：Clopin Trouillefou → 克洛潘·特魯伊弗；Court of Miracles → 奇蹟宮廷 |
| Do you describe yourselves as thieves, beggars, or tramps? | 你們自認是小偷、乞丐或流浪漢嗎？ | script19 | |
| You must then prove yourselves to me! Defeat the dreaded Humbaba and you will know honor in my court. You will find him at the northeast corner of my realm. | 那你們就必須向我證明！擊敗令人聞風喪膽的胡姆巴巴，你們便可在我的宮廷中得到榮耀。你們可以在我領地的東北角找到他。 | script19 | 依 CONTEXT.md：Humbaba → 胡姆巴巴 |

### 6-B 劇情場景（script71）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| There's a gap in the city wall. Far below, you see the water of the harbor through which you entered this dreaded isle. | 城牆上有一處缺口。遠遠在下方，你看見了當初進入這座令人生畏的孤島時所通過的港口水域。 | script71 | |
| Is freedom from Purgatory worth a long dive into what might be shallow water, then a desperate swim through the harbor? | 波卡城的自由，值得你縱身躍入那深度不明的水中，再拼死游過港口嗎？ | script71 | 依 CONTEXT.md：Purgatory → 波卡城 |
| Do you feel lucky? | 你覺得自己運氣夠好嗎？ | script71 | |
| The party climbs through the wall and dives into the cold waters of the sea. | 隊伍翻過城牆，縱身躍入冰冷的海水之中。 | script71 | |
| The stone walls of Purgatory stand as a monument to shattered lives and broken dreams. | 波卡城的石牆，是無數破碎生命與幻滅夢想的永恆見證。 | script71 | 依 CONTEXT.md：Purgatory → 波卡城 |
| The sea is cold and rough. Only a good swimmer has a chance out here. | 海水冰冷而洶湧。在這裡，只有游泳高手才能有一線生機。 | script71 | |
| Do you wish to pray to Irkalla? | 你是否要向伊爾卡拉祈禱？ | script71 | 依 CONTEXT.md：Irkalla → 伊爾卡拉 |
| Who will sacrifice an item? | 誰要獻祭物品？ | script71 | |
| Irkalla is silent. | 伊爾卡拉沉默不語。 | script71 | ⚠語序：「Irkalla is `` silent.」為兩片段組合，`silent.` / `pleased.` 為不同結尾 |
| Irkalla is pleased. | 伊爾卡拉感到滿意。 | script71 | ⚠語序：同上組合句 |
| Ahead lay odd waters. | 前方是詭異的水域。 | script71 | |
| You stand above the dead remains of your opponents basking in glory as the crowd chants your names. | 你們站在倒下的對手屍骸之上，沐浴在榮光之中，人群高呼著你們的名字。 | script71 | |
| "You have given us a spectacular battle and as the law decrees, you are now citizens of Purgatory!" exclaims the gamesmaster. | 「你們為我們帶來了一場精彩絕倫的戰鬥，依照法律規定，你們現在是波卡城的市民了！」賽事主持人大喊道。 | script71 | 依 CONTEXT.md：Purgatory → 波卡城；gamesmaster 譯為「賽事主持人」〔?〕 |
| The guards take your gold as a penalty for losing the combat. | 守衛沒收了你們的金幣，作為敗陣的懲罰。 | script71 | |
| "Better luck next time!" snickers the guards. | 「下次好運！」守衛竊笑道。 | script71 | |
| The crowd grows wild with the hope of more victims. | 觀眾瘋狂叫囂，渴望看到更多犧牲者。 | script71 | |
| You hear the lusty shouts of a large crowd coming from the east. | 你聽到東方傳來一大群人的豪放呼喊聲。 | script71 | |
| You hear the bloodthirsty howls of a great crowd from behind the wall to the north. | 你聽到北邊城牆後方傳來大批人群嗜血的嚎叫聲。 | script71 | |
| A great chorus of voices issues up from the west. | 西方傳來一陣嘹亮的歡呼聲。 | script71 | |
| A breeze crawls in from the harbor, bearing a sickly stench. | 一陣微風從港口飄來，帶著令人作嘔的惡臭。 | script71 | |

---

## 七、死亡訊息（script22）

| English（原文） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| Alas, your brave party has met its match! Your current adventure is over. | 唉，你那英勇的隊伍終究遇上了勁敵！這趟冒險就此畫下句點。 | script22 | |

---

## 八、怪物名稱（res31）

> 來源：`26_MONSTERS_AND_SPRITES.md` 二、怪物名稱表。CONTEXT.md 怪物表為暫定，本節為正式草表。

| English（res31 官方） | 繁中 | 來源 | 備註 |
|---|---|---|---|
| Robber | 強盜 | res31 | 依 CONTEXT.md |
| King's Guard | 國王守衛 | res31 | 依 CONTEXT.md |
| Guard | 守衛 | res31 | 依 CONTEXT.md |
| Soldier | 士兵 | res31 | 依 CONTEXT.md |
| Bandit | 盜匪 | res31 | 依 CONTEXT.md |
| Loon | 瘋子 | res31 | 依 CONTEXT.md |
| Fanatic | 狂熱者 | res31 | 依 CONTEXT.md |
| Yonderboy | 遠方少年 | res31 | 依 CONTEXT.md；〔?〕「Yonder」有「遠方」之意，也可考慮「彼方少年」 |
| Born Loser | 天生輸家 | res31 | 依 CONTEXT.md |
| Unjustly Accused | 蒙冤者 | res31 | 依 CONTEXT.md |
| Giant Spider | 巨蜘蛛 | res31 | 依 CONTEXT.md |
| Wild Dog | 野狗 | res31 | 依 CONTEXT.md |
| Spider | 蜘蛛 | res31 | 依 CONTEXT.md |
| Cannibal | 食人者 | res31 | 依 CONTEXT.md |
| Big Dog | 大狗 | res31 | CONTEXT.md 未列，暫定〔?〕；可考慮「惡犬」 |
| Wild hound | 野獵犬 | res31 | CONTEXT.md 未列，暫定〔?〕；hound 強調獵犬品種，與 Wild Dog 做區分 |
| Rock Spider | 岩蜘蛛 | res31 | 依 CONTEXT.md |
| Jail Keeper | 獄卒 | res31 | 依 CONTEXT.md |
| Drunk | 醉漢 | res31 | 依 CONTEXT.md |
| Humbaba | 胡姆巴巴 | res31 | 依 CONTEXT.md |
| Gladiator | 鬥士 | res31 | 依 CONTEXT.md |
| Wolf | 野狼 | res31 | sprite 168 確認；res31 記錄中以 Wild Dog 為名（記錄 0x042d），Wolf 見於 sprite 確認清單；兩者分開列〔?〕 |
| Innocent Man | 無辜者 | res31 | sprite 200 確認；res31 文字記錄未列，名稱來自 sprite 確認清單，依 CONTEXT.md |
| Pikeman | 長矛兵 | res31 | sprite 210 確認；res31 文字記錄未列，名稱來自 sprite 確認清單，依 CONTEXT.md |

---

## 待確認（Pending）

以下條目需要更多上下文或人工核對：

| # | 問題條目 | 問題描述 |
|---|---|---|
| 1 | `breathes`（script03） | 完整句語境不清，應為「{怪物}吐息」，但組合句結構需確認 |
| 2 | `a ` / `an `（script03） | 中文不定冠詞轉換方式：「一個{怪物}」是否需要依怪物種類調整計量詞（「一頭」/「一隻」/「一名」）？建議依怪物種類分別設定 |
| 3 | `killing {數字} all of them.`（script06） | 「all」與「{數字}」並列結構不標準，可能是 `killing all of them` 與 `killing {數字} of them` 兩個分支的合體；需確認 bytecode |
| 4 | `Zap! The {物品}`（script06） | 「Zap!」音效詞，備選譯法：「嗶！」/「砰！」/「充能！」，待確認此段的實際上下文（充能成功還是失敗？） |
| 5 | `all have brought your own equipment."`（script19） | 前置片段不清，可能為：`"And I see that you `` all have brought your own equipment."` 的 you-all 拆分，待確認完整句 |
| 6 | `gamesmaster`（script71） | 譯為「賽事主持人」；原文 1990 年代用法，可能也譯為「競技場主持」或「主裁」，待確認手冊有無對應詞 |
| 7 | `Big Dog`（res31） | 未出現在 CONTEXT.md，暫定「大狗」，待確認與 Wild Dog / Wild hound / Wolf 的視覺及遊戲差異後決定最終譯名 |
| 8 | `Wild hound`（res31） | 未出現在 CONTEXT.md，暫定「野獵犬」，同上 |
| 9 | `has `` no `` in `` gold`（script13） | `in` 片段功能待確認（可能是空值片段或「…以金幣計」語法） |
| 10 | `New weapon`（script18） | 是否為「換持另一武器（雙手武器）」還是「切換至備用武器」，影響譯法 |
| 11 | `Yonderboy`（res31） | 「遠方少年」或「彼方少年」，待確認遊戲設定與敵人背景 |

---

*草表版本 v0.1 — 以 `ALL_TEXT_FROM_SCRIPTS.txt` 為唯一文字來源，術語依 `CONTEXT.md`，怪物名依 `26_MONSTERS_AND_SPRITES.md`。*
