# 媒體素材：DOS 原版 vs Remake 動態比對

本目錄收錄《火龍之戰》(Dragon Wars, Interplay 1989/90) 原版 DOS 與 `opendw_remake` 的遊玩動態比對 GIF。三個檔案同一段遊玩流程,涵蓋:標題 → 主選單 / 隊伍 → 開場敘事 → 第一人稱走動 → 平面地圖 (automap) → 戰鬥遭遇。

## 檔案

| 檔案 | 內容 | 尺寸 / 幀率 |
|---|---|---|
| `gameplay_dos_vs_remake.gif` | 左 DOS 1990 / 右 Remake 並排對照,加標題列。10 個遊玩節拍 | 968×334,約 2.5 fps |
| `gameplay_dos.gif` | DOS 原版單側遊玩序列 | 960×600 (320×200 放大 3×),約 2.5 fps |
| `gameplay_remake.gif` | Remake 單側遊玩序列 | 960×600 (320×200 放大 3×),約 2.5 fps |

## 遊玩節拍對應

| # | 節拍 | DOS | Remake |
|---|---|---|---|
| 1 | 標題 / 開場 | Dragon Wars 龍圖開場 (Interplay 89-90) | 同一張龍圖開場 splash + 金色「火龍之戰」疊字 + 「按任意鍵繼續」(原版英文 logo 刻意保留) |
| 2 | 主選單 / 隊伍 | Current party… + Begin the game | 目前隊伍 (1 Muskels…4 Cheetah) + B 開始 / C 繼續 |
| 3 | 開場敘事 | Stripped of all possessions… (Press ESC) | 對應事件訊息檢視器 (繁中) |
| 4–8 | 第一人稱走動 | Purgatory viewport,牆面 / 結構透視隨步伐變化 | 同一 Purgatory 走廊的 FP viewport (對 oracle byte-for-byte) |
| 9 | 平面地圖 | `?` automap (ESC to exit) | `--automap` 全圖揭露 (地圖 · Esc:返回) |
| 10 | 戰鬥遭遇 | Innocent Man encounter (Fight/Run/Advance…) | 怪物圖 + 隊伍面板 + 繁中戰鬥指令列 |

DOS 與 Remake 兩側位置 / 朝向不完全逐格相同,主要呈現相同遊玩流程下兩者的版面與渲染對照;畫面差異多為刻意的繁中在地化或現代化輔助 (底部操作提示列、新 / 續遊戲選單),逐畫面保真分析見 `../../docs/assessment/60_DOS_VS_REMAKE_VISUAL.md`。

## 錄製方法

全程於 docker 容器執行 (`dwdos` / `dwsdl` / `dwimg`),未污染系統環境。

DOS 側 (真擷取):
1. `7z` 從 `disk1.ima` (3.5"/1.1) 解出 `DRAGON.COM` / `DATA1` / `DATA2` (原始遊戲檔不入庫)。
2. `dwdos` 容器內 `Xvfb :99` 起虛擬顯示;DOSBox `output=surface scaler=none aspect=false`,autoexec 掛載 + 執行 `DRAGON.COM`。
3. `xdotool` 注入鍵序:config `E` (VGA/MCGA) → 標題 → `B` 開始 → `Esc`×N 清開場敘事 → `I`/`J`/`L` 移動 → `Shift`+`/` 開 automap。每步以 `import -window root -crop 320x200+0+0` 抓 native surface。
4. 戰鬥遭遇格採用既有稽核驗證過的真擷取畫面 (`docs/assessment/dos_compare/dos/06_combat_encounter.png`,Innocent Man),縮回 320×200 接入序列 (Purgatory 起點區隨機遭遇稀疏,盲走腳本無法穩定觸發,故沿用該真擷取幀)。

Remake 側 (headless):
- `SDL_VIDEODRIVER=dummy ./build/opendw_remake <flags> --frames 0 --dump x.ppm --scale 1`,各節拍對應旗標:標題開場 splash (`--title --locale zh-TW`,顯示龍圖 + 火龍之戰疊字)、`--char-sheet`、`--map 1 --fp --at X Y` (走動)、`--automap 1 --mm-seed 0`、`--encounter 3 --combat-rounds 1`。旗標說明見 `../../docs/engine/CONTROLS.md`。

GIF 組裝:`convert` (ImageMagick) 串幀 + 控制 per-frame delay + 放大 3×;並排版以 `+append` 合左右並加 24px 標題列。

## 來源標註

DOS 畫面為《火龍之戰》(Dragon Wars) 原版擷取,版權 Interplay Productions 1989/90,於此僅作技術比對用途。原始遊戲檔 (`DRAGON.COM` / `DATA1` / `DATA2`) 未入庫。
