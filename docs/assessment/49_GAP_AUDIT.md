# 49 — 系統性「遺漏/未實作」稽核(對照原版完整 RPG 機制)

> ## ⚠️ 更新橫幅(2026-06-16):本表為 2026-06-15 的**快照**,以下原標 ⛔ 的項目已由後續 PR 關閉,**閱讀下方各列時以此橫幅為準**:
> | 原稽核項 | 現況 | PR |
> |---|---|---|
> | 升級 / XP 升等、技能習得 / AP X 配點、技能檢定(開鎖/包紮)、裝備穿脫、使用物品 U | ✅/🟡 已實作(`progression.{hpp,cpp}`) | #127 |
> | 商店買賣、招募隊員(酒吧) | ✅ 已實作(`shop`/`recruit`) | #129 |
> | quest flag / gate、結局判定 / 結局事件、主線串接 | 🟡 已實作(135-flag 依賴鏈 + S_ENDING) | #121 / #122 |
> | 跨區連通(世界圖進城 / 子區 relocate / Byzanople) | 🟡 27→**38/40 area** 可達 | #116 / #119 / #120 |
> | K 開門 / 破密門、陷阱、戰鬥外施法(C) | 🟡 已實作(remake 設計;`terrain.hpp`、`verify_terrain`) | #135 |
> | op_46 / 66 / 68 / 69 / 79 / 7A、op_43 / 5F / 60 / 63 | ✅ 已從 DRAGON.COM 反組譯補出(`interpreter.cpp`) | #119 等 |
> | ctest | **20/20 → 27/27** 全綠 | — |
> 仍為 ⛔ 的(本表下方仍準確):4 種特殊攻擊 + Dodge、怪物動畫 / 戰鬥特效、復活(Well of Souls)、撿拾 / 丟棄 / 轉移物品、選單 D 刪除 / R 改名、`O` 重排隊伍、音效 / 音樂、戰鬥真值閉環(🔒,res3 op_89 卡點)、武器 STR bonus 真值(🔒)。
>
> 日期:2026-06-15
> 對象:`opendw_remake/`(C++20 / SDL2 重製《火龍之戰》Dragon Wars, Interplay 1989)
> 方法:**唯讀稽核**。讀 code(`src/`)+ 跑現有 verify(docker `dwsdl`,fresh build + ctest **20/20 全綠**)+ 對照原版機制真值(手冊 `33`、攻略 `38/39`、fraterrisus/SDA `44`、戰鬥 bytecode `42`)。本報告為唯一新增產物,**未改 src / CMakeLists / git**。
> 定位:接續 `47_REMAKE_ASSESSMENT.md`(可玩性 62/100)與 `48_COMPLETABILITY_ROADMAP.md`(連通分析),把「對照原版,哪些機制 已/部分/未/受阻」逐項攤平成一張稽核表,並標可行性。

---

## 0. 圖例與摘要

| 標記 | 意義 |
|:---:|---|
| ✅ | **完整**:對照原版可玩到位(部分另有 oracle 真值背書) |
| 🟡 | **部分**:框架/資料層在,但 gameplay 結算或 UI 不完整 |
| ⛔ | **未實作**:原版有、remake 無 |
| 🔒 | **受阻**:受 oracle(opendw 自身未實作 / 無可對拍路徑)或架構(會破壞既有對拍資產)所阻 |

**一句話**:渲染、VM 算術核心、探索/事件/段落、存讀檔、戰鬥結算公式(bytecode 真值)、法術/道具的「表與格式」層,大多 ✅ 或接近;**未實作的集中在「RPG 系統的動詞」**——升級/技能效果、裝備穿脫/使用物品/商店、招募/quest/結局、特殊攻擊;**受阻的集中在跨區連通(wrap 樞紐 edge→area 對映)與戰鬥真值閉環(怪物 HP 對拍)**。

