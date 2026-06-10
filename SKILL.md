---
name: dragon-wars-cht-remake
description: 推進《火龍之戰》(Dragon Wars, Interplay 1989)的繁體中文化 + C++20/SDL2 現代重寫(opendw_remake)。涵蓋:用 opendw(Devin Smith 的 C 反組譯)當正確性 oracle 做 byte-for-byte / 逐指令差異測試;從 DATA1/DATA2 萃取資產(5-bit 文字、Huffman 解壓、res31 怪物名、sprite、全螢幕場景圖 title_adjust 去交錯);Read Paragraph 防拷段落內嵌顯示;CJK 24×24 渲染進 320×200 framebuffer;手冊/軟體世界攻略以視覺轉寫 OCR;docker 工具鏈。觸發條件:使用者提到「火龍之戰」「Dragon Wars」「opendw」「opendw_remake」「龍之戰中文化」,或要求接續萃取/翻譯/重寫/驗證該遊戲。repo:github.com/wicanr2/opendw_dragon_wars_cht。
---

# 火龍之戰 (Dragon Wars) 中文化 + Remake Skill

## 何時啟用
使用者提到「火龍之戰 / Dragon Wars / opendw / opendw_remake / 龍之戰中文化」,或要接續該專案的萃取、翻譯、重寫、驗證。

## 專案座標
- **Repo**:`github.com/wicanr2/opendw_dragon_wars_cht`(本機 `/home/anr2/tmp/longcat/opendw_dragon_wars_cht/`)
- **oracle 原始碼**:`/home/anr2/tmp/longcat/opendw/`(Devin Smith C 反組譯,**唯讀,當對照**)
- **原始資料**:`Dragon Wars (1990).zip` → DOS 軟碟映像 → 取出 `DATA1`/`DATA2`/`DRAGON.COM`
  - `7z e "Dragon Wars (1990).zip"` → DISK01.IMA;`7z e DISK01.IMA DATA1 DATA2 DRAGON.COM`
- **軟體世界攻略**:`longcat/softworld/` 第 25/26/27 期 PDF(repo 外)

