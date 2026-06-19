# 47 — opendw_remake 重製評估報告(分項評分 + 可玩性 + 640×480/CJK 驗證)

> 日期:2026-06-15
> 對象:`opendw_remake/`(C++20 / SDL2 重製《火龍之戰》Dragon Wars, Interplay 1989)
> 對照基準(oracle):`opendw/`(Devin Smith 的 C 反組譯,唯讀)+ 原版 DOS / X68000 實機
> 方法:唯讀評估。docker `dwsdl` 跑 build / ctest / headless 截圖。本報告與 `docs/assessment/` 截圖為唯一新增產物,未改 src / CMakeLists / git。

---

## 0. 摘要

| 項目 | 分數 | 一句話 |
|------|:----:|--------|
| 渲染保真度 | **8.5 / 10** | viewport/sprite/scene/minimap 全 byte-for-byte 對拍 opendw;CJK 走獨立 TTF 層 |
| VM / bytecode 正確性 | **8 / 10** | 已實作 opcode 逐指令對拍 oracle 一致;戰鬥腳本 op_89 動作指派尚未跑通 |
| 戰鬥系統 | **7 / 10** | to-hit / 徒手傷害 / 武器骰 = bytecode 真值;武器 STR bonus = best-fit;怪物屬性對映暫定 |
| 法術 / 道具系統 | **8.5 / 10** | 法術 61 條 + 道具格式 grounded;施法結算為手冊 grounded 模型(非 bytecode 移植) |
| 內容 / 在地化 | **7 / 10** | 繁中近全覆蓋;日文 events 亮眼但其他層薄;段落 1–147 完整 |
| 可玩流程完整度 | **6 / 10** | 建角→探索→事件→戰鬥→存讀檔各環節可玩;缺連貫劇情與勝利結局 |
| 工程品質 | **8 / 10** | 自包含 bundle + 19 ctest 全綠 + 誠實標示;缺 remake 層獨立 CI |

**可玩性總分:62 / 100**(見 §8)

**640×480 + 24×24/16×16 CJK 判定:部分達成。** 24px 內文 / 16px UI 在 **scale=3(視窗 960×600)精確達成**(實測內文 23–24px、UI ink ≈10–16px);而 **640×N 視窗只在 scale=2(640×400)出現**,該倍率下字級降為內文 16px / UI ~10px。兩個目標(640 寬視窗、24px 字)目前**不在同一倍率**,且 320×200×整數倍永遠到不了 480 高(2×=400、3×=600)。詳見 §9 + 截圖。

---

## 1. 評估方法與證據基礎

- **建置**:`docker run --rm -v "$PWD":/app -w /app dwsdl bash -c "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"` — 乾淨建置成功。
- **測試**:`ctest`(build 目錄)→ **19 / 19 全綠**(初次跑出 not-run/failed 純為 stale CMakeCache 殘留舊掛載路徑 `/app/opendw_remake/build`,`rm -rf build` 重設後全綠)。
- **截圖**:headless(`SDL_VIDEODRIVER=dummy` + software renderer + `SDL_RenderReadPixels`)`--dump *.ppm`,再以 host PIL 轉 PNG 並逐列量測字高(dwsdl image 未內建 ImageMagick,故以 PIL 代替 ADR 0002 所述 `dwimg`,輸出等價)。
- **誠實分級**:本報告對每項數值區分四級 —— **bytecode 真值**(逐指令/byte 對拍 opendw)、**best-fit**(bytecode 有矛盾或無完整 oracle,取最合理值)、**remake 設計**(乾淨室,grounded 手冊)、**受限 demo**(僅 headless 路徑可達)。

ctest 清單(`opendw_remake/CMakeLists.txt`):vm_selftest、verify_save、verify_equipment、verify_combat、verify_combat_loop、verify_chargen、verify_spells、verify_combat_script、verify_encounter_golden_spider、verify_encounter_golden_wolf、verify_areaswitch、verify_op58、verify_i18n、smoke_app、render_sweep、verify_compose_l1、verify_fp_l1、verify_automap_l1、verify_seen_l1。

---

## 2. 渲染保真度 — **8.5 / 10**

