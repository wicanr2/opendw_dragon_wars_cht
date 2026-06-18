# 火龍之戰 Dragon Wars — 繁體中文化 + C++20/SDL2 乾淨重製

> *Dragon Wars*（Interplay, 1989/90）— 一套**從建角玩到結局**的繁體中文化重製
> C++20 + SDL2 乾淨重寫 ✦ 以 opendw（C 反組譯）為逐位元正確性 oracle ✦ 24px 銳利 CJK ✦ 繁中／英文／日文三語

[![CI](https://github.com/wicanr2/opendw_dragon_wars_cht/actions/workflows/ci.yml/badge.svg)](https://github.com/wicanr2/opendw_dragon_wars_cht/actions/workflows/ci.yml)
![ctest](https://img.shields.io/badge/ctest-34%2F34-success)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![SDL2](https://img.shields.io/badge/SDL2-ttf-blue)
![license](https://img.shields.io/badge/code-BSD-green)

![遊玩對照](docs/media/gameplay_dos_vs_remake.gif)

*左：DOS 原版（Interplay 1989/90，DOSBox 擷取）／右：本專案 remake — 並排同一段遊玩。*

---

## 目錄

1. [這是什麼](#what)
2. [快速開始](#quick-start)
3. [實機畫面](#screenshots)
4. [多版本美術主題（F8 切換）](#themes)
5. [世界地圖與結局](#world-ending)
6. [亮點](#highlights)
7. [DOS 原版 vs Remake — 視覺保真](#fidelity)
8. [操作](#controls)
9. [方法論：反組譯當 oracle，乾淨室重寫](#method)
10. [誠實邊界：真值 vs 受阻](#honesty)
11. [專案結構](#layout)
12. [授權與致謝](#credits)

---

<a name="what"></a>
## 🐉 這是什麼

1989 年，Interplay 推出《火龍之戰》（*Dragon Wars*）——一款以「被剝奪所有裝備、丟進罪惡之城地牢」開場的第一人稱迷宮 CRPG。它是 *Bard's Tale* 班底的精神續作，以硬派的探索、技能檢定與回合制群戰著稱。

這個專案做兩件事：

- **乾淨室重製**。不是把組合語言再翻一次，而是理解後的現代重寫——手寫的 script VM + 渲染器 + 資產層，**執行原始（已萃取並驗證）的 bytecode**。正確性靠 [opendw](https://github.com/dswban/opendw)（Devin Smith 的 C 反組譯，對應 Rebecca Heineman 1989 原版引擎）當 **oracle**：每個模組以「與 opendw 逐位元 / 逐指令一致」為驗收。
- **繁體中文化**。menu、角色、戰鬥、法術、物品全繁中，主線事件 200+ 鍵 + 147 段落 + 結局譯成繁中；遊戲中 `F4` 可即時切 繁中 / EN / 日，24×24 點陣與 SDL2_ttf 雙層渲染讓 CJK 永遠銳利。

成果：`opendw_remake/`（C++20 + SDL2）現在能跑出**完整一輪**——**建立人物 → 探索 38/40 連通世界 → 主線繁中事件 → 終戰 Namtar → 結局 → 全劇終**。資產已萃取成自包含 bundle（`assets/bundle/`），**執行期不需要原始 `DRAGON.COM` / `DATA1` / `DATA2`**。

> 想看誠實的完成度數字？技術 / 引擎保真度約 **75%**，玩家可玩內容約 **45–50%**（[docs/57 PM review](docs/57_PM_REVIEW.md)）。這份 README 不會把它說成「完整復刻」——能玩完一條主線、戰鬥核心數值對拍原版、全程繁中，但部分 RPG 養成深度與互動仍在補。詳見[誠實邊界](#honesty)。

---

<a name="quick-start"></a>
## ⚡ 快速開始

### 你需要準備

- **編譯工具**：`g++`（C++20）、`cmake`，以及 `libsdl2-dev`、`libsdl2-ttf-dev`、一份系統 CJK 字型（如 `fonts-wqy-zenhei` / Noto CJK）。
- **（玩家自備）合法的原版《火龍之戰》**：可於 [GOG](https://www.gog.com/game/dragon_wars) 購得。**執行 remake 並不需要原始遊戲檔**（資產已自包含）；保留正版是對原作的尊重，也是萃取資產的合法前提。

### 從原始碼建置

建議走 docker（環境乾淨、與 CI 一致）：

```bash
cd opendw_remake

# docker first（與 GitHub Actions CI 同一條路）
docker build -t dwr -f - . <<'EOF'
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y g++ cmake libsdl2-dev libsdl2-ttf-dev && rm -rf /var/lib/apt/lists/*
EOF
docker run --rm -v "$PWD":/app -w /app dwr bash -c \
  "cmake -S . -B build && cmake --build build -j --target opendw_remake"
```

或直接在本機：

```bash
cd opendw_remake
cmake -S . -B build && cmake --build build --target opendw_remake
```

### 跑起來

```bash
./build/opendw_remake                 # 主選單（B 建立人物 / C 繼續）
./build/opendw_remake --map 0 --fp    # Dilmun 世界圖，第一人稱
./build/opendw_remake --win640        # 640×480 視窗（固定 24/16px CJK）
./build/opendw_remake --read-para 88  # Read Paragraph 段落檢視器
./build/opendw_remake --fight-namtar  # 終戰 Namtar → 結局
cd build && ctest                     # 回歸測試（34/34；CI 亦跑）
```

預設繁體中文，遊戲中 `F4` 循環切 繁中 / EN / 日。

### 拿一個可攜發佈包

```bash
cd opendw_remake
bash tools/package/build_package.sh   # → dist/opendw-remake-<版本>-Linux-x86_64.tar.gz
```

腳本會 build → cpack 產包 → 解開 → headless 執行驗證，**全綠才產出**。解開後直接跑啟動器（會自動切到資產目錄、搜尋系統 CJK 字型）：

```bash
tar xzf opendw-remake-*.tar.gz
./opendw-remake-*/bin/opendw-remake.sh             # 選單
./opendw-remake-*/bin/opendw-remake.sh --win640    # 640×480
```

> 字型不打包（授權考量）；啟動時自動搜尋系統中／日字型（wqy-zenhei / Noto CJK / PingFang / 微軟正黑等），找不到可用 `DWR_FONT=/path/to/cjk.ttf` 指定。

---

<a name="screenshots"></a>
## 📸 實機畫面

| 在地化主選單 | 第一人稱 + 踩格繁中事件 | Dilmun 世界圖 |
|:---:|:---:|:---:|
| ![menu](opendw_remake/docs/media/showcase/menu.png) | ![fp](opendw_remake/docs/media/screenshots/r9_fp_event_twolayer.png) | ![wm](opendw_remake/docs/media/wm_world.png) |
| VM 跑 bundle bytecode → 繁中（操作對齊原版說明書） | 透視走廊 + 踩格顯事件；雙層渲染（像素層整數放大 + SDL2_ttf 24px CJK 恆銳利） | wrap 樞紐世界圖；走到城鎮格 → 切入該城 area |

| 建立人物 | 終戰 Namtar | 結局・全劇終 |
|:---:|:---:|:---:|
| ![cre](docs/chargen_screens/03_chargen_attr.png) | ![nam](opendw_remake/docs/media/screenshots/endgame/namtar_combat.png) | ![end](opendw_remake/docs/media/screenshots/endgame/ending_page4.png) |
| `B` 建角：命名 + 50 點屬性配點 + 性別 → 合法 512B 角色 record | 回合制戰鬥：命中 / 傷害公式 = 原版 bytecode 真值 | 戰勝 → 結局序列（敘事 + 結局段落 + 全劇終），繁中可捲動 |

### F4 三語即時切換（同一畫面）

| 繁體中文 | English | 日本語 |
|:---:|:---:|:---:|
| ![zh](opendw_remake/docs/media/screenshots/r10_event_zh-TW.png) | ![en](opendw_remake/docs/media/screenshots/r10_event_en.png) | ![ja](opendw_remake/docs/media/screenshots/r10_event_ja.png) |
| 主線事件繁中 | 原文英文 | events 212/283 採 X68000 原版日文原文（破解 nibble-swap SJIS） |

### 半透明對話框（踩格事件訊息）

![dialog](opendw_remake/docs/media/showcase/themes/dialog_intro.png)

*踩到事件格 → 跑該關事件 script → 畫面下半彈出**半透明訊息框**（深藍 dither 半透明，底下地圖隱約透出 + 白外框 + 亮藍內框雙線；文字層 24px CJK 恆銳利，自動換行分頁）。配色 theme-aware。*

---

<a name="themes"></a>
## 🎨 多版本美術主題（F8 切換）

《火龍之戰》在不同平台用了不同的美術。本 remake 把「介面外觀隨平台版本而不同」收斂成一組可循環的 UI 主題，遊戲中按 `F8` 即可即時切換、畫面下幀重繪並彈出主題名 toast。同一隻引擎、同一份在地化文字，換的是 title 立繪、16 色調色盤、戰鬥背景與對話框配色。

| DOS（綠龍） | Amiga（金龍） |
|:---:|:---:|
| ![title-dos](opendw_remake/docs/media/showcase/themes/title_dos.png) | ![title-amiga](opendw_remake/docs/media/showcase/themes/title_amiga.png) |
| 預設主題：原生 dragon art（res29）+ DOS 16 色標準盤 | `F8` 切到 Amiga：原生金龍標題 + Amiga 自己的 16 色 palette（讀自 `themes/amiga/title.pic` 檔頭） |

F8 循環順序為 **DOS → Amiga → X68000 → DOS**，三套主題：

| 主題 | 完整度 | 內容 |
|---|---|---|
| **DOS** | 完整 | 原生綠龍標題（res29）、DOS 16 色標準盤、res 24–28 結局五場景 |
| **Amiga** | 完整 | 原生金龍標題、Amiga 16 色 palette（planar 解碼，檔頭帶盤）、單張全螢幕結局圖 |
| **X68000** | **partial** | 目前只有怪物 / 場景 contact sheet（未切圖），**無原生標題 → 回退 DOS res29**，**palette 為 DOS placeholder**（原生 X68000 盤尚未萃取）。誠實標示，toast 也標 partial |

> 誠實邊界：X68000 是 partial（無原生標題、palette 為 placeholder、sprite 未切）。日版《火龍之戰》當年只在 **X68000** 發行，**沒有 PC-98 版本**——本專案不會憑空生出一套不存在的 PC-98 美術。

---

<a name="world-ending"></a>
## 🗺️ 世界地圖與結局

**Dilmun 世界地圖**（按 `?` 開平面地圖）。area 0 是 wrap 樞紐世界區，重畫成橫向美化圖，**24 個地點全繁中標記**（拜占庭、京雄城、凌火魔城、救贖之山、自由港、波卡城、石橋、奴隸莊園…）；走到城鎮格即切入該城 area。

![worldmap](opendw_remake/docs/audit/dos_compare/wm_app_a0.png)

**結局過場：Namtar 被擲回深淵。** 戰勝終戰 Boss 後跑結局序列——DOS 主題為 res **24–28** 五張全螢幕場景（Namtar 墜淵 → 慘叫 → 焚城 → 和平新時代 → 全劇終），每張底部疊一條半透明襯底條承載**繁中敘事**（場景烤進的原版英文立繪保留，下方壓繁中譯文，兩層並陳）。

| Namtar 墜淵（結局首場） |
|:---:|
| ![ending](opendw_remake/docs/media/showcase/themes/ending_namtar_pit.png) |
| 「你們奮力一擲，將納達擲回他當初竄出的那座深淵……」原版英文立繪 + 繁中襯底敘事 |

---

<a name="highlights"></a>
## ✨ 亮點

**一條能走完的主線**
建角 → 探索 38/40 連通區（第一人稱 viewport）→ 主線繁中事件 → 終戰 Namtar → 結局。存讀檔 byte-for-byte round-trip，Read Paragraph 防拷段落有專屬捲動檢視器。

**戰鬥核心 = 原版 bytecode 真值**
不是憑感覺調的數字。命中、傷害、RNG 都從 res3 + DRAGON.COM 反組譯逆出，端到端執行驗證：

- 命中：`roll ≤ 13 + AV − (DV + AC)`（1d16+3，roll-under）
- 徒手傷害：`骰 + floor(STR/5)`；武器傷害骰解碼自角色資料延伸位（武器無 STR bonus）
- RNG：`op_4D`；DOS 實機交叉驗證，命中率與原版吻合（[docs/42](docs/42_COMBAT_BYTECODE.md)）

**繁中 + 日文雙在地化**
menu / 角色 / 戰鬥 / 法術 / 物品 + 序盤事件繁中；events 212/283 採 X68000 原版日文原文。`F4` 切 繁中 / EN / 日，24px 銳利 CJK 雙層渲染。

**對 DOS 原版的視覺保真**
第一人稱 viewport 全 40 關逐像素對拍 opendw（`render_sweep` 154 case byte-for-byte）。整體版面、配色貼著原版 DOS，標題畫面幾乎逐像素還原（刻意保留英文 logo）。詳見[下節對照](#fidelity)與 [docs/60](opendw_remake/docs/audit/60_DOS_VS_REMAKE_VISUAL.md)。

**自包含，工程化**
資產萃取成 `assets/bundle/`，執行期不依賴原始磁碟檔；docker-first 建置；ctest **34/34**；GitHub Actions CI；Linux 可攜包已實機驗證，Windows / macOS CI 設定已備。

---

<a name="fidelity"></a>
## 🖼️ DOS 原版 vs Remake — 視覺保真

下列並排圖**左為真 DOSBox 擷取**（Interplay 1989/90），**右為 remake** headless dump。第一人稱 pipeline 已對 opendw 全 40 關逐位元一致，其餘畫面為人工視覺稽核（[docs/60](opendw_remake/docs/audit/60_DOS_VS_REMAKE_VISUAL.md)）。

| 標題畫面 | 第一人稱走廊 |
|:---:|:---:|
| ![cmp-title](opendw_remake/docs/audit/dos_compare/sidebyside/cmp_01_title.png) | ![cmp-fp](opendw_remake/docs/audit/dos_compare/sidebyside/cmp_04_fp.png) |
| 龍頭 + 紅膚戰士 + 「Dragon Wars / Copyright Interplay 89-90」逐像素還原 | 區名銀幕、右側隊伍面板 + 血條、藍磚邊框、綠柱火炬、透視走廊一致 |

| 戰鬥遭遇 | 世界圖（三方對照） |
|:---:|:---:|
| ![cmp-combat](opendw_remake/docs/audit/dos_compare/sidebyside/cmp_06_combat.png) | ![cmp-wm](opendw_remake/docs/audit/dos_compare/sidebyside/cmp_08_worldmap_3way.png) |
| 怪物圖佈局對齊原版（golden byte-for-byte） | 權威 Dilmun 設計圖 / DOS / remake 三方並排 |

整體視覺保真度高。多數差異是**刻意的在地化（繁中）或現代化輔助**（底部操作提示列、新／續遊戲選單），不是缺口。已知真缺口集中在主選單語意（remake 是「新／續」二選一、DOS 是隊伍管理選單）與 area 0 世界區 automap 全圖渲染——皆載於 [docs/60](opendw_remake/docs/audit/60_DOS_VS_REMAKE_VISUAL.md)，誠實標示。

---

<a name="controls"></a>
## 🎮 操作

操作以臺灣中文版《火龍之戰》操作手冊為準（[CONTROLS.md](opendw_remake/docs/engine/CONTROLS.md)）。啟動先顯示**火龍之戰 dragon art 標題畫面**（金色「Dragon Wars」立繪 + 在地化標題「火龍之戰」+ 閃爍「按任意鍵」），按任意鍵進主選單。

| 鍵 | 動作 | | 鍵 | 動作 |
|---|---|-|---|---|
| `B` | 開始新遊戲（建角） | | `C` | 施法 |
| `C` | 繼續舊遊戲 | | `U` | 使用物品 / 技能 |
| `I` / ↑ | 往前 | | `X` | 屬性配點 |
| `J` / ← | 左轉 | | `O` | 重排隊伍 |
| `L` / → | 右轉 | | `P` | 商店 |
| `K` | 開門 / 破密門 | | `T` | 招募隊員 |
| `V` | 查看人物 | | `?` | 平面地圖 |
| `S` | 儲存遊戲 | | `F4` | 切語言（繁／英／日） |

**全域熱鍵**（任意畫面）：

| 鍵 | 動作 |
|---|---|
| `F1` | Help 覆蓋層：半透明框列目前可用操作鍵（i18n 三語）；`Esc` 或再按 `F1` 關閉 |
| `F8` | 循環切換 UI 主題（DOS → Amiga → X68000），即時重繪 + 主題名 toast |
| `F10` | 離開遊戲：先**自動存檔**，再彈 yes/no 確認視窗（`Y`/`Enter` 離開、`N`/`Esc` 回遊戲） |
| `Esc` | 子畫面 = 返回 / 關閉；頂層（選單 / 探索）= 觸發同一離開確認流程（自動存檔 + yes/no），避免誤觸直接掉出 |

> 選單採快捷字母（與手冊一致），remake 額外提供 ↑↓ + Enter 作為現代輔助。完整鍵表與 headless 測試旗標見 [CONTROLS.md](opendw_remake/docs/engine/CONTROLS.md)。

---

<a name="method"></a>
## 🔧 方法論：反組譯當 oracle，乾淨室重寫

核心策略不是「照抄組語」，而是把反組譯當成**正確性的裁判**，自己手寫可維護的引擎，再用差異測試逼兩邊一致。

1. **逆向破解資料格式**。DATA1/DATA2 的 5-bit 文字編碼、Huffman 樹解壓（res31/res168）、sprite 去交錯、場景圖——全部破解並 round-trip 對拍 opendw byte-for-byte。
2. **手寫 script VM**。目前實作 **126/256 opcode**（模式 / 算術 / 旗標 / 邏輯 / 比較 / 跳轉 / loop / game_state / bit / 字串輸出）。差異測試 harness（`diff_trace`）逐指令比對 remake VM trace 與 opendw oracle trace。
3. **補出 opendw 從未逆向的 opcode**。op_43/5F/60/63、op_68/79/5B 等在 opendw 標 NULL 或無 C oracle 的指令，直接從原始 DRAGON.COM ASM 反組譯補出並驗。
4. **資產脫離磁碟**。ResourceProvider 抽象（oracle 用 Data1Provider / 執行期用 BundleProvider），BundleProvider 載入 == DATA1 byte-for-byte，但執行期不依賴原始檔——換檔即換美術（未來 X68000 / PC-9801 素材）。
5. **每個宣稱都可驗證**。`tools/verify/` 下 34 個 ctest 對拍渲染、存讀檔、戰鬥、連通、i18n…，全綠才算數。

延伸閱讀：
- 🏗️ [opendw_remake/ARCHITECTURE.md](opendw_remake/ARCHITECTURE.md) — VM / 渲染 / 資產層設計與階段表
- ⚔️ [docs/42 戰鬥 bytecode 逆向](docs/42_COMBAT_BYTECODE.md) — 命中 / 傷害公式真值推導
- 📐 [docs/42 為什麼原版要拆 DATA1/DATA2](docs/42_WHY_DATA1_DATA2.md) — 1989 硬體環境下的設計推理
- 📋 [ADR 0001：Asset Bundle 與 ResourceProvider](docs/adr/0001-asset-bundle-and-resource-provider.md)
- 📖 [docs 索引](docs/README.md) · [術語表 CONTEXT.md](CONTEXT.md) · [opcode 雙語參考](docs/OPCODE_REFERENCE.md)

---

<a name="honesty"></a>
## ⚖️ 誠實邊界：真值 vs 受阻

整個專案貫穿一個原則：**bytecode 真值 / remake 設計 / 受阻** 三級分明，從不謊稱 oracle（見 `combat.hpp` 檔頭與各 `docs/42`–`60`）。

**已落地（均經 opendw 對拍 / DOS 實機 / 攻略交叉驗證）**

- ✅ 渲染逐位元對拍 opendw：第一人稱 viewport（全 40 關像素 PASS）、標題 / 場景圖、sprite、俯視地圖、wrap 樞紐
- ✅ VM 126 opcode，`diff_trace` 逐指令 == opendw
- ✅ 戰鬥三大公式（命中 / 徒手 / 武器骰）= 原版 bytecode 真值，端到端執行驗證
- ✅ 連通 38/40 area、61 條法術、特殊攻擊、商店、招募、升級、技能檢定、開門 / 陷阱、戰鬥外施法、存讀檔
- ✅ 主線事件繁中 200+ 鍵 + 147 段落 + 結局；日文 events / 怪名

**誠實受阻（架構或 oracle 所阻，照實說）**

- ⚠️ **怪物逐回合 HP 無法 byte-diff**：opendw 沒有獨立戰鬥入口，無法對拍怪物每回合具體 HP。終戰用 remake `combat_loop`（同 bytecode 真值公式），非 res3 全戰鬥閉環（後者卡 op_89 動作指派的遊戲層 context）。怪物 HP / AC 為暫定值。
- ⚠️ **門 K-on-wall 與部分非戰鬥技能觸發**落在尚未反編的 walking-engine，屬 remake 設計（grounded 手冊），非 bytecode 真值。
- ⚠️ **Namtar Boss 屬性、自由之劍祝福加成、結局序列** = remake 平衡 / 組合設計（原版勝利畫面 script 逆不出）。
- ⚠️ **Phoebus（area 6）/ area 33** 入口資料層隔離，為隔離分量。
- ⚠️ **音訊**：無音訊子系統（op_90 忠實 no-op），全程靜音。

完整分級量化見 [docs/57 PM review](docs/57_PM_REVIEW.md)（技術 ~75% / 玩家內容 ~45–50%）、[docs/49 缺口稽核](docs/49_GAP_AUDIT.md)、[docs/48 可通關 roadmap](docs/48_COMPLETABILITY_ROADMAP.md)。

### 跨平台狀態

| 平台 | 取得 SDL2 | 狀態 |
|---|---|---|
| Linux (x86_64) | apt `libsdl2-dev` `libsdl2-ttf-dev` | ✅ docker 實機驗證：build + ctest 34/34 + 產包 + headless 執行 |
| Windows (x64) | vcpkg `sdl2` `sdl2-ttf`（MSVC） | ⏳ CI 設定已備，未在本環境實機驗證 |
| macOS | Homebrew `sdl2` `sdl2_ttf` | ⏳ CI 設定已備，未在本環境實機驗證 |

---

<a name="layout"></a>
## 📁 專案結構

```
opendw_dragon_wars_cht/
├── opendw_remake/         # ★ 主產物：C++20 + SDL2 乾淨重寫的 runtime（可玩）
│   ├── src/               #   resource / vm / render / game / i18n
│   ├── tools/verify/      #   對拍 / 驗證工具（ctest 34 項）
│   ├── tools/extract/     #   DATA1/DATA2 → 自包含 bundle 萃取
│   ├── assets/bundle/     #   自包含資產（maps/sprites/scenes/scripts/monsters/items/…）
│   ├── assets/i18n/       #   zh-TW / en / ja 在地化 TSV
│   └── docs/              #   remake 專屬截圖 / 設計筆記
├── src/                   # opendw（C 反組譯）— 唯讀，當逐位元正確性 oracle
├── docs/                  # 設計筆記 + 逆向報告（00 索引；42-60 戰鬥/連通/評估/視覺稽核）
└── CONTEXT.md             # 術語表（ubiquitous language）
```

> opendw_remake 不依賴原始磁碟檔（資產已萃取成自包含 bundle）；`src/`（opendw）僅作為差異測試的對照 oracle。

---

<a name="credits"></a>
## 🙏 授權與致謝

**重寫程式碼**（`opendw_remake/`）為原創，採 **BSD** 授權。

**opendw**（C 反組譯，本專案的正確性 oracle）由 **Devin Smith** 製作、採 BSD 授權；它反組譯的**原版遊戲引擎**出自 [Rebecca Ann Heineman](https://www.burgerbecky.com/)（1989 原作程式）。

**《火龍之戰》(Dragon Wars)** 是 **Interplay** 的商標與著作。本專案的執行所需資產衍生自原版（1989/90），屬保存 / 中文化範疇——**本 repo 不散布任何原始遊戲檔**（`DRAGON.COM` / `DATA1` / `DATA2`）。請於 [GOG](https://www.gog.com/game/dragon_wars) 取得合法原版。

| 致謝 | 貢獻 |
|---|---|
| **Interplay** | 《火龍之戰》原作（1989/90） |
| **Rebecca Ann Heineman** | 原版遊戲引擎程式（1989） |
| **Devin Smith / opendw** | C 反組譯——本專案的正確性 oracle |
| **WenQuanYi 文泉驛 / Noto CJK** | 中日文字型 |
| **Chun-Yu Wang**（wicanr2） | 本專案發起人 |

---

*火龍之戰 Dragon Wars 繁體中文化專案 — [wicanr2/opendw_dragon_wars_cht](https://github.com/wicanr2/opendw_dragon_wars_cht)*

> 人生總該做點事情留下紀念。
> 希望這份繁中重製，能讓後來的人更容易認識那些經典 CRPG，知道它們曾經有多迷人。
