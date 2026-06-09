# Dragon Wars 翻譯對照表

## 概述

本檔案包含 Dragon Wars 所有遊戲文字的英文原文與中文翻譯。
這些文字從 DATA1 資源檔中提取，使用 5-bit 壓縮編碼。

**注意**：此檔案可在重新實作時直接嵌入程式碼，無需再由 DATA1 取得。

---

## 1. 主選單 (Main Menu)

| ID | 英文 | 中文 | 指令位置 |
|----|------|------|----------|
| MENU_BEGIN | Begin a new game | 開始新遊戲 | script:21 |
| MENU_CONTINUE | Continue an old game | 繼續舊遊戲 | script:21 |
| MENU_START_WARN | Starting a new game will destroy your last saved game. Do you still wish to start a new game? | 開始新遊戲會摧毀您最後儲存的遊戲。您仍然希望開始新遊戲嗎？ | script:75 |
| MENU_PARTY | Current party... | 目前隊伍... | script:153 |
| MENU_CREATE | Create character | 建立角色 | script:189 |
| MENU_DELETE | Delete | 刪除 | script:266 |
| MENU_RENAME | Rename | 重新命名 | script:286 |
| MENU_VIEW | View | 查看 | script:295 |
| MENU_NAME | What will (name)'s new name be? | （名字）的新名字是什麼？ | script:334 |
| MENU_DELETE_WARN | You are about to delete (name). What has (name) done to deserve such a fate? | 您即將刪除（名字）。（名字）做了什麼以至於落得如此下場？ | script:389 |
| MENU_FOREVER | (name) will be gone forever. Have mercy. | （名字）將永遠消失。請大發慈悲。 | script:449 |
| MENU_BYE | Bye bye, (name). | 再見，（名字）。 | script:478 |
| MENU_NAME_NEW | Name your new character. | 為您的新角色命名。 | script:549 |
| MENU_GENDER | Male or Female | 男性或女性 | script:586 |
| MENU_POINTS | You still have (N) points left to distribute. Do you wish to go back and distribute them? | 您還有（N）點可以分配。您希望回去分配它們嗎？ | script:660 |
| MENU_MUST_HAVE | You must have someone in the party to begin the game. | 您的隊伍中必須有人才能開始遊戲。 | script:747 |

---

## 2. 狀態文字 (Status Text)

| ID | 英文 | 中文 |
|----|------|------|
| STATUS_CHAINED | chained | 被鏈住 |
| STATUS_POISONED | poisoned | 中毒 |
| STATUS_STUNNED | stunned | 昏迷 |
| STATUS_DEAD | dead | 死亡 |

---

## 3. UI 文字 (從 dragon.com 提取)

| ID | 英文 | 中文 |
|----|------|------|
| UI_CONFIG_TITLE | Dragon Wars Configure Menu V1.1 | 龍之戰設定選單 V1.1 |
| UI_COPYRIGHT | Copyright 1989, 1990 Interplay | 版權 1989, 1990 Interplay |
| UI_CGA_RGB | A. CGA RGB monitor | A. CGA RGB 螢幕 |
| UI_CGA_COMP | B. CGA composite monitor | B. CGA 複合式螢幕 |
| UI_TANDY | C. Tandy 16 color | C. Tandy 16 色 |
| UI_EGA_VGA | D. EGA/VGA 16 color | D. EGA/VGA 16 色 |
| UI_MOUSE_ON | 1. Mouse On | 1. 滑鼠開啟 |
| UI_MOUSE_OFF | 2. Mouse Off | 2. 滑鼠關閉 |
| UI_SELECT_SCREEN | Select a screen format by typing its letter. | 輸入字母選擇螢幕格式。 |
| UI_PRESS_KEY | Press [KEY] to begin the game or press "S" to save configuration | 按 [KEY] 開始遊戲或按 "S" 儲存設定 |
| UI_ESC_EXIT | or press ESC to return to MS-DOS. | 或按 ESC 返回 MS-DOS。 |
| UI_MOUSE_HELP | Press 1 or 2 for enabling/disabling mouse support. | 按 1 或 2 啟用/停用滑鼠支援。 |
| UI_SAVING | Saving game state. | 儲存遊戲進度。 |
| UI_SAVED | Game state saved. | 遊戲進度已儲存。 |
| UI_DRIVE_ERR | Drive error. | 磁碟錯誤。 |
| UI_WRITE_PROT | Write protected. | 防寫保護。 |
| UI_FATAL | Fatal error : Out of memory.$ | 嚴重錯誤：記憶體不足。$ |
| UI_LOADING | Loading... | 載入中… |

