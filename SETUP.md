# SETUP — 在另一台電腦重建本專案開發環境

火龍之戰 Dragon Wars 繁中重製(opendw_remake)。本檔說明從 dev-setup bundle 解開後如何重建
build/打包/除錯環境,並用 `claude -r` 接續同一個 Claude 對話。配套見 `previous-work.md`。

## 0. 解開 bundle

```bash
tar --zstd -xf dev-setup-YYYYMMDD.tar.zst -C <目標>
```

內含:`opendw_dragon_wars_cht/`(source + 完整 .git + 含音樂的 assets + .scratch 原始資料)、
`docker/`(Dockerfile)、`SETUP.md`、`previous-work.md`、`claude-session/`(Claude 對話 + 記憶)。

## 1. 還原 Claude session(★ 跨機接續關鍵)

```bash
mkdir -p ~/.claude/projects
cp -a claude-session/projects/-home-anr2-tmp-longcat ~/.claude/projects/
```

`claude -r` 的 session 依「當前工作目錄編碼」分目錄;本 session 編碼為 `-home-anr2-tmp-longcat`
(= cwd `/home/anr2/tmp/longcat`)。接續方式(三選一):
- **路徑相同**:把專案放回 `/home/anr2/tmp/longcat/opendw_dragon_wars_cht`,`cd /home/anr2/tmp/longcat && claude --continue`。
- **路徑不同**:直接 `claude --resume 70d6018e-f43b-4631-a28f-187225fa3d5b`(用 UUID 不卡路徑)。
- 最近 session UUID 見 `previous-work.md` § 接續。

## 2. 重建 docker 映像

```bash
cd opendw_dragon_wars_cht
bash docker/build-images.sh        # dwsdl(主建置)+ dwpil(Python)
```

其餘映像(mingw / appimagetool / wine / UADE)由各打包腳本按需自裝,不需預建。

## 3. build + 測試 + 打包(全在 docker 內,不污染系統)

```bash
cd opendw_remake
docker run --rm -v "$PWD":/app -w /app dwsdl bash -c \
  "cmake -S . -B build && cmake --build build -j && cd build && ctest --output-on-failure"   # 37/37

# 打包(各平台;產物落 dist/)
docker run --rm -v "$PWD":/app -w /app dwsdl bash tools/package/build_package.sh    # Linux tarball
docker run --rm -v "$PWD":/app -w /app dwsdl bash tools/package/build_appimage.sh   # AppImage
docker run --rm -v "$PWD":/app -w /app dwsdl bash tools/package/build_windows.sh    # Windows(mingw 交叉編譯)
```

macOS 走 GitHub Actions 原生 runner(`.github/workflows/ci.yml` macos job,arm64)。

## 4. 鐵則(務必遵守)

- **全程 docker build**,不污染系統;Python 一律 docker(dwpil / uv venv)。
- **原始遊戲檔(DRAGON.COM/DATA1/DATA2/.tune/.adf/水印圖)與渲染音樂 WAV 不入 git**
  (gitignore);本地 full release(dist-all/)才含音樂。GitHub repo 不放 release(版權)。
- 只提交已驗證的工作;feature 分支 → PR → merge 需確認;commit trailer 見既有 git log。

## 5. 音樂(若 assets/bundle/audio/music/*.wav 不在)

bundle 已含已渲染音樂。若需重渲染:`tools_build/render_music.sh`(UADE 把 Amiga .tune → WAV;
需自備 .tune 素材,見 .scratch / docs)。