## 兩條主線
1. **資產 + 中文化資料**(`opendw_dragon_wars_cht/docs/` + `data/` + `CONTEXT.md`)= remake 的資產來源 + 翻譯。
2. **opendw_remake/**:C++20 + SDL2 乾淨重寫的 **runtime**(VM + 渲染 + 資產層),**執行原始 bytecode**,以 opendw 為正確性 oracle,走向不依賴原始磁碟檔的自包含資產。設計見 `opendw_remake/ARCHITECTURE.md`。

## ⚠️ 必懂的技術 lore(踩過的雷)

1. **遊戲文字不是靜態表**:內嵌在 script bytecode,緊接 `op_77/op_78/op_7B` 之後、byte 對齊的 5-bit 壓縮串。**逐 byte 暴力解全 section = 雜訊**(`docs/_deprecated/20_ALL_TEXT_FROM_DATA1.txt` 的 3926 條就是這樣來的,已作廢)。乾淨文字 = `docs/ALL_TEXT_FROM_SCRIPTS.txt`(跟 bytecode 解出)。
2. **section 0x08–0x16 不是文字表**(暴力萃取假象)。
3. **5-bit 文字編碼**:`alphabet[]`(92 項,compress.c)+ 5-bit 取位(carry=tmp>(tmp<<1))+ 0x1E 大小寫切換 + 0xAF/0xDC escape(變數代入)。`extract_string` 回傳值 = 下一條起始 offset。已移植 `opendw_remake/src/resource/text_codec`。
4. **資源壓縮**:section > 0x17 是 Huffman 樹字典(compress.c build_dictionary+decompress)。開頭 2 bytes LE = 解壓後大小。已移植 `resource/decompress`,對 res31/res168 **byte-for-byte == opendw**。
5. **DATA2 bug**:89 個資源**只在 DATA2**(DATA1 header[N] ≥ 0xFF00)。原 resource.c 寫死只讀 data1 → 讀錯資料(怪物 sprite 半數錯誤)。修正:header≥0xFF00 改從 data2 載入(`tools_build/resource_data2.patch`,已內建於 remake archive)。
6. **怪物名在 res31**(壓縮,2177B);record byte 對齊,name 在 record+0x21(5-bit)。`monsters.txt` 的 168/196/200/210/222 是 **sprite 圖編號,非名字**。見 `docs/26_MONSTERS_AND_SPRITES.md`。
7. **怪物 sprite 渲染**:走 `show_random_encounter`/`draw_random_encounter_graphic`(160×136 viewport,nibble-per-pixel,DOS 16 色)。`tools_build/sprite_dump.cpp`。
8. **全螢幕場景圖**(res 24-29,解壓=32000B=320×200):是**垂直 XOR delta 交錯**,渲染前**必須先做 title_adjust 還原**(opendw main.c),否則紅/青條紋亂碼。`tools_build/scene_render.py`、中文版 `scene_localize.py`。
9. **Read Paragraph 防拷**:遊戲只顯示「Read paragraph N」(字串在 section 0x08@0x2d9,op_78 + op_81 數字)。段落文字在**印刷手冊**。手冊段落號 = 遊戲 N(已驗證:段落 137=Magan/Irkalla=玩家截圖)。段落 DB:`data/paragraphs/`(147 段),工具 `tools_build/build_paragraphs.py`。功能規劃 `docs/08_READ_PARAGRAPH_FEATURE.md`。
10. **CJK 渲染**:24×24 文泉驛(wqy-zenhei,GPL)點陣,進 320×200 framebuffer,每行約 13 中文字。驗證圖 `docs/cjk_demo/`,原型 `tools_build/cjk_render_proto.py`。
11. **VM quirk**:用 `word_3AE6` 當旗標(bit0 carry/bit6 zero/bit7 sign)、`byte_3AE1` 模式、`word_3AE2/3AE4` 主次暫存器,且「ax 高位 vs 模式」決定字/位元組運算。remake VmState 刻意**鏡像這些變數**以利逐字移植 + 差異測試。

## docker 工具鏈(必用 docker,勿污染系統)
- image:`dwtools`(ubuntu:22.04 + g++)。
- **opendw 原始碼編不過**,需 shim(見 `tools_build/`):補 op_43/4C/4D/55/5B/5F/60/62/63 stub + op_56/59/5C/72 前向宣告 + sub_2AEE() stub;ui.c draw_right_pillar 重複定義改名 + 刪 line49 static 前向宣告。
- remake 工具:`opendw_remake/` `cmake -S . -B build`(C++20);headers 有 extern "C" → lib .c 用 gcc、tool .cpp 用 g++。

## 譯名(一律以 `CONTEXT.md` 為準)
波卡城/罪惡之城(Purgatory)、瑪根地底世界(Magan)、納達(Namtar)、**奈羅(Nergal,Irkalla 之夫,≠Namtar)**、伊爾卡拉(Irkalla)、胡姆巴巴(Humbaba)、初級/高級/德魯伊/太陽/雜項魔法、生命/法力/暈眩值、力量/敏捷/智力/精神、京雄城/拜占儂/菲巴斯/蘭斯克/塔斯廢墟/黃泥蟾蜍城、靈魂之泉、自由之劍、龍捲風。譯名來源優先序:**官方臺灣手冊(珍066)> 約定俗成 > 直譯**。

## 進度(2026-06-10,8 個 PR 已合併)
- **R0 資產層**:archive + text_codec + decompress(含 DATA2 修正)對拍 opendw byte-for-byte ✅
- **R1 batch 1**:VM 核心(VmState/dispatch/trace)+ 15/256 純 opcode,自測通過
- 翻譯草表 v0.1(`docs/15_TRANSLATION_DRAFT.md`,258 條)、中文場景圖、手冊+段落+三期攻略視覺轉寫、docs 審查整併(錯誤結論作廢、譯名對齊)

## 下一步(建議順序)
1. **R1 差異測試 harness**(關鍵):在 opendw 加 trace hook 輸出 `(pc,op,r2,r4,flags,gamestate diff)`,remake 跑同 bytecode 逐行 diff。立起來後每加 opcode 自動驗。
2. R1 補齊其餘 opcode(對拍)。
3. **R2 render**:framebuffer + SDL2 pixel scaling(640×480)+ 8×8 字 + title 畫面 golden 測試。
4. R3:字串輸出 opcode + i18n + CJK + 4 個文字類未實作 opcode(0x79/7E/7F/8F)。
5. 法術名對照附表(從 `33_MANUAL_TRANSCRIPTION` 法術表)。
6. R4 viewport/sprite/map;R5 戰鬥/法術/存讀檔;R6 Read Paragraph 內嵌 + 自包含 asset bundle。

## 工作慣例
- 每個改動開 feature branch → PR → 合併 main(使用者授權公開發佈;repo 為保存/中文化性質)。commit/PR 帶 Co-Authored-By 與 Claude Code 標記。
- 大量平行轉寫/分析用背景 agent(視覺轉寫掃描圖 > tesseract)。
- 不確定譯名/結論 → 列待確認給使用者裁決,不擅改。

## 關鍵檔案
- `opendw_remake/ARCHITECTURE.md`(重寫設計 + 驗證策略 + 階段表)
- `docs/07_REVISED_PLAN.md`(萃取修正計畫)、`docs/00_DOC_AUDIT.md`(文件審查)
- `docs/OPCODE_REFERENCE.md`(中英雙語 256-opcode)、`docs/25_OPCODE_INTERPRETATION.md`
- `docs/26_MONSTERS_AND_SPRITES.md`、`docs/08_READ_PARAGRAPH_FEATURE.md`
- `docs/15_TRANSLATION_DRAFT.md`、`CONTEXT.md`
- `docs/33/34/35/36/37`(手冊/段落/攻略轉寫)
- `tools_build/`(docker 工具 + patch + 渲染腳本)、`data/paragraphs/`