---

## 4. 對話文字 (待提取)

**注意**：以下文字需要從 DATA1 script 中進一步提取。
目前由於 bit_extract 同步問題，部分文字尚未完整解碼。

### 已知部分解碼的文字
```
[21]  [c4]n vns uiqh rnww\n\nRegim a meu gale\nSnmrimse am nkd gale
     → "Do you wish to..\n\nBegin a new game\nContinue an old game"

[75]  [d3]raprimg a meu gale uikk deqrpnv vnsp kaqr qated galew [c4]n vns qrikk uiqh rn qrapr a meu gale[bf]
     → "Starting a new game will destroy your last saved game. Do you still wish to start a new game)\n\n"

[153] [c3]sppemr oaprvwww
     → "Current party..."

[189] \nSpeare chapacrep
     → "\nCreate character"

[213] \nRegim rhe gale
     → "\nBegin the game"

[266] [c4]n vns uiqh rnww\n\nTekere
     → "Do you wish to..\n\nDelete "

[286] \n]emale
     → "\nRename "

[295] \n,ieu
     → "\nView "

[324] [d7]har uikk
     → "What will "

[334] yq meu male be[bf]
     → "'s new name be)\n\n"

[389] [d9]ns ape abnsr rn dekere
     → "You are about to delete "

[419]  dnme rn deqepte qsch a fare[bf][bf]\n\nTekere
     → " done to deserve such a fate))\n\nDelete "

[449]  fnpetepw\nXate lepcvw
     → " forever.\nHave mercy."

[478] [c2]ve bvez
     → "Bye bye, "

[549] [ce]ale vnsp meu chapacrepw
     → "Name your new character."

[586] \n\n/ake np\nVelake[bf]
     → "\n\nMale or Female)\n"

[660]  onimrq kefr rn diqrpibsrez dn vns uiqh rn gn bacj amd diqrpibsre rhel[bf]
     → " points left to distribute, do you wish to go back and distribute them)\n\n"

[747] [d9]ns lsqr hate qnlenme im rhe oaprv rn begim rhe gale00
     → " must have someone in the party to begin the game.."
```

---

## 5. 物品名稱 (待提取)

**注意**：物品名稱儲存於 script 中，需要完整解壓才能取得。

---

## 6. 技術細節

### 5-bit 編碼字母表
```
0xa0 = 空格
0xe1-0xfa = a-z, 0-9 (小寫字母和數字)
0xb0-0xb9 = 0-9 (備用數字)
0x41-0x5a = A-Z (大寫字母)
0x8d = 換行 (\n)
0xa8 = '
0xa9 = :
0xaf = /
0xdc = \
0xa3 = !
0xaa = (
0xbf = )
0xbc = ]
0xbe = -
0xba = +
0xbb = *
0xad = ,
0xa5 = .
```

### 控制碼
```
[c4] = B (Begin)
[c3] = C (Continue/Create)
[d3] = S (Start)
[d0] = D (Delete)
[52] = R (Rename)
[56] = V (View)
[4d] = M (Male)
[46] = F (Female)
[48] = H (Have mercy)
[bf] = )
[be] = -
[aa] = (
[d7] = W (What)
[ce] = N (Name)
[c9] = I (Is)
[c2] = Y (Bye)
[59] = .
[5a] = ?
```

### 解壓演算法
```python
def extract_string(data, byte_offset, max_len=500):
    be = BitExtractor(data, byte_offset)
    result = []
    for _ in range(max_len):
        letter = be.extract_letter()
        if letter == 0: break
        result.append(letter)
    return bytes(result), be.byte_offset
```

---

## 7. 使用方式

在重新實作時，可以直接嵌入翻譯文字：

```c
// 定義翻譯字串
const char* MENU_BEGIN_EN = "Begin a new game";
const char* MENU_BEGIN_ZH = "開始新遊戲";

// 或使用結構體
struct TranslationEntry {
    const char* id;
    const char* english;
    const char* chinese;
};

const TranslationEntry translations[] = {
    {"MENU_BEGIN", "Begin a new game", "開始新遊戲"},
    {"MENU_CONTINUE", "Continue an old game", "繼續舊遊戲"},
    // ...
};
```

---

## 8. 待辦事項

- [ ] 完整解壓所有對話文字
- [ ] 完整解壓所有物品名稱
- [ ] 建立完整的翻譯表
- [ ] 實作中文顯示（24×24 點陣）
- [ ] 實作中文輸入
