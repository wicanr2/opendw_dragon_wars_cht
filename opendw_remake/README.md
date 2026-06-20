# OpenDW Remake — 工程/技術層 README

以 **C++20 + SDL2** 乾淨重寫的 Dragon Wars (Interplay, 1989) 執行環境,內建繁體中文化,資產已萃取成**自包含 bundle**,執行期不依賴原始 `DRAGON.COM` / `DATA1` / `DATA2`。

> **這份 README 給開發者 / 想理解實作的人。** 玩家向的「這是什麼 / 怎麼玩 / 截圖 / 視覺保真」在[根 README](../README.md);本檔聚焦架構、build、VM-oracle 方法論、opcode 覆蓋、驗證、真值層級與 docs 地圖。
>
> 設計與驗證策略的完整論述見 [`ARCHITECTURE.md`](ARCHITECTURE.md);本檔不重抄,只連結。

---

## 1. 架構總覽

核心是一個**手寫 script VM + 渲染器 + 資產層**,去**執行原始(已萃取並驗證)的 bytecode** —— 不是把組語再翻一次,而是理解後的現代重寫。遊戲邏輯本身存在 DATA1 各 section 的 script bytecode,remake 用乾淨 C++ 把它跑起來,並用差異測試逼 remake 與 oracle 逐指令一致。

- **正確性 oracle = opendw**(Devin Smith 的 C 反組譯,對應 Rebecca Heineman 1989 原版引擎)。每個模組以「與 opendw byte-for-byte / 逐指令一致」為驗收。
- **資產來源 / 逆向工具 = 上層 `../`**(`opendw_dragon_wars_cht/`):逆向報告、萃取工具、中文化資料。

模組邊界、VM 設計、三層驗證策略、自包含資產管線、階段表 R0–R6 —— 全部在 [`ARCHITECTURE.md`](ARCHITECTURE.md)。

### src/ 模組導覽

按功能切的 vertical slices(deep modules,窄對外介面):

| 目錄 | 職責 |
|------|------|
| `src/vm/` | script 虛擬 CPU(`interpreter.cpp` 的 `kImpl[256]` dispatch 表、`vm_state`、`trace` 差異測試 hook)—— remake 的心臟 |
| `src/resource/` | 資產層:DATA1/DATA2 archive reader、Huffman 解壓、5-bit text codec、自包含 bundle 格式;隱藏「原始磁碟檔 vs 自有格式」的差異 |
| `src/render/` | SDL2 渲染:320×200 indexed framebuffer、16 色盤、整數放大、8×8 ASCII 字、24×24 CJK glyph、第一人稱 viewport 合成 |
| `src/world/` | map / level / viewport 資料 |
| `src/entities/` | player / party / monster / item / spell(對照原版 `player.c` 結構) |
| `src/i18n/` | 翻譯表、Read Paragraph DB、glyph cache;中文化是一等公民,不是事後 patch |
| `src/game/` | scene 狀態機:title / explore / combat / dialogue / shop / 建角 / 結局 |
| `src/audio/` | 音效子系統:SDL2 方波合成(門/撞牆頻率由 opendw `dx/bx` 推導)+ Amiga/X68000 真實 PCM 取樣播放;`op_90` 接 `func_5060` dispatch + 門/撞牆/命中/施法事件音 |
| `src/platform/` | OS adapter(file / time / input)—— 只在邊界放 adapter |

---

## 2. Build 與開發(docker first)

### docker(與 GitHub Actions CI 同一條路)

```bash
cd opendw_remake

docker build -t dwsdl -f - . <<'EOF'
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y g++ cmake libsdl2-dev libsdl2-ttf-dev \
    && rm -rf /var/lib/apt/lists/*
EOF

docker run --rm -v "$PWD":/app -w /app dwsdl bash -c \
  "cmake -S . -B build && cmake --build build -j && cd build && ctest --output-on-failure"
```

### 本機(已備 SDL2 / SDL2_ttf)

```bash
cd opendw_remake
cmake -S . -B build && cmake --build build -j --target opendw_remake
cd build && ctest --output-on-failure          # 回歸測試:34 項
```

### 回歸測試(ctest 34 項)

`CMakeLists.txt` 註冊 **34** 個 `add_test`,涵蓋 VM 自測、存讀檔 round-trip、戰鬥(公式 / loop / special / script / round / golden 怪物)、結局鏈、建角、升級、隊伍操作、法術、商店、招募、地形、area switch / city entry / wrap、op58、i18n、render_sweep(第一人稱全 40 關)、compose / fp / automap / seen viewport、smoke。CI 全綠才算數。

### headless 開發旗標

