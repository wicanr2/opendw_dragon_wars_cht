# 怪物翻譯對照表

> ⚠️ **大幅修正(2026-06-10)**:本檔有兩類問題,使用前務必注意:
> 1. **§1/§4.1/§6 的 168/196/200/210/222「怪物名稱」是錯的** —— 那是 **sprite 圖形資源編號**,不是名字。
>    真正怪物名在 **resource 31**(20+ 隻:Robber/King's Guard/Soldier/Bandit/Loon/Fanatic/Wild Dog/Giant Spider/Humbaba…)。
>    **正解請見 `26_MONSTERS_AND_SPRITES.md`**。
> 2. **§4.2「待確認怪物」(Skeleton/Zombie/Goblin/Dragon/Slime…)整段是 LLM 依 RPG 慣例臆測**,Dragon Wars 並無這些怪物,請忽略。
> 譯名對齊請以 `../CONTEXT.md` 怪物表為準。

## 概述

本檔案包含從 DATA1 和資源中提取的怪物名稱，以及中文翻譯。

**資料來源**：
- `doc/monsters.txt` — 怪物圖像資源
- `doc/resources.md` — 資源 31（怪物字串）

---

## 1. 怪物名稱

| 英文 | 中文 | 資源編號 | 備註 |
|------|------|----------|------|
| Wolf | 狼 | 168 (0xA8) | 野狗？ |
| Spider | 蜘蛛 | 196 (0xC4) | 或岩石蜘蛛 |
| Innocent Man | 無辜者 | 200 (0xC8) | 人類敵人 |
| Pikeman | 長矛兵 | 210 (0xD2) | 使用長矛的敵人 |
| Fanatic | 狂熱者 | 222 (0xDE) | 或瘋子 |

---

## 2. 怪物類型說明

### 2.1 動物類
- **Wolf**：狼，可能成群出現，是常見的野外敵人
- **Spider**：蜘蛛，可能有毒，常在洞穴中出現

### 2.2 人類類
- **Innocent Man**：無辜者，可能是被誤認為敵人的平民
- **Pikeman**：長矛兵，使用長矛的戰士
- **Fanatic**：狂熱者，可能是宗教狂熱分子或瘋子

---

## 3. 翻譯注意事項

1. **Wolf**：
   - 应翻译为「狼」，而非「野狼」或「惡狼」

2. **Spider**：
   - 应翻译为「蜘蛛」，而非「蜘蛛怪」或「大蜘蛛」

3. **Innocent Man**：
   - 应翻译为「無辜者」，而非「無辜之人」或「平民」

4. **Pikeman**：
   - 应翻译为「長矛兵」，而非「槍兵」或「矛兵」

5. **Fanatic**：
   - 应翻译为「狂熱者」，而非「瘋子」或「狂信者」

---

## 4. 完整怪物清單（從 DATA1 資源 31 提取）

### 4.1 已確認怪物（從 doc/monsters.txt）

| 資源編號 | 英文 | 中文 | 類型 | 備註 |
|----------|------|------|------|------|
| 168 (0xA8) | Wolf | 狼 | 動物 | 野外常見，成群出現 |
| 196 (0xC4) | Spider | 蜘蛛 | 動物 | 可能有毒，常在洞穴中出現 |
| 200 (0xC8) | Innocent Man | 無辜者 | 人類 | 被誤認為敵人的平民？ |
| 210 (0xD2) | Pikeman | 長矛兵 | 人類 | 使用長矛的戰士 |
| 222 (0xDE) | Fanatic | 狂熱者 | 人類 | 宗教狂熱分子或瘋子 |

### 4.2 待確認怪物（從遊戲截圖和資料推斷）

以下怪物可能在資源 31 或其他資源中：

**動物類（Animal）**：
- Rat（老鼠）- 常見於地牢
- Bat（蝙蝠）- 常見於洞穴
- Snake（蛇）- 有毒
- Scorpion（蝎子）- 有毒
- Spider（蜘蛛）- 已確認
- Wolf（狼）- 已確認

**不死類（Undead）**：
- Skeleton（骷髏）- 常見於墓地
- Zombie（殭屍）- 常見於墓地
- Ghost（幽靈）- 常見於墓地
- Vampire（吸血鬼）- 強力敵人
- Lich（巫妖）- 強力不死生物

**惡魔類（Demon）**：
- Imp（小鬼）- 低階惡魔
- Demon（惡魔）- 中階惡魔
- Devil（魔鬼）- 高階惡魔
- Balrog（炎魔）- Boss 級

**人類類（Human）**：
- Bandit（強盜）- 常見敵人
- Thief（小偷）- 常見敵人
- Assassin（刺客）- 強力敵人
- Pikeman（長矛兵）- 已確認
- Fanatic（狂熱者）- 已確認
- Knight（騎士）- 強力敵人
- Wizard（巫師）- 施法敵人

**奇幻類（Fantasy）**：
- Goblin（哥布林）- 常見敵人
- Orc（獸人）- 常見敵人
- Troll（巨魔）- 強力敵人
- Ogre（巨魔）- 強力敵人
- Dragon（龍）- Boss 級
- Golem（石像鬼）- 構裝生物
- Elemental（元素精靈）- 火/水/風/土元素

**特殊類（Special）**：
- Innocent Man（無辜者）- 已確認，特殊遭遇
- Slime（史萊姆）- 常見於地牢
- Mimic（寶箱怪）- Boss 級

---

## 5. 怪物相關文字

| 英文 | 中文 | 備註 |
|------|------|------|
| You still face | 您仍然面對 | 戰鬥開始訊息 |
| retreats back! | 撤退！ | 敵人撤退訊息 |
| calls for more help | 呼叫更多幫助 | 敵人召喚援軍 |
| is out of range. | 超出範圍。 | 攻擊失敗訊息 |
| dodges! | 閃避！ | 敵人格檔訊息 |
| blocks! | 擋住！ | 敵人格檔訊息 |

---

## 6. 怪物 ID 對照表

從 DATA1 資源 31 提取的部分怪物 ID：

| 怪物 ID | 英文 | 中文 | 資源編號 |
|---------|------|------|----------|
| MONSTER_WOLF | Wolf | 狼 | 168 (0xA8) |
| MONSTER_SPIDER | Spider | 蜘蛛 | 196 (0xC4) |
| MONSTER_INNOCENT | Innocent Man | 無辜者 | 200 (0xC8) |
| MONSTER_PIKEMAN | Pikeman | 長矛兵 | 210 (0xD2) |
| MONSTER_FANATIC | Fanatic | 狂熱者 | 222 (0xDE) |

---

## 7. 待辦事項

- [ ] 解壓資源 31 提取完整怪物清單
- [ ] 確認怪物圖像資源編號
- [ ] 建立完整的怪物 ID 對照表
- [ ] 從遊戲截圖確認怪物名稱
- [ ] 從中文手冊提取怪物描述
- [ ] 與 TRANSLATION.md 中的怪物名稱保持同步

---

## 7. 参考资料

- `doc/monsters.txt` — 怪物圖像資源
- `doc/resources.md` — 資源 31
- `docs/ALL_TEXT_FROM_DATA1.txt` Section 0x03
