# 43 — 原版 DOS《火龍之戰》實機戰鬥觀察(命中/傷害經驗 oracle)

> 日期:2026-06-14
> 目的:在 DOSBox 容器跑原版 DOS《Dragon Wars》(Interplay 1989/90, v1.1),走到開局第一場戰鬥,記錄**實際**的命中/傷害數值、暈眩、死亡、經驗值與早期事件,作為 combat 公式校準的**經驗 oracle**。
> 動機:`docs/reverse-engineering/42_COMBAT_BYTECODE.md` 已證明 remake 的戰鬥結算仍是乾淨室 placeholder——怪物 roster pipeline 尚未完整逆向、無法獨立跑一場戰鬥對拍。實機觀察提供「輸入屬性 → 輸出數值」的黑箱真值,可在不依賴 bytecode 逆向的情況下,獨立校準/驗證命中與傷害模型。
>
> **可信度標記**:本文嚴格區分
> - **【觀察】** = 螢幕截圖直接讀到的數字 / 訊息(可複現)。
> - **【推斷】** = 由觀察點外推的公式或關係(尚未證明,標明證據強度與反例風險)。

---

## 1. 環境:Docker DOSBox(headless 自動化)

**鐵則遵守**:dosbox 裝在容器,不污染系統;截圖走 Xvfb framebuffer。

### 1.1 Image(`dwdos`)

```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    dosbox xvfb imagemagick x11-utils xdotool scrot p7zip-full ca-certificates
WORKDIR /game
```

> 註:Ubuntu 22.04 官方 repo 無 `dosbox-x`,改用 `dosbox`(SDL surface 輸出),VGA/MCGA 模式可正常跑本作。

### 1.2 遊戲檔取得

```bash
7z e "Dragon Wars (1990).zip" "5.25/1.1/DISK01.IMA" "5.25/1.1/DISK02.IMA"
7z e DISK01.IMA DRAGON.COM DWTRAN.COM DATA1   # disk1
7z e DISK02.IMA DATA2                          # disk2
```