remake 支援大量 headless dump 旗標(配 `SDL_VIDEODRIVER=dummy` 與 `--frames 0` 只 dump 不開窗),用於對拍與驗證。完整旗標表見 [`docs/engine/CONTROLS.md`](../docs/engine/CONTROLS.md)。常用:

```bash
SDL_VIDEODRIVER=dummy ./build/opendw_remake --map 1 --fp --frames 0 --dump /tmp/fp.ppm
SDL_VIDEODRIVER=dummy ./build/opendw_remake --map 1 --read-para 88 --frames 0 --dump /tmp/p.ppm
SDL_VIDEODRIVER=dummy ./build/opendw_remake --encounter 5 --combat-seed 1 --frames 0 --dump /tmp/c.ppm
```

互動執行(玩家向)的選單 / 鍵位 / 三語切換在[根 README](../README.md);完整鍵表見 [`docs/engine/CONTROLS.md`](../docs/engine/CONTROLS.md)。

---

## 3. VM-oracle 方法論

核心策略不是照抄組語,而是把反組譯當成**正確性的裁判**,自己手寫可維護的引擎,再用差異測試逼兩邊一致。

1. **逆向破解資料格式**。DATA1/DATA2 的 5-bit 文字編碼、Huffman 樹解壓(res31/res168)、sprite 去交錯、場景圖去交錯 —— 全部破解並 round-trip 對拍 opendw **byte-for-byte**。
2. **手寫 script VM**。`interpreter.cpp` 的 `kImpl[256]` dispatch 表保留與 opendw `targets[]` 相同的索引↔語意對應(差異測試基準)。每個 opcode 是命名語意的 method(`op78_set_msg`、`op4D_prng`…),不留裸 `op_78`。
3. **差異測試 harness(`diff_trace`)**。在 opendw 加 trace hook 輸出每步 `(pc_offset, opcode, r2, r4, flags, game_state_diff, emitted_text)`;remake 跑**同一段 bytecode** 輸出同格式 trace;逐行 diff,**第一個分歧點 = remake 的 bug**。
4. **golden 對拍**。對關鍵畫面(title / viewport / 戰鬥遭遇)擷取 opendw 的 framebuffer hash + 顯示字串為 golden,remake 必須吻合(中文化模式另存一組 golden)。第一人稱 viewport 已對 opendw **全 40 關逐像素一致**(`render_sweep` 154 case)。
5. **補出 opendw 從未逆向的 opcode**。op_43/5F/60/63、op_5B/68/79/6B/8D 等在 opendw 標 NULL 或無 C oracle 的指令,**直接從原始 `DRAGON.COM` ASM 反組譯補出並驗**(如 `op43_jump_above`、`op5F_or_char_data`、`op5B_get_map_tile`)。這些誠實標示為「ASM 反組譯真值」,非 opendw C oracle。

---

## 4. Opcode 覆蓋

| 項目 | 數值 |
|------|------|
| 總 opcode 空間 | 256(0x00–0xFF) |
| **remake 已實作**(`interpreter.cpp` `kImpl` 非 null) | **126 / 256** |
| 涵蓋類別 | 模式切換 / 算術 / 旗標 / 邏輯 / 比較 / 跳轉 / loop / game_state / bit / 跨資源 call / 資料資源讀寫 / 角色資料存取 / PRNG / viewport / 字串輸出 / UI |

> 未實作的多為 0xA0–0xFF 區段的原始碼殘留(非真 opcode,ASM 位址呈 x86 機器碼特徵)及少數遊戲層 context 受阻指令。實作以「跑得到主線一輪所需」為優先,逐 batch 補齊、每批用差異測試驗。判讀背景見上層 [`../docs/reverse-engineering/25_OPCODE_INTERPRETATION.md`](../docs/reverse-engineering/25_OPCODE_INTERPRETATION.md) 與 [`../docs/reverse-engineering/OPCODE_REFERENCE.md`](../docs/reverse-engineering/OPCODE_REFERENCE.md)。

---

## 5. 資產 bundle(自包含)

一次性把 DATA1/DATA2 的 bytecode + map + sprite + scene + text 萃取成 remake 自有的 **asset bundle**(`assets/bundle/`),執行期 remake **只讀 `assets/`,不碰原始 Dragon Wars 檔**。

- **ResourceProvider 抽象**:`Data1Provider`(oracle 對拍用,直讀 DATA1)/ `BundleProvider`(執行期用,讀自包含 bundle)。
- **驗證**:`BundleProvider` 載入的每個 resource == DATA1 `resource_load()` 輸出 **byte-for-byte**;sprite 走 bundle `.spr` + PNG + manifest,從 bundle 渲染與 DATA1 解碼**像素一致**。
- **換檔即換美術**:執行期不依賴 DATA1 → 未來可換 X68000 / PC-9801 素材而不動引擎。
- 設計決策見 [ADR 0001:Asset Bundle 與 ResourceProvider](../docs/adr/0001-asset-bundle-and-resource-provider.md)。

