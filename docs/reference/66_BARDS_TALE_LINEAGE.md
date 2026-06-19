# 66 — 《吟遊詩人傳說》(The Bard's Tale)與《火龍之戰》(Dragon Wars)的血緣脈絡

本文釐清 Interplay 三部曲《吟遊詩人傳說》(The Bard's Tale)與《火龍之戰》(Dragon Wars, 1989)之間的關係,並說明這條血緣為何與本 remake 專案(opendw / opendw_remake)直接相關。每個關鍵主張都附來源並標示信心。

## 一句話結論

《火龍之戰》是《吟遊詩人傳說》的精神續作:它在發行前一個月之前都叫《吟遊詩人傳說 IV》(Bard's Tale IV),因為「Bard's Tale」這個名稱的商標權握在美商藝電(Electronic Arts, EA)手上,而非開發商 Interplay,為避免授權費才在最後關頭改名換背景、改由 Activision(當時品牌名 Mediagenic)發行。程式設計師 Rebecca "Burger Becky" Heineman 同時是三部曲與《火龍之戰》的核心程式,而本 remake 反組譯的對象正是 Heineman 1989 年的《火龍之戰》原版引擎 —— 血緣在此呼應。(信心:高)

## 時間線

| 年份 | 作品 | 開發 | 發行 | 設計 / 主創 | 主程式 |
|---|---|---|---|---|---|
| 1985 | The Bard's Tale(原名 *Tales of the Unknown, Volume I*) | Interplay | Electronic Arts(北美) | Michael Cranford(設計+程式) | Michael Cranford |
| 1986 | The Bard's Tale II: The Destiny Knight | Interplay | Electronic Arts | Michael Cranford | Michael Cranford |
| 1988 | The Bard's Tale III: Thief of Fate | Interplay | Electronic Arts | Rebecca Heineman、Michael A. Stackpole(劇本/地圖)、Brian Fargo 等 | Rebecca Heineman |
| 1989 | Dragon Wars | Interplay | Activision / Mediagenic | Paul O'Connor(劇本設計,130 頁設計文件)、Rebecca Heineman | Rebecca Heineman |

說明:
- Cranford 主導前兩作的設計與程式,完成第二作後離開 Interplay 去研讀哲學與神學,故未參與第三作。(信心:高)
- 第三作《Thief of Fate》改由 Heineman 任主程式、Stackpole 寫劇本與地圖。Heineman 表示該作原本想叫 *Tales of the Unknown - Volume III: The Thief's Tale*。(信心:高)
- 《火龍之戰》北美 Apple II 版約 1989 年 10 月推出,後續移植 C64、MS-DOS、Amiga、Apple IIGS;日本另有 1991 年 Famicom(Kemco)、PC-98、X68000 版。(信心:高)

## 核心關聯:《火龍之戰》到底是不是「Bard's Tale IV」

這是本文最重要、也查證最充分的一點。多個來源(英文維基、Heineman 本人受訪)一致指出:

> 直到發行前一個月,本作都以《Bard's Tale IV》之名開發;但該標題的權利仍由 Electronic Arts 持有,因此需要一個新標題與新設定。
> — English Wikipedia, *Dragon Wars*

Heineman 在 Game Developer(原 Gamasutra)專訪中親述商標歸屬:

> 「我們和 Bard's Tale 的出版合約是這樣:即使我們擁有程式碼、劇本等等,『Bard's Tale』這個名稱的商標權是 Electronic Arts 的。」
> — Rebecca Heineman, *Game Developer* 專訪

關鍵細節(信心:高):
- **商標 vs 著作權分離**:Interplay 擁有引擎程式碼與遊戲劇本(scenario),但 EA 擁有「Bard's Tale」這個品牌名稱的商標。這是當年「開發商做、發行商掛名持商標」的典型結構。
- **為省授權費而改名**:為避免向 EA 支付授權費,遊戲在接近完成時改名 Dragon Wars,並重寫故事以塞進一條龍。Heineman 自陳得「在最後關頭生出一個有龍的故事」,而成品其實龍的戲份很少。
- **改由 Activision 發行**:既已脫離 Bard's Tale 品牌,發行也轉到 Activision(當時對外品牌為 Mediagenic),不再是 EA。多數來源寫「發行商 Activision」;部分敘述細分為「Interplay 出品、Activision/Mediagenic 經銷」。兩種說法不矛盾,差別在用「publisher」還是「distributor」描述同一段 Interplay–Mediagenic 經銷合作。(信心:中高;publisher/distributor 用詞各源略有出入)
- **行銷仍明示血緣**:Interplay 廣告打出「Bard's Tale Fans Rejoice!」,並主打《火龍之戰》可匯入《吟遊詩人傳說》三部曲的角色。Heineman 等人認為本作比《Bard's Tale III》更好、甚至優於整個三部曲,但少了舊招牌與 EA 的行銷火力,銷售未及預期。(信心:高)
- **設計上的融合**:《火龍之戰》被描述為《吟遊詩人傳說》與《廢土》(Wasteland, 1988)兩條設計脈絡的融合 —— 前者給介面與第一人稱地城骨架,後者給更開放的敘事/技能與「段落書」(paragraph book)式劇情。(信心:高)

**「精神續作」這個說法成立嗎?** 成立,且有 Heineman 本人佐證的事實基礎(本來就是 BT IV、同一批人、可匯入角色、共用設計語彙),不只是後人的浪漫化標籤。唯一需要小心的是別把它講成「官方第四部 Bard's Tale」—— 因為商標問題,它在法律與品牌意義上是獨立 IP。(信心:高)

## 引擎血緣

- 「Bard's Tale 引擎」由 Michael Cranford 為初代打造,Interplay 之後沿用/改寫於數款作品。一般歸入此引擎家族的有 Bard's Tale I/II/III 與 Dragon Wars(Wasteland 常被一併討論為同期同團隊的設計脈絡,但引擎沿用程度的精確界定各源不一)。(信心:中)
- Heineman 在初代《吟遊詩人傳說》的具名貢獻是「資料壓縮常式」(讓 Cranford 能塞進大量圖像與動畫),並自述寫了圖形編輯器等開發工具、以及各平台移植。換言之,從初代起,引擎層級的關鍵技術(壓縮、工具鏈、移植)就有 Heineman 的手筆;到第三作與《火龍之戰》她更直接任主程式。(信心:高)
- 因此《火龍之戰》與《吟遊詩人傳說》在引擎上不是「無關的重寫」,而是同一條技術脈絡延續到 Heineman 主導下的成熟形態。「程式碼歸 Interplay 所有」這點(Heineman 受訪明言)也支持引擎為延續而非另起爐灶。(信心:中高;「逐模組共用比例」無逐行公開佐證,故不宣稱 byte 級沿用)

## 與本 remake 的連結

本專案的反組譯基準 **opendw** 由 Devin Smith 製作,對應的是 **Rebecca Heineman 1989 年《火龍之戰》16-bit x86 real-mode 組語原版**(見 `63_OPENDW_VS_REMAKE_ARCH.md`)。opendw_remake 則以 opendw 為正確性 oracle,做 C++20/SDL2 乾淨室重寫 + 繁中化。

血緣呼應在此收束:本專案逐指令研究的這套 1989 引擎,出自同一位從初代《吟遊詩人傳說》就在做壓縮與工具、一路寫到《火龍之戰》的程式設計師之手。研究《火龍之戰》引擎,等於在研究 Bard's Tale 技術脈絡的成熟末端。(信心:高)

## 待考 / 信心較低處

- **publisher vs distributor 的精確措辭**:Activision 與 Mediagenic 是同一公司的不同時期品牌名;各來源對「Interplay/Activision/Mediagenic 誰是 publisher、誰是 distributor」用詞不一。本文採「Interplay 開發、Activision(Mediagenic 品牌)發行/經銷」,細分措辭待更原始的合約級資料佐證。(信心:中)
- **引擎逐模組沿用比例**:可確認是同一技術脈絡延續,但沒有公開的逐行/逐模組 diff 證明 BT 與 DW 共用多少程式。不宣稱 byte 級沿用。(信心:中)
- **Wasteland 引擎關係**:Wasteland(1988)與本脈絡同團隊、同期,設計影響明確;但「是否共用同一引擎程式」各源界定不一,本文僅述「設計脈絡融合」,不斷言引擎同源。(信心:中)
- CRPG Addict 原文(primary-ish,含 Heineman 受訪連結)直接抓取回 403,本文相關引述以維基與 Game Developer 專訪為準。

## 來源清單(URL)

- Dragon Wars — English Wikipedia: https://en.wikipedia.org/wiki/Dragon_Wars
- The Bard's Tale (1985) — English Wikipedia: https://en.wikipedia.org/wiki/The_Bard%27s_Tale_(1985_video_game)
- The Bard's Tale III: Thief of Fate — English Wikipedia: https://en.wikipedia.org/wiki/The_Bard%27s_Tale_III:_Thief_of_Fate
- Rebecca Heineman — English Wikipedia: https://en.wikipedia.org/wiki/Rebecca_Heineman
- Michael Cranford — English Wikipedia: https://en.wikipedia.org/wiki/Michael_Cranford
- "Bard's Tale Co-Creator Heineman Lays Out Series' Demise" — Game Developer(原 Gamasutra)專訪: https://www.gamedeveloper.com/design/-i-bard-s-tale-i-co-creator-heineman-lays-out-series-demise
- Dragon Wars — MobyGames: https://www.mobygames.com/game/2026/dragon-wars/
- Dragon Wars — C64-Wiki: https://www.c64-wiki.com/wiki/Dragon_Wars
- The CRPG Addict, "Game 100: Dragon Wars (1989)": http://crpgaddict.blogspot.com/2013/06/game-100-dragon-wars-1989.html(抓取 403,僅列為延伸閱讀)
- 本專案內部交叉參考:`opendw_remake/docs/reference/63_OPENDW_VS_REMAKE_ARCH.md`(opendw = Devin Smith 反組譯,對應 Heineman 1989 原版)

---
查證日期:2026-06。所有事實主張以上列來源為據,矛盾處已並陳並標信心;未獲可靠來源者標「待考」。
