# OpenDW Dragon Wars 中文化專案

OpenDW 是 Interplay 1989/1990 年遊戲 **Dragon Wars** 的開源重製版。
本專案旨在將 OpenDW 中文化（繁體中文），並整合 SDL2 顯示層。

## 專案結構

```
opendw_dragon_wars_cht/
├── docs/                    # 文件
│   ├── PLAN.md              # 中文化規劃
│   ├── ANALYSIS.md          # 反組譯還原分析
│   ├── dragon.asm           # 原始 DOS 反組譯（參考）
│   └── README.md            # 本說明
├── src/                     # OpenDW 原始碼
│   ├── fe/                  # 前端（SDL2 輸出層）
│   │   ├── main.c           # 程式進入點
│   │   ├── vga_sdl.c        # SDL2 顯示驅動
│   │   ├── vga_dos.c        # DOS VGA 驅動
│   │   └── vga_null.c       # 空驅動
│   ├── lib/                 # 核心引擎
│   │   ├── engine.c         # 虛擬 CPU + 115 個 opcode
│   │   ├── ui.c             # UI 繪製
│   │   ├── resource.c       # 資源載入
│   │   ├── tables.c         # 字型表
│   │   ├── state.c          # 遊戲狀態
│   │   ├── player.c         # 角色資料
│   │   └── ...
│   ├── tools/               # 輔助工具
│   └── tests/               # 單元測試
├── CMakeLists.txt           # CMake 建置
└── Makefile                 # Make 建置
```

## 原始 OpenDW 資訊

Original game engine by [Rebecca Ann Heineman](https://www.burgerbecky.com/).

This game can be purchased at [GOG](https://www.gog.com/game/dragon_wars).

## 快速開始

### 建置

```bash
mkdir build && cd build
cmake ..
make
```

### 執行

需要原始遊戲檔案（dragon.com, data1, data2）：

```bash
./src/fe/sdldragon
```

## 中文化狀態

- [ ] 640×480 + 24×24 CJK 顯示
- [ ] 外部字型載入
- [ ] Big5 編碼支援
- [ ] 字串萃取工具
- [ ] UI 佈局調整

## 授權

OpenDW 原始碼採用 BSD 授權。
Dragon Wars 是 Interplay 的商標，原始遊戲檔案僅供個人使用。

## 貢獻者

- Chun-Yu Wang
