# 59 — 版面忠實度比對(Remake vs 1990 原版 Dragon Wars)

> 唯讀分析。把 `opendw_remake` 各畫面的版面配置,逐項對照 1990 年原版 *Dragon Wars*(Interplay / Rebecca Heineman)的實機配置,確認重製版是否貼近原始。
> **權威參考(原版 ground truth)**:`docs/dos_playtest/`(dwdos 跑原版 DOS 抓的真畫面)+ `docs/33_MANUAL_TRANSCRIPTION.md`(臺灣中文版手冊轉寫)。
> **remake 現況**:用現有 headless dump(`--fp` / `--encounter` / `--char-sheet` / `--map`)在 docker(`dwsdl` 影像)重產,scale 3(960×600,等比 320×200)。
> 並排對照圖在 `docs/layout_compare/`;remake 單張原圖在 `docs/layout_compare/remake_raw/`。

---

## 結論先行(BLUF)

remake 把每個畫面該有的**元素都畫到了、相對位置大致正確**(viewport 在左、隊伍面板在右、訊息/提示在下、角色屬性與選單為獨立子畫面),核心 viewport 與怪物立繪甚至對拍 opendw 達 byte-for-byte。但**整體視覺框架與原版差距明顯**,主要是兩類:

1. **缺少原版的裝飾框架與標題 logo**。原版整個畫面被一圈藍色石磚邊框包住,右上角有金色「Dragon Wars」logo;remake 是純黑底、無邊框、探索/戰鬥畫面**完全沒有 logo**(只有右上 `[繁中]` 語系標)。
2. **子畫面(角色屬性、戰鬥選單)的排版邏輯被改寫**。原版角色屬性是「全螢幕、每列 4 欄」的緊湊網格;remake 是「半螢幕藍框、每列 1 個屬性」的直式清單。原版戰鬥有「遭遇選單 + 逐人動作選單」佔據右側面板;remake 用底部一行 `F:戰鬥 R:逃跑 C:施法` 取代。

下表先給總覽,後面逐畫面展開。多數偏差是**重製期刻意簡化或為了 CJK 可讀性**(非 bug),但「右上 logo 缺失」「裝飾邊框缺失」「角色屬性版面重排」是可以、也值得往原版靠攏的項目。

| # | 畫面 | 一致度 | 一句話 |
|---|------|:---:|------|
| 1 | 第一人稱探索 | 🟡 小偏 | viewport/面板/位置對,缺右上 logo + 石磚框,訊息列改成控制提示 |
| 2 | 戰鬥 / 遭遇 | 🟡 小偏 | 怪物圖位置對(byte-for-byte),但未框進 viewport 框、放大溢出;缺 logo/框 |
| 3 | 角色屬性 | ❌ 明顯偏 | 全螢幕網格 → 半螢幕直式清單,欄位增減,版型重寫 |
| 4 | 遭遇選單 | ❌ 明顯偏 | 原版右側「Fight/Quickly fight/Run/Advance」選單缺,改底部一行提示 |
| 5 | 戰鬥動作選單 | ❌ 明顯偏 | 原版右側「X, choose: Attack/Dodge/…」逐人選單缺(戰鬥結算本身為 placeholder) |

> 一致度判讀:✅ 貼近原版(位置/比例/元素皆對)/ 🟡 小偏(主結構對,缺裝飾或局部移位)/ ❌ 明顯偏(版型或互動結構被改寫)。

---

## 原版版面的基準座標(共用框架)

原版探索與戰鬥共用同一套「框架」,從 `docs/dos_playtest/05_world_map_ui.png`、`16_guard_hits_7_stun.png` 與手冊 L131–138 可歸納:

| 區塊 | 原版位置(320×200 內) | 內容 |
|------|------|------|
| 裝飾外框 | 包住整個 UI 一圈 | 藍色石磚紋邊框(底部一條最明顯) |
| 場景名 | 左上(白字) | 如 `Purgatory` / 遭遇時為怪物群名 `King's Guards` |
| **Dragon Wars logo** | **右上(金色圖)** | 隊伍面板正上方的招牌 logo |
| viewport | 左方,160×136 @ framebuffer (16,8) | 3D 透視牆面 / 天空地板 / 怪物立繪 |
| 隊伍面板 | 右方,logo 下 | 隊員名 + 其下 2–3 條狀態橫線 |
| 訊息列 | 下方,viewport 下整條 | 白底黑字訊息框(戰鬥訊息 / 旅行指南提示) |

狀態橫線語意(手冊 L135,逐字):**第一條=紫色=健康**、第二條=綠色=體力、第三條=藍色=法力;愈長愈好。

