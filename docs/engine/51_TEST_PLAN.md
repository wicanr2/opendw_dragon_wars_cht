# OpenDW Dragon Wars 中文化 - 測試計畫

本文檔說明 OpenDW Dragon Wars 中文化專案的測試策略與計畫。

## 目錄

- [測試策略](#測試策略)
- [單元測試](#單元測試)
- [整合測試](#整合測試)
- [中文渲染測試](#中文渲染測試)
- [翻譯驗證測試](#翻譯驗證測試)
- [自動化測試](#自動化測試)
- [測試資料](#測試資料)

## 測試策略

### 測試層級

```
┌─────────────────────────────────────┐
│         E2E / 遊戲流程測試          │  ← 少量，關鍵路徑
├─────────────────────────────────────┤
│         整合測試                    │  ← 元件間互動
├─────────────────────────────────────┤
│         單元測試                    │  ← 大量，快速回饋
└─────────────────────────────────────┘
```

### 測試優先順序

1. **核心引擎** - engine.c, resource.c, state.c
2. **顯示層** - vga_sdl.c, ui.c
3. **資料處理** - compress.c, bufio.c
4. **中文渲染** - CJK 顯示, 字型載入
5. **翻譯驗證** - 編碼轉換, 文字正確性

## 單元測試

### 現有測試

| 測試檔案 | 測試目標 | 狀態 |
|----------|----------|------|
| `test_compress.cpp` | 壓縮/解壓縮 | ✅ 完成 |
| `test_opendw.cpp` | 核心引擎 | ✅ 完成 |
| `test_vga.cpp` | VGA 顯示 | ✅ 完成 |

### 計畫新增測試

#### 中文渲染測試 (test_cjk.cpp)

```cpp
// 測試案例
TEST_CASE("CJK font loading") {
    // 載入中文字型
    // 驗證字型不為 NULL
    // 驗證字元數量 > 0
}

TEST_CASE("Chinese character rendering") {
    // 渲染「龍」字
    // 驗證像素不為空
    // 驗證寬度 > 0
}

TEST_CASE("Big5 encoding conversion") {
    // Big5 → UTF-8
    // UTF-8 → Big5
    // 驗證雙向轉換正確
}

TEST_CASE("CJK text layout") {
    // 測量字串寬度
    // 驗證換行正確
    // 驗證邊界截斷
}
```

#### 翻譯驗證測試 (test_translation.cpp)

```cpp
// 測試案例
TEST_CASE("Translation table integrity") {
    // 載入翻譯表
    // 驗證每條翻譯都有原文與譯文
    // 驗證沒有空條目
}

TEST_CASE("Encoding consistency") {
    // 驗證所有翻譯使用 UTF-8
    // 驗證沒有亂碼
    // 驗證標點符號正確
}

TEST_CASE("UI text fitting") {
    // 驗證翻譯文字適合 UI 空間
    // 驗證沒有截斷
    // 驗證換行正確
}
```

#### 資源載入測試 (test_resource.cpp)

```cpp
// 測試案例
TEST_CASE("DATA1 parsing") {
    // 載入 DATA1
    // 驗證 24 個 section
    // 驗證每個 section 的 magic number
}

TEST_CASE("Font resource loading") {
    // 載入字型資源
    // 驗證字型資料完整性
    // 驗證字元映射表
}

TEST_CASE("String extraction") {
    // 提取字串資源
    // 驗證字串以 null 結尾
    // 驗證沒有溢位
}
```

## 整合測試

### SDL2 顯示整合

| 測試項目 | 驗證方式 | 通過標準 |
|----------|----------|----------|
| 視窗建立 | 自動化 | SDL_CreateWindow 成功 |
| 渲染器建立 | 自動化 | SDL_CreateRenderer 成功 |
| 中文字型載入 | 自動化 | TTF_OpenFont 成功 |
| 文字渲染 | 像素比對 | 輸出與基準圖一致 |
| 事件處理 | 自動化 | 鍵盤/滑鼠事件正常 |

### 遊戲流程整合

| 測試項目 | 驗證方式 | 通過標準 |
|----------|----------|----------|
| 啟動遊戲 | 自動化 | 不崩潰，顯示主選單 |
| 載入存檔 | 自動化 | 成功載入 |
| 顯示對話 | 截圖比對 | 中文正確顯示 |
| 移動角色 | 自動化 | 角色移動正常 |
| 戰鬥畫面 | 截圖比對 | UI 元素正確 |

### 中文顯示整合

| 測試項目 | 驗證方式 | 通過標準 |
|----------|----------|----------|
| 選單文字 | 截圖比對 | 無亂碼 |
| 對話文字 | 截圖比對 | 換行正確 |
| 狀態面板 | 截圖比對 | 數值正確 |
| 道具名稱 | 截圖比對 | 顯示完整 |
| 技能名稱 | 截圖比對 | 無截斷 |

## 中文渲染測試

### 測試矩陣

| 字元類型 | 範例 | 測試重點 |
|----------|------|----------|
| 繁體常用字 | 龍、戰、王 | 基本渲染 |
| 繁體罕用字 | 龘、靐、齉 | 複雜筆畫 |
| 簡體字 | 龙、战、王 | 向下相容 |
| 日文漢字 | 龍、戦、王 | CJK 統一 |
| 標點符號 | ，。！？、； | 排版正確 |
| 數字 | 0-9 | 寬度一致 |
| 英文 | A-Z | 混合排版 |
| 空白 | 、　 | 全形半形 |

### 字型品質測試

```cpp
TEST_CASE("Font metrics consistency") {
    // 驗證所有中文字元寬度一致（等寬字型）
    // 驗證行高一致
    // 驗證基線對齊
}

TEST_CASE("Font fallback") {
    // 測試缺少字元時的 fallback
    // 測試多語言混排
    // 測試 emoji 支援（如果有的話）
}

TEST_CASE("Font rendering at different sizes") {
    // 測試 12px, 16px, 24px, 32px
    // 驗證每種大小都清晰
    // 驗證沒有模糊或鋸齒
}
```

### 效能測試

```cpp
TEST_CASE("Text rendering performance") {
    // 渲染 1000 個中文字
    // 測量時間
    // 驗證 < 16ms（60 FPS）
}

TEST_CASE("Font loading performance") {
    // 載入中文字型
    // 測量時間
    // 驗證 < 100ms
}
```

## 翻譯驗證測試

### 自動化翻譯檢查

#### 1. 編碼正確性

```bash
# 驗證所有翻譯檔案為 UTF-8
file docs/TRANSLATION*.md | grep -v "UTF-8"

# 驗證沒有 BOM
hexdump -C docs/TRANSLATION.md | head -1 | grep -v "ef bb bf"

# 驗證換行為 LF（非 CRLF）
file docs/TRANSLATION.md | grep -v "CRLF"
```

#### 2. 翻譯完整性

```python
# 驗證每個原文都有譯文
import re
with open('docs/TRANSLATION.md', 'r', encoding='utf-8') as f:
    content = re.findall(r'\|\s*(.+?)\s*\|\s*(.+?)\s*\|', content)
    for original, translated in content:
        assert translated.strip(), f"Empty translation for: {original}"
```

#### 3. 用詞一致性

```bash
# 驗證術語一致性
# 例如：所有 "Strength" 都翻譯為 "力量"
grep -c "力量" docs/TRANSLATION.md
grep -c "Strength" docs/TRANSLATION.md
```

#### 4. UI 適合性

```python
# 驗證翻譯長度適合 UI
MAX_LENGTHS = {
    'item_name': 20,
    'skill_name': 16,
    'character_name': 12,
    'menu_item': 8,
}

with open('docs/TRANSLATION.md', 'r') as f:
    for line in f:
        category, original, translated = parse_line(line)
        max_len = MAX_LENGTHS.get(category, 30)
        assert len(translated) <= max_len, f"Translation too long: {translated}"
```

### 人工翻譯審查清單

- [ ] 专有名词翻譯一致（角色名、地點名）
- [ ] 技能名翻譯符合遊戲風格
- [ ] 對話自然流暢
- [ ] 沒有性別/數量錯誤
- [ ] 文化差異已在地化
- [ ] 沒有政治/宗教敏感性內容

## 自動化測試

### GitHub Actions .workflow

```yaml
# .github/workflows/test.yml
name: Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build-and-test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v3

    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y \
          libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev \
          libcheck-dev \
          fonts-wqy-zenhei fonts-noto-cjk

    - name: Configure
      run: |
        mkdir build && cd build
        cmake -DCMAKE_BUILD_TYPE=Debug ..

    - name: Build
      run: cmake --build build --parallel $(nproc)

    - name: Run tests
      run: |
        cd build
        ctest --output-on-failure

    - name: Upload artifacts
      uses: actions/upload-artifact@v3
      with:
        name: build-output
        path: build/src/fe/sdldragon
```

### Pre-commit Hooks

```yaml
# .pre-commit-config.yaml
repos:
  - repo: local
    hooks:
      - id: cmake-format
        name: cmake-format
        entry: cmake-format
        language: system
        files: CMakeLists.txt

      - id: clang-format
        name: clang-format
        entry: clang-format -i
        language: system
        files: '\.(c|h|cpp|hpp)$'

      - id: cppcheck
        name: cppcheck
        entry: cppcheck --enable=all --suppress=missingInclude
        language: system
        files: '\.(c|h|cpp|hpp)$'

      - id: translation-check
        name: translation-check
        entry: python3 scripts/check_translation.py
        language: system
        files: docs/TRANSLATION*.md
```

### 持續整合矩陣

| 平台 | 編譯器 | SDL2 版本 | 狀態 |
|------|--------|-----------|------|
| Ubuntu 22.04 | GCC 11 | 2.0.22 | ✅ |
| Ubuntu 20.04 | GCC 9 | 2.0.10 | ✅ |
| macOS 13 | Clang 14 | 2.28.0 | ✅ |
| Windows Server 2022 | MSVC 2022 | 2.28.0 | 🔲 |

## 測試資料

### 測試字型

放置於 `tests/data/fonts/`：

| 檔案 | 用途 |
|------|------|
| `wqy-zenhei-test.ttc` | 文泉驛正黑（測試用） |
| `test-pattern.png` | 中文渲染基準圖 |

### 測試指令碼

放置於 `tests/data/scripts/`：

| 檔案 | 用途 |
|------|------|
| `test_dialog.txt` | 測試對話腳本 |
| `test_big5.txt` | Big5 編碼測試 |

### 測試預期輸出

放置於 `tests/data/expected/`：

| 檔案 | 用途 |
|------|------|
| `menu_screenshot.png` | 主選單基準截圖 |
| `dialog_screenshot.png` | 對話畫面基準截圖 |
| `battle_screenshot.png` | 戰鬥畫面基準截圖 |

## 測試執行

### 執行所有測試

```bash
cd build
ctest --output-on-failure
```

### 執行特定測試

```bash
# 只執行 CJK 測試
ctest -R cjk --output-on-failure

# 只執行翻譯測試
ctest -R translation --output-on-failure

# 顯示詳細輸出
ctest -V
```

### 產生測試覆蓋率

```bash
# 建置時啟用覆蓋率
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="--coverage" \
      -DCMAKE_CXX_FLAGS="--coverage" ..

# 執行測試
ctest

# 產生覆蓋率報告
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

## 測試報告

### 報告格式

每次 CI 執行後產生：

```
reports/
├── test-results.xml    # JUnit XML 格式
├── coverage-report/    # HTML 覆蓋率報告
└── screenshots/        # 視覺迴歸測試截圖
```

### 品質門檻

| 指標 | 目標 | 警告 |
|------|------|------|
| 測試通過率 | 100% | < 95% |
| 程式碼覆蓋率 | > 80% | < 70% |
| 翻譯完整性 | 100% | < 98% |
| 渲染基準比對 | > 99% | < 95% |
