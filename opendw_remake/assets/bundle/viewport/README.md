# viewport 模板資產(第一人稱視圖渲染的資料層)

從 DRAGON.COM 抽出的 viewport run-length 模板(對拍 opendw `ui_load`,ui.c:785)。
這是讓「進入遊戲後畫面與原版一致」的資料層;描線核心(`decode_viewport_data` +
`process_quadrant` + 4× `draw_viewport_*`)的 port 計畫見 [`../../docs/VIEWPORT.md`](../../docs/VIEWPORT.md)。

| 檔案 | DRAGON.COM 位址 | 大小 | 用途(opendw) |
|------|----------------|------|---------------|
| `vp0.bin` | com 0x6758 | 44 (4+4×0x0A) | `viewports[0].data` 主視圖象限 0 |
| `vp1.bin` | com 0x6784 | 44 | `viewports[1].data` 象限 1 |
| `vp2.bin` | com 0x67B0 | 56 (4+4×0x0D) | `viewports[2].data` 象限 2 |
| `vp3.bin` | com 0x67E8 | 56 | `viewports[3].data` 象限 3 |
| `data6820.bin` | com 0x6820 | 316 (4+0x0D×0x18) | `data_6820`,minimap viewport run-length |
| `minimap.bin` | com 0x695C | 388 (4+0x10×0x18) | `minimap_viewport` |

> COM 檔 offset = com 位址 − 0x100(COM_ORG_START)。
> 格式:前 2 byte 多為 `runlength, numruns`(見 `decode_viewport_data`),其後為
> run-length 描線資料(nibble 圖樣,如 data6820 開頭 `0D 18 FE F8 66 66…`)。

## 狀態
- ✅ 資料層抽出(本目錄,自包含)。
- ⏳ 描線核心 port(`decode_viewport_data` 等)→ 先以 minimap(`?` 平面地圖)為首個可
  golden 對拍的 in-game 畫面目標,再推進第一人稱主視圖。UI 框件(com 0x6AE0,`ui_pieces`)
  另行抽取。