remake 採**雙層渲染**(ADR 0002):像素層 320×200 indexed framebuffer 整數倍 nearest 放大;CJK / UI 文字走獨立 SDL2_ttf 高解析層疊上,永不縮放。像素層與 opendw 逐像素對拍,文字層與像素正確性正交。

| 子項 | 對拍程度 | 證據 | 缺口 |
|------|----------|------|------|
| 第一人稱 viewport(透視牆面) | **byte-for-byte** | `render/viewport.cpp`(5 分派函式 port 自 opendw `ui.c`)、遮罩表 verbatim;`verify_compose_l1`、`verify_fp_l1`、`render_sweep` 全 40 關 ×4 朝向 154 case viewport_memory PASS | 與右側 UI 面板「整幀」組合未對實機截圖 |
| sprite(怪物圖) | **byte-for-byte** | `render/sprite.cpp`;`verify_encounter_golden_spider/wolf` 對 oracle 160×136 PPM | 僅 2 樣本(蜘蛛/狼) |
| 全螢幕場景圖 | byte-for-byte(title) | `render/picture.cpp`(XOR delta + nibble);title golden | 非全部場景掃過 |
| 俯視地圖 + fog of war | **byte-for-byte**(area1) | `render/minimap.cpp`、`game/seen_map.cpp`;`verify_automap_l1`、`verify_seen_l1` | 全 40 關 minimap sweep 未入 ctest |
| CJK 文字層 | 視覺 + 像素正交 | `render/text_layer.cpp`(`TTF_RenderUTF8_Blended`)、`render/cjk_font.cpp`(備用點陣);像素層 sweep 154/154 不受影響 | 24×24 點陣 atlas 已停用改 TTF(設計遺留) |

**評語**:核心地城渲染(viewport / sprite / scene / minimap)達 byte-for-byte,且有 154 case 廣度掃描背書,這是本專案最硬的成就。扣分在 viewport + UI 邊框的整幀組合、全場景/全關 minimap 尚未納入自動對拍。原版有但 remake 未逐項驗的:遊戲進行中「viewport + 隊伍狀態欄 + 訊息列」同框構成。

---

## 3. VM / bytecode 正確性 — **8 / 10**

| 面向 | 狀態 | 證據 |
|------|------|------|
| opcode 實作數 | 已從 R1 初期 15/256 推進到約 **117/256**(`src/vm/interpreter.cpp` kImpl 表,batch 1–12) | grep `op_` 實作表 |
| 逐指令差異測試 | **成立且一致**:同段 bytecode 丟 opendw(oracle)與 remake VM,`(pc,op,r2,r4,flags,mode)` 逐指令比對一致 | `trace_remake`、`vm_selftest`(ctest)、README/READINESS 所述 `diff_trace.sh`(註:該 shell 腳本現於 repo 未見,屬文件參考漂移,但 trace harness 本身在用) |
| 覆蓋類別 | 模式/算術(含乘除 op_33–36)/旗標/邏輯/比較/跳轉/loop/game_state/bit/字串輸出/跨資源 call(op_58/59/5C)/角色資料皆有 | interpreter.cpp |
| 無 oracle 的 opcode | op_43 / 0x5F / 0x60 / 0x63 在 opendw 為 NULL(targets[] 裸名無實作);另約 22 個 NULL opcode。這些需 ASM/spec 自實作 + 人工驗,**不能純 diff** | READINESS §風險、docs/42 §5 |

**評語**:把「重寫」從賭一把變成可機械化驗證,是這專案方法論的核心價值,已端到端證明。扣分:(1) 戰鬥腳本路徑卡在 `op_89` 動作指派狀態機(res3@0x08b6 的逐角色完成標記未逆出),導致完整戰鬥 VM 在互動主迴圈跑不通;(2) README 仍寫「56/256」與 `diff_trace.sh`,與現況(117 opcode、腳本未見)有文件漂移。

---

## 4. 戰鬥系統 — **7 / 10**

