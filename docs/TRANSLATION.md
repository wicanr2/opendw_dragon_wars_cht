# Dragon Wars 翻譯對照表

## 遊戲內文字 (從 DATA1 script 提取)

### 選單文字
| 英文 | 中文 | 位置 |
|------|------|------|
| Begin a new game | 開始新遊戲 | script:21 |
| Continue an old game | 繼續舊遊戲 | script:21 |
| Starting a new game will destroy your last saved game. Do you still wish to start a new game? | 開始新遊戲會摧毀您最後儲存的遊戲。您仍然希望開始新遊戲嗎？ | script:75 |
| Current party... | 目前隊伍... | script:153 |
| Create character | 建立角色 | script:189 |
| Delete | 刪除 | script:266 |
| Rename | 重新命名 | script:286 |
| View | 查看 | script:295 |
| Male or Female | 男性或女性 | script:586 |
| Name your new character. | 為您的新角色命名。 | script:549 |
| What will (name)'s new name be? | （名字）的新名字是什麼？ | script:334 |
| You are about to delete (name). What has (name) done to deserve such a fate? | 您即將刪除（名字）。（名字）做了什麼以至於落得如此下場？ | script:389 |
| (name) will be gone forever. Have mercy. | （名字）將永遠消失。請大發慈悲。 | script:449 |
| Bye bye, (name). | 再見，（名字）。 | script:478 |
| You still have (N) points left to distribute. Do you wish to go back and distribute them? | 您還有（N）點可以分配。您希望回去分配它們嗎？ | script:660 |
| You must have someone in the party to begin the game. | 您的隊伍中必須有人才能開始遊戲。 | script:747 |

### 狀態文字
| 英文 | 中文 | 位置 |
|------|------|------|
| chained | 被鏈住 | DATA1 script |
| poisoned | 中毒 | DATA1 script |
| stunned | 昏迷 | DATA1 script |
| dead | 死亡 | DATA1 script |

### UI 文字 (從 dragon.com 提取)
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

### 物品名稱
| 英文 | 中文 | 位置 |
|------|------|------|
| (需要進一步提取) | | |

### 對話文字
| 英文 | 中文 | 位置 |
|------|------|------|
| (需要進一步提取) | | |

## 注意事項

1. DATA1 中的文字使用 5-bit 壓縮編碼，需要正確解壓才能取得完整文字
2. 對話和物品名稱儲存在 script section (Section 0x00)
3. 每個選單項目的第一個字是快捷鍵控制碼（如 'B' = Begin, 'C' = Continue）
4. 特殊字元：[bf] = ')', [be] = '-', [aa] = '(', [c4] = 'B', [c3] = 'C', [d3] = 'S'

## 解壓方式

```c
// 從 script 提取文字
struct bit_extractor be = {0};
be.data = script_data;
be.offset = text_offset;

while (1) {
    uint8_t letter = extract_letter(&be);
    if (letter == 0) break;
    // letter 是 alphabet 編碼的值
    // 需要對照 alphabet[] 表轉換為 ASCII
    putchar(alphabet_index_to_char(letter));
}
```

## 待辦事項

- [ ] 完整提取所有對話文字
- [ ] 完整提取所有物品名稱
- [ ] 建立完整的翻譯表
- [ ] 實作中文顯示（24×24 點陣）
- [ ] 實作中文輸入