> ⚠️ **與 docs/assessment/47/48 的漂移更新**:本輪實測發現 codebase 已**領先** docs/assessment/48 ——
> (1) `sync_relocation` 對 wrap 邊界**不再「明確跳過」,改為以 modular 環繞慣例載入並可走動**(`src/main.cpp:641-648`);新增 `verify_wrap` ctest(故 ctest 由 19 → **20**)。
> (2) **但這只解決「wrap 地圖能載入 + 內部環繞可走」,未解決「走到地圖邊緣 → 切換到對應 area」的 edge→area 對映**(grep `edge.*area` 無實作)。連通的結構性缺口性質不變,只是地圖層基礎更扎實。

---

## 1. 總表(面向 → 狀態 → 缺口)

### A. VM opcode 覆蓋

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 實作數 | ✅ | `interpreter.cpp` kImpl 表實作 **117/256** opcode(實測計數),`vm_selftest` + 逐指令對拍 oracle 一致 | — |
| opendw 自身 NULL | 🔒 | opendw `targets[]` 有 **21** 個 NULL opcode(從未逆向):`02 1B 1E 20 29 2C 37 64 65 67 68 6B 6E 70 79 7E 7F 8E 8F 9C 9F` | 純 diff 不可驗;需從 DRAGON.COM raw RE |
| 其中 remake 已補 | ✅ | 21 個 NULL 中 **remake 已逆出並實作 op_68**(原始 DRAGON.COM 反組譯,武器傷害骰來源)| 其餘 20 個 NULL 未補 |
| oracle-able 範圍內未補 | 🟡 | 0x00–0x9F 內、**opendw 非 NULL 但 remake 未實作 = 23 個**:`1D 1F 57 5B 6A 6C 6D 6F 71 72 82 84 85 86 87 8D 91 92 95 96 97 98 9E` | 含 **op_71(level script 事件觸發)** —— remake 是在 `main.cpp` 以等價邏輯模擬(非 VM opcode);其餘多為未踩到的路徑 |

> 重點:op_71(踩格事件)remake **沒有當成 VM dispatch opcode 實作**,而是在 `main.cpp:507-567` 用等價「run level script」邏輯做(對拍 op_71 語意)。所以「未在 kImpl」不等於「事件不能跑」——事件能跑,只是入口不同。

### B. 渲染

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 第一人稱 viewport(透視牆) | ✅ | byte-for-byte 對拍 opendw;`verify_compose_l1`/`verify_fp_l1`/`render_sweep`(40 關 ×4 朝向 154 case)PASS | viewport+UI 整幀組合未對實機 |
| sprite(怪物圖) | ✅ | byte-for-byte;`verify_encounter_golden_spider/wolf` PASS | 僅 2 樣本 |
| 全螢幕場景圖 | ✅(title) | XOR delta + nibble;title golden PASS | 非全部場景掃過 |
| 俯視地圖 + fog of war | ✅(area1) | `verify_automap_l1`/`verify_seen_l1` PASS | 全 40 關 minimap sweep 未入 ctest |
| CJK / 640×480 字級 | 🟡 | TTF 原生層,24px 內文/16px UI 在 scale=3(960×600)精確命中;像素層真整數放大 | 真「640×480 視窗」與 24px 字不在同一倍率(200×整數倍永遠到不了 480,見 docs/assessment/47 §9) |
| 動畫 / 過場 / 戰鬥特效 | ⛔ | sprite 為靜態幀;`init_monster_animation`(op_8A)只記怪物 id、不跑動畫;無施法/打擊特效、無過場動畫 | 怪物動畫幀切換、戰鬥視覺回饋(屬 render 副作用,非結算阻塞) |

