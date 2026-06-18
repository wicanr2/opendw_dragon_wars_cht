# themes/ — 多版本美術素材

為「遊戲中切換 theme」準備,從各平台原版抽出的美術。DOS 版美術 = 既有 bundle 預設 theme,
不放這裡。抽取方法、格式、受阻清單見 `docs/reference/61_MULTIVERSION_ASSETS.md`。

| theme | 來源平台 | 內容 | palette |
|---|---|---|---|
| `amiga/` | Amiga (Interplay 1990) | title.png(完整標題)、scenes/endgame.png | 真實 12-bit Amiga palette |
| `x68000/` | Sharp X68000 (Starcraft/Hudson) | monsters/、scenes/、tiles/ 的 .PIX contact sheet | **DOS EGA-16 placeholder**(形狀正確,色相待還原) |

注意:
- `x68000/` 的 palette 目前是 placeholder。X68000 原生 16 色 CLUT 尚未從 DRAGON.X 還原,
  形狀/構圖/sprite 邊界完全可辨識,但色相與原機不同。屬色彩精修 TODO。
- 受阻項:X68000 標題(TITLE.PKH)與 3D/結局(3D*/END*.PKH)為壓縮未破解;Amiga picparts 部分。
- 原始遊戲檔不入庫,只放 PNG。
