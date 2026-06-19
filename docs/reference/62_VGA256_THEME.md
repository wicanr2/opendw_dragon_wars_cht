# 62. VGA-256 主題（「真 VGA」256 色增強）

F8 循環的第 4 個主題:`DOS → Amiga → X68000 → VGA → DOS`。

VGA-256 是 **remake 加值**——原版《Dragon Wars》沒有這個版本。它以 DOS EGA 16 色畫面
為來源,在顯示前用**演算法**(漸層 + 邊緣壓暗)把 flat 色塊擴成 256 色更精緻的版本。

## 誠實標示(這是什麼、不是什麼)

- **是**:演算法增強(algorithmic enhancement)——色深擴展(16 → 256 色)、垂直漸層
  (光照/景深立體感)、邊緣柔化(描邊立體感)。確定性、無手繪。
- **不是**:逐像素手繪重畫的 tile、原版任何平台的真實 VGA 美術。
- `partial = false`:這個「256 色增強 pass」本身是完整、確定性的演算法,不是未完成的移植;
  但其性質為「增強」而非「重繪」,如上標示。

## 為什麼用演算法增強,而非離線重畫 tile

本引擎的世界畫面是**程式化即時繪製**,不是讀預烤 tile bitmap:

- 第一人稱 viewport 牆面/地面:`viewport_compose.cpp` 逐 4-bit nibble blit 元件 → 320×200
  indexed framebuffer(每像素 0–15)。
- 俯視世界地圖地形:`worldmap.cpp` 以 `fb.put(x, y, color)`(color 為 0–15 常數)程式化
  畫地形/海洋/地點 tile。

兩者最終都產生一張 **16 色 indexed framebuffer**。因此 256 色增強最合身的作法是在這張
framebuffer 上做 **post-process pass**,而非離線重畫不存在的 tile bitmap。這也讓增強自動
涵蓋所有畫面(viewport、worldmap、UI 純色塊),不需逐畫面接線。

## 渲染管線怎麼打通到 256 色

像素層 framebuffer 維持 320×200 **16 色** indexed(`framebuffer.hpp` 不動,golden 對拍根基)。
256 色只發生在「framebuffer → RGB」這一步:

```
16 色 framebuffer ──(VGA theme)──> enhance_to_256() ──> 256 色索引 buffer ──> vga_palette()[i] ──> RGB
                  ──(其他 theme)──> to_rgb(fb, 16色盤) ──────────────────────────────────────> RGB
```

- `render/vga256.{hpp,cpp}`(deep module):
  - `vga_palette()`:256 色盤 = 16 DOS 基色 × 16 階 ramp。`[base*16 + s]`,`s=8 ≈ 原 DOS 色`,
    `s<8` 漸暗、`s>8` 漸亮。**明度走非線性曲線**(感知更自然):暗階用 gamma 1.45(陰影集中於
    末端、中段壓得少 → 陰影柔不死黑)、亮階用 gamma 0.80(高光前段抬升明顯、末端收斂 → 立體
    高光不過曝);暗端保留 18% 原色、亮端最亮 80% 白,維持色相不顯髒。
  - `enhance_to_256(fb)`:讀 16 色 framebuffer,逐像素以浮點累積各項光照貢獻、最後才量化
    (過渡平滑、無提前跳階)輸出 256 色索引——
    - **自適應垂直漸層**:量測「同色直向連續區段」高度,區段 ≥4 像素時以 smoothstep 由上(亮)
      到下(暗)鋪漸層 → 立體浮雕。幅度隨「背景平面程度」`bg_flat`(由 `min(run,hrun)` 判定)
      衰減:物體尺寸(單塊牆磚/地形 tile)滿幅、大片背景平面(海面/天空/UI 底)趨平 → 避免整片
      背景被鋪強漸層而出現水平條紋。小細節(run<4)維持原色,避免雜訊。
    - **水平微梯度**:物體尺寸橫段左略亮、右略暗,與垂直漸層合成柔和方向光 → 牆磚/地形塊更有
      體積感。
    - **智慧描邊**:只在與「更亮鄰色」交界處壓暗(模擬受光方向的陰影側),幅度依明暗差調節;
      與更暗鄰色不壓(免雙重黑線)。**同色系交界免描邊**(色相方向接近、僅明度差,如深藍↔亮藍
      的海波、同材質明暗紋理)→ 不把材質內部明暗交替放大成硬條紋。
    - **ordered-dither(Bayer 4×4)**:量化前加入空間抖動打散漸層的水平跳階線,使背景過渡呈點狀
      交錯而非整條橫線(抗鋸齒概念);幅度隨 `bg_flat` 收斂、描邊像素不抖(保描邊乾淨)。
    - 黑色(base 0)維持純黑不漸層(避免邊框/letterbox 變灰)。