### C. 探索 / 世界

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 第一人稱移動 I/J/L | ✅ | `main.cpp:2077-2089` 前進/左右轉,含 wrap 走動 | — |
| K 開門 / 破密門 | 🟡 | PR #135:開門/鎖門 Lockpick 檢定/破密門/石牆擋路 + per-area 狀態存檔 v3(`terrain.hpp`、`verify_terrain`)。真值層級=remake 設計(opendw 主遊戲 K handler 未反編);門牆可識別到「前方牆型 byte」但開/鎖/阻擋語意受阻 | 真實 .lvl 哪些格是門/陷阱需逐格跑 bytecode(受阻);保留 tile 0x30..0x34 真實地圖未含 → 機制完備但實戰未觸發 |
| 陷阱 | 🟡 | PR #135:陷阱格踩中觸發傷害(remake 1d8)、Sense/Disarm Trap 結算(`terrain.hpp`、`verify_terrain`)。真值層級=remake 設計 | 同上:真實 .lvl 陷阱格座標需逆向;傷害值非原版真值 |
| 自動地圖(?) | ✅ | `SeenMap` per-tile fog of war,存讀檔保留;`render_with_seen` | — |
| 同區傳送(area27 樓梯) | ✅ | `sync_relocation` gs[2] 未變 → 挪位;`verify_areaswitch` 1-6 PASS | — |
| 單一 tile 跨區換場 | 🟡 | 機制就緒(`sync_relocation` 換 area 分支);實測全 40 關只 area28→26 一例(`probe_areaswitch`)| 承載換區的 tile 事件幾乎不存在(原版靠 wrap 樞紐) |
| wrap 邊界地圖載入 + 內部環繞走動 | ✅(自洽,非 oracle) | **較 docs/assessment/48 進展**:不再跳過,改 modular 環繞載入/走動;`verify_wrap` PASS | 非 oracle 真值(opendw `exit(1)`);連通數字為 remake 慣例 |
| **跨區連通(edge→area 對映 / 世界圖航海 / 瑪根樞紐)** | 🔒 | wrap 圖能載入能走,但**「走到地圖邊緣 → 切到對應 area + 入口座標」的對映未逆向/未實作**(grep `edge.*area` 0 hit)| 主線骨幹邊(area 0 世界圖、area 18 瑪根),受 oracle + 需 raw RE 阻 |

### D. 戰鬥

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| to-hit(1d16+3,門檻 13+AV−def) | ✅(bytecode 真值) | `combat.cpp:130-138` roll-under;roll3 恆中/roll18 恆失;`verify_combat_script` 對拍 res3 | — |
| 徒手傷害(dice + STR/5) | ✅(bytecode 真值) | `combat.cpp:60-66`;端到端對拍(STR10→[3,6] 含5) | — |
| 武器主傷害骰來源/解碼(op_68 0x08=byte[8]) | ✅(bytecode 真值) | op_68 已反組譯;descriptor sides/count 解碼端到端驗證 | — |
| 武器 STR bonus | 🟡🔒 | `combat.cpp` 保 +floor(STR/5) **best-fit**;self-modifying-code 矛盾、無完整戰鬥 oracle | 真值定論受 oracle 阻(需 actor 迴圈跑武器攻擊觀察自改碼殘留) |
| 怪物屬性對映(21B blob → HP/AV/DV/骰/AC) | 🟡 | `combat.cpp:104-120` 標「暫定」;opendw monster_info 未完整逆向 | 真值欄位對映 |
| 可玩迴圈(進入/下令/多回合/勝負/XP+80) | ✅(確定性模型) | `combat_loop.cpp:62-101` 群戰推進、勝負、`kXpPerVictory=80` | — |
| **全戰鬥 VM 閉環(怪物 HP 真扣 + oracle 對拍)** | 🔒 | bytecode 路徑跑到 actor 迴圈前**卡在 res18↔res4 逐角色動作指派狀態機**(非 opcode 缺失,`last_unimpl=0`);且 opendw 無「獨立跑一場戰鬥 dump 逐回合 HP」路徑 | 動作指派狀態機收斂(可逆向)+ oracle 加 headless 戰鬥入口(受 oracle 阻);見 docs/reverse-engineering/42 §9-14 |
| 逃跑 Run | ✅ | `combat_loop.cpp:103-106` flee 標記;R 鍵 | — |
| 閃避 Dodge | 🟡 | DV 進命中公式;但戰鬥選單「閃避」指令本身無互動實作 | Dodge 指令(原版戰鬥選項) |
| 暈眩 Stun | 🟡 | HP=Stun 池;`dazzle_turns` 控制狀態 | 暈眩昏倒的完整回合跳過語意 |
| 特殊攻擊(強力一擊/卸武裝/前進/快速戰鬥) | ⛔ | grep `Mighty/Disarm/Advance/Quick` 0 hit | 手冊明列的 4 種戰鬥選項全缺 |
| 遠程武器 / 彈藥 / 射程 | 🟡 | `equipment` 解析 ammo_type/range/secondary_dmg,但**戰鬥無遠程結算流程** | 距離/選彈/射程命中修正 |

