# 中文字型資源目錄

此目錄放置遊戲使用的中文字型檔案。

## 支援的字型格式

- **TrueType Collection (.ttc)** - 推薦
- **TrueType (.ttf)** - 支援
- **OpenType (.otf)** - 支援

## 推薦字型

### 1. 文泉驛正黑 (wqy-zenhei.ttc)
- 授權：GPL
- 風格：現代、清晰
- 用途：UI 文字、對話

### 2. 文泉驛微米黑 (wqy-microhei.ttc)
- 授權：GPL
- 風格：纖細、優雅
- 用途：標題、選單

### 3. Noto Sans CJK
- 授權：SIL Open Font License
- 風格：中性、國際化
- 用途：多語言混排

## 字型檔案放置

```
data/
├── fonts/
│   ├── wqy-zenhei.ttc      # 主要 UI 字型
│   ├── wqy-microhei.ttc    # 標題字型
│   └── NotoSansCJK.otf     # 備選字型
├── DATA1                    # 遊戲資源（原始）
└── DATA2                    # 遊戲資源（原始）
```

## 使用方式

程式會自動偵測 `data/fonts/` 目錄下的字型檔案。

也可以透過環境變數指定：

```bash
export OPENDW_FONT_PATH=/path/to/font.ttc
```

## 注意事項

- 中文字型檔案通常很大（數 MB），不建議提交到 Git
- 建議使用 `.gitignore` 排除
- 建置系統會自動從系統字型目錄複製