remake 的對應實作(`src/game/party_panel.cpp`、`src/main.cpp`):

- viewport:`160×136 @ (16,8)`,**與原版一致**(對拍 `verify_fp` 4/4、`render_sweep` 全 40 關)。
- 隊伍面板:名字 x=216(`0x1B<<3`),狀態條 x=216–312,首列 y=32,每員 stride 16 掃描線。
- 狀態條色:HP=`0x0C` **亮紅**、體力=`0x0A` 亮綠、法力=`0x09` 亮藍。
- 場景名:左上 (8,2),色 14(對齊原版位置)。
- 標題 logo:`add_title()` 畫在 (8,6),**但僅選單/建角/分支畫面才呼叫**;探索/戰鬥畫面不畫 logo。
- 右上 (kW−56,2) 畫 `[繁中]/[EN]/[日]` 語系標(remake 新增,原版無)。

---

## [1] 第一人稱探索 First-person Exploration

對照圖:`docs/layout_compare/01_fp_explore.png`
(左=原版 `05_world_map_ui.png`;右=remake `--map 1 --fp`,迷宮透視牆面)
remake 戶外版另見 `remake_raw/fp_explore.png`。

| 元素 | 原版 | remake | 判定 |
|------|------|--------|:---:|
| viewport 位置/大小 | 160×136 @ (16,8) | 同(160×136 @ (16,8),透視牆面對拍 golden) | ✅ |
| 場景名 | 左上,白字 `Purgatory` | 左上 (8,2),金字 `Dilmun`/`Purgatory` | ✅ 位置一致(色偏金) |
| **Dragon Wars logo** | **右上金色 logo** | **無 logo**(只有 `[繁中]` 語系標) | ❌ 缺 logo |
| 隊伍面板 | 右側,緊貼 viewport,名 + 狀態條 | 右側,名 + 狀態條;但**起點偏右、與 viewport 間留大片黑** | 🟡 偏右、間距大 |
| 狀態條色 | 紫 / 綠 / 藍(手冊) | 紅 / 綠 / 藍 | 🟡 HP 條紅≠紫 |
| 訊息列 | 下方整條白底黑字框 | **無訊息框**;改畫控制提示 `I:fwd J/L:turn …` | 🟡 列被改用途 |
| 裝飾外框 | 藍石磚框包住全畫面 | 無(純黑底) | ❌ 缺框 |

**小結**:結構正確、viewport 像素級對齊;主要偏差是「右上缺 logo、缺石磚外框、訊息列改成控制提示、隊伍面板偏右」。

**建議**:
- 把 logo 圖(原版資源)blit 到右上隊伍面板正上方(約 viewport 右側、y≈0 起);至少在探索/戰鬥畫面補上。
- 隊伍面板 x 起點目前 216,與原版相當;但右側橫線過長拉到 ~kW,視覺上比原版寬,可收斂條長到貼近原版(x 312 截止已對,問題在 scale 後比例感)。
- 訊息列回填白底黑字訊息框(空白時留框),控制提示移到框內或框下,不要佔掉訊息區。
- HP 條色若要逐字對手冊,改成紫色(原版第一條為紫);目前紅色是可接受的近似,屬刻意選擇可保留,但需在文件標明與手冊不同。

---

## [2] 戰鬥 / 遭遇 Combat / Encounter

對照圖:`docs/layout_compare/02_combat_encounter.png`
(左=原版 `16_guard_hits_7_stun.png`;右=remake `--encounter 0`)

| 元素 | 原版 | remake | 判定 |
|------|------|--------|:---:|
| 怪物立繪錨點 | viewport 內 @ (16,8) | @ (16,8)(對齊 `draw_random_encounter_graphic`,golden byte-for-byte) | ✅ 錨點一致 |
| 怪物立繪框 | **框在 160×136 viewport 內** | 立繪放大、**未受 viewport 框約束**,向右溢出超過 x≈177 | 🟡 大小/裁切偏 |
| 遭遇/怪物名 | 左上 `King's Guards` | 左上 `6隻 強盜 (存活6)` | ✅ |
| 隊伍面板 | 右側,名 + 條 + 行內狀態(`is stunned`) | 右側,名 + 條(狀態文字邏輯已有,`is %s`) | ✅ 結構對 |
| 訊息列 | 下方白框:攻擊/傷害敘述 | 底部控制提示 `F:戰鬥 R:逃跑 C:施法` | 🟡 改用途 |
| logo / 外框 | 右上 logo + 石磚框 | 無 | ❌ 缺 |