### E. 法術

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 61 條法術表 | ✅ | `spells.cpp:29-172` id 0x00–0x3C,五大 school;效果值 grounded 手冊;`verify_spells` PASS | — |
| 戰鬥內施法(C 鍵) | ✅(grounded 模型) | `combat_loop.cpp:165-248` 傷害/治療/buff/debuff/控制結算;非 opendw byte-for-byte | — |
| **戰鬥外施法(探索時 C)** | ⛔ | `main.cpp` C 鍵僅在 S_COMBAT 有效(`2039-2042`);探索態無施法 UI/結算 | 探索施法(法師魔光、導引、感知陷阱、軟化石等地形/工具法術) |
| 法術習得 | 🟡 | `character_knows_spell`/`castable_spells` 讀 bitfield 判定;但**無「找人傳授 → 學會」的習得流程** | NPC 教學、Enkidu 德魯伊祝福 flag、升級習得 |
| 控制 / 召喚 / 工具類結算 | 🟡 | 控制類(Daze/Flee/Disarm/Dispel)有;**召喚(元素/精靈/野獸)+ 工具(光源/補給)11 條標 `handled=false` TODO** | 召喚生物參戰、工具類世界效果、variable_power 倍率校準 |

### F. 角色 / 隊伍系統

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 建角(命名/配點/性別/50點/3頁) | ✅ | `chargen.cpp` DraftCharacter + serialize;`main.cpp` PhName/PhAttr UI;`verify_chargen` PASS | 系統內定起始值原版未給數字(取 D&D 中庸 9) |
| CharacterRecord 解析(AV/DV/AC/XP/skills/spells/inventory) | ✅ | `party.cpp:145-168` 逐欄解析 512B + 13 格 23B 物品欄;effective AV/DV/AC | — |
| **升級 / XP 升等** | ⛔ | `party.cpp:170-178` 只 award +80 XP;**無「XP 達門檻 → level++」觸發** | 升等門檻表、升級觸發 |
| **技能習得 / AP 花用(X 鍵)** | ⛔ | 無 X 鍵屬性分配畫面、無 AP(`[59]`)花用邏輯 | 整個升級配點子系統(手冊:升級給 2 技能點) |
| **技能實際效果(開鎖/偷竊/包紮/lore/追蹤/官僚/游泳)** | ⛔ | `skills[27]` 只存數值;**僅武器技能進命中公式**(`combat.cpp:113-125`);非戰鬥技能 0 檢定 | Lockpick/Pickpocket/Bandage/各 Lore 的 gameplay 檢定與效果 |
| 狀態(死亡 / 鎖鏈 / 中毒) | 🟡 | bitfield 解析 + 面板渲染(`party_panel.cpp`);死亡在戰鬥結算用;**中毒/鎖鏈有欄位無效果** | 中毒扣血、鎖鏈行動限制 |
| 復活(Well of Souls) | ⛔ | 無復活機制 | 靈魂之泉復活流程(依賴 area18 連通) |

