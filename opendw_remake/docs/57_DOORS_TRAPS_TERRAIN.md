# 探索互動深度第二類:開門 / 破密門 / 陷阱 / 戰鬥外地形法術

逆向結論與真值層級。對應 remake 程式:`src/game/terrain.{hpp,cpp}`、`src/game/real_terrain.{hpp,cpp}`、`src/main.cpp`(K / C 鍵段 + 真陷阱接線)、`src/game/spells.cpp`(地形法術效果)。

## 更新(2026-06-17):真實 .lvl 陷阱格已識別,座標來自原版

> 取代舊結論「真實 .lvl 未含這些保留值 → 不影響既有關卡 / 未觸發」。
> **陷阱已逐關掃出原版真實座標並接線**;門的真實格仍受阻(無乾淨識別管道)。

| 項目 | 結果 | 真值層級 |
|---|---|---|
| **陷阱位置** | 路徑 B 逐關跑 VM 識別出 **3 關共 111 個真實陷阱格**(area 16 矮人鑄爐 1、area 27 尼塞山腹 109、area 31 魔法學院 tripwire 1) | **可識別(byte 真值)**:tile→script_pc→VM emit 字串已逐指令對拍 opendw |
| **陷阱觸發 / 傷害結算** | 踩格觸發(`trigger_trap_here`)、Sense / Disarm 對真陷阱生效 | 位置=真值;**傷害數值=remake 設計**(扣減走 op_58 + 未反編 settlement,受阻) |
| **門位置** | 牆 sprite 路徑(路徑 A)無法乾淨識別門格 | **受阻**:全 40 關牆 nibble 只選 5 個牆/岩 sprite tag,無專屬門 tag;`hilo` 是重載牆變體碼 |

### 路徑 A(門 = 門 sprite):受阻(已用渲染證據確認)

- `tools/verify/dump_door_table` 全 40 關 dump:牆面 nibble → `data_56C6` → `data_59E4` 只選到
  **tag 110 / 115 / 122 / 125 / 126**(+ 0x7F 透明)五種 sprite,**無專屬「門」sprite tag**。
- 牆型碼 `hilo`(高半 byte 低 nibble)**不能判定門**:area 34「Lanac'toor's Lab」整間實驗室
  內牆 64 格 `hilo=1` 是該關標準牆;area 27 主牆 `hilo=0`,門牆候選 rare。`hilo` 是**重載的
  牆變體碼**,非門標記。
- `tools/verify/render_fp_cell` 對 area 33「門牆候選」(hilo!=0)與一般牆組第一人稱 viewport:
  兩者**目視皆為相同磚牆造型**,無門框 / 門板。
- 結論:**門的真實格無法由牆資料乾淨識別**。原版真正的「門」多以**特殊事件格**(換區 / 對話
  gate,如拜占儂城門、單向西門)承載 —— 那些已由 `Level::worldmap_dest` / `subarea_relocs` /
  `area_entry_relocs` 機制處理;通用「K 對牆開門」的引擎級動作仍在未反編主迴圈(見 §1)。

### 路徑 B(陷阱 = 事件格 script 行為):可行(已接線)

- `src/game/real_terrain.cpp` `RealTraps::identify`:對每個特殊事件格(word_11C8≥2)的 tile →
  `script_pc` → 在 VM 跑該關 bytecode,攔截 emit 字串。字串屬「對隊伍即時傷害 / 敵意環境」
  語意類(攻略 docs/38 描述的陷阱)→ 該格 = 原版真陷阱格。
- **與攻略抽樣吻合**:
  - area 27 尼塞山腹 tile 0x1A 四角 `(14,10)(24,10)(14,21)(24,21)` emit「The floor moved!」
    = 攻略「the floor is moving 訊息時回原位」陷阱;tile 0x04/0x05 emit「icy winds of despair
    tear at your soul」、tile 0x11 emit「fury of the sun … burn in this corridor」(灼燒走廊)。
  - area 31 魔法學院 tile 0x10 emit「tripwire … granite block smashs the party」= 攻略測驗五陷阱。
  - area 16 矮人城 tile 0x06 emit「dwarven forge … saps your very life essence」= 鑄爐耗命格。
- **誠實標示**:傷害結算 HP/Stun 實際扣減走 op_58 跨資源呼叫 + 未反編 settlement
  (已實測 area27 trap script 走 op_52 跳轉而非 op_5E 寫 Health)→ **傷害數值 remake 設計**;
  本路徑只給**位置真值**。逐關掃描工具 `tools/verify/detect_traps`(`--list` 列座標)。

### 接線與驗證

- `src/main.cpp` `enter_map` 進關即 `RealTraps::identify` 重算本關真陷阱;`trigger_trap_here`
  / `resolve_explore_cast`(Sense / Disarm)優先吃真陷阱座標(相容保留 0x33 測試關 tile)。