**小結**:怪物錨點與隊伍面板結構對齊原版;但 remake 立繪未被 viewport 框裁切而向右溢出,且同樣缺 logo/框、訊息列改控制提示。戰鬥結算本身在 remake 為乾淨室 placeholder(opendw C 本身也未實作結算,見 `42_COMBAT_BYTECODE.md`),非原版真值。

**建議**:
- 怪物立繪限制在 160×136 viewport 區內(裁切或縮放到框內),避免覆蓋到右側面板區。
- 戰鬥訊息回填到下方訊息框;控制提示挪位。
- 補 logo / 外框同 [1]。

---

## [3] 角色屬性 Character Stats

對照圖:`docs/layout_compare/03_char_stats.png`
(左=原版 `06_muskels_stats.png`;右=remake `--map 0 --char-sheet 1`)
remake 原圖:`remake_raw/charsheet.png`。

| 面向 | 原版 | remake | 判定 |
|------|------|--------|:---:|
| 覆蓋範圍 | **近全螢幕**框,底部見石磚邊 | **半螢幕藍框**,右側 viewport+隊伍面板仍露出 | ❌ |
| 排版 | **每列多欄網格**:Str/Dex/Int/Spr 一列、Attack/Defense/Level/AC 一列、Health…Stun 一列、Power…Exp 一列 | **直式清單**:每列一個屬性(力量/敏捷/智力/精神/生命/暈眩值/法力/等級/成長點數/金幣/狀態/性別) | ❌ 版型重寫 |
| 標題 | 左上 `Muskels's statistics.` | `角色 1/4    Muskels` | 🟡 |
| 攻防欄位 | 顯示 `Attack:5 Defense:5 AC:0` | **未顯示 AV/DV/AC**;改顯示 狀態/性別/成長點數 | ❌ 欄位差異 |
| 持有物 | `Carried items` 區 + 物品列(A) Gold) | 主屬性表不含;物品另走 `--inventory` 子畫面 | 🟡 拆成兩頁 |
| 結束提示 | 置中 `ESC to exit` | 左下 `[繼續]` + `1-4 E:Items X:AP Esc` | 🟡 |

**小結**:這是**偏差最大的畫面**。原版屬性是一張全螢幕、資訊密度高的網格表;remake 改成現代直式 label:value 清單,且少了 AV/DV/AC、多了狀態/性別。對「貼近原版」目標來說,版型已被改寫。

**建議**(若以「忠於原版版面」為目標):
- 改回全螢幕框,採原版多欄網格:第一列 力量/敏捷/智力/精神,第二列 攻擊/防禦/等級/AC,第三列 生命/暈眩,第四列 法力/經驗,下接「持有物」清單。
- 補回 AV/DV(攻擊/防禦)、AC 三欄。
- 結束提示置中 `ESC 離開`。
- 若團隊決定保留現代直式清單(可讀性較好、CJK 友善),則屬**刻意設計選擇**,需在 `docs/CONTROLS.md` 或本文件標注「角色屬性版面為 remake 重新設計,非原版網格」。

---

## [4] 遭遇選單 Encounter Prompt(Fight / Run)

對照圖:`docs/layout_compare/04_encounter_prompt.png`
(左=原版 `11_encounter_kingsguards.png`;右=remake `--encounter 0`)

原版遭遇開場:**右側面板區**變成白底黑字描述框,內含怪物登場敘述 + 選單
`Will the party: Fight / Quickly fight / Run / Advance ahead`。怪物立繪在左方 viewport(框內),下方訊息列空白。

remake:沒有這個遭遇描述/選單框,直接進戰鬥畫面,底部一行 `F:戰鬥 R:逃跑 C:施法`。

| 元素 | 原版 | remake | 判定 |
|------|------|--------|:---:|
| 遭遇描述框 | 右側白框 + 怪物敘述 | 無 | ❌ |
| 行動選單 | `Fight/Quickly fight/Run/Advance ahead`(右側) | `F:戰鬥 R:逃跑 C:施法`(底部一行,缺 Quickly/Advance) | ❌ 位置+項目皆偏 |
| 怪物立繪 | viewport 框內 | 放大溢出(同 [2]) | 🟡 |

**小結**:原版的「遭遇描述 + 四選項」在 remake 缺席。屬戰鬥系統尚未完整實作(placeholder)的範疇。

**建議**:戰鬥系統落地時,在右側面板區實作遭遇描述框 + `Fight / Quickly fight / Run / Advance ahead` 選單(取代當前底部一行),貼回原版互動位置。

---

## [5] 戰鬥動作選單 Combat Action Menu

對照圖:`docs/layout_compare/05_combat_action_menu.png`
(左=原版 `12_combat_action_menu.png`;右=remake `--encounter 0`)

