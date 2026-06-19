# 全螢幕 cutscene 場景圖(英文原版 + 中文版)

DOS 版 Dragon Wars 共 6 張全螢幕 320×200 圖(res 24-29,片頭/結局過場)。
這些圖把文字烤進點陣圖,需先做 `title_adjust` 垂直 delta 還原才能正確解碼(見 `tools_build/scene_render.py`)。

| 資源 | 內容 | 英文原版 | 中文版 |
|------|------|----------|--------|
| 24 | Namtar 被擲回深淵 | `fullscreen_24.png` | `fullscreen_24_zh.png` |
| 25 | Namtar 慘叫墜落瑪根地底世界 | `fullscreen_25.png` | `fullscreen_25_zh.png` |
| 26 | 波卡城焚城 | `fullscreen_26.png` | `fullscreen_26_zh.png` |
| 27 | 和平新時代 | `fullscreen_27.png` | `fullscreen_27_zh.png` |
| 28 | The End / 全劇終 | `fullscreen_28.png` | `fullscreen_28_zh.png` |
| 29 | 標題畫面 / 火龍之戰 | `fullscreen_29.png` | `fullscreen_29_zh.png` |

- **英文版**:`scene_render.py` 解碼原圖,保留作為對照。
- **中文版**:`scene_localize.py` 清掉烤進圖的英文字、以 wqy 渲染中文(譯名依 `../../CONTEXT.md`),藝術保留。
- 為 cutscene 在地化的視覺樣張;最終遊戲內顯示由 remake render 模組產生。