> 「自包含」指**執行期不需使用者提供原始磁碟檔**,非版權聲明;asset 內容仍衍生自原作。本 repo 不散布任何原始遊戲檔。

### 可攜發佈包

```bash
bash tools/package/build_package.sh    # → dist/opendw-remake-<版本>-Linux-x86_64.tar.gz
```

流程:cmake build → `cpack -G TGZ` 產包 → 解開 → 啟動器 headless `--frames 0` 執行驗證(**全綠才產出**)。安裝佈局:`bin/opendw-remake`(binary)+ `bin/opendw-remake.sh`(啟動器,自動 `cd` 至資產目錄)+ `share/opendw-remake/assets/`(自包含 bundle / i18n)。字型不打包(授權考量),執行期搜尋系統 CJK 字型,可用 `DWR_FONT` 指定。亦可 `cmake --install build --prefix <dir>`。

跨平台 CI(`.github/workflows/ci.yml`)五個 job,皆產**可玩產物**(GitHub Actions Artifacts):

| 平台 | 產物 | 狀態 |
|---|---|---|
| **Linux tarball** | `opendw-remake-*.tar.gz`(binary + 啟動器 + assets) | ✅ 本機 docker 實機驗證(build + ctest 35 + package + headless) |
| **Linux AppImage** | `*.AppImage`(雙擊即玩,打包 SDL2 依賴;`tools/package/build_appimage.sh`) | ✅ AppDir 結構本機驗證;appimagetool 打包於 CI |
| **Windows x64** | `opendw-remake-windows-x64/`(exe + SDL2/SDL2_ttf DLL + assets) | ⏳ vcpkg/MSVC CI 設定完整,GitHub Actions 實跑產出 |
| **macOS** | `opendw-remake-macos/`(binary + SDL dylib + assets + 啟動器) | ⏳ Homebrew CI 設定完整,GitHub Actions 實跑產出 |
| **Android APK** | scaffold(`android/`;Gradle + NDK) | ⏳ 建置 scaffold 已備;**需 asset manager 整合 + 觸控 UX + 實機測試**(見 `android/README.md`) |

所有平台**自包含 assets(bundle / i18n / fonts),不含原始遊戲檔**。字型不打包(授權),執行期搜尋系統 CJK 字型或以 `DWR_FONT` 指定。

---

## 6. 真值層級總表

貫穿全專案的原則:**bytecode 真值 / remake 設計 / 受阻** 三級分明,從不謊稱 oracle(見 `src/game/combat.hpp` 檔頭與各 docs)。

### ✅ bytecode / oracle 真值(已對拍驗證)

| 項目 | 真值來源 / 驗證 |
|------|----------------|
| 渲染:第一人稱 viewport(全 40 關)、title / 場景圖、sprite、俯視地圖、wrap 樞紐 | byte-for-byte 對拍 opendw(`render_sweep` 154 case) |
| VM 126 opcode | `diff_trace` 逐指令 == opendw |
| 戰鬥命中公式 `roll ≤ 13 + AV − (DV + AC)`(1d16+3 roll-under) | res3 + DRAGON.COM 反組譯逆出,DOS 實機交叉驗證 |
| 徒手傷害 `骰 + floor(STR/5)`、武器傷害骰(無 STR bonus) | 同上,端到端執行驗證 |
| PRNG(`op_4D`) | DOS 實機命中率吻合 |
| 連通 38/40 area、61 法術、特殊攻擊、商店、招募、升級、技能檢定、開門 / 陷阱、戰鬥外施法、存讀檔 | ctest 對拍 |
| 主線事件繁中 200+ 鍵 + 147 段落 + 結局;日文 events 212/283、怪名 | i18n 對拍 |

### ⚠️ 誠實受阻(架構或 oracle 所阻,照實說)

