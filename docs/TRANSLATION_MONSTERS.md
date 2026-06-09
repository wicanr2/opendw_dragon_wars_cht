# 怪物翻譯對照表

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

## 4. 待確認怪物

以下怪物可能在其他區段或以圖像格式儲存：

### 4.1 常見 RPG 怪物
- Goblin（哥布林）
- Orc（獸人）
- Skeleton（骷髏）
- Zombie（殭屍）
- Troll（巨魔）
- Dragon（龍）
- Slime（史萊姆）
- Ghost（幽靈）
- Vampire（吸血鬼）
- Werewolf（狼人）
- Goblin（哥布林）
- Bandit（強盜）
- Thief（小偷）
- Assassin（刺客）
- Wizard（巫師）
- Warlock（術士）
- Necromancer（死靈法師）
- Demon（惡魔）
- Devil（魔鬼）
- Golem（石像鬼）
- Elemental（元素精靈）

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

## 6. 待辦事項

- [ ] 找出完整怪物名稱清單（可能在資源 31）
- [ ] 確認怪物圖像資源編號
- [ ] 建立怪物 ID 對照表
- [ ] 從遊戲截圖確認怪物名稱
- [ ] 從中文手冊提取怪物名稱

---

## 7. 参考资料

- `doc/monsters.txt` — 怪物圖像資源
- `doc/resources.md` — 資源 31
- `docs/ALL_TEXT_FROM_DATA1.txt` Section 0x03