- headless 驗證:`verify_terrain <bundle>` §4 斷言 area 27 識別 >50 格 + floor-moved 四角座標 +
  Sense/Disarm 對真陷阱生效 + area 0 世界圖 0 誤報;app `--map 27 --trap-probe` 整合測:踩真
  陷阱格 HP 下降。ctest 30 項全綠。

## 真值層級總表

| 機制 | 真值層級 | 依據 |
|---|---|---|
| 牆屬性 word_11C6 nibble → 牆/門 sprite | bytecode 真值 | engine.c `move_player_on_map`(0x536B)、`draw_minimap_row`(0x1861)、`viewport_compose` 已逐指令逆出 nibble → `data_56C6` → sprite |
| **面向前方牆型 byte → `gs[0x26]`(門牆識別線索)** | **可識別(byte 真值)** | engine.c `refresh_viewport` 5663-5673:前方中央格牆 nibble → `data_56C6[nibble+0xF]` → `gs[0x26]`;`hilo` = 逐關牆型碼。**但其值→門/牆語意受阻**(見 §2) |
| 移動可走判定(`tile != 0`) | bytecode 真值 | engine.c `get_map_tile_data`(0x54D8):`word_11C8 = data_5521[di+2]`,0=void/牆、1=地面、≥2=特殊事件格 |
| 特殊格事件腳本入口(op_71 / `script_pc`) | bytecode 真值 | `Level::script_pc` 已對拍 op_71;特殊格(word_11C8≥2)以 script PC 進事件 VM |
| **K 開門 / 破密門「動作 + 開/鎖/阻擋語意」** | **受阻 → remake 設計** | 能識別前方牆型 byte 與門牆候選位置,但 `gs[0x26]` 消費者(開門語意)在未反編主迴圈(見 §1/§2) |
| **陷阱觸發 / 傷害 / 解除** | **受阻 → remake 設計** | opendw 乾淨反編**無陷阱結算**;grounded 手冊 |
| **戰鬥外地形法術結算** | **受阻 → remake 設計** | opendw C 碼法術未實作(同 docs/44 戰鬥結算);grounded 手冊 |

## 受阻證據(誠實標示,絕不謊稱 oracle 真值)

### 1. 主遊戲 K 開門 handler 不在乾淨反編範圍

- engine.c:3312 的 `'K' -> 0x17C0` 是**小地圖**(minimap)輸入(K=往下平移),**非**主遊戲開門。
- `play_sound_door_open`(engine.c:5870 @0x5064)與 `play_sound_wall_bump`(@0x5066)**只在函式表 `func_5060[]` 出現,從未被 `dispatch_sound_effect` 實際呼叫到**(唯一呼叫者 `op_sound_effect` @0x49E7 由 bytecode operand 驅動,乾淨反編未走到開門路徑)。
- `move_player_on_map`(engine.c:5332)**只負責建構第一人稱 viewport 的周邊牆 nibble**(讀前方/側方格、打包成 `word_11CA`/`word_11CC` 供 FOV 元件選擇),**不消費「開門動作」**。
- 結論:Dragon Wars 主遊戲移動 / K 開門邏輯位於 indirect-jump 後的主迴圈,**不在 opendw 乾淨 C 反編**。

### 2. .lvl 牆屬性的門線索:可識別到「前方牆型 byte」,但門語意受阻

(本節為 2026-06-16 有界深掘的修訂結論,取代舊版「牆屬性完全無門線索」的粗略說法。)

牆屬性 word_11C6 的**低 byte**含兩個 nibble(高/低),經 `data_56C6` 牆/門型表選元件 sprite。
`data_56C6` 建表(engine.c:5096-5110)每筆 2 byte → 拆成兩半:

- **低半** `data_56C6[n]` = `byte_lo & 0x7F` → 元件型 index(選 `data_59E4[idx]` sprite)。
- **高半** `data_56C6[n+0xF]` = `byte_hi`(原始,含旗標)。

**可識別到的一步(byte-level)**:`refresh_viewport`(engine.c:5663-5673)取**面向前方中央格**
(`data_5A56[10]`)的牆 nibble,若非 0 → `gs[0x26] = data_56C6[nibble+0xF]`(前方牆的高半 byte)。
即引擎**確實把「面向前方牆的型別 byte」算出來並存進 `gs[0x26]`**,正是「開門/碰牆」動作會讀的位置。
其低 nibble(本文記為 `hilo`)為**逐關牆型碼**:dump 全 40 關(`dump_door_table` / `dump_door_cells`)
可見地牢類關卡(area 27/33/34/8…)有少數牆 nibble 的 `hilo != 0`,與該關「主牆」(`hilo == 0`)區隔。

