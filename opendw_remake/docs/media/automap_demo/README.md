# 俯視平面地圖 (automap) demo

原版手冊 `?` 鍵 = 顯示目前關卡的俯視平面地圖。本目錄為 remake automap 渲染的驗證截圖。

## 演算法 (port 自 opendw)

來源:`opendw/src/lib/engine.c`
- `process_minimap_commands` (3260) — `?` (script op_6D) 進入點
- `draw_minimap` (3204) — 以玩家為中心,9 格 × 8 pass 的捲動視窗
- `draw_minimap_row` (2994) — 每格依 tile record (`word_11C6`) 位元選 sprite
- `plot_minimap_resource` (3150) / `draw_minimap_segment` (2979) / `draw_minimap_from_data6820` (3184)

要點:
- 每格 32 px 寬;`init_offsets(0x90)` → minimap viewport_memory 列 stride = 144 byte (288 px)。
- **fog of war**:tile record 高 byte bit3 (0x08) =「已探索 (seen)」。只有探索過的格畫內容;
  該旗標由遊戲走動時 `refresh_viewport` (engine.c:5688 `data_5521[word_551F+1] |= 0x8`) 累積。
- sprite 來源與第一人稱共用 (`data_59E4` 元件 + `data_56C6`/`data_56E5` 索引表)。
- 用到的資源:`minimap.bin` (com 0x695C,空地磚模板) + `data6820.bin` (com 0x6820,玩家標記)。
- 繪線核心 = 與第一人稱相同的 `decode_viewport_data`,已 byte-for-byte 對拍。

## 對拍 (byte-for-byte)

- golden:`tools_build/minimap_golden/golden_minimap.c` — verbatim port 上述函式,
  輸出 minimap viewport_memory (36864 byte = 0x90 stride × 0x100 列)。
- remake:`src/render/minimap.{hpp,cpp}` 的 `dw::render::Minimap`。
- 驗證:`verify_automap` (ctest `verify_automap_l1`) 對 area 1 (Purgatory) 三個玩家位置
  (10,10)/(16,16)/(8,20) **byte-for-byte PASS** (seed=全圖探索)。

## 遊戲內 fog of war (真實探索揭露)

遊戲內 automap **只顯示走過/看過的格**,隨移動逐步揭露:
- `game::SeenMap` (`src/game/seen_map.{hpp,cpp}`):per-area「已看過」格,玩家每步只標記
  **當前格** (對齊 oracle refresh_viewport,engine.c:5688:每幀只標記玩家站的那一格,
  非整個 FOV 視野)。換 area 各關 seen 獨立。
- `src/main.cpp`:`enter_map` + 每次前進 / 傳送 / 換區後呼叫 `seen.mark(...)`;
  `?` automap 改用 `Minimap::render_with_seen(seen bitmap)` 渲染真實探索進度。
- **存檔**:seen 納入存檔 (savegame v2,追加 `seen_blob`;可讀 v1 舊檔,seen 視為空);
  讀回後探索進度還原。
- `--mm-seed N` 僅供測試/展示 (0=全圖、1=只玩家格、2=不 seed);不給時遊戲用真實 fog of war。

### 確定性驗證

`verify_seen` (ctest `verify_seen_l1`):從固定起點走固定路徑,
- `[accumulate]` seen 集合 == 沿途每格的聯集 (逐格比對)。
- `[per-area]` 在 area 1 走動不影響 area 2。
- `[reveal]` `render_with_seen(累積 seen)` 與等價 bitmap 渲染 byte-for-byte 相同
  (揭露集合 = seen 集合)。
- `[sanity]` 全圖 seen vs 空 seen 渲染不同 (確認 seen 確實控制揭露)。
- `[save round-trip]` seen serialize→deserialize 還原一致。

`verify_save` (v2) 的 round-trip 也涵蓋 `seen_blob` 欄位。

## 截圖

| 檔 | 說明 |
|----|------|
| `golden_1.16_16.png` | golden_minimap.c 在 (16,16) 的 minimap viewport_memory 直接上色 (oracle 參考)。可見紅地磚 + 藍色玩家標記 + 右側灰牆 sprite。 |
| `automap_area1.png` | remake app `--automap 1 --mm-seed 0` (全圖探索) headless 合成畫面 (960×600,含標題/語言/圖例文字層)。 |
| `automap_area1_playerseed.png` | remake app `--automap 1 --mm-seed 1` (只探索玩家起始格)。 |
| `automap_area1_fogofwar.png` | remake app `--automap 1` (**無 --mm-seed → 真實 fog of war**)。剛進關卡只探索起始格 (corner (1,1)),視窗大半未揭露 → 近全黑,對比 explore-all 的滿地磚。 |

> remake 與 golden 的 minimap viewport_memory 已 byte-for-byte 相同 (verify_automap_l1);
> 兩者上色差異僅為 demo 取景 (golden 直接 288×192 上色;app 走完整雙層合成 + 整數放大)。

## 重現

```bash
# golden (docker dwsdl)
gcc -O0 -std=c11 -o golden_minimap tools_build/minimap_golden/golden_minimap.c
./golden_minimap maps/1.lvl 16 16 assets/bundle/viewport assets/bundle/components out.mmem

# remake app headless 截圖 (docker dwsdl, SDL_VIDEODRIVER=dummy)
./opendw_remake --bundle assets/bundle --font assets/fonts/dw8x8.bin \
  --automap 1 --mm-seed 0 --frames 1 --dump docs/media/automap_demo/automap_area1.ppm

# 對拍 (ctest)
ctest -R verify_automap_l1
```

游玩時:在第一人稱地圖中按 `?` 進俯視地圖,`Esc` 關閉回遊戲。