### G. 道具 / 經濟

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 裝備格式(23B bit-packed) | ✅ | `equipment.cpp:96-127` 完整解析;`verify_equipment` 對拍真實 DATA1 7 樣本 PASS | 完整物品表(現 7 樣本足驗格式) |
| 背包顯示(13 格) | ✅ | `party.cpp:72-95` inventory;CharSheet E 鍵物品欄子畫面 | — |
| **裝備穿脫** | ⛔ | 無 toggle equipped bit 的 UI/邏輯 | 穿脫指令 |
| **使用物品效果(U 鍵)** | ⛔ | `main.cpp` 無 U 鍵;magic_effect(casts_spell/teaches_spell/restores_power)解析了但不落地 | U 使用物品/技能(手冊明列) |
| **商店買賣** | ⛔ | 無商店 UI/邏輯(售價已可解碼) | 買/賣流程、金幣增減 |
| **撿拾 / 丟棄 / 轉移物品** | ⛔ | 無背包操作 UI | 物品轉移/丟棄(手冊「命名 Item」段) |
| 金幣經濟 | 🟡 | `gold` 欄解析(0x55 4B + fraterrisus [81] 1B)| 無任何花費/獲得金幣的遊戲循環 |

### H. NPC / 劇情

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| NPC 對話 | 🟡 | 經 op_71 事件腳本 emit 文字 → 訊息框;無專門對話 UI/狀態機 | 結構化對話、選項分支 |
| **招募隊員(酒吧 Ulrik/Louie/Valar/Halifax)** | ⛔ | 無酒吧 UI、無招募邏輯、無隊伍上下限管理 | 招募流程、NPC 角色記錄注入 |
| **quest flag / gate** | ⛔ | `game_state[256]` 可作旗標且跨事件持久,但**無「持有 X → 解鎖 Y」判定層** | 市民證/眼鏡/翠玉之眼/朝聖者之袍/拉娜碎片等 gate(docs/assessment/48 §1.3) |
| Read Paragraph(旅行指南) | ✅ | `main.cpp:544-552` op_81 → 段落書查繁中全文;ParaViewer 捲動;段落 1–147 完整 bundle | — |
| **主線串接** | 🔒 | 受跨區連通(C)阻,主線從序盤就斷(連通 <10%) | 依賴 wrap 樞紐連通 + quest 體系 |
| **結局判定** | ⛔ | 無勝利條件檢測、無結局分支(決戰 Namtar → 屍體送靈魂之泉 → 納達之坑) | 結局觸發(且 area27 受連通阻,進不去) |

### I. 存讀檔

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 單槽存讀檔 | ✅ | `savegame` 捕捉 area/座標/gs[256]/隊伍/seen;`verify_save` byte-for-byte round-trip | — |
| 多槽 | 🟡 | `--save-path` 架構支援,**無多槽選擇 UI** | 多槽 UI |
| 自動存檔 | ⛔ | 無 | (原版亦無強自動存檔,低優先) |

### J. 音訊

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 音效(op_90 / Ctrl+S) | ⛔(忠實 no-op) | `interpreter.cpp:1314` op_90 忠實消耗 operand、不播放(無音訊子系統);`main.cpp` 無 Ctrl+S 切換 | 整個音效/音樂子系統(屬 render/audio 副作用,非結算阻塞) |

### K. 在地化

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 繁中覆蓋(menu/chars/combat/items/spells) | ✅ | ~95%;`verify_i18n` PASS | — |
| 繁中 events | 🟡 | `events.tsv` 僅 ~13 條(序盤波卡城);完整遊戲 ~100+ | 中後期事件全覆蓋(~85% 缺,純內容工) |
| 日文 events | ✅ | 13 條 100%(X68000 反萃取,亮點,docs/reverse-engineering/46) | — |
| 日文其他層 | 🟡 | chars/combat/spells/menu 8–20% | 補齊(內容工) |
| 法術 / 物品名 | ✅ | spells.tsv/items.tsv zh-TW 全填,譯名對齊 CONTEXT.md | — |