| 公式 / 資料 | 分級 | 證據 |
|-------------|------|------|
| To-hit 命中(roll=1d16+3,HIT ⟺ roll ≤ 13+AV−(DV+AC)) | **bytecode 真值** | docs/42 §12;掃 AV/def 跑 res3 bytecode @0x0F73,5 case PASS;`combat.cpp` 常數 |
| 徒手傷害(dmg=骰+floor(STR/5),無 ×3/2) | **bytecode 真值** | docs/42 §11(自改碼修復後雙向確認);`combat.cpp` `str_damage_bonus` |
| 武器主傷害骰(descriptor=op_68 byte[8];sides 表 [d4..d100]) | **bytecode 真值** | docs/42 §13 + op_68 原始反組譯;descriptor 0x00/0x21/0x05/0xA3 對拍 |
| 武器 STR bonus | **best-fit** | self-modifying-code 矛盾、無完整戰鬥 oracle;保留 +floor(STR/5) 守 DOS 校準(`combat.cpp` 註明) |
| 怪物屬性對映(21 bytes blob → HP/AV/DV/骰/AC) | **remake 暫定** | opendw monster_info 未完整逆向;`combat.hpp` 逐欄註「暫定」 |
| 完整迴圈(4 人 vs 怪群、多回合、勝負、XP +80) | remake 設計(確定性) | `combat_loop.cpp`;`verify_combat`、`verify_combat_loop`、`verify_combat_script` |

**評語**:命中 / 徒手 / 武器骰三項已 bytecode 真值化,是難得的硬對拍成果;誠實標示武器 STR bonus 與怪物屬性為非 oracle。缺口:(1) 互動主迴圈無法走完一場戰鬥(op_89 卡點);(2) 無「整場戰鬥」的 oracle 基線 —— opendw C 碼本身無可獨立跑出逐回合 HP 的戰鬥輸出,故 `verify_combat_script` 明確只驗指令軌跡確定性、**不驗 HP**(CMakeLists 註)。原版有但 remake 缺/簡化:武器 STR 加成真值、怪物真實屬性、完整動作指派 UI。

---

## 5. 法術 / 道具系統 — **8.5 / 10**

- **法術(8/10)**:61 條(0x00–0x3C)涵蓋五大 school,9 種效果型 + 4 種控制類(`spells.cpp:29–172`)。效果值 grounded 手冊 docs/33,**非臆造**;但施法結算公式**非 opendw byte-for-byte 移植**(原版 bytecode 未逆出),屬 remake grounded 模型(檔頭誠實宣告)。`verify_spells` 驗表規模 / 效果值對拍手冊 / bitfield / 扣 Power / 確定性。缺:工具/召喚類(5 條)未數值結算、variable_power 倍率基準待校準(7 條)。
- **道具(9/10)**:23B/件 bit-packed 格式對齊 fraterrisus(docs/44 §2);`verify_equipment` 以真實 DATA1 的 7 件樣本 byte-grounded 對拍類型/AV/售價/名稱,13 格物品欄 + AC 聚合 PASS。缺:完整物品表(現 7 樣本足以驗格式)、magic_effect 功能未全落地。

**評語**:格式與表格層 grounded 紮實;扣分主要在「結算」深度(施法效果結算是模型而非 oracle)與「全表」廣度。原版有但 remake 缺:遊戲中使用物品(U)、施法在群戰的完整套用。

---

## 6. 內容 / 在地化 — **7 / 10**

| 語系 | 覆蓋 | 重點 |
|------|------|------|
| zh-TW | **~95%** | menu/chars/combat/items/spells 完整;**events 僅波卡城序盤 13 條**(完整遊戲 ~100+);段落 1–147 完整轉寫 |
| en | passthrough(設計) | tr() 查無回退英文源,非缺陷 |
| ja | **不均(8–81%)** | **events 13 條 100%(X68000 反萃取,亮點,docs/46)**;其他層 chars 8% / combat 15% / spells 20% / menu 20% |

譯名依上層 `CONTEXT.md`:法術 61 名、地名(波卡城/罪惡之城/瑪根地底世界/銀輪/陳屍所)、角色名(Namtar→納達、Irkalla→伊爾卡拉)皆對齊官方手冊。`verify_i18n` 驗 TSV 格式 / fallback 契約 / zh-TW 可載入(ja 缺項回退英文不算 FAIL)。

