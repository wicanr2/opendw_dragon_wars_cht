# Android 移植 scaffold(火龍之戰 remake)

> **狀態:scaffold(建置骨架已備,需實機完成)**。本目錄提供 Android 建置的起點 ——
> Gradle + NDK + SDL2 整合骨架。**尚未產出可玩 APK**;下列三項需在有 Android 裝置 / 模擬器
> 的環境完成並實測。誠實標示,不謊稱已可玩。

引擎本體是 C++20 + SDL2,SDL2 原生支援 Android,故移植**不需重寫渲染/輸入核心**,但有三個
Android 專屬工項:

## 待完成(移植清單)

### 1. SDL2 Android 專案整合
SDL2 的 Android 以「引擎編成 `libmain.so`,由 `SDLActivity` 載入」運作。需:
- 取 SDL2 + SDL2_ttf 的 Android 原始碼 / prebuilt AAR(`org.libsdl.app`),放進 `app/jni/SDL`、
  `app/jni/SDL2_ttf`(或用 prefab）。
- `app/jni/src/CMakeLists.txt`(本目錄已備骨架)把引擎各 `.cpp` 編成 `libmain.so`,連結 SDL2。
- `settings.gradle` / `app/build.gradle`(已備骨架)用 `externalNativeBuild` 跑 CMake + NDK。

### 2. 資產載入改 Android asset(最關鍵)
引擎現以 `std::fopen` + **相對 cwd** 載 `assets/`(bundle / fonts / i18n)。Android 的 assets
封在 APK 內,**不能直接 fopen**。對策(擇一):
- **首次啟動把 APK assets/ 解壓到 internal storage**(`getFilesDir()`),再把 cwd 設到該處 →
  引擎的 fopen 原樣可用(改動最小,推薦)。`MainActivity.java`(已備骨架)示範解壓流程。
- 或改資產層走 `SDL_RWFromFile`(SDL 在 Android 自動讀 APK asset)—— 動到 `provider.cpp` /
  `level.cpp` 等所有 fopen,改動大。

### 3. 觸控操作(keyboard CRPG → 觸控)
本作是鍵盤操作(I/J/L 移動、F/R/K/V/S/G… 指令)。Android 需**螢幕觸控 → 按鍵對映**:
- 螢幕分割熱區:左下方向 D-pad(I/J/L/K)、右下動作鈕(F/R/C/V/G/Esc)。
- 在 SDL event loop(`sdl_video.cpp poll`)加 `SDL_FINGERDOWN` → 依座標落點映射成對應 `Input`
  欄位 / `key`。建議疊一層半透明觸控 overlay 提示熱區。
- F4/F8 等次要鍵放選單。

## 已備骨架檔

- `app/jni/src/CMakeLists.txt` — 引擎 → `libmain.so`(需補 SDL 路徑)。
- `app/build.gradle` / `build.gradle` / `settings.gradle` — Gradle + NDK。
- `app/src/main/AndroidManifest.xml` — SDLActivity / 權限。
- `app/src/main/java/cc/opendw/dragonwars/MainActivity.java` — 解壓 assets + 設 `DWR_ASSET_DIR` + SDLActivity。
- 引擎端已備:`main.cpp` 讀 `DWR_ASSET_DIR` 環境變數 → `chdir` 到解壓後的 assets(Android 資產載入第一步已通,
  跨平台編譯驗證 ctest 35/35)。
- **CI job 待加**:需先把 SDL2 + SDL2_ttf Android 原始碼放進 `app/jni/`(§1)才能 `gradle assembleDebug`;
  在無 SDL 原始碼前 CI 無法建,故暫不加會失敗的 job。

## 為何不在此環境完成

開發沙箱為 headless Linux docker,**無 Android SDK/NDK + 無裝置/模擬器**,觸控 UX 必須在
裝置上反覆調(熱區大小、回饋),資產解壓也要實機驗證。故此處只到「**可建置骨架 + 明確移植
計畫**」,實作與實測留給有 Android 環境者。三項中第 2、3 項是真正的工(第 1 項是設定)。