### L. UI / 流程

| 子項 | 狀態 | 現況一句話 | 缺什麼 |
|---|:---:|---|---|
| 狀態機(6 態) | ✅ | S_MENU/S_BRANCH/S_GAME/S_COMBAT/S_MAP/S_CREATE 流程完整(`main.cpp:1911-2139`) | — |
| 開局選單 B / C | ✅ | B 新遊戲 / C 繼續(`main.cpp:1007-1027`) | — |
| 選單 D 刪除 / R 改名 / V 查看 | 🟡 | **V 查看有**(`2073,2194`);**D 刪除 / R 改名 = ⛔ 缺**(開局四人選擇的 D/R 未做) | D 刪除人物、R 改名(手冊明列) |
| 控制鍵 C/F/I/J/L/R/S/?/F4/1-4 | ✅ | 施法/戰鬥/移動/逃跑/存檔/地圖/切語言/查隊員 皆有 | — |
| 控制鍵 D / O / U / X / Ctrl+S / K | ⛔ | 遣散/重排隊伍/使用物品/屬性分配/音效/開門 全缺 | 6 個手冊明列控制鍵 |
| 戰鬥選單(Mighty Blow/Disarm Blow/Advance/Quick Fight/Dodge) | ⛔ | 僅 F 戰鬥 / R 逃跑 / C 施法;特殊攻擊與閃避指令未做 | 手冊戰鬥選項 |

---

## 2. 離可通關還缺的關鍵路徑(對照 docs/assessment/48)

依攻略真值,通關鏈 = 波卡城開局 → 序盤逃脫 → 中期收集(拉娜碎片/眼鏡/船票)→ 後期(鑄自由之劍 + 取龍寶石)→ 尼塞山腹決戰 Namtar → 結局。把「demo → 完整 RPG」的必要項按阻塞性質排序:

| 關鍵路徑項 | 為何必要 | 狀態 | 阻力性質 | 可行性 |
|---|---|:---:|---|---|
| **跨區連通(edge→area 對映 + wrap 樞紐換區)** | 主線骨幹靠 area 0 世界圖 + area 18 瑪根轉運;不通則序盤就卡、終戰 area27 進不去 | 🔒 | 受 oracle(opendw `exit(1)`)+ **需 raw RE**(從 DATA1/DRAGON.COM 重建邊界轉移表)| **需 raw RE,中大型工項**。wrap 地圖已能載入/走動是好基礎,但 edge→area 對映仍缺 |
| **戰鬥真值閉環(動作指派狀態機收斂 → 怪物 HP 真扣)** | 對「可通關」非死結(確定性模型已能打完一場);對「oracle 真值」是缺口 | 🔒 | 狀態機收斂可逆向;HP 對拍受 oracle 無獨立戰鬥路徑阻 | 狀態機**可做**;真值對拍需動 oracle |
| **quest flag / 物品 gate 判定層** | 市民證過橋、眼鏡看入口、朝聖者之袍進 Nisir、拉娜碎片重組等 gate 全靠它 | ⛔ | 無硬阻(remake 設計)| **可做**(需先跑通對應事件 script 看原版條件) |
| **使用物品(U) / 裝備穿脫 / 商店** | 鑄劍鏈、藥水、裝備換用都需要;經濟循環 | ⛔ | 無硬阻 | **可做**(售價/magic_effect 已解碼) |
| **招募隊員(酒吧)** | 隊伍補強(原版序盤即可招募)| ⛔ | 無硬阻 | **可做**(需 NPC 記錄注入 + UI) |
| **升級 / 技能效果(開鎖/lore/包紮)** | 通關需開鎖、Lore 找路、Bandage 回血等技能檢定 | ⛔ | 無硬阻 | **可做**(需逆向各技能檢定門檻) |
| **戰鬥外施法 + 控制/工具法術結算** | 軟化石穿牆進 Namtar 基地、導引、感知陷阱等地形 gate | 🟡 | 無硬阻 | **可做**(探索態施法 UI + 11 條 TODO 法術落地) |
| **結局事件** | 決戰後屍體送靈魂之泉 → 納達之坑 → 結局 | ⛔ | 無硬阻(依賴連通進得去 area27)| **可做**(設計 + 觸發) |
| **事件文字在地化(~13 → ~100+ 條)** | 主線體驗;非可玩性阻塞但沉浸必要 | 🟡 | 純內容工 | **大量內容工** |