| 項目 | 受阻原因 |
|------|----------|
| 怪物逐回合 HP byte-diff | opendw 無獨立戰鬥入口,無法對拍每回合 HP;終戰用 remake `combat_loop`(同 bytecode 公式),非 res3 全戰鬥閉環(卡 op_89 動作指派的遊戲層 context);怪物 HP / AC 為暫定值 |
| 門 K-on-wall / 部分非戰鬥技能觸發 | 落在尚未反編的 walking-engine → remake 設計(grounded 手冊),非 bytecode 真值 |
| Namtar Boss 屬性、自由之劍祝福加成、結局序列 | remake 平衡 / 組合設計(原版勝利畫面 script 逆不出) |
| Phoebus(area 6)/ area 33 | 入口資料層隔離,為隔離分量 |
| 音樂素材渲染 | **音效已實作**(SDL2 方波 + Amiga/X68000 真實 PCM,接門/撞牆/命中/施法/`op_90`);**背景音樂引擎端就緒**(`sound.cpp` music 頻道 + 依 state 切 title/game/combat/end);僅缺把 Amiga `.tune` 經 **UADE** 渲染成 WAV(沙箱網路受限抓不到 UADE,留給本機渲染,見 `assets/bundle/audio/music/README.md`)。WAV 放進去即循環播放。DOS 版原本無背景音樂 |

量化完成度與缺口稽核見上層 [`../docs/assessment/57_PM_REVIEW.md`](../docs/assessment/57_PM_REVIEW.md)(技術 ~75% / 玩家內容 ~45–50%)、[`../docs/assessment/49_GAP_AUDIT.md`](../docs/assessment/49_GAP_AUDIT.md)、[`../docs/assessment/48_COMPLETABILITY_ROADMAP.md`](../docs/assessment/48_COMPLETABILITY_ROADMAP.md)。

---

## 7. docs 地圖

全部文件統一收在 repo 根的 [`../docs/`](../docs/README.md) 樹,依用途分成子目錄;原本分散在 `opendw_remake/docs/` 的引擎規格與玩法文件已併入其中(對照見 [`docs/README.md`](docs/README.md))。同號檔以子目錄路徑區分(例:`gameplay/57_DOORS_TRAPS_TERRAIN` vs `assessment/57_PM_REVIEW`),引用時帶完整 `docs/<子目錄>/…` 路徑即可。

remake 相關的系統文件分類:

- **玩法系統實作 `gameplay/`**:[`docs/gameplay/56_PLAYABLE_ENDING_CHAIN.md`](../docs/gameplay/56_PLAYABLE_ENDING_CHAIN.md)(可通關結局鏈)、[`docs/gameplay/57_DOORS_TRAPS_TERRAIN.md`](../docs/gameplay/57_DOORS_TRAPS_TERRAIN.md)(開門 / 陷阱 / 地形法術)、[`docs/gameplay/58_MAGIC_REFERENCE.md`](../docs/gameplay/58_MAGIC_REFERENCE.md)(法術參考表)、[`docs/gameplay/59_SKILL_CHECK_TRIGGERS.md`](../docs/gameplay/59_SKILL_CHECK_TRIGGERS.md)(技能檢定觸發點)
- **引擎規格 / 渲染 `engine/`**:[`docs/engine/VIEWPORT.md`](../docs/engine/VIEWPORT.md)、[`docs/engine/VIEWPORT_COMPOSE.md`](../docs/engine/VIEWPORT_COMPOSE.md)(第一人稱 viewport 逆向)、[`docs/engine/CONTROLS.md`](../docs/engine/CONTROLS.md)(操作 + headless 旗標)、[`docs/engine/REWRITE_READINESS.md`](../docs/engine/REWRITE_READINESS.md)(重寫就緒度評估)
- **視覺稽核 `audit/`**:[`docs/audit/60_DOS_VS_REMAKE_VISUAL.md`](../docs/assessment/60_DOS_VS_REMAKE_VISUAL.md)(DOS vs remake 逐畫面稽核 + `dos_compare/` 對照圖)
- **多版本素材 `reference/`**:[`docs/reference/61_MULTIVERSION_ASSETS.md`](../docs/reference/61_MULTIVERSION_ASSETS.md)(Amiga / X68000 素材抽取)
- **ADR**:[`docs/adr/0002-two-layer-cjk-rendering.md`](../docs/adr/0002-two-layer-cjk-rendering.md)(雙層 CJK 渲染決策)

延伸閱讀(根 docs 的核心逆向):[`../docs/reverse-engineering/42_COMBAT_BYTECODE.md`](../docs/reverse-engineering/42_COMBAT_BYTECODE.md)(戰鬥公式真值推導)、[`../CONTEXT.md`](../CONTEXT.md)(術語表)。

---

## 8. 授權

重寫程式碼(本目錄)為原創,採 **BSD**。

正確性 oracle **opendw** 由 Devin Smith 製作(BSD);它反組譯的**原版遊戲引擎**出自 Rebecca Ann Heineman(1989)。**《火龍之戰》(Dragon Wars)** 是 Interplay 的商標與著作;執行所需資產衍生自原版(1989/90),屬保存 / 中文化範疇 —— 本 repo 不散布任何原始遊戲檔(`DRAGON.COM` / `DATA1` / `DATA2`)。完整致謝見[根 README](../README.md#credits)。
