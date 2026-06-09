# OpenDW Dragon Wars 中文化 - 建置指南

本文檔說明如何在各種平台上建置 OpenDW Dragon Wars 中文化專案。

## 目錄

- [快速開始](#快速開始)
- [系統需求](#系統需求)
- [Linux 建置](#linux-建置)
- [macOS 建置](#macos-建置)
- [Windows 建置](#windows-建置)
- [Docker 建置](#docker-建置)
- [中文字型支援](#中文字型支援)
- [疑難排解](#疑難排解)

## 快速開始

```bash
# 複製專案
git clone https://github.com/wicanr2/opendw_dragon_wars_cht.git
cd opendw_dragon_wars_cht

# 使用 Docker（推薦）
docker-compose up --build

# 或手動建置
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 系統需求

### 必要依賴

| 依賴 | 版本 | 用途 |
|------|------|------|
| CMake | ≥ 3.13.4 | 建置系統 |
| GCC / Clang | C11 支援 | C 編譯器 |
| SDL2 | ≥ 2.0.5 | 顯示/音訊/輸入 |
| pkg-config | - | 尋找程式庫 |

### 可選依賴

| 依賴 | 用途 |
|------|------|
| Check | 單元測試 |
| SDL2_image | 圖片載入 |
| SDL2_mixer | 音訊混合 |
| SDL2_ttf | TrueType 字型渲染 |
| fonts-wqy-zenhei | 中文字型（文泉驛正黑） |
| fonts-wqy-microhei | 中文字型（文泉驛微米黑） |
| fonts-noto-cjk | 中文字型（Noto Sans CJK） |

## Linux 建置

### Ubuntu / Debian

```bash
# 安裝依賴
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-mixer-dev \
    libsdl2-ttf-dev \
    libcheck-dev \
    fonts-wqy-zenhei \
    fonts-wqy-microhei \
    fonts-noto-cjk

# 建置
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# 執行測試
ctest --output-on-failure

# 安裝
sudo make install
```

### Fedora / RHEL / CentOS

```bash
# 安裝依賴
sudo dnf install \
    gcc gcc-c++ \
    cmake \
    ninja-build \
    pkg-config \
    SDL2-devel \
    SDL2_image-devel \
    SDL2_mixer-devel \
    SDL2_ttf-devel \
    check-devel \
    wqy-zenhei-fonts \
    wqy-microhei-fonts \
    noto-cjk-fonts

# 建置
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Arch Linux

```bash
# 安裝依賴
sudo pacman -S \
    base-devel \
    cmake \
    ninja \
    sdl2 \
    sdl2_image \
    sdl2_mixer \
    sdl2_ttf \
    check \
    ttf-wqy-zenhei \
    ttf-wqy-microhei \
    noto-fonts-cjk

# 建置
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## macOS 建置

### 使用 Homebrew

```bash
# 安裝 Homebrew（如果還沒有）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安裝依賴
brew install \
    cmake \
    ninja \
    sdl2 \
    sdl2_image \
    sdl2_mixer \
    sdl2_ttf \
    check \
    font-wqy-zenhei \
    font-wqy-microhei \
    font-noto-sans-cjk

# 建置
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
```

## Windows 建置

### 使用 MSYS2（推薦）

```bash
# 安裝 MSYS2：https://www.msys2.org/

# 在 MSYS2 終端機中
pacman -S \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-SDL2 \
    mingw-w64-x86_64-SDL2_image \
    mingw-w64-x86_64-SDL2_mixer \
    mingw-w64-x86_64-SDL2_ttf \
    mingw-w64-x86_64-check \
    mingw-w64-x86_64-wqy-zenhei \
    mingw-w64-x86_64-wqy-microhei

# 建置
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### 使用 vcpkg

```bash
# 安裝 vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# 安裝依賴
./vcpkg install sdl2 sdl2-image sdl2-mixer sdl2-ttf check

# 建置
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake ..
cmake --build .
```

## Docker 建置

### 使用 Dockerfile

```bash
# 建置映像
docker build -t opendw:latest .

# 執行（需要 X11 轉發）
docker run -it --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v $(pwd)/data:/app/data \
    opendw:latest

# 或使用 VNC（不需要 X11）
docker run -it --rm \
    -p 5900:5900 \
    -v $(pwd)/data:/app/data \
    opendw:latest
```

### 使用 docker-compose

```bash
# 建置
docker-compose build

# 開發 shell
docker-compose run shell

# 執行測試
docker-compose run test

# 執行遊戲（需要 X11）
docker-compose --profile game up game
```

### Docker 多階段建置

```bash
# 僅建置（不執行）
docker build --target builder -t opendw:builder .

# 僅執行階段（較小映像）
docker build --target runtime -t opendw:runtime .

# 比較映像大小
docker images opendw
```

## 中文字型支援

### 字型優先順序

建置系統會自動偵測以下中文字型（按優先順序）：

1. **wqy-zenhei.ttc** - 文泉驛正黑（推薦）
2. **wqy-microhei.ttc** - 文泉驛公尺黑
3. **NotoSansCJK-Regular.ttc** - Noto Sans CJK
4. **DroidSansFallbackFull.ttf** - Droid Sans Fallback

### 手動安裝字型

如果系統沒有中文字型，可以手動下載：

```bash
# 文泉驛正黑
wget -O data/fonts/wqy-zenhei.ttc \
    "https://github.com/googlefonts/noto-cjk/raw/main/Sans/OTF/SimplifiedChinese/NotoSansSC-Regular.otf"

# 或複製系統字型
cp /usr/share/fonts/truetype/wqy/wqy-zenhei.ttc data/fonts/
```

### 驗證字型安裝

```bash
# 列出系統中文字型
fc-list :lang=zh | head -20

# 驗證特定字型
fc-list | grep -i "wqy\|noto.*cjk\|droid.*fallback"
```

## 疑難排解

### 常見問題

#### 1. SDL2 找不到

```
Could NOT find SDL2 (missing: SDL2_LIBRARY SDL2_INCLUDE_DIR)
```

**解決方案：**
```bash
# Ubuntu/Debian
sudo apt-get install libsdl2-dev

# 或指定 SDL2 路徑
cmake -DSDL2_DIR=/path/to/sdl2 ..
```

#### 2. 中文字型找不到

```
WARNING: No CJK font found - Chinese display will not work
```

**解決方案：**
```bash
# 安裝中文字型
sudo apt-get install fonts-wqy-zenhei

# 或手動放置字型
mkdir -p data/fonts
cp /path/to/font.ttc data/fonts/
```

#### 3. Check 測試庫找不到

```
Could NOT find Check
```

**解決方案：**
```bash
# Ubuntu/Debian
sudo apt-get install check

# Fedora
sudo dnf install check-devel

# macOS
brew install check
```

#### 4. 編譯錯誤：`-Werror` 導致

```
error: implicit declaration of function 'xxx'
```

**解決方案：**
```bash
# 暫時停用 -Werror
cmake -DCMAKE_C_FLAGS="-Wno-error" ..
```

#### 5. 執行時找不到字型

```
Failed to load font: ...
```

**解決方案：**
```bash
# 確認字型存在
ls -la build/fonts/

# 或設定環境變數
export OPENDW_FONT_PATH=/path/to/font.ttc
```

#### 6. 遊戲資料找不到

```
Error: Cannot open DATA1
```

**解決方案：**
```bash
# 將遊戲資料複製到 data/ 目錄
cp DRAGON.COM DATA1 DATA2 data/

# 或指定資料目錄
./sdldragon --data-dir /path/to/game/data
```

### 建置選項

| 選項 | 說明 | 預設值 |
|------|------|--------|
| `CMAKE_BUILD_TYPE` | 建置類型 | `Release` |
| `CMAKE_INSTALL_PREFIX` | 安裝路徑 | `/usr/local` |
| `SDL2_DIR` | SDL2 路徑 | 自動偵測 |
| `ENABLE_TESTS` | 啟用測試 | `ON` |

### 取得協助

如果遇到其他問題：

1. 查看 [GitHub Issues](https://github.com/wicanr2/opendw_dragon_wars_cht/issues)
2. 查看 [SDL2 文件](https://wiki.libsdl.org/)
3. 查看 [CMake 文件](https://cmake.org/documentation/)
