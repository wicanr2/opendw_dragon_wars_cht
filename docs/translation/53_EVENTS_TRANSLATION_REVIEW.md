# 主線事件繁中 — 待審清單（語言權威裁決用）

> 本檔列出 `opendw_remake/assets/i18n/zh-TW/events.tsv` 主線擴充中**信心較低或固有名詞未入 CONTEXT.md** 的譯法，請使用者裁決。
> 譯名一律以 `CONTEXT.md` 為準；以下為 CONTEXT.md **尚未收錄**的新出現專名，我給了 provisional 音/意譯，**待確認**後再回填 CONTEXT.md。
> 來源：`docs/reverse-engineering/52_MAINLINE_EVENT_STRINGS.md`（bytecode 萃取）+ `docs/translation/15` 草表 + 攻略 `docs/walkthrough/38/39`。

## A. 固有名詞（CONTEXT.md 未收錄，provisional 待審）

| English | 暫譯（provisional） | 出處 / 備註 |
|---|---|---|
| Outlander(s) | 外鄉人 | 拜占儂/京雄城官員對玩家的稱呼。亦可考慮「外來者/化外之人」 |
| Mystalvision | 密斯塔維恩 | 背叛的太陽高階祭司（菲巴斯日光殿）。純音譯，待定 |
| Stosstrupen | 突擊隊 | 德文 Stoßtruppen＝突擊隊；遊戲設定為納達的精銳追捕部隊。可改音譯「史托斯特魯本」 |
| Utnapishtim | 烏特納比西丁 | 取自蘇美洪水神話人物（CONTEXT.md 標「保留原文待確認」）。音譯待定 |
| Apsu (waters) | 阿普蘇之水 | 蘇美神話原初淡水深淵 Abzu/Apsu；波卡城事件。音譯待定 |
| Zaton | 薩頓 | 立石銘文「Master Zaton」（CONTEXT.md 標「保留原文待確認」）。音譯待定 |
| Icarian Triumph Tavern | 伊卡里安凱旋酒館 | 自由港酒館招牌。Icarian 音譯待定 |
| King Drake | 德瑞克國王 | 京雄城肖像長廊。音譯待定 |
| Prince Jordan | 喬丹王子 | 同上。德瑞克之子、拜占儂的 |
| Myrilla / Myrolla | 蜜瑞拉／蜜蘿拉 | 雙胞胎公主。音譯待定（需區分兩名相近音） |
| Buck Ironhead | 鐵頭巴克 | 納達首席將軍（意譯姓 Ironhead＝鐵頭）。或全音譯「巴克·艾恩赫德」待定 |
| Slaveholder Mog / Isle of Forlorn | 奴隸主莫格 / 孤絕島 | 莫格宅院（area 37）繼承劇情。Forlorn 意譯「孤絕」待定 |
| King's Island / Quag | 國王之島 / 奎格 | 神祕林傳送樞紐選項（字串以 emit 拼接，原文 `...King's IslandQuag`）。Quag 音譯待定 |

## B. 譯法/語境待確認

| English（key） | 暫譯 | 疑點 |
|---|---|---|
| `Another baby killed.` | 又一隻幼龍被殺害了。 | 龍谷（area 32）語境推定 baby＝幼龍；若指人類嬰兒則須改。攻略語境偏「幼龍」 |
| `Do you wish to enter the Apsu waters` | 你要進入阿普蘇之水嗎 | 原文無問號（問號為後續 number-sink emit）。是否補「？」待審 |
| `north.` / `west.` | 北方。/ 西方。 | 多 emit 拼接的方位尾段（前段為「…位於」之類）；單獨成句時譯法可能突兀 |
| `A Lansk official smiles at you. ` | 一名蘭斯克官員對你微笑。 | 結尾有空白（對齊 emit），勿 trim |
| Universal God / Priest of the Universal God | 宇宙之神 / 宇宙之神的祭司 | 納達信仰體系；是否另有官方手冊譯名待查 |
| `Department of Lubrication."` | 潤滑事務部。」 | 拜占儂官僚機構戲謔招牌；直譯，語氣待確認 |
| Solarium | 日光殿 | 菲巴斯太陽神殿區；或譯「日光浴場/向陽廳」待定 |

## C. 多 emit 拼接鍵（技術說明）

部分事件文字由 VM **分多條 emit** 後在 `run_event` 以空白接起（如京雄城肖像長廊 = 「This is a gallery...On the facing wall of the 」+「gallery you see portraits...」）。
events.tsv **逐條 emit 為獨立 tr 鍵**（含結尾不完整的句子），因此單看某一條鍵的中文會像半句——**這是正常的**，實機顯示時會接成完整句。已逐條翻譯使拼接後語意連貫。

## D. 未納入（本輪不譯）

- `Read paragraph N`：防拷段落，內容在手冊/段落書（`data/paragraphs/`，ParaViewer 處理），非 events.tsv 範圍。
- op_79 gate 卡住而未 emit 的事件格（見 docs/reverse-engineering/52 halt 表）：補完 op_79 後可能 emit 新字串，屆時再萃取補譯。
- area 0 世界圖 op_6B gate 卡住格：app 走 `worldmap_dest` 靜態進城，非主線阻斷。
