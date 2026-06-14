# 火龍之戰 — 資料格式與戰鬥機制(逆向參考)

> **來源(致謝)**:本文整理自社群逆向資料,作為 opendw_remake 戰鬥/角色系統的領域對齊依據。
> - **資料格式**:fraterrisus,《Dragon Wars Hex Editing Guide (PC)》v1.0, 2022-09-22(GameFAQs)。
> - **戰鬥機制**:SDA Knowledge Base,《Dragon Wars (1989)/Game Mechanics》(speeddemosarchive,2018-04-13)。
> - **官方規格**:臺灣中文版手冊(`33_MANUAL_TRANSCRIPTION.md`)。
> 三者交叉比對;opendw C 反編譯本身未實作戰鬥結算,故以上述為公式來源,並以 DOS 實機觀察(`43_DOS_PLAYTEST.md`)校準確切骰分布。

## 1. 角色記錄格式(512-byte,DATA1 @ 0x2E19 起,每員 +0x200)

> 來源:fraterrisus。base = 0x2E19 / 0x3019 / 0x3219 / 0x3419 / 0x3619 / 0x3819 / 0x3A19(最多 7 員)。
> 以下為自 base 起的 **byte offset**(little-endian)。

| Offset | 欄位 |
|---|---|
| `[00-11]` (12B) | 角色名(7-bit ASCII,**高位元設 1**,最後一字元高位元為 0;見字串格式) |
| `[12-13]` | 力量 Strength(current, max,各 1B) |
| `[14-15]` | 敏捷 Dexterity(current, max) |
| `[16-17]` | 智力 Intelligence(current, max) |
| `[18-19]` | 精神 Spirit(current, max) |
| `[20-23]` | 生命 Health(current, max;**各 2B**) |
| `[24-27]` | 暈眩 Stun(current, max;各 2B) |
| `[28-31]` | 法力 Power(current, max;各 2B) |
| `[32-58]` (27B) | 技能 Skills(每技能 1B = 等級;順序見技能表) |
| `[59]` | 可花用的 AP(技能點) |
| `[60-67]` (8B) | 法術 Spells(bitfield;順序見法術表) |
| `[68-75]` (8B) | 未知(恆 0?) |
| `[76]` | 狀態 Status bitfield |
| `[77]` | NPC 識別碼 |
| `[78]` | 性別 Gender |
| `[79]` | 等級 Level |
| `[80]` | 經驗值 XP |
| `[81]` | 金幣 Gold |
| `[82]` | **AV(攻擊值,命中)** |
| `[83]` | **DV(防禦值,閃避)** |
| `[84]` | **AC(護甲等級)** |
| `[85]` | Flags(祝福旗標) |
| `[86-235]` (143B) | 未知(恆 0?) |
| `[236-258]` (23B) | 物品欄 A(見裝備格式) |
| `[259-281]` … `[489-511]` | 物品欄 B … M(每格 23B,共 13 格) |

> ⚠️ remake 目前 `game::CharacterRecord` 解析較簡(STR/DEX/INT/SPI、HP/STUN/PWR、status、gender、level、gold),**未含 AV[82]/DV[83]/AC[84]/XP[80]/skills/spells/inventory**——戰鬥需補。

### 狀態 Status `[76]` bitfield
`bit0` 死亡 Dead · `bit1` 被鎖鏈 Chained · `bit2` 中毒 Poisoned

### 性別 Gender `[78]`(手冊作「Sex」)
`0` 男 he/him · `1` 女 she/her · `2` 有時 it/it · `3` 從不(用角色名代稱)

### Flags `[85]`(低 4 bit 恆 0)
`0b00010000` 宇宙之神祝福(+3 全屬性)· `0b00100000` Enkidu 祝福(習得德魯伊魔法)· `0b10000000` 受 Irkalla 祝福

## 2. 裝備格式(11 byte = 88 bit + 物品名;物品欄內名補滿至 12B)

> 來源:fraterrisus。以下為 **bit offset**。