**最短可通關路徑(承 docs/assessment/48)**:跨區連通(🔒,raw RE)→ quest gate + 使用物品 + 招募(⛔ 但可做)→ 結局 + 事件在地化(可做 + 內容工)。戰鬥用既有確定性模型即可支撐打完(真值對拍留後)。**沒有單點解;最高槓桿仍是跨區連通,且它是唯一「受阻」的關鍵路徑項——其餘關鍵缺口都是「可做但未做」的工項。**

---

## 3. ⛔/🔒 項的可行性判定(誠實標示)

| 項目 | 標記 | 可行性判定 | 依據 |
|---|:---:|---|---|
| 20 個 opendw NULL opcode | 🔒 | **需 raw RE**(DRAGON.COM 反組譯);多數遊戲路徑未踩到,按需補 | docs/reverse-engineering/42 §5;op_68 已示範可從 raw COM 反組譯逆出 |
| 跨區連通(edge→area / wrap 樞紐) | 🔒 | **需 raw RE + 中大型實作**;最高槓桿但非一行修補 | docs/assessment/48 §3-4;grep `edge.*area` 0 hit;wrap 地圖載入已就緒 |
| 戰鬥 HP 真值對拍 | 🔒 | 狀態機收斂**可做**;HP 對拍**需動 oracle**(加 headless 戰鬥入口)| docs/reverse-engineering/42 §9-14(`last_unimpl=0`,卡 res18↔res4 動作指派) |
| 武器 STR bonus 真值 | 🔒 | **受 oracle 阻**;在此之前維持 best-fit 標示 | docs/reverse-engineering/42 §13(self-modifying-code 矛盾) |
| K 開門 / 密門 / 陷阱 | ⛔ | **可做**(需逆向門/陷阱 tile 語意與事件條件)| `main.cpp:2089` stub |
| 升級 / 技能效果 / X 配點 | ⛔ | **可做**(需逆向升等門檻 + 各技能檢定)| party/chargen 數值層已備 |
| 裝備穿脫 / 使用物品(U) / 商店 / 物品轉移 | ⛔ | **可做**(售價/magic_effect/13 格欄位已解碼)| equipment.cpp |
| 招募隊員 / quest gate / 結局 | ⛔ | **可做**(remake 設計;quest 需先跑通事件 script 看條件)| gs[256] 持久旗標已備 |
| 戰鬥外施法 / 召喚 / 工具法術(11 條 TODO)| 🟡⛔ | **可做**(探索施法 UI + 結算落地)| spells.cpp `handled=false` |
| 特殊攻擊(Mighty/Disarm/Advance/Quick)/ Dodge | ⛔ | **可做**(需逆向各攻擊的傷害/命中修正)| grep 0 hit |
| 音效 / 音樂 | ⛔ | **可做**(需接音訊子系統 + 對映音效編號);非結算阻塞 | op_90 忠實 no-op |
| 事件 / 日文非 events 在地化 | 🟡 | **純內容工**(VM emit 英文鍵可逐條抽出)| events.tsv ~13 條 |
| 怪物屬性對映真值 | 🟡 | **需 RE**(opendw monster_info 未完整逆向)| combat.cpp:104-120 暫定 |

