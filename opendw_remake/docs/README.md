# OpenDW Remake — docs 索引(remake 級)

本目錄是 **remake 級文件樹**:引擎規格、玩法系統實作文件、視覺稽核、多版本素材、圖 demo。
與專案級的[根 `../../docs/`](../../docs/README.md)分屬**兩棵 docs 樹**(分工見下)。

工程層總覽見 [`../README.md`](../README.md);設計 / 驗證策略見 [`../ARCHITECTURE.md`](../ARCHITECTURE.md)。

---

## 兩層 docs 分工(先讀這段避免混淆)

| 樹 | 範圍 | 結構 |
|----|------|------|
| **根 [`../../docs/`](../../docs/README.md)** | **專案級**:逆向工程史 + 資料格式 + 攻略整合 + 翻譯資料 + PM / 稽核評估 | 扁平編號 00 / 01–08 / 10–14 / 20–26 / 30–40 / 41–61 / 99 |
| **本目錄 `opendw_remake/docs/`** | **remake 級**:引擎規格 + 玩法系統實作 + 視覺稽核 + 多版本素材 + 圖 demo | 子目錄分類 `gameplay/` `engine/` `audit/` `reference/` `media/` `adr/` |

本樹已由扁平編號改為**子目錄分類**(2026-06);原 56–61 / CONTROLS / VIEWPORT 等檔分入對應子目錄,檔名(含編號)保留。

> ⚠️ **編號碰撞**:兩棵樹都有編號 **57 / 58 / 59 / 60 / 61**,**同號不同樹、不同內容**:
>
> | 編號 | 本樹(remake 級,現路徑) | 根樹(專案級) |
> |------|---------------------------|---------------|
> | 56 | `gameplay/56_PLAYABLE_ENDING_CHAIN` 可通關結局鏈 | `56_*`(無;見根索引) |
> | 57 | `gameplay/57_DOORS_TRAPS_TERRAIN` 開門 / 陷阱 / 地形法術 | `57_PM_REVIEW` PM 完成度評估 |
> | 58 | `gameplay/58_MAGIC_REFERENCE` 法術參考表 | `58_DOC_AUDIT_AND_DRIFT` 文件稽核 |
> | 59 | `gameplay/59_SKILL_CHECK_TRIGGERS` 技能檢定觸發點 | `59_LAYOUT_FIDELITY` 版面保真 |
> | 60 | `audit/60_DOS_VS_REMAKE_VISUAL` 視覺差異稽核 | `60_SKILL` Skill 經驗記錄 |
> | 61 | `reference/61_MULTIVERSION_ASSETS` 多版本素材抽取 | `61_SHOP_AND_RECRUIT` 商店 / 招募 |
>
> **引用時務必帶完整路徑**(`opendw_remake/docs/<子目錄>/…` 或從根目錄 `../docs/…`),避免指錯樹。

---

## `gameplay/` — 玩法系統實作

| 檔案 | 說明 |
|------|------|
| [`gameplay/56_PLAYABLE_ENDING_CHAIN.md`](../../docs/gameplay/56_PLAYABLE_ENDING_CHAIN.md) | 可通關結局鏈:打贏終戰 Namtar → 結局序列(收官);誠實標示 Boss / 結局 = remake 設計 |
| [`gameplay/57_DOORS_TRAPS_TERRAIN.md`](../../docs/gameplay/57_DOORS_TRAPS_TERRAIN.md) | 探索互動深度:開門 / 破密門 / 陷阱 / 戰鬥外地形法術 |
| [`gameplay/58_MAGIC_REFERENCE.md`](../../docs/gameplay/58_MAGIC_REFERENCE.md) | 法術效果參考表(對齊 fraterrisus 攻略) |
| [`gameplay/59_SKILL_CHECK_TRIGGERS.md`](../../docs/gameplay/59_SKILL_CHECK_TRIGGERS.md) | 非戰鬥技能檢定觸發點(逆向結果) |

