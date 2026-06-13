# 關卡(level)地圖資源 — 從原版 DATA1/DATA2 抽出

每個 `<area>.lvl` 是一個關卡的**原始解壓資料**(opendw `resource_load(area + 0x46)`),
即遊戲真實地圖。檔名 `<area>` = 區域編號(game_state[2])。

- 抽取工具:[`tools/extract/extract_level.cpp`](../../../tools/extract/extract_level.cpp)
  (`scan` 看名稱/維度、`dump` 抽成 .lvl、`parse` 解析單關)。
- 格式(依 opendw `read_level_metadata`):前 4 byte = 高/寬/旗標;變長 section
  (component lists)+ 關卡名 offset(`word_5864`)+ tile 格(每格 3 byte:牆屬性 +
  事件/script 索引)。關卡名以 5-bit codec 解出。

## 區域對照(關卡名 100% 對應《軟體世界》攻略 38/39)

| area | res | H×W | 關卡名(原版解出) | 攻略對應(38/39) |
|---|---|---|---|---|
| 0 | 0x46 | 47×32 | Dilmun | 迪瑪大陸(世界圖) |
| 1 | 0x47 | 34×34 | **Purgatory** | 波卡城 |
| 2 | 0x48 | 16×17 | Slave Camp | 奴隸營 |
| 3 | 0x49 | 8×8 | Guard Bridge | 守橋 |
| 4 | 0x4A | 16×16 | Salvation | 救贖之山 |
| 5 | 0x4B | 17×18 | Ruins | 塔斯廢墟 |
| 6 | 0x4C | 18×18 | Phoebus | 菲巴斯城 |
| 7 | 0x4D | 8×9 | Bridge | 橋 |
| 8 | 0x4E | 17×17 | Mud Toad | 黃泥蟾蜍城 |
| 9 | 0x4F | 16×16 | Byzanople | 拜占儂市 |
| 10 | 0x50 | 8×8 | Smugglers Cove | 海盜竊穴 |
| 11 | 0x51 | 8×8 | War Bridge | 橋樑 |
| 12 | 0x52 | 8×8 | Scorpion Bridge | 蠍橋 |
| 13 | 0x53 | 8×8 | Bridge of Exiles | 放逐橋 |
| 14 | 0x54 | 17×17 | Necropolis | 奈羅波裡 |
| 15 | 0x55 | 8×8 | Dwarf Ruins | 矮人廢墟 |
| 16 | 0x56 | 16×16 | Dwarf Clan Hall | 矮人城堡 |
| 17 | 0x57 | 16×17 | Freeport | 自由港 |
| 18 | 0x58 | 32×32 | Magan Underworld | 瑪根地底世界 |
| 19 | 0x59 | 16×16 | Mines | 礦場 |
| 20 | 0x5A | 17×18 | Lansk | 蘭斯克 |
| 21 | 0x5B | 9×9 | Sunken Ruins | 沉沒之城(陸上) |
| 22 | 0x5C | 8×8 | Sunken Ruins | 沉沒之城(水下) |
| 23 | 0x5D | 17×16 | Mystic Wood | 神祕林 |
| 24 | 0x5E | 16×16 | Snake Pit | 蛇窟 |
| 25 | 0x5F | 15×17 | Kingshome | 京雄城 |
| 26 | 0x60 | 8×9 | Pilgrim Dock | 朝聖者碼頭 |
| 27 | 0x61 | 32×32 | Depths of Nisir | 尼塞山腹 |
| 28 | 0x62 | 8×8 | Old Dock | 老碼頭 |
| 29 | 0x63 | 16×16 | Siege Camp | 軍營 |
| 30 | 0x64 | 16×16 | Game Preserve | 皇家專有獵區 |
| 31 | 0x65 | 8×8 | Magic College | 魔法學院 |
| 32 | 0x66 | 16×16 | Dragon Valley | 龍谷 |
| 33 | 0x67 | 17×16 | Phoeban Dungeon | 菲巴斯地牢 |
| 34 | 0x68 | 16×16 | Lanac'toor's Lab | 拉娜的實驗室 |
| 35 | 0x69 | 16×15 | Byzan. Dungeon | 拜占儂地下指揮部 |
| 36 | 0x6A | 17×17 | Kingshome | 京雄城地牢 |
| 37 | 0x6B | 16×16 | Slave Estate | 莫格的宅院 |
| 38 | 0x6C | 17×17 | Lansk Undercity | 蘭斯克地下城 |
| 39 | 0x6D | 8×8 | Tars Ruins | 塔斯廢墟(地下) |

> 40 個關卡名與《軟體世界》攻略地區**完全對應**,證明抽取正確,也即「原版地圖 ↔ 攻略紀錄」的對照。

## 狀態

- ✅ 關卡資料抽成獨立資源(本目錄 `.lvl`),不依賴 DATA1/DATA2。
- ✅ 維度 + 關卡名解析正確(與攻略地區 1:1)。
- ⏳ **tile 逐格牆屬性 / 事件格(訊息 N 觸發點)的位元意義仍在校正**;校正後即可
  在 remake 渲染真實地圖 + 標出事件地點,並與攻略圖例(訊息 N)逐格比對。