**評語**:繁中可玩層幾乎全覆蓋,段落書(防拷手冊)完整是大加分。扣分:事件文字僅序盤(完整遊戲缺 ~85%)、日文除 events 外多數薄。

---

## 7. 可玩流程完整度 — **6 / 10**

狀態機(`src/main.cpp`):S_MENU / S_BRANCH / S_GAME / S_COMBAT / S_MAP / S_CREATE。

**可實際互動玩(主迴圈)**:選單(B 新遊戲 / C 繼續)→ 建角(命名 + 配點 STR/DEX/INT/SPI + 性別,最多 4 員)→ 進波卡城第一人稱探索(I/J/K/L)→ 踩事件格看繁中事件文字(分頁 MsgViewer)/ Read Paragraph 長段落捲動(ParaViewer)→ 區域切換(sync_relocation,`verify_areaswitch` PASS)→ 角色屬性表 / 背包(V、1–4、E)→ 俯視地圖(?,fog of war)→ 存檔(S,`verify_save` byte-for-byte round-trip)/ 讀檔(C)→ F4 即時切繁中/EN/日。

**戰鬥(S_COMBAT)**:遭遇畫面(怪物圖 + 怪名 + 隊伍面板 + F 戰鬥 / R 逃跑 / C 施法)、群戰迴圈推進、傷害結算、勝負、XP 可玩。

**受限 / demo**:完整怪物戰鬥的「真實傷害結算閉環」因 op_89 動作指派卡點,主要在 headless / `--encounter` 路徑驗證;互動主迴圈進得去戰鬥但**走不完一場完整 oracle 對拍的怪物戰**。

**「完整一輪遊戲」還缺**:連貫劇情推進(quest flag / NPC 狀態機尚未成體系)、跨多區的主線、勝利條件 / 結局判定、戰鬥真實數值閉環、事件文字完整在地化(~85% 缺)。

**評語**:單看每個環節都「可玩」且在地化到位;但串成一條從建角到通關的主線尚未打通 —— 這是 demo 級可玩(體驗原版氛圍 + 序盤探索 + 存讀檔)與「完整 RPG」之間的差距。

---

## 8. 可玩性總分 — **62 / 100**

| 構面 | 權重 | 得分 | 加權 |
|------|:---:|:---:|:---:|
| 能不能進到遊戲、操作流暢(建角 / 探索 / 選單 / 存讀檔) | 25 | 21 | 21 |
| 探索 + 事件 + 段落體驗(第一人稱 + 繁中文字) | 20 | 16 | 16 |
| 戰鬥可玩迴圈(進入 / 指令 / 結束;真值化程度) | 20 | 11 | 11 |
| 在地化沉浸(繁中覆蓋、譯名一致) | 15 | 11 | 11 |
| 主線推進 / 結局(連貫劇情、勝利條件) | 20 | 3 | 3 |
| **合計** | 100 | | **62** |

**玩家視角總評**:現在啟動 `opendw_remake`,玩家能用**全繁中**介面建立 4 人隊伍,走進波卡城以**第一人稱**(牆面與原版逐像素一致)探索真實的 40 關地圖,踩到事件格讀到在地化劇情與防拷段落全文,翻角色表 / 背包、開俯視地圖、隨時存讀檔、按 F4 即切繁中 / 英 / 日。遭遇怪物能進戰鬥畫面、逐回合下令、看到勝負與經驗。**這已是一個可實際把玩、氛圍到位的序盤 demo**。但它**還不是能通關的完整 RPG**:戰鬥的真實數值閉環、跨區主線劇情、勝利結局尚未串通,事件在地化也只到序盤。定位是「**可驗證、誠實、序盤可玩的高保真重製基座**」,而非成品遊戲。

---

## 9. 640×480 + 24×24/16×16 CJK 嚴謹驗證

### 9.1 機制(讀碼確認)