`DRAGON.COM`(56673B)+ `DATA1`(296500B)+ `DATA2`(352430B)放進 `/game`。`DRAGON.COM` header 含「Created on an Apple ][ G…」字串(IIgs 移植痕跡)。

### 1.3 自動化:Xvfb + xdotool + import 截圖

`dosbox.conf`:`machine=vga`、`cycles=3000`、autoexec `mount c /game` → `DRAGON.COM`。

驅動腳本 `drive.sh`(單次執行):啟 Xvfb :99 → 跑 dosbox →(retry 30×)抓 `xdotool search --name DOSBox` 視窗 → 逐行讀 `steps.txt`(`KEY` / `TYPE` / `SLEEP` / `SHOT`)用 `xdotool key --window` 注入、`import -window root` 截圖。

**已知不穩定**:約 1/4 機率視窗在 dosbox 起動前抓不到(`win=` 空)→ 該次按鍵打空、截到全黑。**對策**:重跑即可(retry 後幾乎必成);長序列拆成可重入的「preamble + 該段操作」,失敗只重跑該段。

### 1.4 進到遊戲的固定前導(preamble)

| 步驟 | 按鍵 | 結果(截圖) |
|---|---|---|
| 設定選單 | `E`(VGA/MCGA 16 色)→ `Return` | `01_config_menu.png` |
| 標題畫面(龍 + Logo) | `Return` ×2 / `Space` | `02_title_vga.png` |
| Current party… | — | 預設隊伍已存在:`03_default_party.png` |
| Begin the game | `B` | 進入世界 |
| 開場事件文字 | `Escape` ×N 翻頁 | `04_intro_purgatory.png` |

**預設隊伍**(免建角,直接可玩,4 人):**Muskels、Theb、Elendil、Cheetah**。本次全程用此預設隊伍。

---

## 2. 隊伍屬性(oracle 輸入端)— 【觀察】

按數字鍵 `1`~`4` → `General overview` 讀到的 Lv1 起始屬性(全員 AC:0、無武器,僅 Gold;Exp 0):

| 角色 | Str | Dex | Int | Spr | **Attack(AV)** | **Defense(DV)** | AC | Health | Stun | Power |
|---|---|---|---|---|---|---|---|---|---|---|
| Muskels | 21 | 20 | 10 | 10 | **5** | **5** | 0 | 16/16 | 16/16 | 0/0 |
| Theb | 14 | 24 | 10 | 10 | **6** | **6** | 0 | 14/14 | 14/14 | 0/0 |
| Elendil | 10 | 16 | 12 | 14 | **4** | **4** | 0 | 12/12 | 12/12 | 28/28 |
| Cheetah | 11 | 12 | 16 | 13 | **3** | **3** | 0 | 13/13 | 13/13 | 26/26 |

技能(`Abilities`,以 Muskels 為例):Cave Lore:1、Forest Lore:1、Mountain Lore:1、Swim:1、Tracker:1、**Flails:1**(武器技能)。
> Muskels 有 Flails 技能=1 但未持鏈鎚,**徒手作戰**;故下方 AV=5 是「無武器、武器技能未生效」的基準值。

**【推斷 A — AV/DV 公式(Lv1、徒手)】證據強**
四筆 (Dex, AV, DV):(20,5,5)、(24,6,6)、(16,4,4)、(12,3,3)。皆滿足 **AV = DV = Dex ÷ 4**(整除)。
- 反例風險:四點都恰好被 4 整除,無法區分 `floor(Dex/4)` 與 `round(Dex/4)`;也無法判定是否有 `+Lv` 或 `+武器技能` 項(此處 Lv 都=1、武器技能未生效)。後續升級 / 持武器後須再採點。
- 與手冊一致性:`docs/manual/33` 手冊只說「AV 高易擊中、DV 高難被擊中、武器技能提高 AV」,未給數字(原文標 `〔?〕`);此處補上 Lv1 基準。

---

## 3. 第一場戰鬥:走到戰鬥的路徑 — 【觀察】

開局在 **Purgatory(波卡城)** 貧民窟。兩條都能在數十步內觸發戰鬥:

### 3.1 路徑甲:Main Gate 守衛戰(事件格,必觸發)

preamble 後沿走廊移動(`Up`/`Left`/`Right`/`Down` 轉向+前進)抵達事件格「**the main gate to Purgatory**」(`11→…→g04`)。文字提示:闖門的話守衛會打你。對著門前進(`Up` ×3)即觸發:

> **遭遇**:`King's Guards` — **5 King's Guards 30'** + **6 Pikemen 50'**。選單:**Fight / Quickly fight / Run / Advance ahead**。(`11_encounter_kingsguards.png`)

對 Lv1 徒手隊伍偏硬(數量多、有甲),適合採「敵→我」傷害與暈眩,不適合採「我→敵」傷害(命中率低)。

### 3.2 路徑乙:貧民窟隨機遭遇(較弱、單體,推薦)

preamble 後往另一方向繞貧民窟,觸發「**You have just attracted some unwanted attention**」→ **1 Pikeman 30'**(`20_encounter_pikeman.png`)。實戰中該遭遇接著湧出 **Robbers(無甲)**,命中率高、HP 低,**最適合採「我→敵」傷害**。

### 3.3 戰鬥操作流程(選單結構)— 【觀察】

```
遭遇選單: Fight / Quickly fight / Run / Advance ahead
  └ Fight → 每名角色依序「<Name>, choose:」
        Attack / Dodge enemies / Block attack / Use item /
        New weapon / Load weapon / Run / Move / ? View the party
        └ Attack → Attack style: Attack blow / Mighty blow / Disarm enemy
              └ Target: A) <群組1> / B) <群組2> ...
  → 四人指令排隊完 → 「Use these commands? Yes/No」(按 Y 確認)
  → 回合結算(訊息列逐條顯示命中/傷害/暈眩;Return 翻頁)
```

要點:**指令先全隊排隊、按 `Y` 統一結算**(不是逐人即時)。對應 `42_COMBAT_BYTECODE` 提到的 `op_89` 鍵盤等待 + key→addr 跳轉。手冊的 Normal/Mighty/Disarm 三種 blow 對應實機「Attack blow / Mighty blow / Disarm enemy」(本次只採 **Attack blow**)。

距離機制:近戰徒手攻 30' 外目標 → 訊息「**…and he is out of range.**」;**每回合敵方推進 10'**(「The Pikemen/Robbers advance 10' feet.」)。30' 目標約第 3 回合進入近戰。

---

## 4. 命中 / 傷害觀察值(oracle 輸出端)— 【觀察】

訊息列格式(實機原文,功能性結果字串):
- 命中:`<Attacker> attacks <Defender> and hits 1 time for N points of damage[, stunning him][, killing him].`
- 失手:`<Attacker> attacks <Defender> and misses.`
- 超距:`<Attacker> attacks <Defender> and he is out of range.`

### 4.1 敵 → 我(King's Guard 攻擊隊員)

| 截圖 | 攻擊者 | 目標 | 結果 | 傷害 | 暈眩? | 目標 Stun |
|---|---|---|---|---|---|---|
| `16_guard_hits_7_stun.png` | King's Guard | Elendil | hits 1 time | **7** | **是**(stunning him) | 12 |
| `17_guard_hits_4.png` | King's Guard | Muskels | hits 1 time | **4** | 否 | 16 |
| (多幀) | King's Guard | 各員 | misses | — | — | — |

回合推進後 Elendil/Cheetah/Muskels 陸續顯示「is stunned」(隊伍面板),最終 4 人有 3 人被暈(此戰太硬,中止)。

### 4.2 我 → 敵(隊員攻擊,Attack blow,徒手)

| 截圖 | 攻擊者 | Str | 目標 | 傷害 | 結果 |
|---|---|---|---|---|---|
| `18_theb_hits_4_guard.png` | Theb | 14 | King's Guard(有甲) | **4** | hit |
| `19_theb_hits_3_guard.png` | Theb | 14 | King's Guard(有甲) | **3** | hit |
| `21_theb_kills_robber_4.png` | Theb | 14 | Robber(無甲) | **4** | killing him |
| `23_theb_kills_robber_3.png` | Theb | 14 | Robber(無甲) | **3** | killing him |
| `22_muskels_kills_robber_6.png` | Muskels | 21 | Robber(無甲) | **6** | killing him |

### 4.3 戰鬥結果 — 【觀察】

`24_victory_80xp.png`:清掉 Robbers 後「**Each member gets 80 experience points for combat.**」(每人 +80 XP)。

---

## 5. 推斷的命中 / 傷害模型(標明證據與反例)

### 5.1 傷害模型 — 【推斷 B】證據中等

採到的「我→敵」單次傷害:Theb(Str14)= {3,4,4},Muskels(Str21)= {6}。
- **Str 與傷害正相關**:Str21 的 Muskels(6)> Str14 的 Theb(3~4)。與手冊「力量愈大傷害愈大」一致(`docs/manual/33` 第 276 行)。
- **量級**:Lv1 徒手單擊個位數(3~7,含敵方)。Robber「一擊即死」→ Robber HP 約 ≤ 個位數(3 點就能 kill,故 Robber HP ≲ 3~4 或本就殘血;單筆無法定 HP)。
- **形態(推測,證據弱)**:疑似「小骰 + Str 修正」。例如徒手 ≈ `1dK + g(Str)`,其中 g 單調增。Theb 觀測 3~4(跨度小)、Muskels 觀測 6。**樣本太少,無法定骰面與修正係數**;不應據此寫死公式。
- **反例 / 風險**:有甲目標(King's Guard)Theb 仍打出 3~4,與無甲 Robber 的 3~4 同量級 → **本作傷害疑似不被目標 AC 直接減免**(AC 可能只影響命中,不減傷),但樣本不足,僅標為待驗。

### 5.2 命中模型 — 【推斷 C】證據弱(僅定性)

- 命中=擲骰比較,**AV 對 DV/AC**(手冊定性),失手顯示「misses」、命中顯示「hits 1 time」。
- 對有甲 King's Guard 命中率明顯低(多次 misses);對無甲 Robber 幾乎必中(連續 killing)。→ **目標護甲(AC/DV)壓低命中**,與手冊一致。
- **未採到擲骰原始值**(訊息列只報 hit/miss,不顯示 d20/門檻),故 `d20+AV vs 10+DV` 之類**無法由實機畫面證實**;需配合 `42_COMBAT_BYTECODE` 的 `op_4D`(PRNG)+ `op_33~36`(乘除)反推。實機只能提供「命中率隨 AC 上升而下降」的黑箱證據。

### 5.3 暈眩(Stun)模型 — 【推斷 D】證據中等

- 每名角色有獨立 **Stun 池**(= 起始 Health 同值:16/14/12/13)。
- 受擊傷害可能扣 Stun;**單次傷害 ≥ 某門檻 → 暈眩**:Elendil(Stun12)中 7 → 暈;Muskels(Stun16)中 4 → 不暈。
- **【推斷】**:暈眩門檻與「傷害 vs 剩餘 Stun」或「傷害佔比」有關(7/12 ≈ 0.58 暈;4/16 = 0.25 不暈)。兩點不足以定門檻函數,僅標方向。

---

## 6. 早期事件對照(攻略「訊息 N」)— 【觀察】

| 事件(實機文字摘要) | 觸發點 | 對照 |
|---|---|---|
| 開場:被剝奪一切、赤手空拳被丟進 Purgatory 貧民窟(奉 Namtar 之命) | Begin game 後 | `04_intro_purgatory.png`;攻略 §4 開場敘事(Namtar 放逐)一致 |
| 「No one escapes Purgatory alive…」 | 開場第 2 頁 | 對應「有進無出的城」(`docs/walkthrough/38` §5.1 簡介) |
| Main Gate 守衛:闖門會被守衛打 | 城門事件格 | 攻略波卡城圖例「重兵把守城門」 |
| King's Guards(5)+ Pikemen(6)遭遇 | 闖門 | 守衛戰(攻略多處「守衛」) |

> 本次未走到攻略明確編號的 Read Paragraph 觸發點(訊息 5/9/67/77 等,需深入城內特定格);開場兩段為過場敘事文字,非段落書防拷。後續若要採段落觸發,可循 `docs/walkthrough/38` §5.1 行動清單格號導航。

---

## 7. 結論與後續

### 7.1 本次確立的 oracle(可直接用於校準)

1. **AV/DV 基準**(Lv1 徒手):`AV = DV = Dex/4`(四點齊證,**推斷 A 證據強**)。
2. **傷害量級**:Lv1 徒手單擊 3~7;Str↑ → 傷害↑(Str21→6 vs Str14→3~4)。
3. **暈眩**:獨立 Stun 池(=起始 HP),大傷害觸發暈眩(7/12 暈、4/16 不暈)。
4. **護甲**:目標 AC 主要壓**命中率**,疑似**不減傷**(待驗)。
5. **流程常數**:敵每回合進 10';指令全隊排隊後 `Y` 統一結算;清怪每人 +80 XP。
6. **選單/狀態機**:遭遇 4 選項、角色 9 動作、攻擊 3 style、目標分群 — 與 `42_COMBAT_BYTECODE` 的 `op_89` 跳轉表結構吻合,可作 remake UI 對照。

### 7.2 仍是缺口的(實機無法直接採)

- **命中擲骰原始值與門檻**(d20? 1d100? 命中公式係數):訊息列只報 hit/miss,**黑箱**;須配合 bytecode `op_4D`+`op_33~36` 反推。
- **傷害骰面與 Str 修正係數**:樣本太少(Theb 3 筆、Muskels 1 筆),只能定方向不能定公式。建議**擴大採樣**:同一角色連打同種無甲怪 20+ 次取分布,反推 `damage = aD b + c·f(Str)`。
- **怪物 HP / DV / AC 絕對值**:訊息不顯示怪物數值;只能由「N 點即死」推 HP 上界。

### 7.3 建議下一步(若續做)

1. **大樣本傷害分布**:鎖定 Robber(無甲、必中)當固定靶,讓單一角色(如 Muskels)連續 Attack blow ≥20 次,記錄傷害直方圖 → 反推骰式。
2. **持武器對照**:給角色裝武器(城內取得 / 作弊)後重採 AV 與傷害,分離「武器技能 / 武器骰」對 AV、damage 的貢獻。
3. **升級對照**:練到 Lv2+ 後重採 AV/DV,驗證是否含 `+Lv` 項。
4. **存檔點**:dosbox 內 `Ctrl+F5` 截圖、或建立 dosbox savestate,固定戰前狀態以便重複實驗(本次未用 savestate,靠固定 preamble 重入)。

---

## 8. 重現

```bash
# 1) 建 image
docker build -t dwdos /tmp/dwdos        # Dockerfile 見 §1.1

# 2) 抽遊戲檔到 /game(見 §1.2),放 dosbox.conf + drive.sh

# 3) 跑某段(steps.txt = preamble + 操作),失敗重跑該段
timeout 90 docker run --rm -v /tmp/dwgame:/game dwdos bash /game/drive.sh

# 截圖落在 /game/out/*.png
```

`steps.txt` 語法:`KEY <xdotool鍵>` / `TYPE <字串>` / `SLEEP <秒>` / `SHOT <名>`。
到戰鬥的可重入 preamble + 路徑甲/乙序列見本專案 tools 暫存(未入庫,原始遊戲檔不入庫)。

### 截圖清單(`docs/dos_playtest/`)

01 設定選單・02 VGA 標題・03 預設隊伍・04 開場(Purgatory)・05 世界地圖 UI・06~09 四角屬性・10 Muskels 技能・11 King's Guards 遭遇・12 角色動作選單・13 攻擊 style・14 目標選單・15 超距・16 守衛打 Elendil 7 點致暈・17 守衛打 Muskels 4 點・18~19 Theb 命中守衛 4/3・20 Pikeman 遭遇・21~23 隊員擊殺 Robber(4/6/3)・24 勝利 +80XP。
25 Muskels 對無甲 Wild Dog 造成 8 傷・26 Muskels 對有甲 King's Guard 造成 7 傷(AC 不減傷對照)。

---

## 9. 大樣本傷害研究(2026-06-14 第二輪)

> 目的:把第 5 節「傷害模型」從 7 筆樣本(證據中等)推進到統計可辨。方法:`dwdos` 容器 + tesseract OCR **全自動**採樣,同隊伍對多場弱怪(無甲)與 King's Guard(有甲)連打,逐幀 OCR 戰鬥訊息列、解析「攻擊者 → 目標 → 傷害」。

### 9.1 自動化升級(本輪新增)

- **OCR 管線**:`dwdos` image 加裝 `tesseract-ocr`。截圖訊息列裁切 `crop 630x130+5+290 → resize 200% → grayscale → tesseract --psm 6`,可穩定讀出「`<Attacker> attacks a <Target> and hits 1 time for N points of damage`」中的 N。
- **採樣 harness**:`run_fight.sh <tag> <rounds>` 跑一場戰鬥、每回合截 12~14 幀;`aggregate.py`(host 端 Python)正規化 OCR 雜訊(`Hild→Wild`、`Fime→time`、角色名修正)、用 regex 抽 HIT/MISS、**去連續重複**(同訊息跨多幀)。`stats.py` / `stats_ac.py` 出分布。
- **取樣規模**:無甲 11 場有效(遇怪隨機:Wild Dog / Spider / Robber / Wolf / Cannibal / Innocent Man / Unjustly Accused 等,皆無甲)、有甲 3 場(King's Guards path 甲)。**合計 53 筆命中**(無甲 34、有甲 19)。
- **harness 不穩定**:dosbox 視窗偵測在連續批次下偶發 race(`NO WINDOW`,本輪 12 場丟 2 場 = f06/f08);`drive3.sh` 加「視窗抓不到 → 重啟 dosbox 一次 + 拉長 settle」緩解。OCR 偶把怪名讀錯(如 `Rock Syider`=Rock Spider)不影響傷害數字。

### 9.2 傷害分布(pooled,有甲+無甲)— 【觀察】

因 §9.4 證明 AC 不減傷,可把同角色的有甲/無甲傷害合併:

| 攻擊者 | Str | N | min | max | mean | 傷害值次數分布 |
|---|---|---|---|---|---|---|
| Muskels | 21 | 13 | 3 | **8** | 5.23 | {3:5, 6:4, 7:3, 8:1} |
| Theb | 14 | 10 | 3 | 6 | 3.70 | {3:7, 4:1, 6:2} |
| Cheetah | 11 | 16 | 3 | 6 | 4.25 | {3:6, 4:5, 6:5} |
| Elendil | 10 | 14 | 3 | 6 | 3.71 | {3:10, 4:1, 6:3} |

**觀察到的傷害值全集(53 筆)= {3, 4, 6, 7, 8}**。**`5` 完全沒出現過**(53 筆 0 次)。

### 9.3 反推徒手傷害公式 — 【推斷,證據強(結構性指紋)】

> **⚠ 已被 bytecode 反推修正(2026-06-14,見 docs/reverse-engineering/42 §11)**:本節 `damage = max(3, floor(1.5×1d4))`
> → {3,4,6}「無 5」是 **53 筆小樣本近似**。端到端跑 res3 bytecode 的**真值**為
> **`damage = 1d4 + floor(STR/5)`**(加法 STR/5,**無 ×3/2、無 floor3**)→ STR10 = {3,4,**5**,6}**含 5**。
> 「缺 5」「×1.5」均被 bytecode 證偽(小樣本剛好沒採到 5)。以下原推斷保留作歷史紀錄;
> 確切公式以 docs/reverse-engineering/42 §11(bytecode 反推 + verify_combat_script 對拍)為準。

三個硬指紋:

1. **下限恆為 3**:四名角色 min 皆 = 3,且 3 是多數人的眾數。→ 傷害有 `max(3, …)` 樓地板。
2. **缺 5、且 3→6 有斷層**:Muskels 值集 {3,6,7,8} 完全跳過 4、5;低 Str 值集 {3,4,6} 跳過 5。「缺 5」在乘以 1.5 的縮放下自然出現。
3. **上限隨 Str 抬升**:Str 10/11/14 → max 6;Str 21 → max 8。

**最佳擬合模型**:**`damage = max(3, floor(1.5 × raw))`**,其中 `raw` = 小骰 + Str 相關修正。

驗證(把小骰代入):

- **低 Str(Str 10–14):`raw = 1d4`**

  | d4 | 1.5×d4 | floor | max(3,·) |
  |---|---|---|---|
  | 1 | 1.5 | 1 | **3** |
  | 2 | 3.0 | 3 | **3** |
  | 3 | 4.5 | 4 | **4** |
  | 4 | 6.0 | 6 | **6** |

  → 輸出集 **{3,4,6}**,**與 Theb/Cheetah/Elendil 實測值集完全吻合**(max 6、缺 5)。

- **高 Str(Muskels Str 21):`raw` 整體上移**(較大骰或加 Str 修正)→ 中段被推高:輸出 {3,6,7,8}。
  - `round(1.5×5)=8`、`floor(1.5×4)=6`、`floor(1.5×5)=7`;Muskels **缺 4** 而低 Str **有 4** → 不是同骰放大,而是**整條分布上移**(Str bonus 把 raw 的低值墊高,4 被推到 6+)。

**Str → 傷害斜率(粗略線性,N=53)**:約 **+0.12 傷害 / 點 Str**(即 +10 Str ≈ +1.2 平均傷害)。注意 Str 10→14 區間平均幾乎不動(3.71→3.70),Str 21 才明顯跳到 5.23 → **Str 修正在低段很小、非線性,只在高 Str 才把上限/中段抬起**。

> **誠實邊界**:
> - `×1.5 縮放 + floor 3` 是**結構性結論(證據強)**:由「缺 5」「3↔6 斷層」「{3,4,6} 完全命中 1d4×1.5」三點交叉支持。
> - **小骰面數(1d4 vs 1d6)、Str 修正的確切函數(查表?Str/K?)仍是推斷**:Muskels 高段樣本(7、8 各僅 3/1 筆)不足以唯一定出骰面與修正係數。要鎖死,需 §9.6。
> - 與 `42_COMBAT_BYTECODE` 對齊:該檔指出傷害走 `op_33~36`(乘/除法子系統)。**`×1.5` 正是「乘 3 除 2」**,與 bytecode 有乘除 primitive 完全自洽 —— 這是雙向佐證(實機分布 ⇄ bytecode 乘除),非巧合。

### 9.4 AC 是否減傷?→ **不減傷,只壓命中** — 【觀察→判定,證據強】

同角色對 **有甲 King's Guard** vs **無甲雜怪** 的命中傷害分布:

| 攻擊者 | Str | 有甲 mean (N) | 無甲 mean (N) | 有甲值集 | 無甲值集 |
|---|---|---|---|---|---|
| Muskels | 21 | 5.00 (5) | 5.38 (8) | {3,6,7} | {3,6,7,8} |
| Theb | 14 | 4.00 (4) | 3.50 (6) | {3,4,6} | {3,6} |
| Cheetah | 11 | 4.40 (5) | 4.18 (11) | {3,4,6} | {3,4,6} |
| Elendil | 10 | 3.60 (5) | 3.78 (9) | {3,6} | {3,4,6} |

**判定**:有甲與無甲的傷害分布**統計上無法區分** —— 同 min(3)、同 max(隨 Str)、同值集、mean 差在抽樣雜訊內(各方向都有,非系統性偏低)。**直接反例**:Muskels 對有甲 King's Guard 打出 **7**(截圖 26),與對無甲怪同量級;Theb 對有甲打 4、6,也不低於無甲。

→ **AC 不進入傷害計算;AC/DV 只影響命中(hit/miss)**。這**解決了與 SDA 的矛盾**:之前看到「打守衛總是 miss、傷害卻和無甲一樣」並非 AC 減傷,而是 **AC 抬高 miss 率、一旦命中傷害不打折**。與手冊「AC 愈高防禦力愈好、AV vs DV 決定命中」一致 —— AC 是**命中側**參數。

### 9.5 命中率觀察 — 【觀察,弱證據(有 confound)】

| 攻擊者 | AV | 無甲 命中/失手(率) | 有甲(在近戰且揮擊後) |
|---|---|---|---|
| Theb | 6 | 6/2 (75%) | 4/0 |
| Muskels | 5 | 8/2 (80%) | 5/0 |
| Elendil | 4 | 9/0 (100%) | 5/0 |
| Cheetah | 3 | 11/4 (73%) | 5/0 |

> **重要 confound,勿過度解讀**:
> - 採樣器只記「明確 misses」與「hits」,**未計入大量 "out of range" 早回合**(怪在 30'/50',前 2~3 回合徒手打不到)。故「有甲 100% 命中」是**假象**:它只反映「已進近戰且該次揮中」的條件機率,排除了守衛在遠處時的大量無效回合。
> - 無甲命中率(73~80%)較具代表性(雜怪較快進近戰),但 N 小、AV 跨度小(3~6),**看不出 AV 與命中率的乾淨單調關係**。
> - 結論:**命中率須讀 bytecode 的擲骰原始值才能定**(實機訊息列不顯示 d20/門檻);本輪僅能定性確認「有甲明顯較難命中(早回合一直 out-of-range/miss)、命中後傷害不變」。

### 9.6 仍未鎖死 + 下一步

1. **骰面與 Str 修正係數**:Muskels(Str21)高段樣本太少(7×3、8×1)。需**對單一無甲固定靶連打 ≥40 次**(Cheetah Str11 已 16 筆最穩,可先把它打到 40 筆定出低 Str 骰式;再對 Muskels 同樣加採)。
2. **中間 Str**:現有 Str = {10,11,14,21},10–14 太密、14–21 太疏。**建角補 Str 17/18 左右**一名,填滿曲線中段,才能分辨「Str 修正是連續函數還是查表階梯」。
3. **持武器**:本輪全徒手。裝武器後 `raw` 應換成「武器骰 + Str 修正」,可分離武器骰貢獻;`×1.5 floor3` 樓地板是否仍在(還是武器有自己的下限)待測。
4. **bytecode 對拍**:把 `×1.5 = ×3÷2` 假設拿去比對 res3 的 `op_33~36` 實際運算元(乘 3、除 2?),即可從 byte 層**證實**縮放係數,把 §9.3 的「推斷」升級為「觀察+bytecode 雙證」。

### 9.7 重現(大樣本)

```bash
# image 加 OCR
docker build -t dwdos /tmp/dwdos          # Dockerfile +tesseract-ocr

# 跑一場無甲採樣(隨機遇怪)
docker run --rm -v /tmp/dwgame:/game dwdos bash /game/run_fight.sh f01 7
# 跑一場 King's Guard(有甲)採樣
docker run --rm -v /tmp/dwgame:/game dwdos bash /game/run_guard3.sh g01 9

# OCR + 解析一場 → 結構化 HIT/MISS
docker run --rm -v /tmp/dwgame:/game dwdos bash /game/parse_ocr.sh 'f01_' | python3 aggregate.py

# 統計
cat hits_*.txt | python3 stats.py        # 分布
cat hits_*.txt | python3 stats_ac.py     # 有甲 vs 無甲
```

工具(`drive3.sh`/`run_fight.sh`/`run_guard3.sh`/`parse_ocr.sh`/`aggregate.py`/`stats*.py`)在 `/tmp/dwgame`(暫存,未入庫;原始遊戲檔不入庫)。