| Bit | 欄位 |
|---|---|
| `[00]` | 是否已裝備 |
| `[03-07]` | 充能/使用次數 |
| `[08]` | AV 修正為負(=1 時) |
| `[10-15]` | 需求屬性/技能 |
| `[19-23]` | 需求值(0=無需求) |
| `[24-27]` | **AV 修正**(bit08=1 時為負) |
| `[28-31]` | **AC 修正**(恆正?) |
| `[32-39]` | 售價(指數 3b + 尾數 5b,值 = M×10^E) |
| `[40-47]` | 物品類型(僅低 5 bit) |
| `[48-63]` | 魔法效果(2 byte) |
| `[64-71]` | **主傷害骰** |
| `[72-79]` | **次傷害骰**(非 0 時;有射程的近戰武器) |
| `[80-83]` | 彈藥類型(弓與彈須相符) |
| `[84-87]` | 武器射程(×10 呎) |

### 傷害骰編碼(1 byte)
> 來源:fraterrisus。
- **高 3 bit = 骰面數**:`000`=d4 `001`=d6 `010`=d8 `011`=d10 `100`=d12 `101`=d20 `110`=d30 `111`=d100
- **低 3 bit = 骰數 − 1**(`000`=1 顆 … `111`=8 顆)。中間 2 bit 恆 0。
- 範例:`0b101_00_001` = 2d20。
- **次傷害**:有射程近戰武器(Kalah 之斧、長錘、投擲錘)主傷害用於 10' 攻擊,次傷害用於更遠。

### 物品類型 `[40-47]`(低 5 bit)
`00` 一般 · `01` 盾 · `02` 全身盾 · `03` 斧 · `04` 連枷 · `05` 劍 · `06` 雙手 · `07` 錘 · `08` 弓 · `09` 弩 · `0A` 槍 · `0B` 投擲武器 · `0C` 彈藥 · `0D` 手套 · `0E` 法師手套 · `0F` 彈夾 · `10` Cuir Bouilli 甲 · `11` Brigandine 甲 · `12` 鱗甲 · `13` 鎖鏈甲 · `14` 板鏈甲 · `15` 全板甲 · `16` 頭盔 · `17` 卷軸 · `18` 靴

## 3. 戰鬥機制(公式)

> 來源:SDA Knowledge Base + 手冊。確切骰分布以 DOS 實機校準。
>
> **DOS 實機觀察(`43_DOS_PLAYTEST.md`,24 張截圖)**:
> - ✅ **`AV = DV = Dex÷4` 經實機證實**(Lv1 徒手四角齊證:Dex 20→5 / 24→6 / 16→4 / 12→3)。
> - 傷害與 Str 正相關、個位數量級(Theb Str14→3~4 / Muskels Str21→6);疑似「小骰 + Str 修正」,樣本少未能定係數。
> - ⚠️ **AC 行為待釐清**:實機有甲/無甲目標傷害同量級 → 疑似 **AC 影響命中、不直接減傷**,**與 SDA「AC 先減傷」說法矛盾**。remake 目前依 SDA 實作「AC 減傷」,此分歧待更多樣本或 bytecode RE 釐清。
> - ⚠️ **確切 to-hit 骰式無法由實機證實**:戰鬥訊息列不顯示擲骰原始值 → 需由 res3 戰鬥 bytecode(op_4D RNG + op_33~36 算術)反推(那才是原版公式真值)。
> - 大傷害觸發暈眩(7/12 暈、4/16 不暈);清怪每員 +80 XP;HP=Stun 獨立池(印證下方)。

- **基礎 AV / DV = DEX ÷ 4**(命中 to-hit / 閃避 dodge 同源;最佳值為 4 的倍數)。
- **最終 AV = (基礎 DEX/4 ± 武器 AV 修正) + 武器技能**(技能 1:1 隱形加成,顯示值不含)。
  - 例:Rusty Axe 傷害最高(非魔法斧)但 **−3 AV**,可用斧技能補回。