- **像素層**:`render/framebuffer.hpp` `kW=320, kH=200`。`render/sdl_video.cpp:31` 視窗 = `kW*scale × kH*scale`;`SDL_HINT_RENDER_SCALE_QUALITY="0"`(nearest)+ 320×200 streaming texture → `SDL_RenderCopy(..., nullptr, nullptr)` 放大到全視窗。**確認:像素層為真整數 nearest 放大**,非任意縮放。
- **CJK / 文字**:`render/text_layer.cpp` 走 `TTF_OpenFont(ttf, px)` 以**原生字級**開字、`TTF_RenderUTF8_Blended` 抗鋸齒繪製,flush 時座標 `vx*scale` 定位、texture **1:1 blit 不縮放**(`text_layer.cpp:86`)。**確認:CJK 走 TTF 文字層原生繪製,非點陣縮放**(舊 24×24 atlas / `draw_half` 已停用,見 ADR 0002)。
- **字級公式**(`src/main.cpp:1027–1029`):`PX_TITLE=48*scale/3`、`PX_BODY=24*scale/3`、`PX_UI=16*scale/3`。預設 `scale=3`(`main.cpp:268`)。

→ 推論:**字級與 scale 線性綁定**,只有 **scale=3** 時 `PX_BODY=24`、`PX_UI=16`(命中目標);`scale=2` 時 `PX_BODY=16`、`PX_UI≈10`。

### 9.2 實測(headless dump + 逐列量測)

| 視窗倍率 | 視窗實際尺寸 | CJK 內文 ink 高 | UI tag ink 高 | 截圖 |
|---------|------------|----------------|--------------|------|
| `--scale 2` | **640 × 400** | ~15–16 px | ~7 px | `docs/assessment/menu_scale2.png`、`para_scale2.png` |
| `--scale 3` | **960 × 600** | **23–24 px** | ~10–16 px | `docs/assessment/menu_scale3.png`、`para_scale3.png` |

量測法:PIL 逐列偵測白色字素帶,取連續帶高度。scale=3 內文 6 條行帶實測 23/24/23/24/24/24 px → **與 `PX_BODY=24` 吻合**;scale=2 同段為 15/15/15/15/16 px → 與 `24*2/3=16` 吻合。標題「火龍之戰」scale=3 ink 高 45px(`PX_TITLE=48` 之 ink box,字面框略小於 point size 屬正常)。`[繁中]`(PX_UI=16)scale=3 ink ≈10px(該串為小字面 CJK,ink 小於 nominal,屬 TTF 正常現象)。

> 名詞註:TTF 的「字級」是 nominal point size,實際筆畫 ink box 通常略小於 nominal;故 24px 字級量到 23–24px ink、16px 字級量到 10–16px ink(視字面而定)皆為正確結果,不是縮水。

### 9.3 判定:**部分達成**

- ✅ **24px 內文 / 16px UI 字級:達成** —— 在 **scale=3(視窗 960×600)** 精確命中,且為 TTF 原生繪製(銳利、非點陣縮放)。
- ✅ **CJK 走文字層原生繪製、像素層真整數 nearest 放大:達成**(讀碼 + 截圖雙證)。
- ⚠️ **「640×480」視窗:未達成**。原因有二:
  1. **640 寬只在 scale=2 出現**,而 scale=2 時字級降為 16/10px —— 即「640 視窗」與「24px 字」**不在同一倍率**,無法同時滿足。
  2. **高 480 永遠到不了**:base 200 × 整數倍 = 400(2×)/ 600(3×),**沒有任何整數倍會等於 480**。現況是 **scale2=640×400、scale3=960×600**。

換言之:24×24 + 16×16 中文字目標在 **scale=3** 已達成,但那是 960×600 視窗;若把目標字面理解成「要在 640×480 視窗內呈現 24px 中文」,則目前**不成立**。

### 9.4 若要真 640×480 模式 — 最小改法建議(評估,不實作)

需求差異本質:**640×480 的長寬比是 4:3,而 320×200 是 8:5**。要剛好 640×480,有三條路(由小到大):

