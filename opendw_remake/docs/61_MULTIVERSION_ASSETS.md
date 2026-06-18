# 61. 多版本美術素材萃取（Amiga / X68000 / PC-98）

為 remake 後續「遊戲中切換 theme」準備,從《Dragon Wars》三個原版磁碟抽出美術素材
(標題畫面、怪物 sprite、場景圖、UI 圖示),轉成 PNG。DOS 版美術已在既有 bundle
(預設 theme),本文件處理另外三個平台。

原始遊戲檔(.dim / .PIX / .PKH / .adf / DRAGON.X / data*)一律**不入庫**,只放抽出
並轉成可用格式的 PNG + 抽取工具 + 本文件。全程 docker,不污染系統。

## 0. 三版總結

| 版本 | 磁碟形式 | 標題畫面 | 怪物 sprite | 場景/過場 | UI 圖示 | 狀態 |
|---|---|---|---|---|---|---|
| **Amiga** | .adf + WHDLoad HD(`data/`) | ✅ title.pic | (待查) | ✅ endgame、picparts(部分) | ✅ cursors | **大部分成功** |
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
| Amiga picparts | ⚠️ 部分 |
| X68000 怪物 (MON.PIX) | ✅ contact sheet(EGA placeholder palette) |
| X68000 場景 (PIC.PIX) | ✅ contact sheet |
| X68000 UI 圖示 (ICON.PIX) | ✅ |
| X68000 標題 (TITLE.PKH) | ❌ 受阻(PKH 壓縮未破解) |
| X68000 3D 地城 / 結局 (3D*.PKH / END*.PKH) | ❌ 受阻(PKH 壓縮) |
| X68000 原生 16 色 palette | ⚠️ TODO(需 trace DRAGON.X CLUT 載入) |
| PC-98 全部 | — 素材不存在 |
