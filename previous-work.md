# previous-work — 火龍之戰 remake 工作交接(dev-setup)

接手者:讀本檔 + `SETUP.md` 即可在新機重建環境並用 `claude -r` 接續同一對話。

## 專案現況快照

- repo:`opendw_dragon_wars_cht`(GitHub: wicanr2/opendw_dragon_wars_cht,branch `main`)。
- 主產物:`opendw_remake/`(C++20 + SDL2 乾淨重製),以 opendw(C 反組譯)為逐位元 oracle。
- 可玩度:建角 → 探索 40/40 連通世界 → 主線繁中事件 → 終戰 Namtar → 結局,可通關。
- ctest **37/37**。opcode 129/256。
- 四平台 **full release**(含音樂)在 `dist-all/`(本地、gitignore,不上 GitHub 避版權):
  Linux tarball / AppImage / Windows zip(mingw)/ macOS arm64 .app。

## 本次 session 做的工作(主題)

1. **寶箱開箱 grounded**:踩寶箱格 → K 開鎖 → 給真實物品 + 持久(`verify_chest_acquire`)。
   修 bug:事件偵測比對英文原文(非翻譯後 event_msg)。
2. **劇情物 grounded 給予**:攻略驅動編目 `assets/bundle/quest/grants.tsv`(9 件:朝聖者之袍/
   光譜眼鏡/龍石/國王戒指/護身符/黃金之靴/尖刺連枷/靈魂之碗/自由之劍),首次進區且未持有 →
   給並持久(`verify_quest_grant`)。**12-byte 物品名限制突破**:槽存 ≤12 ID,顯示走 i18n 全名。
3. **物品名在地化**:items.bin 內嵌英文名(Earth Shield→大地之盾 等)過 tr。Dragon Stone→龍石。
4. **視窗動態縮放**:RESIZABLE + SDL_RenderSetLogicalSize 等比 letterbox。
5. **跨平台打包**:Windows mingw 交叉編譯(`build_windows.sh`,wine+xvfb smoke);
   macOS 改 GitHub Actions 原生 runner(.app + dylibbundler + 原生 headless smoke,arm64)。
   修 3 個可攜性 bug:narrowing、SDL_ttf struct tag(改 void*)、SDL2_ttf LIBRARY_DIRS。
6. **CI**:linux/appimage/windows(ubuntu mingw)/macos(arm64)四 job 全綠;Intel macOS 放棄。

## 工具鏈 / harness

- docker 映像:`dwsdl`(主建置;`docker/Dockerfile.dwsdl`)、`dwpil`(Python)。其餘按需自裝。
- 打包腳本:`opendw_remake/tools/package/`(build_package / build_appimage / build_windows)。
- 驗證:`opendw_remake/tools/verify/*`(ctest 37);編目工具 find_chests / dump_event_catalog。
- 音樂:`tools_build/render_music.sh`(UADE)。

## 待辦 / 開放項目

- **Android(進行中)**:`opendw_remake/android/` scaffold。下一步=規劃並實作觸控 UX
  (鍵盤 CRPG → 螢幕觸控熱區 + 命令面板;見 android/README.md 第 3 項),並用 GitHub Action build APK。
- Release:決定**只放本地 dist-all**(full,含音樂),GitHub 不放 release(版權)。

## 鐵則 / 硬約束

- 全程 docker build,不污染系統;Python 用 docker。
- 原始遊戲檔 + 渲染音樂 WAV 不入 git(gitignore);本地 full release 才含音樂。
- 只提交已驗證工作;commit trailer:`Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`。
- 領域:ErrorCode2 only(此專案不涉);繁中回應。

## § 在別台電腦接續(claude -r)

1. 還原 session:`cp -a claude-session/projects/-home-anr2-tmp-longcat ~/.claude/projects/`
2. 接續(擇一):
   - 路徑相同(`/home/anr2/tmp/longcat`):`cd /home/anr2/tmp/longcat && claude --continue`
   - 任意路徑:`claude --resume 70d6018e-f43b-4631-a28f-187225fa3d5b`
- **最近 session UUID:`70d6018e-f43b-4631-a28f-187225fa3d5b`**(cwd 編碼 `-home-anr2-tmp-longcat`)。

## 記憶索引(claude-session/projects/-home-anr2-tmp-longcat/memory/)

- MEMORY.md(索引)+ 各 .md:OpenDW 專案總覽、寶箱/quest grounded、事件偵測比對英文、
  攻略已 OCR、直接執行不反問偏好。