- `render/sdl_video`:`set_vga256(bool)` 開關 256 色路徑;`compose()` 依旗標選 16 色或 256 色
  轉換。`set_palette()` 會同時關閉 256(切回 16 色路徑),確保 DOS/Amiga/X68000 永遠走原本
  16 色 `to_rgb`。
- `render/ui_theme`:`UiTheme.vga256` 旗標;`theme_list()[3]` 為 vga 主題(16 色 base = DOS 盤,
  title 回退 DOS dragon art,結局序列回退 DOS,`vga256 = true`)。
- `main.cpp`:`render_now()` 每幀最後依當前 `theme.vga256` 設 `vid.set_vga256(...)`;F8 toast 顯示
  「主題: vga (256色)」(i18n key `256 colours`)。

## DOS 16 色回歸保證(golden 沒破)

- framebuffer 仍是 16 色 indexed,`to_rgb(fb)`(不帶 palette)維持 DOS 盤——既有 golden 對拍
  與所有 16 色呼叫端不變。
- DOS/Amiga/X68000 三主題 `vga256 = false`,完全不經 enhance pass。
- `kDosPalette` 常數未被汙染(256 色為獨立盤,不覆寫)。
- ctest 全綠:34 tests(原 33 + 新增 `verify_vga256`);`verify_theme` 更新為 4 主題 + VGA 旗標。

## 效果(dump 對照 DOS)

| 畫面 | 16 色(DOS) | 256 色(VGA)增強 |
|---|---|---|
| 第一人稱 viewport(`--fp`) | flat 灰磚牆 / cyan tile / 紅地 / 綠柱 | 每塊磚有上高光下陰影 + 左亮右暗方向光 + 柔描邊 → 石材立體浮雕;天空平整(無條紋)、雲柔邊;綠柱/藍框立體感 |
| 世界地圖(`--automap 0`) | flat 綠陸 / 藍海(資料層含海波紋理) | 陸塊內亮外暗 + 海岸暗邊;海面同色系免描邊 → 不放大海波條紋;地點標記倒角 |
| 室內小地圖(`--automap 1`) | 小色塊 | 增強仍套用,但大片留白 → 漲幅小(viewport 最有感) |

distinct 顏色數(像素層+文字層):viewport 661→812、世界地圖 1917→2843。

> 海面的深淺水平紋理來自 `worldmap.cpp` 資料層的「海波」設計(深藍/亮藍隔列交替,DOS 16 色版即有),
> 非本 256 增強產生;增強已透過「同色系免描邊」避免將其放大成硬條紋。

最有感:viewport 牆面與世界地圖地形(大片同色區);稀疏小地圖漲幅最小(設計使然,
小細節刻意不漸層以避雜訊)。

對照截圖(`docs/media/showcase/vga256/`):

| DOS 16 色 | VGA 256 色增強 |
|---|---|
| `01_viewport_dos.png` | `02_viewport_vga256.png` |
| `03_world_dos.png` | `04_world_vga256.png` |
| F8 toast「主題: vga (256色)」:`05_f8_toast_vga.png` | |

## 哪些是 16 色回退

- title splash:VGA 主題回退 DOS dragon art(無原生 VGA 標題),經 256 色增強呈現。
- 結局過場:回退 DOS res 24..28 序列(`theme_ending_scenes`),同樣經增強。
- 怪物 sprite:沿用 DOS bundle/sprites(VGA 主題未另切 sprite),戰鬥畫面整體經 256 色增強。

## 重現

```bash
# DOS vs VGA 第一人稱 viewport
./opendw_remake --bundle assets/bundle --fp --no-splash --map 1 --at 5 5 --frames 2 --dump dos.ppm
./opendw_remake --bundle assets/bundle --fp --no-splash --map 1 --at 5 5 --theme vga --frames 2 --dump vga.ppm
# F8 循環到 VGA(toast「主題: vga (256色)」)
./opendw_remake --bundle assets/bundle --fp --no-splash --map 1 --at 5 5 --keys F8,F8,F8 --frames 4 --dump-frame 3 --dump cycle.ppm
```