## `engine/` — 引擎規格 / 渲染 / 操作 / 評估

| 檔案 | 說明 |
|------|------|
| [`engine/VIEWPORT.md`](../../docs/engine/VIEWPORT.md) | 進入遊戲後的第一人稱 viewport(與原版一致的計畫) |
| [`engine/VIEWPORT_COMPOSE.md`](../../docs/engine/VIEWPORT_COMPOSE.md) | 第一人稱 viewport「組景」深度逆向 |
| [`engine/CONTROLS.md`](../../docs/engine/CONTROLS.md) | 操作規範(與原版說明書一致)+ 完整 headless 測試旗標表 |
| [`engine/REWRITE_READINESS.md`](../../docs/engine/REWRITE_READINESS.md) | 重寫可行性 / 就緒度評估 |

## `audit/` — 視覺稽核

| 檔案 | 說明 |
|------|------|
| [`audit/60_DOS_VS_REMAKE_VISUAL.md`](../../docs/assessment/60_DOS_VS_REMAKE_VISUAL.md) | 原版 DOS 與 remake 視覺差異稽核(第一人稱已 byte-for-byte,其餘人工視覺稽核) |
| [`audit/dos_compare/`](../../docs/assessment/dos_compare) | DOS vs remake 三向對照圖:`dos/` 真機擷取、`remake/` dump、`sidebyside/` 並排 |

## `reference/` — 多版本素材

| 檔案 | 說明 |
|------|------|
| [`reference/61_MULTIVERSION_ASSETS.md`](../../docs/reference/61_MULTIVERSION_ASSETS.md) | Amiga / X68000 等多版本素材的抽取方法、格式與受阻清單 |

## `adr/` — 架構決策紀錄

| 檔案 | 說明 |
|------|------|
| [`adr/0002-two-layer-cjk-rendering.md`](../../docs/adr/0002-two-layer-cjk-rendering.md) | ADR 0002:雙層渲染(像素層整數放大 + 高解析 TTF 文字層,內外解析度解耦) |

> ADR 0001(Asset Bundle 與 ResourceProvider)在根樹:[`../../docs/adr/0001-asset-bundle-and-resource-provider.md`](../../docs/adr/0001-asset-bundle-and-resource-provider.md)。

---

## `media/` — 圖 demo

remake 自身管線 headless dump 的截圖,供文件 / README 引用與視覺稽核:

| 目錄 / 檔案 | 內容 |
|------|------|
| [`media/showcase/`](../../docs/media/remake/showcase) | 代表性畫面(title / menu / map / sprite),README 門面用 |
| [`media/screenshots/`](../../docs/media/remake/screenshots) | 系統畫面總集(含 `endgame/` 終戰與結局序列、養成 / 事件 / 三語) |
| [`media/assessment/`](../../docs/media/remake/assessment) | 640×480 模式各畫面(menu / combat / charsheet / para) |
| [`media/automap_demo/`](../../docs/media/remake/automap_demo) | 俯視平面地圖(`?` 鍵)與 fog-of-war seeding |
| [`media/combat_screens/`](../../docs/media/remake/combat_screens) | 戰鬥遭遇 / 回合 / 勝利(中 / 英 / 日) |
| [`media/paragraph_demo/`](../../docs/media/remake/paragraph_demo) | Read Paragraph 段落檢視器(跨頁、三語) |
| [`media/party_demo/`](../../docs/media/remake/party_demo) | 角色屬性表(中 / 英 / 日) |
| [`media/shop_recruit/`](../../docs/media/remake/shop_recruit) | 商店購買 / 酒館招募 |
| [`media/wrap_demo/`](../../docs/media/remake/wrap_demo) | wrap 樞紐世界圖(area 0 Dilmun) |
| [`media/wm_world.png`](../../docs/media/remake/wm_world.png) · [`media/wm_dragonvalley.png`](../../docs/media/remake/wm_dragonvalley.png) | 世界圖 dump(供根 README 引用) |
