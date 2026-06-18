# 61. 多版本美術素材萃取（Amiga / X68000 / PC-98）

為 remake 後續「遊戲中切換 theme」準備,從《Dragon Wars》三個原版磁碟抽出美術素材
(標題畫面、怪物 sprite、場景圖、UI 圖示),轉成 PNG。DOS 版美術已在既有 bundle
(預設 theme),本文件處理另外三個平台。

原始遊戲檔(.dim / .PIX / .PKH / .adf / DRAGON.X / data*)一律**不入庫**,只放抽出
並轉成可用格式的 PNG + 抽取工具 + 本文件。全程 docker,不污染系統。

## 0. 三版總結

| 版本 | 磁碟形式 | 標題畫面 | 怪物 sprite | 場景/過場 | UI 圖示 | 狀態 |
|---|---|---|---|---|---|---|
| **Amiga** | .adf + WHDLoad HD(`data/`) | ✅ title.pic | ✅ data4 4-bitplane(**50 隻已切**，見 §1.5） | ✅ endgame、**viewport 牆面 data3(已逆 + 接 §1.6)** | ✅ cursors | **大部分成功** |
| **X68000** | .DIM(Human68k FAT12) | ❌ TITLE.PKH(壓縮) | ✅ MON.PIX | ✅ PIC.PIX、❌ 3D/END(壓縮) | ✅ ICON.PIX | **未壓縮 .PIX 成功,.PKH 受阻** |
| **PC-98** | — | — | — | — | — | **素材不存在(見 §3)** |

產出路徑:`opendw_remake/assets/bundle/themes/<version>/`(`amiga` / `x68000`)。

---

## 1. Amiga 版

### 磁碟結構
來源 `dragonwars_amiga_win.7z`(8.7MB)= FS-UAE 打包的 WHDLoad 版。實際美術在
**WHDLoad 硬碟安裝的 `data/` 目錄**(未壓縮個別檔,非磁碟映像):

```
DragonWars/fsuae/Hard Drives/data/
  title.pic   21805 B   標題畫面(壓縮)
  endgame     32468 B   結局過場圖
  picparts    16777 B   過場圖元件
  data1..6              地圖/事件/資料
  dw          75404 B   主程式或資料
  cursors       574 B   游標
```
另有 `Floppies/disk.adf`(901120 B,原版開機磁碟)。

### 格式與解碼(已確認)

**關鍵發現:Amiga 與 DOS 共用同一個 Huffman 壓縮 codec**(`src/resource/decompress.cpp`,
`[size_LE][tree][bitstream]`,big-endian 16-bit bit reader)。只有 X68000 的 `.PKH`
是另一套不相容的 codec(見 §2)。

title.pic 解碼流程(逐字驗證):
```
title.pic (21805B, 壓縮)
  → Huffman decompress (DOS codec)
  → 35996B = [32B palette][32000B bitplane data][尾端少量 bytes]
     palette: 16 個 big-endian word,0x0RGB 格式(每 nibble × 17 → 8-bit)
              word[0]=000(黑) word[1]=fff(白) word[2]=f59(金) … → 金色龍頭
     image:   320×200,**4 bitplanes,sequential(plane0 全圖→plane1→…,非 Amiga interleaved)**
              dataoff = 32(palette 之後)
```

- **title.pic → `themes/amiga/title.png`**:**成功**(640×400 = 320×200 ×2)。完整標題 =
  金色龍頭(紅舌)+ 紅甲持斧戰士 + "Dragon Wars / Copyright Interplay 89-90" logo。真實 palette。
- **endgame → `themes/amiga/scenes/endgame.png`**:**成功**(320×200)。同 codec/格式。結局場景
  (爆發的鬍鬚巨人 = Namtar,藍天 + 月 + 山)。
- **picparts**:⚠️ **部分/受阻**。過場圖元件(viewport/dungeon frame),已解出結構但未完整還原
  正確 planar 佈局,目前僅 1bpp 預覽看得出輪廓。

### 工具
- `tools_build/amiga_pic_extract.py <raw_decompressed.bin> <out.png>`:吃「已 Huffman 解壓」
  的 buffer,依 `[32B palette][320×200×4 seq planes]` 佈局渲染成 PNG。
- 解壓用既有 DOS Huffman 解壓器(`src/resource/decompress.cpp` 同演算法;tools_build/scratch
  亦有可移植版本)。

---

## 1.5 Amiga 怪物 sprite（data4，已逆向 + 接進戰鬥）

> **2026-06-18 更新（全套抽取 + 權威對映）**:由 6 隻擴到 **50 隻**(data4 全部偶數資源)。
>
> - **格式關鍵更正**:data4 怪物資源 = **偶數 res ID**(140/144/146/152/…/254)為怪物 sprite,
>   奇數 res ID 解出垃圾尺寸(非 sprite,語意未明)。50 個偶數資源全部解碼成合理尺寸的立繪
>   (目視 contact sheet 確認:人類/野獸/惡魔/龍/騎士/巨人…完整圖鑑)。
> - **權威 name→sprite 對映(本次確立)**:`MonsterRecord::sprite_res() = (attr[0x0B]<<1)+0x8A`
>   **正確**(舊 docs/26 §二「+0x8A 有偏差」的疑慮為誤判)。monsters.bin 全 25 隻記錄的
>   `sprite_res()` 皆落在已抽的偶數資源(Spider→196、Wolf→168、Pikeman→210、Fanatic→222、
>   Innocent Man→200、King's Guard/Soldier→166、Humbaba→218、Gladiator→202…)。
> - **入庫命名**:全套以 `<res>.spr` 命名(`themes/amiga/sprites/166.spr` …);原 6 隻具名檔
>   (`196_spider.spr` 等)保留(verify_theme 既有斷言用)。
> - **接線改用權威對映**:`main.cpp` `sprite_for_monster(name, sprite_res)` 在 Amiga theme 下
>   **優先**載 `<sprite_res>.spr`(全套命中),名稱關鍵字降為次選、DOS bundle 為末選回退。
>   F8 切 Amiga → 每隻怪物呈現自己的 Amiga 立繪(不再只有名稱命中的 6 隻;含 Humbaba/Gladiator
>   等先前缺的)。headless `--theme amiga --encounter <idx>` dump 目視確認 Gladiator(202)、
>   Humbaba(218)為原生 Amiga 美術。
> - **工具**:`tools_build/amiga_res_extract`(C++,連 decompress.cpp)批次抽 data4 →
>   `amiga_sprite_extract.py --auto-crop` 逐隻轉 .spr。
> - **受阻**:仍只切主格(多動畫格未逐格切);200/242/244 等少數自帶盤 bg=黑(非紅),
>   自動裁切只取單格,品質次於紅底主流。


### archive 結構（已驗證）
Amiga `data1`–`data6` 與 DOS opendw archive **同格式、同 codec、同資源編號**:每檔 768B
header(384 個 LE16 size,≥0xFF00 = 資源不在此檔)+ 串接 Huffman 壓縮資源。怪物 sprite 主力
在 **data4(res 140–255)**。各檔自帶 header,offset = 768 + 該檔內前序 present 資源 size 總和。

- 抽取工具:`tools_build/amiga_res_extract.cpp`(連結既有 `src/resource/decompress.cpp`,
  不重寫 Huffman)。`amiga_res_extract <datafile> <res_id> <out.bin>`。

### sprite 格式（本次逆向確認）
怪物資源解壓後佈局:
```
[0..31]   16 個 BE word palette(0x0RGB,每 nibble ×17 → 8-bit;同 title.pic）
[32..]    8-word sprite header:
            word[1] = 高度 H(px)
            word[2] = 每 plane 每列 bytes(bpr）→ 寬 W = bpr*8
          影像資料即自 offset 32 起(header 與 plane0 前幾列共用前 16 bytes)：
          4 bitplanes、**plane-sequential**（plane0 整張→plane1 整張→…）、MSB-first。
解碼:val = Σ plane_p[y][x] << p（p=0..3）→ palette index 0..15
```
逆向過程(由結構分析 + 目視比對 DOS contact sheet 收斂):確認非 DOS encounter XOR-delta
blit、非 chunky、而是 plane-sequential 4-bitplane;width/height 由 header word[2]/word[1] 取得。

### 已切 sprite（6 隻，目視確認）
背景紅(palette index 8 = 0xF00,各怪物自帶盤一致)→ 戰鬥用 index 8 為透明色。
多動畫格資源以「整列同色 solid bar」自動分隔取主格(`--auto-crop`)。

| 資源 | 怪物 | 尺寸(px) | 備註 |
|---|---|---|---|
| 196 | Spider | 94×63 | 藍蜘蛛,乾淨 |
| 168 | Wolf | 83×112 | 棕狼,乾淨 |
| 222 | Fanatic | 67×132 | 白髮藍衣人,乾淨 |
| 152 | Guard/Soldier | 88×112 | 持斧戰士,乾淨 |
| 210 | Pikeman | 93×137 | 持矛裝甲兵,乾淨 |
| 200 | Innocent Man | 33×104 | ⚠️ 自帶盤 bg=黑、多格自動裁切只取單格;品質次於上列 5 隻 |

- 轉檔工具:`tools_build/amiga_sprite_extract.py <res.bin> <name> <out_dir> --auto-crop`
  → remake `.spr`(indexed + 自帶 16 色 Amiga palette)+ `.png`(目視)。
- 入庫:`assets/bundle/themes/amiga/sprites/`(原始遊戲檔不入庫)。

### 接進 Amiga 主題戰鬥（theme-aware combat art）
- `UiTheme` 新增 `sprite_dir` / `sprite_own_palette` / `sprite_transparent`;Amiga theme =
  `themes/amiga/sprites` / true / 8。`main.cpp` `sprite_for_monster()` 依當前 theme 選來源
  (Amiga 缺檔回退 DOS bundle,誠實降級)。
- 各 Amiga 怪物自帶 palette(蜘蛛綠金 / 狼棕 / 狂信者藍紅各異)→ combat 渲染前
  `set_palette(sprite.palette)`(index 0=黑 1=白 8=紅 各盤一致,UI/backdrop 仍可讀);
  探索/地圖等非戰鬥畫面於 `draw_base` 還原 `theme.palette`,避免色盤殘留。
- **F8 切 Amiga → 戰鬥怪物圖即時換成 Amiga 美術**(戰鬥中 F8 會 reload `enc.sprite`)。
- headless 驗證:`--theme amiga --encounter <id>`;dump 已目視確認 Spider/Wolf/Fanatic/Pikeman
  皆為 Amiga 原生立繪。DOS theme combat 像素 dump 與改動前 **byte-identical**(golden 未破)。
- 回歸保護:`verify_theme`(ctest)新增 6 隻 Amiga sprite 載入 + 尺寸 + 自帶盤 + sprite 接線斷言。

### 受阻 / TODO
- 多動畫格:目前每隻只切**主格**(自動分隔)。逐 frame 動畫(攻擊/受擊)未切,需逆向
  frame 表細節(各資源含 2–3 格水平並排)。
- 196/152/210 主格右緣偶留 1–2 px 黑分隔條(crop 邊界);200_innocent_man 自動裁切只取到單格,
  品質次於其餘 5 隻。
- header word[0]、word[3] 等其餘欄位語意未全解(已知 word[1]=H、word[2]=bpr 足以正確渲染)。

---

## 1.6 Amiga 第一人稱 viewport 牆面（data3，已逆向 + 接進 Amiga 第一人稱）

對應舊「picparts ⚠️ 部分/受阻」與 docs/26 §七「viewport 場景組譯器」待辦 —— **本次逆出格式並接進
Amiga 第一人稱**。

### archive 結構（已驗證）
viewport 元件主力在 **data3(res 110–135)**,與 DOS `bundle/components/<tag>.bin` **同資源編號**
(110=Castle wall、111=Sky、112=Road、116=Water…;DOS 用到的 19 個 tag 在 data3 全present)。

### Amiga viewport 圖塊格式（本次逆向確認）
與怪物 sprite **不同**(怪物是單一帶 palette 的 sprite;viewport 是「多子圖塊容器」):
```
[BE word offset 表]  N 個遞增 BE-u16,各指向一個子圖塊。Castle wall(110)= 10 個子圖塊
                     (正面牆 96×96 + 近/中/遠 側牆 trapezoid + 距景小牆面);單純 tile
                     (Road 112 / Water 116)= 1 個子圖塊。
各子圖塊 @off:       word[1]=高度 H、word[2]=每 plane 每列 bytes bpr → 寬 W=bpr*8;
                     影像自 off 起 4-bitplane plane-sequential MSB-first(同 sprite,
                     **惟無自帶 palette** → 共用 viewport 全域盤)。
```
目視確認:110 解出完整城堡牆面(不規則石塊 + 灰泥縫 + 頂部裝飾條 + 透視側牆)。

### palette（2026-06-18 已從 dw 抽出原生盤,取代 stone 近似色）

> **更新**:viewport 原生 16 色盤 **已找到並接上**。先前用的 stone 相容近似色已退役。

- **來源**:`dw` 主程式(75404 B)內 16-word CLUT,出現於 **@0x0fdf4 / @0x0fe34 / @0x10d34**
  (同一份盤多處引用 = 預設世界盤)。格式同 title.pic:16 個 BE word `0x0RGB`,每 nibble ×17 → 8-bit。
- **盤值**(hex 0RGB):`000 fff 640 666 970 070 444 289 090 1cc 860 888 167 2ac 000 000`
  - index0=黑、index1=白(同 title.pic 起手);牆面石塊用 `640`/`970`/`444`/`666`/`888`(棕/灰),
    mortar/距景偏冷 `289`/`1cc`/`2ac`(青藍),頂部裝飾 `070`/`090`(綠)。
- **判別方法**:docker 掃 `dw` 全檔,找連續 16 個 `≤0x0FFF` 的 BE word(0x0RGB 特徵),
  以「index0 暗 + index1 亮」評分。全檔僅此一份符合世界盤特徵的 16 色塊;與 title.pic 的金龍盤
  (`f59 f28 d15…`)明顯不同 → 確認是獨立的 viewport/世界盤,非標題盤。
- **目視對照**:F8 Amiga `--fp --map 1` dump(`dwshot_amiga_native_fp.ppm`)。原生盤 vs 舊 stone
  近似色明顯有別:原生為青藍石塊 + 棕側牆 + 綠頂條(真實 Amiga 觀感),近似色為暖灰石。
  原生盤為遊戲實際資料,採用之(正確性 > 美觀)。
- **逆不出的部分**:`dw` 內只找到這一份世界盤;若不同區域/地城有切換盤,其載入邏輯(哪個 area
  對應哪份 CLUT)未 trace —— 但全檔無第二份候選,推定為單一全域世界盤。

### 工具
- `tools_build/amiga_viewport_extract.py <res.bin> <tag> <out_dir> [--legacy-stone]`:offset 表 →
  逐子圖塊 4-bitplane 解碼 → `<tag>_<blockidx>.spr`(indexed + 內嵌**原生 viewport palette**;
  `NATIVE` 常數即上述 dw CLUT)。傳 `--legacy-stone` 可改回舊 stone 近似色對照。
- 入庫:`assets/bundle/themes/amiga/components/`(15 個 tag,共 60 個子圖塊 .spr;已以原生盤重生)。

### 接進 Amiga 第一人稱（theme-aware viewport;DOS golden 不破）
- `UiTheme` 新增 `component_dir`;Amiga = `themes/amiga/components`,DOS 為空(走原 golden 路徑)。
- 新模組 `render/viewport_amiga.{hpp,cpp}`:**元件選擇/落點沿用 DOS golden
  `compose_draw_sequence`(read-only,不改一字)**,只把「像素來源」換成 Amiga 子圖塊 ——
  以 DOS template header(runlen→寬、numruns→高)做**維度匹配**挑該 tag 尺寸最接近的子圖塊,
  blit 到 DOS DrawCmd 的 (xpos,ypos)。
- `main.cpp` `draw_game_fp`:theme.component_dir 非空且 Amiga 元件可用 → 套**原生 viewport 盤**
  (`AmigaComponentStore::viewport_palette()`)+ `render_first_person_amiga`(疊 DOS 透視框線維持
  景深);**否則回退 DOS `render_first_person`**。
- **F8 切 Amiga + `--fp` → 地牢牆面變 Amiga 石牆美術**(headless `--fp --theme amiga --map 1`
  dump 目視確認:石塊牆面 + 側牆 trapezoid + water tile)。
- **DOS theme 完全不經此模組** → verify_fp / verify_compose / render_sweep golden 全綠(未破)。

### 受阻 / TODO
- **透視非 byte-faithful**:DOS 用精確 perspective decode(填滿天花/地板、側牆完美收斂);
  Amiga 路徑用維度匹配 + DOS 落點近似,牆面為 Amiga 美術且結構可辨,但中央遠景/收斂不如 DOS 精準。
  要完全對位需替 Amiga 子圖塊重寫 DOS perspective placement(本次未做,風險為動到 golden 幾何)。
- ~~**viewport 原生 palette 未抽**(用 stone 相容色)~~ → **2026-06-18 已抽出並接上**(見上 palette 節)。
- Sky(111)/120/128/133 等 tag 非 offset-表結構(0 子圖塊),Amiga 路徑跳過留底色(天空走 DOS
  `fill_sky_flat` 同義的留空)。

---

## 1.7 Amiga data5 / data6（res 257–266）= 音訊資產（已檢視,非美術）

2026-06-18 檢視 data5(res 257/259–265,共 8 個)+ data6(res 266,共 1 個)。**結論:全部為
Amiga 音效 / 音樂取樣(8-bit signed PCM,delta 風格編碼),非視覺美術 → 不入 themes/amiga/。**

### 判別過程(docker,可重現)
- 用 `amiga_res_extract`(連 decompress.cpp)解壓全 9 個資源(同 Huffman codec,解壓成功)。
- 結構:每個資源共用 **16-byte header** `[8×00][01 00 00 00][LE32 payload_size][...]`,接著常數
  `20 06 00 00`(0x620=1568)、`3c 00 00 00`(0x3c=60)—— 像 sample-rate / block 參數。
- body **無 offset 表、無 4-bitplane sprite header、無 0x0RGB palette 起手**(逐項排除美術格式)。
- byte 分佈:集中在 `00/01/ff/fe/fd/02`(signed −3..+3 小幅振盪)+ 單值長 run(`5d`/`ff`/`03`/`22`
  = 靜音 / 持續段);熵 0.26–6.41。
- **決定性驗證**:把 payload 當 8-bit signed PCM 畫波形 → 標準音訊包絡(attack → noisy body →
  decay → 靜音)。`d5_264`/`d6_266` 為密集音效 / 較長持續音(疑音樂 / 環境音),`d5_259`/`d5_265`
  幾近靜音 / 單純音。波形圖證實為 sound。

### 受阻 / 未做
- **Amiga 音訊播放格式未完整逆向**(sample rate、loop point、與 remake 既有音訊系統的接法)——
  本次任務為「美術精修」,音訊接線超出範圍,僅**檢視確認=非美術**並記錄。後續若做 Amiga 原音,
  從此處 `[16B header][PCM body]` + `0x620`/`0x3c` 參數續查。

| 資源 | 解壓後大小 | 熵 | 內容 |
|---|---|---|---|
| data5 res 257 | 42513 B | 1.07 | PCM(大量 `5d` run = 靜音/持續) |
| data5 res 259 | 53764 B | 0.26 | PCM(幾近靜音) |
| data5 res 260 | 26123 B | 1.29 | PCM |
| data5 res 261 | 29449 B | 1.05 | PCM |
| data5 res 262 | 30765 B | 2.91 | PCM |
| data5 res 263 | 48969 B | 2.44 | PCM |
| data5 res 264 |  9757 B | 6.15 | PCM(密集音效;波形含 attack/decay) |
| data5 res 265 | 50437 B | 0.33 | PCM(幾近靜音) |
| data6 res 266 | 16458 B | 6.41 | PCM(較長持續音,疑音樂/環境音) |

---

## 2. X68000 版（Starcraft / Hudson soft 1990)

### 磁碟結構
來源 = 三個 `.DIM`(DiskImage,256B header + 2HD raw)。`.DIM` 去 header → Human68k
FAT12 → 抽檔。流程與工具見 docs/46;FAT12 抽取 `tools_build/fat12_extract.py`。

```
zip → .DIM → dd bs=256 skip=1 → raw → fat12_extract.py → 個別檔
```

| 磁碟 | 美術檔 | 大小 | 內容 |
|---|---|---|---|
| Disk 2 (A) | **PIC.PIX** | 518400 B | NPC/場景 portrait(未壓縮) |
| Disk 3 (B) | **MON.PIX** | 810000 B | 怪物 sprite sheet(未壓縮) |
| Disk 3 (B) | **ICON.PIX** | 16640 B | UI/viewport 圖示(未壓縮) |
| Disk 3 (B) | TITLE.PKH | 53987 B | 標題畫面(**壓縮,受阻**) |
| Disk 3 (B) | 3D1-4.PKH | ~35-50 KB | 3D 地城圖(**壓縮,受阻**) |
| Disk 3 (B) | END1-5.PKH | ~15-53 KB | 結局過場(**壓縮,受阻**) |
| Disk 2 (A) | SUBTTL.PKH | 13423 B | 副標題(**壓縮,受阻**) |

`.PIX` = 未壓縮圖;`.PKH` = 壓縮。

### .PIX 格式(已破解)
- **headerless chunky 4bpp**:每 byte = 2 像素,**high nibble 在前**,index 進 16 色 CLUT。
- geometry(row stride)由 **byte-autocorrelation** 還原(熵 3.2-3.7 bits/byte 確認未壓縮,
  stride 相鄰列 byte 相符率 0.6-0.77):

  | 檔 | stride | 寬度 | 備註 |
  |---|---|---|---|
  | ICON.PIX | 16 B | 32 px | 圖示;sheet 中每格水平加倍(64=32+32) |
  | MON.PIX | 24 B | 48 px | 怪物;垂直 cell 週期 ~112 px,每隻水平加倍(96=48+48) |
  | PIC.PIX | 48 B | 96 px | portrait;垂直週期 ~112 px |

- **palette**:X68000 原生 16 色 CLUT **尚未還原**(需 trace DRAGON.X 的 GVRAM CLUT 載入)。
  目前用 **DOS EGA-16 placeholder**(`tools_build/scene_render.py` 的 P[]):形狀、構圖、
  sprite 邊界完全可辨識,僅色相與原機不同。屬色彩精修 TODO,不影響「抽成可辨識 PNG」目標。

### 產出
```
themes/x68000/monsters/mon_contact_sheet.png   完整怪物 bestiary(骷髏/惡魔/巨魔/蛇/犬/蜘蛛/龍/騎士…)
themes/x68000/scenes/pic_contact_sheet.png     NPC/場景 portrait(王座國王/書房賢者/商人/寶藏…)
themes/x68000/tiles/icon_contact_sheet.png     UI 圖示(盾/劍/箭/寶箱/卷軸/符文/火把)
themes/x68000/tiles/icon_sheet_w32.png         icon 原寬(32px)直條版
```

### 工具(docker dwpil,可重現)
- `tools_build/x68k_pix_extract.py <file.PIX> <width> <out.png>`:單一寬度 chunky 4bpp → PNG。
- `tools_build/x68k_contact_sheet.py <file> <cell_w> <band_h> <cols> <out.png>`:把 tall strip
  切 band 後左右排成接觸表(便於目視)。
- docker image `dwpil`(`python:3.12-slim` + pillow,Dockerfile 在 `.scratch/Dockerfile.pillow`)。

### 受阻:.PKH 壓縮(TITLE / 3D1-4 / END1-5 / SUBTTL)
- 這些檔熵 ~6.0 bits/byte、無顯著 stride 相關 → 真正壓縮。
- 嘗試用 remake 既有的 **DOS Huffman 解壓器**(`src/resource/decompress.cpp`,
  `[size_LE][tree][bitstream]` big-endian 16-bit)套用,off=0/2/4 皆輸出近零熵垃圾
  → **X68000 PKH 與 DOS codec 不相容**。
- 還原需 RE `DRAGON.X`(284KB,X68000 `.X` 執行檔)中的 PKH 解碼常式。本批**未破解**,
  標記受阻。標題畫面因此暫缺 X68000 版(僅 Amiga 有完整標題)。

---

## 3. PC-98 版 — 素材不存在

任務假設「PC-98 日本語版」,但實際查證:

- 在 `NEC_PC_9801_TOSEC_2012_04_23.zip`(2.1GB TOSEC 合集)中,58 個 "Dragon*" 條目
  **無任何 "Dragon Wars"**,亦無 "Interplay"/"Starcraft" 發行的對應磁碟。
- 與 docs/46 的考證一致:Dragon Wars 的日本在地化(Starcraft / Hudson soft 發行)
  **只出在 Sharp X68000**,從未有 PC-98 版。「PC-98」是檔名/任務假設的誤稱,實體即 X68000。

結論:**PC-98 沒有 Dragon Wars 可抽**。日本版美術全部來自 X68000(見 §2)。

---

## 4. 累積/受阻清單

| 項目 | 狀態 |
|---|---|
| Amiga 標題畫面 (title.pic) | ✅ 完整彩色 PNG |
| Amiga 結局 (endgame) | ✅ |
| Amiga 怪物 sprite (data4 4-bitplane) | ✅ **50 隻**(全偶數資源)+ 權威 sprite_res() 對映接進戰鬥(F8;見 §1.5) |
| Amiga viewport 牆面 (data3 4-bitplane) | ✅ 19 tag / 60 子圖塊已切 + 接進 Amiga 第一人稱(F8 `--fp`;見 §1.6) |
| Amiga viewport 透視對位 | ⚠️ 近似(維度匹配;非 DOS byte-faithful perspective) |
| Amiga viewport 原生 palette | ✅ 從 dw CLUT(@0x0fdf4/0x10d34)抽出並接上(取代 stone 近似色;見 §1.6 palette 節) |
| Amiga data5/6(res 257–266) | ✅ 已檢視 = **音訊取樣(8-bit PCM)**,非美術 → 不入庫(見 §1.7) |
| Amiga picparts(過場圖元件) | ⚠️ 部分 |
| X68000 怪物 (MON.PIX) | ✅ contact sheet(EGA placeholder palette) |
| X68000 場景 (PIC.PIX) | ✅ contact sheet |
| X68000 UI 圖示 (ICON.PIX) | ✅ |
| X68000 標題 (TITLE.PKH) | ❌ 受阻(PKH 壓縮未破解) |
| X68000 3D 地城 / 結局 (3D*.PKH / END*.PKH) | ❌ 受阻(PKH 壓縮) |
| X68000 原生 16 色 palette | ⚠️ TODO(需 trace DRAGON.X CLUT 載入) |
| PC-98 全部 | — 素材不存在 |
