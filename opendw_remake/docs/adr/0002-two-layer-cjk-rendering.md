# ADR 0002 — 雙層渲染:像素層整數放大 + 高解析 TTF 文字層(內外解析度解耦)

- 狀態:Accepted
- 日期:2026-06-14
- 取代:無(沿用 ADR 0001 的「資產與引擎分離」原則)
- 相關:`/home/anr2/.claude/skills/retro-game-remake/references/04-engine-localization.md`
  「CJK 渲染:雙層 + 內外解析度解耦」

## 背景

原本所有文字(UI/選單/事件/段落/標題)都畫進 320×200 indexed framebuffer
再整體放大顯示。中文字以 24×24 點陣 atlas 塞進 framebuffer,訊息區因空間不足
改用 `CjkFont::draw_half()`(24×24 → 12×12 OR 降採樣),經 SDL 視窗放大後**糊**、
筆畫黏連,可讀性差。

問題本質:**像素層的解析度(320×200)同時決定了文字的解析度**。像素遊戲畫面要維持
低解析(對齊原版),文字卻需要高解析才銳利——兩者耦合在同一個 framebuffer 上,無法兼顧。

## 決策

把渲染拆成兩層,內外解析度解耦:

### 像素層(pixel layer)
- 維持 320×200 indexed `Framebuffer`:viewport 牆面/sprite/全螢幕場景圖/地圖 tile。
- 以**整數倍 nearest 放大**到視窗(預設 3× = 960×600)。
- **像素正確性不變**:這層內容與原版逐像素對拍的結果完全不受文字層影響
  (verify_fp / verify_fov / verify_compose / verify_viewport / sweep 續綠)。

### 文字層(text layer)
- UI/選單/事件/段落/標題等 CJK + 在地化文字,改用 **SDL2_ttf + host TTF(wqy-zenhei)**
  在**視窗高解析**原生繪製,疊在像素層之上,**永不被縮放** → 恆銳利。
- 字型:`/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc`(dwsdl image 內建),
  可 `--font-ttf PATH` 覆寫(為日後日文 / Noto Sans CJK 留路)。多語仍走 i18n `tr()`。

### 座標與字級
- 文字層 API(`render::TextLayer`)收「文字繪製指令」:
  `add(virtual_x, virtual_y, utf8, palette_color_index, native_px)`。
- **座標**用 320×200 **虛擬座標**(與像素層同座標系);flush 時 `virtual × scale` 定位到視窗。
- **字級**為**視窗原生像素**(不隨 scale 變),確保字實際大小固定、銳利。
  基準(scale=3):標題 48px、CJK 內文 24px、ASCII UI 16px;`px = base × scale / 3` 等比。
- 顏色沿用 DOS 16 色 palette index(與像素層一致),TTF 以 `TTF_RenderUTF8_Blended`
  抗鋸齒、alpha 邊緣繪製。

### 解析度解耦
- `--scale N`(預設 3 → 視窗 960×600);內部像素層恆 320×200。
- glyph 快取:key = `native_px | color | hash(text)`,texture 隨 renderer 生命週期持有,
  避免每幀重建同一串文字。

### 合成與 headless dump
- `SdlVideo::present(fb)`:先畫像素層(framebuffer → 320×200 texture → nearest 整數放大),
  再 `TextLayer::flush()` 疊文字層,最後 `SDL_RenderPresent`。
- `--dump`:輸出**合成後高解析畫面**(例 960×600 PPM)。headless 路徑用
  `SDL_VIDEODRIVER=dummy` + software renderer + `SDL_RenderReadPixels`,不需 X11。
  PPM → PNG 用 dwimg(ImageMagick `convert`)。

## 後果

正面:
- 中文在任何視窗倍率下都銳利(對比舊 `draw_half` 12×12 糊字)。
- 像素遊戲內容與文字內容各自最佳化,互不犧牲。
- 多語切換、字型替換只動文字層,不碰像素管線。

代價 / 限制:
- `opendw_remake` target 新增 SDL2_ttf 相依(CMake `pkg_check_modules(SDL2_ttf)`,
  僅此 target;其他 verify_* / extract_* 不連 SDL)。
- 文字層位置以虛擬座標規劃,排版邏輯(換行、行高)需用 TTF 度量(`measure_vwidth` /
  `wrap`),而非固定字寬。
- 舊 `CjkFont`(24×24 atlas / draw_half)在 app 主路徑停用;cjk24.atlas 不再為渲染所需
  (保留模組與既有 render_cjk_demo 等驗證工具,不影響)。

## 驗證

- 重生展示(960×600 PNG,headless 合成):
  - `docs/screenshots/r9_menu_twolayer.png`(在地化選單 + 大標題)
  - `docs/screenshots/r9_fp_event_twolayer.png`(`--map 1 --fp` 第一人稱 + 事件文字)
  - `docs/screenshots/r9_paragraph146_twolayer.png`(`--map 31 --fp --at 6 5` 段落 146 內嵌全文)
  → 中文銳利、抗鋸齒、24px、排版落在畫面內;像素 viewport 仍為 nearest 放大的銳利方塊。
- 像素層回歸(docker dwsdl):vm_selftest、verify_viewport 3/3、verify_fov/compose/fp(area1)4/4、
  全 40 關 sweep 子集 **154/154 byte-for-byte PASS**。
- 自包含:像素資產來自 bundle;文字字型用 host TTF(image 內建),不依賴 DATA1。