**受阻的一步(語意,誠實標示)**:`hilo != 0` **不能乾淨判定為「門」**——它是**重載**的牆變體碼:
- area 27「尼塞山腹」:主牆 `hilo=0`,僅 1 格 rare nibble `hilo=1`(像門);
- area 34「Lanac'toor's Lab」:**整間實驗室內牆** `hilo=3`(x64 格)——是該關標準牆,**非門**。

且全 40 關牆 nibble 只選到固定 5 個 tag(110/115/122/125/126 + 0x7F 透明),**無專屬「門」sprite tag**。
因此能識別「前方牆型 byte(`gs[0x26]` 來源)」與「哪些 nibble 牆型異於主牆」,但**「哪個牆型是可開的門 vs 鎖門 vs 實心牆」「K 開門後是否變可走」的語意,完全在消費 `gs[0x26]` 的未反編主迴圈**,靜態無法乾淨逆出。

連可走性本身(move walkability gate)也不在乾淨反編:`move_player_on_map` 只打包 FOV nibble,
真正的「tile!=0 才可走」判定同樣在未反編主迴圈(remake 沿用此慣例)。

- 補充:真正的門 / 陷阱在 Dragon Wars 多以**特殊事件格**(word_11C8 ≥ 2)+ `script_pc(tile)` 事件腳本承載;
  哪些特殊格是「門 / 陷阱」需逐格跑 bytecode 才能斷定,亦無法靜態乾淨逆出。

**判定**:可逆到「識別前方牆型 byte 與門牆候選位置」;**開門/上鎖/阻擋語意受阻** → 保留 remake 設計。

## remake 設計(grounded 手冊)

因上述受阻,門 / 密門 / 陷阱的**狀態與行為**採乾淨室 remake 設計,grounded 臺灣中文版手冊:

- 手冊 p176/184:`K` / ↑ =「打開關閉的門、粉碎牆中密門」——引擎級、對面向前方的牆做。
- 手冊 p33 §技能:Lockpick 開鎖可進鎖住的房間。
- 手冊 p25/p29 §法術:Sense Traps(偵測陷阱 0x14)、Disarm Trap(解除陷阱 0x36)、Soften Stone(軟化石 0x22,移開石牆/障礙)。
- 攻略 docs/37/38 測驗五(魔法學院 area 31):以 Soften Stone / Disarm Trap 通關。

### 地形互動狀態:`TerrainState`(per-area 座標旗標)

與 `SeenMap` 同模式(.lvl 為只讀資產,不就地改 byte,改維護獨立 per-area 狀態,存檔一併保存)。對每格 `(area,x,y)` 記四種旗標:`DoorOpen` / `SecretBroken` / `TrapDisarmed` / `TrapSprung`。序列化進存檔(SaveState `terrain_blob`,v3)。

### 門 / 密門 / 陷阱的 remake tile 約定

remake 以 word_11C8 tile 型的一段**保留值**作門 / 密門 / 陷阱標記(**remake 設計約定**,非 oracle:opendw 對應值由 bytecode 決定,本約定僅供 remake 機制落地與 headless 驗證):

| tile 型 | 語意 | K / 踩格行為 |
|---|---|---|
| `0x30` | 關閉的門(可走但需先開) | 未開時擋路;K 面向它 → 開啟(`DoorOpen`)+ 門音效 log,之後可走 |
| `0x31` | 鎖住的門 | 同上,但需隊伍 Lockpick 技能 ≥ 門等級;不足 → 提示「鎖住的」 |
| `0x32` | 牆中密門 | 移動視為牆(擋);K 面向它 → 粉碎揭示(`SecretBroken`)後可走 |
| `0x33` | 陷阱格 | 踩到(未解除/未觸發)→ 觸發傷害;Sense Traps 標記可見;Disarm Trap 解除(`TrapDisarmed`) |
| `0x34` | 石牆障礙(Soften Stone 目標) | 視為牆;Soften Stone 軟化後可走(`SecretBroken` 重用) |

注:**0x30..0x34 為 remake 測試關保留約定**(真實 .lvl 不含這些值)。**真實 .lvl 的陷阱格改由
路徑 B(`RealTraps`,見上節 + `real_terrain.hpp`)以「事件格 script 行為」識別 → 原版真座標**;
此 0x30..0x34 表僅供 remake 測試關與 #135 機制相容並存(真陷阱優先)。門的真實格仍受阻(路徑 A)。

### 地形法術(戰鬥外)

S_GAME 以 `C` 鍵(手冊:施法)開探索施法選單,複用 `castable_spells` / `find_spell`。地形效果:

- **Sense Traps(0x14)**:標記當前 area 已知陷阱格可見(`TrapSensed`,UI 提示)。
- **Disarm Trap(0x36)**:解除面向前方或當前格的陷阱(`TrapDisarmed`)。
- **Soften Stone(0x22)**:軟化面向前方的石牆障礙(0x34)→ 可走。

傷害 / 持續 / 確切結算公式 = remake 設計,grounded 手冊文字;**非 oracle 真值**。