---

## 4. 結論

- **已扎實(✅)**:渲染對拍(viewport/sprite/scene/minimap)、VM 算術核心(117 opcode + 逐指令對拍)、探索移動/自動地圖/事件觸發/Read Paragraph、單槽存讀檔、戰鬥三大結算公式(to-hit/徒手/武器骰皆 **bytecode 真值**)、法術/道具的表與格式層、繁中可玩層、6 態 UI 流程。
- **未實作但可做(⛔,無硬阻)**:RPG 系統的「動詞」—— 升級/技能效果/X 配點、裝備穿脫/使用物品(U)/商店/物品轉移、招募/quest gate/結局、特殊攻擊/Dodge、K 開門/陷阱、戰鬥外施法、音效。這些是「demo → 完整 RPG」的主要內容/設計工項,**多數無技術硬阻**。
- **受阻(🔒)**:跨區連通(wrap 樞紐 edge→area 對映,**唯一受阻的關鍵路徑項**,需 raw RE + 中大型實作)、戰鬥 HP oracle 對拍(動作指派狀態機 + oracle 無獨立戰鬥路徑)、武器 STR bonus 真值、20 個 opendw NULL opcode。

**一句話定位(對齊 docs/assessment/47 §8)**:opendw_remake 是「**可驗證、誠實、序盤可玩、高保真**的重製基座」——核心引擎與序盤體驗到位且有 oracle 背書;距離「可通關完整 RPG」的差距,**結構性卡點只有一個(跨區連通,受阻),其餘是大批『可做但未做』的 RPG 系統動詞與內容工**。

---

## 附:本報告實據(絕對路徑 + 量化)

- VM:`opendw_remake/src/vm/interpreter.cpp`(kImpl 表 **117/256** 實作,實測 `grep -oE 't\[0x..\]'` 去重計數);opendw NULL **21** 個(docs/reverse-engineering/42 §5),其中 remake 已補 **op_68**;oracle-able 範圍內未補 **23** 個(含 op_71,以 main.cpp 等價邏輯模擬)。
- 渲染:`src/render/*`;ctest `verify_compose_l1`/`verify_fp_l1`/`render_sweep`/`verify_automap_l1`/`verify_seen_l1`/`verify_encounter_golden_*`。
- 探索/連通:`src/main.cpp:507-567`(op_71 等價事件)、`619-662`(`sync_relocation`,**wrap 改 modular 環繞載入**)、`2077-2089`(移動/K stub);`tools/verify/verify_wrap.cpp`(新增,wrap 地圖載入+環繞 flood-fill,**不對拍 oracle**);grep `edge.*area` = 0 hit(edge→area 對映未實作)。
- 戰鬥:`src/game/combat.{hpp,cpp}`、`combat_loop.{hpp,cpp}`;`verify_combat`/`verify_combat_loop`/`verify_combat_script`(後者**不驗 HP-vs-oracle**,docs/reverse-engineering/42 §9)。
- 法術/道具:`src/game/spells.{hpp,cpp}`(11 條召喚/工具 `handled=false`)、`equipment.{hpp,cpp}`;`verify_spells`/`verify_equipment`。
- 角色:`src/game/chargen.*`/`party.*`(`award_xp` +80 but 無升等觸發;`skills[27]` 僅武器技能進命中);`verify_chargen`/`verify_save`。
- 在地化:`assets/i18n/{zh-TW,ja}/events.tsv`(**實測 zh-TW 16 行 / ja 17 行含表頭,即 ~13-16 條序盤**);`verify_i18n`。
- 音訊:`src/vm/interpreter.cpp:1314`(op_90 忠實 no-op)。
- **驗證環境**:docker `dwsdl`,fresh build(`rm -rf build` 後 cmake + build),**ctest 20/20 全綠**(本輪實測;較 docs/assessment/47 的 19 多一個 `verify_wrap`)。