原版逐人行動:**右側面板區**(原本顯示隊伍狀態的位置)換成
`Muskels, choose:` + `Attack / Dodge enemies / Block attack / Use item / New weapon / Load weapon / Run / Move / ? View the party`,底部 `ESC to go back`。viewport 與下方訊息框維持。

remake:無此逐人動作選單(戰鬥結算為 placeholder),底部僅 `F/R/C` 提示。

| 元素 | 原版 | remake | 判定 |
|------|------|--------|:---:|
| 逐人動作選單 | 右側面板區,`X, choose:` + 9 項 | 無 | ❌ |
| viewport / 訊息框 | 維持 | viewport 維持;訊息框缺 | 🟡 |

**小結**:同 [4],屬戰鬥系統未完整實作。CONTROLS.md 已明載「戰鬥結算為乾淨室 placeholder」。

**建議**:戰鬥系統落地時,逐人回合在右側面板區顯示 `<角色>, choose:` 動作選單(Attack/Dodge/Block/Use item/New weapon/Load weapon/Run/Move/? View party),`ESC to go back`,對齊原版位置。

---

## 偏差優先清單(Top 偏差,依「貼近原版」價值排序)

| 優先 | 偏差 | 畫面 | 性質 | 建議 |
|:---:|------|------|------|------|
| ★1 | **右上 Dragon Wars logo 缺失** | 1,2 | 可補(原版資源存在) | 探索/戰鬥畫面把 logo blit 到右側面板正上方 |
| ★2 | **藍石磚裝飾外框缺失** | 1,2,(3) | 可補 | 補一圈石磚邊框框住 UI(或至少底部框) |
| ★3 | **角色屬性版面重寫**(全螢幕網格 → 半螢幕直式) | 3 | 刻意/可改 | 決定:回原版網格 or 明文標注為 remake 設計;補回 AV/DV/AC |
| ★4 | **訊息列改成控制提示**(下方白框被佔用) | 1,2 | 可改 | 回填白底黑字訊息框,控制提示移位 |
| ★5 | **怪物立繪溢出 viewport 框** | 2,4 | bug-ish | 立繪裁切/縮放限制在 160×136 框內 |
| ★6 | **遭遇選單 / 逐人動作選單缺席** | 4,5 | 系統未實作 | 戰鬥系統落地時補右側面板選單(Fight/Quickly/Run/Advance;X,choose:…) |
| ★7 | HP 條色 紅≠紫 | 1,2 | 刻意/小 | 對手冊可改紫;保留紅則文件標注 |
| ★8 | 場景名色 金≠白 | 1,2 | 小 | 可改白字貼近原版 |
| — | `[繁中]` 語系標、F4 切語、CJK 訊息框深藍底 | 全 | **刻意**(中文化/i18n) | 保留,屬 remake 正當增強 |

## 偏差性質:刻意 vs 該修

- **刻意(保留)**:`[繁中]/[EN]/[日]` 語系標、F4 即時切語、訊息/段落框改深藍底白邊以利 CJK 24px 字渲染、角色屬性若採直式清單為可讀性設計 — 這些是中文化/重製的正當取捨,**不建議為了像素忠實而移除**,但應在文件標注「與原版不同且為刻意」。
- **該修(往原版靠)**:右上 logo、石磚外框、訊息列被控制提示佔用、怪物立繪溢出框 — 這些不影響中文化,補回即可更貼近原版,成本可控。
- **待系統落地**:遭遇選單、逐人戰鬥動作選單 — 受限於戰鬥結算尚為 placeholder(`42_COMBAT_BYTECODE.md`),非版面問題,屬功能缺口。

---

## 附:重製此分析(headless dump 指令)

於 docker(`dwsdl` 影像)內,以 dummy SDL driver 重產(scale 3,等比 320×200):

```
SDL_VIDEODRIVER=dummy ./build/opendw_remake --map 1 --fp        --frames 0 --dump fp.ppm    --locale zh-TW
SDL_VIDEODRIVER=dummy ./build/opendw_remake --encounter 0       --frames 0 --dump enc.ppm   --locale zh-TW
SDL_VIDEODRIVER=dummy ./build/opendw_remake --map 0 --char-sheet 1 --frames 0 --dump cs.ppm --locale zh-TW
```

PPM → PNG:以最小 zlib PNG 編碼器轉(docker 內 python3 zlib;見產出流程)。並排對照以本機 PIL 合成。

> 註:`--char-sheet N` 需 `state==S_GAME && party>0`,故須配 `--map`;單獨 `--char-sheet` 會落回主選單畫面。