- **AC 先從物理傷害扣除**,再作用到 **STUN**。重甲(非魔法)會降 AV。
- **HP = Stun**;傷害作用於 STUN;STUN 高 = 耐打。STR 愈大傷害愈大。
- **命中判定**:攻擊者 AV vs 目標 DV(確切骰式待 DOS 校準;疑似小骰 2dN 風格,D&D-like)。
- **傷害**:擲武器傷害骰(上述編碼)+ STR 修正 − 目標 AC,作用於 STUN。
- 戰鬥距離 10'–150'(近戰多 10',遠程/特殊武器距離不一);近戰/遠程一般**隨機選一目標**。
- 升級只給 **2 技能點**(等級影響小);技能點投資於屬性/技能/魔法類別。
- INT:增加咒語命中率(「幾乎不會 miss」),戰鬥外用途少。

### 屬性/技能索引(op_5D/op_61 selector;`[32-58]` 技能順序)
> 來源:fraterrisus。
`0x0-0x1` 力量 · `0x2-0x3` 敏捷 · `0x4-0x5` 智力 · `0x6-0x7` 精神
技能:`0x24` 奧術學 · `0x25` 洞穴學 · `0x26` 森林學 · `0x27` 山嶽學 · `0x28` 城鎮學 · `0x29` 包紮 · `0x2A` 攀爬 · `0x2B` 徒手 · `0x2C` 躲藏 · `0x2D` 開鎖 · `0x2E` 扒竊 · `0x2F` 游泳 · `0x30` 追蹤 · `0x31` 官僚 · `0x32` 德魯伊魔法 · `0x33` 高級魔法 · `0x34` 初級魔法 · `0x35` 商人 · `0x36` 太陽魔法 · `0x37` 斧 · `0x38` 連枷 · `0x39` 錘 · `0x3A` 劍 · `0x3B` 雙手 · `0x3C` 弓 · `0x3D` 弩 · `0x3E` 投擲武器

### 法術索引(`[60-67]` bitfield;L=初級 H=高級 D=德魯伊 S=太陽 M=雜項)
> 來源:fraterrisus。對照手冊法術表(`33_MANUAL_TRANSCRIPTION.md` 第 20-24 頁)。
`0x00` L:魔火 · `0x01` L:解除武裝 · `0x02` L:吸引力 · `0x03` L:祈福 · `0x04` L:輕微醫療 · `0x05` L:法師魔光 · `0x06` H:火燄之光 · `0x07` H:大火(Elvar's Fire)· `0x08` H:烈燄旋渦(Poog's Vortex)· `0x09` H:寒冰術 · `0x0A` H:大寒冰術 · `0x0B` H:眩目強光 · `0x0C` H:神力 · `0x0D` H:幻影現形 · `0x0E` H:迅捷術(Sala's Swift)· `0x0F` H:保護罩(Vorn's Guard)· `0x10` H:膽怯 · `0x11` H:治療 · `0x12` H:集體治療 · `0x13` H:遮蔽奧術 · `0x14` H:感知陷阱 · `0x15-0x18` H:召喚(風/土/水/火)· `0x19` D:死亡詛咒 · `0x1A` D:火焰爆 · `0x1B` D:蟲災 · `0x1C` D:旋風 · `0x1D` D:驚嚇 · `0x1E` D:荊棘 · `0x1F` D:高級治療 · `0x20` D:全面治癒 · `0x21` D:造牆 · `0x22` D:軟化石頭 · `0x23` D:召喚靈魂 · `0x24` D:野獸呼喚 · `0x25` D:木靈 · `0x26` S:日炙 · `0x27` S:驅魔 · …(完整見 fraterrisus / 手冊)

## 4. 字串格式
> 來源:fraterrisus(與 remake `text_codec` 一致)。
7-bit ASCII,**高位元設 1**,僅最後一字元高位元為 0。例:"Shield" = `D3 E8 E9 E5 EC 64`。

## 5. 對 remake 的應用(進度)
1. ✅ `game::CharacterRecord` 已補解析 AV[82]/DV[83]/AC[84]/XP[80]/skills[32-58]/spells[60-67]/主武器欄[236-258]。
   - **byte-grounded 發現**:`default_party.bin` 的 4 員 **stored AV/DV/AC=0、XP=0、inventory 全 0**——
     起始隊伍的 AV/DV/AC 是原版 **runtime 計算**(非存於 blob),與 SDA「base AV/DV = DEX÷4」一致。
   - 故 remake 補了 `effective_av()/effective_dv()/effective_ac()`:stored 為 0 時回退 SDA 公式(DEX/4 + 武器技能 ± 武器 AV 修正)。
     實測 effective DV == DEX/4(Muskels 20→5、Theb 24→6、Elendil 16→4、Cheetah 12→3)。
   - base offset 與 fraterrisus 一致(STR@12、status@76、gender@78、level@79、AV@82…),**無需校正**。
2. ✅ `game::combat` 結算改用真實 stats:命中 = 攻擊者 AV vs 目標 DV;傷害 = 解碼武器主傷害骰 + STR 修正 − 目標 AC(AC 先扣)→ 作用於 STUN(HP=Stun);STUN≤0 → status bit0 死亡。
   - **to-hit 骰分布為暫定**(2d10 + 門檻,參數化於 `combat.hpp`),**待 `43_DOS_PLAYTEST.md` 的 DOS 實機校準**。
   - 起始隊伍無武器 → 徒手傷害骰回退(暫定 1d2,手冊未明列)。
   - 標示已從「乾淨室 placeholder」改為「依 fraterrisus/SDA/手冊規格;非 opendw byte-for-byte(opendw C 未實作結算);不宣稱 oracle 真值」。
3. ✅ 法術系統依手冊 + 法術索引建立(`src/game/spells.{hpp,cpp}`)。
   - 法術表 61 條(id 0x00–0x3C,對齊 fraterrisus 索引;L/H/D/S/M 五大 school)。
     效果值(傷害/治療範圍、目標、Power 消耗)**全部取自手冊**(`33` 第 20–34 頁)。
     L/H 段 id 對 doc 44 既有索引(0x00–0x18);D 段 0x19–0x25;S 段自 0x26 起、M 段續延伸
     為 **remake 依 doc 44 規律(L→H→D→S→M、各 school 按手冊列序)推得**,
     若 fraterrisus 完整 S/M 索引釋出有出入,只需改表的 id 欄。
   - 施法結算整合進 combat(C 鍵):傷害/治療擲手冊範圍 → 作用 STUN;PowerScaled = STR×rng[1..N];
     buff(+AV/+DV/+AC/+STR/+DEX)、debuff(-AV/-DV)數值化;控制/工具/召喚類只扣 Power、標 `handled=false`(TODO)。
     擲骰走 `CombatRng`(確定性)。**非 opendw byte-for-byte**(opendw C 未實作法術結算)→ 不宣稱 oracle。
   - i18n:`assets/i18n/{zh-TW,en,ja}/spells.tsv`(zh-TW 全填、en passthrough、ja 部分)。
   - 驗證:`verify_spells`(ctest)— 表筆數/效果值對拍手冊抽樣、扣 Power、傷害/治療範圍、bitfield、確定性,全 PASS。
4. ✅ **道具/裝備系統**(`src/game/equipment.{hpp,cpp}` + `src/game/damage_dice.hpp`)依本節 §2 grounded。
   - 完整解析一格 23B 物品欄:已裝備/充能/AV 修正(含 bit[08] 負號)/AC 修正/需求屬性+值/
     售價(M×10^E,= 購買價 ÷ 2)/物品類型(低 5 bit)/魔法效果(2B)/主+次傷害骰/彈藥/射程/物品名。
   - `CharacterRecord` 擴充:`inventory()` 解析全 13 格(slot A..M,[236-511]);`item_at(slot)`;
     `main_weapon()` 改取「第一件已裝備武器」;`effective_ac()` 改累加「所有已裝備物品」AC 修正
     → 戰鬥 `Combatant::from_player` 的護甲 AC 自動納入。
   - **byte-grounded 萃取驗證**:`tools/extract/extract_items`(curated 偏移)從 DATA1 抽 7 件真實物品:
     Air Talons(雙手/1d12)、Air Armor + Water Wings(硬皮甲)、Earth/Fire/Aura Shield(盾,AV −10/−15/−12)、
     Dragon Stone(一般物品,售價 250→125,魔效=回復法力)。**對拍解碼 type/AV/名稱合理**。
     發現:`default_party.bin` 物品欄全空(起始裝備為原版 runtime 給予),故樣本改取 DATA1 召喚物/任務物品偏移。
     兩種佈局比對結論:**fraterrisus bit-packed(本節 §2)正確**,opendw player.c 的 byte-aligned `item_info`
     對同一筆 Dragon Stone 解碼失敗(type=0x39 無效)→ 採用 §2。
   - UI:CharSheet 新增物品欄子畫面(E 鍵切換屬性/物品;`--inventory` headless;`--demo-items` 注入樣本)。
     顯示名稱(已裝備亮綠)+ 類型(i18n)+ AV/AC 修正 + [已裝備] 標記。
   - i18n:`assets/i18n/{zh-TW,en,ja}/items.tsv`(物品類型名 + 背包 UI,zh-TW 全填)。
   - 驗證:`verify_equipment`(ctest)— 類型/售價編碼、真實 DATA1 物品對拍、13 格物品欄、AC 聚合,全 PASS。