1. **letterbox / pillarbox(最小、不動 base)**:維持 320×200 base、scale=2 得 640×400,再把 640×400 置中貼到 640×480 黑底畫布(上下各 40px 黑邊)。字級需顯式設 scale=3 等級的 24/16px(把字級與 scale 解綁,改吃固定 px 參數)。改動:`sdl_video` 視窗開 640×480、`RenderCopy` 目標 rect 置中;`main.cpp` 字級常數改成固定值或新增 `--font-px`。**風險低**,但畫面非滿框(有黑邊),其實是「640×400 內容 + 黑邊」。
2. **320×240 base(改 base 解析度,最貼近原意)**:把 `kW/kH` 改為 320×240(或新增可選 base),×2 = 640×480 滿框。但原版 framebuffer 是 320×200,viewport / scene / sprite 全部對拍 200 高 —— 改 base 會**破壞既有 byte-for-byte 對拍**(viewport_memory 尺寸、blit 偏移、154 case golden 全需重做)。**風險高**,違背「像素層對齊原版」原則,不建議。
3. **像素層 200 高置中於 240 邏輯畫布**(折衷):像素層仍 320×200(對拍不破),但合成階段把它貼到 320×240 的合成 target 中央(上下 20 行留給 UI / 黑邊),整體 ×2 = 640×480。改動集中在 `sdl_video::compose`(新增 240 高合成 target + 偏移),字級解綁為固定 px。**風險中**,可同時得到 640×480 滿框 + 24/16px 字 + 不破壞像素對拍。

**建議**:若使用者要的是「640×480 視窗且中文 24px」,採**方案 3**(像素層 200 置中於 240 合成畫布 + 字級與 scale 解綁)。最小可行則方案 1(letterbox,但接受黑邊與非真滿框)。方案 2 不建議(代價是放棄已建立的渲染對拍資產)。

---

## 10. 缺口總表(對照原版,依優先序)

| 優先 | 缺口 | 分類 | 位置 |
|:---:|------|------|------|
| 高 | 戰鬥動作指派狀態機 op_89(res3@0x08b6)未逆出 → 互動戰鬥走不完一場 | 受限 | `docs/42` §14、`combat_loop` |
| 高 | 連貫劇情 / quest flag / 勝利結局未成體系 | remake 待做 | `main.cpp` 狀態機 |
| 高 | 事件文字在地化僅序盤 ~13/100+ | 內容 | `assets/i18n/*/events.tsv` |
| 中 | 武器 STR bonus(best-fit)、怪物屬性對映(暫定)無 oracle 真值 | best-fit | `combat.cpp/hpp` |
| 中 | 日文非 events 層覆蓋薄(8–20%) | 內容 | `assets/i18n/ja/` |
| 中 | viewport + UI 整幀組合、全關 minimap 未入自動對拍 | 渲染 | `render_sweep` |
| 低 | 法術工具/召喚類未數值結算、variable_power 倍率待校準 | remake 設計 | `spells.cpp` |
| 低 | README 文件漂移(56/256、diff_trace.sh 路徑)、remake 層無獨立 CI | 工程 | `README.md`、`.github` |

---

## 11. 發現需修的 bug / 問題(評估期間)

1. **(非 bug,環境)stale CMakeCache**:repo 內既有 `build/` 是以掛載路徑 `/app/opendw_remake` 配置的,直接在 remake 為根的掛載(`/app`)跑 ctest 會找不到執行檔(全 not-run)。`rm -rf build` 重配後 19/19 全綠。建議:`.gitignore` 已含 build,確保不要 commit `build/`;或 CI 一律 fresh configure。
2. **文件漂移**:`README.md` / `REWRITE_READINESS.md` 仍寫「VM 15/256 或 56/256」與「`bash tools_build/diff_trace.sh`」,但實際已約 117 opcode 且該 shell 腳本在 repo 未見。建議更新文件數字與驗證入口路徑(屬文件,非程式 bug)。

無發現會導致崩潰或資料損毀的程式 bug;smoke_app(headless 全模式不崩)+ 19 ctest 全綠。

---

## 附錄:截圖清單(`docs/assessment/`)

- `menu_scale2.png`(640×400)/ `menu_scale3.png`(960×600)— 在地化選單,大標題「火龍之戰」。
- `para_scale2.png`(640×400)/ `para_scale3.png`(960×600)— 段落 146 內嵌全文(CJK 內文字高量測來源)。
- `scale_compare_menu.png` — scale2(上,640×400)vs scale3(下,960×600)同畫面對比(視窗尺寸與字級差異一目了然)。
