# OpenDW Dragon Wars 中文化專案 - 建置容器
# 基於 Ubuntu 22.04 LTS，支援 SDL2 與中文字型渲染

FROM ubuntu:22.04 AS base

# 避免互動式安裝卡住
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Taipei

# 安裝編譯工具鏈與 SDL2 依賴
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        git \
        wget \
        ca-certificates \
        # SDL2 核心
        libsdl2-dev \
        libsdl2-image-dev \
        libsdl2-mixer-dev \
        libsdl2-ttf-dev \
        # 中文字型支援
        fonts-wqy-zenhei \
        fonts-wqy-microhei \
        fonts-noto-cjk \
        # 其他依賴
        libcheck-dev \
        libpthread-stubs0-dev \
    && rm -rf /var/lib/apt/lists/*

# 設定工作目錄
WORKDIR /app

# 複製專案原始碼
COPY . /app/

# 建置階段
FROM base AS builder

# 建立建置目錄
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          .. && \
    cmake --build . --parallel $(nproc) && \
    cmake --install .

# 執行階段（較輕量）
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Taipei

# 執行階段只需要 SDL2 runtime 與中文字型
RUN apt-get update && apt-get install -y --no-install-recommends \
        libsdl2-2.0-0 \
        libsdl2-image-2.0-0 \
        libsdl2-mixer-2.0-0 \
        libsdl2-ttf-2.0-0 \
        fonts-wqy-zenhei \
        fonts-wqy-microhei \
        fonts-noto-cjk \
    && rm -rf /var/lib/apt/lists/*

# 從建置階段複製執行檔
COPY --from=builder /usr/local/bin/sdldragon /usr/local/bin/sdldragon
COPY --from=builder /usr/local/bin/ndragon /usr/local/bin/ndragon

# 建立遊戲資料目錄
RUN mkdir -p /app/data
VOLUME ["/app/data"]

# 設定環境數定
ENV SDL_VIDEODRIVER=x11
ENV SDL_AUDIODRIVER=pulse

# 預設工作目錄
WORKDIR /app

# 啟動遊戲（需要掛載遊戲資料）
CMD ["sdldragon"]
