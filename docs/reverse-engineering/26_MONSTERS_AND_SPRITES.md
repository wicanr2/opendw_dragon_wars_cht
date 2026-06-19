# 怪物名稱與 Sprite 抽取結果

> **日期**：2026-06-10
> **工具**：自建 `resextract` / `monster_info` / `sprite_dump`(docker，見 `tools_build/`)
> **資料來源**：DATA1 → resource 31(怪物字串，壓縮，解壓後 2177 bytes）+ 各怪物 sprite 資源

---

## 一、重要修正(對先前文件)

| 先前說法 | 實際 |
|----------|------|
| `monsters.txt` 的 168/196/200/210/222 是「怪物名稱」清單 | ❌ 那是怪物 **sprite 圖形資源編號**,不是文字 |
| 怪物名稱要從 DATA1 section 0x15/0x16 萃取 | ❌ 怪物名稱在 **resource 31**(壓縮),用偏移表 + 5-bit 解碼 |
| DATA2 可能含文字 | ❌ DATA2 全為壓縮圖形/地圖/音效,**無明文**;翻譯只需動 DATA1 |

DATA1 / DATA2 分工(已驗證):
- **DATA1**：所有 script(內嵌對話)、character data、怪物字串(res31)、關卡(res71…)、標題(res29)、以及多數圖形資源。**翻譯只需處理 DATA1。**
- **DATA2**：resource 0x17 以後的高編號資源(地圖視埠、sprite、PCM 音訊),header 前 23 項為 `0xFFFF`(代表在 DATA1)。

---

## 二、怪物名稱(從 res31 解出)

走訪 res31 怪物記錄(每筆 record:0x21 bytes 屬性 + 變長 5-bit 名字,名字在 record+0x21):

| 名稱(英) | record offset | record byte[0x0B]+0x8A |
|-----------|---------------|------------------------|
| Robber | 0x022c | 169 |
| King's Guard | 0x0254 | 152 |
| Guard | 0x0259 | 138 |
| Soldier | 0x0281 | 152 |
| Bandit | 0x02aa | 169 |
| Loon | 0x02fd | 180 |
| Fanatic | 0x0324 | 180 |
| Yonderboy | 0x034d | 180 |
| Born Loser | 0x0377 | 180 |
| Unjustly Accused | 0x03a3 | 169 |
| Giant Spider | 0x0400 | 167 |
| Wild Dog | 0x042d | 153 |
| Spider | 0x0457 | 167 |
| Cannibal | 0x047f | 169 |
| Big Dog | 0x04a9 | 153 |
| Wild hound | 0x04d3 | 153 |
| Rock Spider | 0x04fe | 167 |
| Jail Keeper | 0x057c | 152 |
| Drunk | 0x05d5 | 169 |
| Humbaba | 0x05fd | 178 |
| Gladiator | 0x0624 | 152 |

名字含單複數 escape(0xAF/0xDC):如 `Rock Spider` / `Rock Spiders`。record 表尾(>0x6a8)為其他資料,解出為雜訊,已濾除。

> 注意:record byte[0x0B]+0x8A 推出的 sprite 編號與 `monsters.txt` 驗證值有偏差(例:Spider 推為 167,但 monsters.txt 標 196 才是正確 sprite)。**名稱清單可信**;**name→sprite 精確對應仍需逐一視覺核對**(用下方 contact sheet)。

---

## 三、Sprite 抽取(已可渲染成 PNG)

`sprite_dump <res_index> <out.ppm>` 走遊戲原生的 `show_random_encounter` 繪圖路徑(`draw_random_encounter_graphic`),把 sprite 畫進 viewport memory(160×136,nibble-per-pixel,DOS 16 色盤)再輸出。

### 已確認命名(來自 monsters.txt,視覺核對無誤)
| 資源 | 怪物 | 檔案 |
|------|------|------|
| 168 | Wolf(灰狼) | `monster_sprites/168_Wolf.png` |
| 196 | Spider | `monster_sprites/196_Spider.png` |
| 200 | Innocent Man(藍衣襤褸人) | `monster_sprites/200_Innocent_Man.png` |
| 210 | Pikeman(持矛裝甲兵) | `monster_sprites/210_Pikeman.png` |
| 222 | Fanatic | `monster_sprites/222_Fanatic.png` |
| 152 | Guard / Soldier | `monster_sprites/152_Guard_Soldier.png` |

### 全量掃描
資源 138–240 範圍內,**59 個** sprite 有實際內容(其餘為空/背景)。全部存於 `monster_sprites/all/res_NNN.png`,總覽圖 `monster_sprites/monster_contact_sheet.png`。相鄰編號常為同怪物的動畫格或同圖重複。

---

## 四、可重現建置(docker)

先前 cht 版 `engine.c` / `ui.c` 有編譯錯誤(前向宣告缺失、`draw_right_pillar` 重複定義、`sub_2AEE`/`op_43/4C/4D/55/5B/5F/60/62/63` 未定義),已用最小 shim 修正(見 `tools_build/`)。

```bash
# 1. 取得 data1 (從 DOS 軟碟映像)
7z e "Dragon Wars (1990).zip"            # → DISK01.IMA
7z e DISK01.IMA DATA1 -o.                 # FAT12 內取出 DATA1
mv DATA1 data1

# 2. 建工具 image
docker build -t dwtools -f tools_build/Dockerfile.dwtools tools_build/

# 3. 編譯(掛載 opendw 原始碼 + shim)、執行
#    resextract -i 31 -o res31.bin     # 解壓 res31(怪物字串)
#    sprite_dump 168 wolf.ppm          # 渲染怪物 sprite
```

> sprite 背景色為 `0x66`(DOS 棕色),非透明 key;真正遊戲中 sprite 疊在 viewport 上靠 and/or mask 表做遮罩混合,不是單色透明(見 §五的歷史對照)。

---

## 五、重大修正:DATA2 資源讀取(2026-06-10 補)

**問題**:原始 `resource.c` 寫死只讀 `data1`。但 89 個資源**只存在 DATA2**(DATA1 header 對應項為 `0xFFFF`),其中包含**大量怪物 sprite**(152/153/170/178/180/198/212/224/236/248…)。原工具讀這些編號時,因 0xFFFF 未被正確處理,會**讀到 DATA1 相鄰區段的錯誤資料** → 渲染出錯圖(例:152/153 渲染成同一張)。

**修正**:patch `resource.c`(見 `tools_build/resource_data2.patch`)— 載入 data2 header,當 DATA1 header[sec] ≥ 0xFF00 時改從 data2 算 offset 載入。修正後:
- 怪物 sprite **完整且正確**:範圍內 59 個有內容資源,涵蓋藍魔/骷髏/綠龍/洋紅騎士/紫魅/綠蟾/哥布林弓手/鹿怪…完整圖鑑。
- 總覽圖:`monster_sprites/d2_sheet1.png`、`d2_sheet2.png`(DATA2 修正後)。
- `monster_sprites/all/res_NNN.png` 已全部以修正版覆蓋。

## 六、全螢幕場景圖(320×200,新發現)

6 個資源解壓後正好 32000 bytes = 320×200 全螢幕圖(nibble-per-pixel,16 色),用 `title_build` 格式(非 encounter sprite,非怪物):

| 資源 | 內容 |
|------|------|
| 24 | 結局:Namtar 被埋回深淵 |
| 25 | 結局:Namtar 慘叫死亡 |
| 26 | 結局:城堡/大地場景 |
| 27 | 結局:和平時代「And may that time never pass」 |
| 28 | 「The End」字卡 |
| 29 | 標題畫面(RESOURCE_TITLE3) |

存於 `docs/scene_pictures/fullscreen_NN.png`。**DOS 版只有這 6 張全螢幕圖,全是片頭/結局過場。**

> **渲染關鍵(2026-06-10 修正)**:全螢幕圖為**垂直 XOR delta 交錯格式**,渲染前必須先做 `title_adjust` 還原(對照 opendw `main.c`),否則出現紅/青交錯條紋、配色錯亂。已用 `tools_build/scene_render.py` 重渲染並與玩家提供的參考圖核對一致(Namtar 結局:青天背景 + 紅色 Namtar)。

## 七、關於「Magan Underworld」場景圖(使用者回報缺失)

使用者提供的 `org_dialogue/images.jpeg`(坐姿紅衣人物 + Read paragraph 137)**不屬於上述任一類**:
- 不是戰鬥 encounter sprite(全 0–383 掃描無此圖)。
- 不是 6 張全螢幕過場圖。

研判為**地點場景圖**,畫在 viewport 區、由 viewport 圖塊(resources.md 標的 110 Castle wall / 111 Sky / 112 Road / 116 Water 等「viewport」資源)經 `decode_viewport_data` / `draw_graphic_to_viewport` **合成**而來,而非單一資源。要抽出需實作 viewport 場景組譯器(ppm.cpp 的 `process_ground` 是此方向雛形,但其呼叫的 `sub_CF8` 在現版已改名 `draw_graphic_to_viewport`,需改寫)。**亦不排除該截圖來自 Amiga/Apple IIGS 版**(美術較精細),需與 DOS 版交叉確認。

→ 待辦:實作 viewport 場景組譯器,渲染 Magan Underworld 等地點場景。

## 八、待辦
- [ ] 實作 viewport 場景組譯器(抽 Magan Underworld 等地點圖)
- [ ] 逐一視覺核對 name ↔ sprite 精確對應
- [ ] 翻譯怪物名稱(繁中)
- [ ] 釐清 res31 byte[0x0B] → sprite 資源的正確換算(+0x8A 有偏差)
