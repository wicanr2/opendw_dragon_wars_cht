# viewport 模板資產(第一人稱視圖渲染的資料層)

從 DRAGON.COM 抽出的 viewport run-length 模板(對拍 opendw `ui_load`,ui.c:785)。
這是讓「進入遊戲後畫面與原版一致」的資料層;描線核心(`decode_viewport_data` +
`process_quadrant` + 4× `draw_viewport_*`)的 port 計畫見 [`../../../docs/engine/VIEWPORT.md`](../../../../docs/engine/VIEWPORT.md)。

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
- ✅ 描線核心 port(`decode_viewport_data` 等)完成,第一人稱主視圖 byte-for-byte 對拍。
- ✅ **minimap(`?` 俯視平面地圖)**已 port + byte-for-byte 對拍:
  - `minimap.bin`(com 0x695C)= minimap 每格空地磚模板,`draw_minimap_segment` 用。
  - `data6820.bin`(com 0x6820)= 玩家位置標記,`draw_minimap_from_data6820` 用。
  - 實作 `src/render/minimap.{hpp,cpp}`;golden `tools_build/minimap_golden/`;
    ctest `verify_automap_l1`(area 1 三點 PASS);截圖 `docs/media/automap_demo/`。
- ✅ **UI 框件(com 0x6AE0,`ui_pieces`)已抽出 + chrome 升級為真值**:
  - `ui_pieces.bin`(magic `DWUIP`)= 43 片 chrome 框件,**byte-for-byte 同 DRAGON.COM
    com 0x6AE0**(對拍 opendw `ui_load` ui.c:785;extract 校驗第一筆偏移 == 表尾 0x6B36)。
    含:石磚邊框各段(pieces 0/3 底部橫條 + 頂端分隔、1/2/4/6 側欄)、Dragon Wars
    **原版立繪 logo**(piece 5,48×32 @ x=216 y=0)、右側 pillar(piece 9,20×144 @ x=176)、
    頂端石磚磚塊(pieces 0x17..,4×8 tiling)、右側面板狀態條框件(pieces 10–26)。
  - 萃取工具 `tools/extract/extract_ui_pieces`;渲染 `src/render/ui_pieces.{hpp,cpp}`
    (`UiPieces::draw_chrome`,忠實 port `draw_ui_piece`@546 / `ui_draw`@744 / `ui_header_draw`@762)。
  - golden `golden/ui_pieces.chrome.ppm`;ctest `verify_ui_pieces_golden`(320×200 byte-for-byte
    對拍獨立 oracle `tools_build/ui_pieces_golden/golden_ui_pieces.c`)。對照圖 `docs/ui_chrome_demo/`。
  - **版本注意**:本 bundle 全部用 **DRAGON.COM v1.1(56,673 bytes,md5 3aa427d4…)**,
    非 opendw doc 標的 v1.0(55,217 bytes)。v1.0 在 com 0x6758 / 0x6AE0 版面不同
    (0x6AE0 已是像素而非偏移表);既有 `vp0.bin` 等與 ui_pieces 同源 v1.1(已對拍確認)。
  - 格式:`magic "DWUIP\0"`(6)+ `version u16=1` + `count u16=43`,其後每片
    `w u8, h u8, offset_delta u8, y_pos u8, data_len u16, data[data_len]`(nibble bitmap,
    byte=2px hi/lo;x 起點 = offset_delta*4,y = y_pos)。逐片 com offset 見 `ui_pieces.manifest.json`。
