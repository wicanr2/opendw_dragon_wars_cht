# Dragon Wars 翻譯對照表

## 遊戲內文字 (從 dragon.com 提取)

### UI 文字
| 英文 | 中文 | 位置 |
|------|------|------|
| Dragon Wars Configure Menu V1.1 | 龍之戰設定選單 V1.1 | dragon.com |
| Copyright 1989, 1990 Interplay | 版權 1989, 1990 Interplay | dragon.com |
| A. CGA RGB monitor | A. CGB RGB 螢幕 | dragon.com |
| B. CGA composite monitor | B. CGA 複合式螢幕 | dragon.com |
| C. Tandy 16 color | C. Tandy 16 色 | dragon.com |
| D. EGA/VGA 16 color | D. EGA/VGA 16 色 | dragon.com |
| 1. Mouse On | 1. 滑鼠開啟 | dragon.com |
| 2. Mouse Off | 2. 滑鼠關閉 | dragon.com |
| Select a screen format by typing its letter. | 輸入字母選擇螢幕格式。 | dragon.com |
| Press [KEY] to begin the game or press "S" to save configuration | 按 [KEY] 開始遊戲或按 "S" 儲存設定 | dragon.com |
| or press ESC to return to MS-DOS. | 或按 ESC 返回 MS-DOS。 | dragon.com |
| Press 1 or 2 for enabling/disabling mouse support. | 按 1 或 2 啟用/停用滑鼠支援。 | dragon.com |
| Saving game state. | 儲存遊戲進度。 | dragon.com |
| Game state saved. | 遊戲進度已儲存。 | dragon.com |
| Drive error. | 磁碟錯誤。 | dragon.com |
| Write protected. | 防寫保護。 | dragon.com |
| Fatal error : Out of memory.$ | 嚴重錯誤：記憶體不足。$ | dragon.com |
| Loading... | 載入中… | DATA1 script |

### 狀態文字
| 英文 | 中文 | 位置 |
|------|------|------|
| chained | 被鏈住 | DATA1 script |
| poisoned | 中毒 | DATA1 script |
| stunned | 昏迷 | DATA1 script |
| dead | 死亡 | DATA1 script |

### 對話文字
| 英文 | 中文 | 位置 |
|------|------|------|
| (需要從 DATA1 script 提取) | | |

### 物品名稱
| 英文 | 中文 | 位置 |
|------|------|------|
| (需要從 DATA1 script 提取) | | |

## 注意事項

1. DATA1 中的文字使用 5-bit 壓縮編碼，需要解壓才能取得完整文字
2. 對話和物品名稱儲存在 script section (Section 0x00)
3. 需要正確實作 bit_extract() 和 extract_letter() 才能解壓

## 解壓方式

```c
// 從 script 提取文字
struct bit_extractor be = {0};
be.data = script_data;
be.offset = text_offset;

while (1) {
    uint8_t letter = extract_letter(&be);
    if (letter == 0) break;
    // letter 是 ASCII 字元 (bit 7 可能設為 1)
    putchar(letter & 0x7F);
}
```
