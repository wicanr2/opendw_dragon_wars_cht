# OpenDW Remake — docs 索引(remake 級)

本目錄是 **remake 級文件樹**:引擎規格、玩法系統實作文件、圖 demo。
與專案級的[根 `../../docs/`](../../docs/README.md)分屬**兩棵 docs 樹**(分工見下)。

工程層總覽見 [`../README.md`](../README.md);設計 / 驗證策略見 [`../ARCHITECTURE.md`](../ARCHITECTURE.md)。

---

## 兩層 docs 分工(先讀這段避免混淆)

| 樹 | 範圍 | 編號區段 |
|----|------|----------|
| **根 [`../../docs/`](../../docs/README.md)** | **專案級**:逆向工程史 + 資料格式 + 攻略整合 + 翻譯資料 + PM / 稽核評估 | 00 / 01–08 / 10–14 / 20–26 / 30–40 / 41–61 / 99 |
| **本目錄 `opendw_remake/docs/`** | **remake 級**:引擎規格 + 玩法系統實作 + 圖 demo | 56–60 / CONTROLS / VIEWPORT / VIEWPORT_COMPOSE / REWRITE_READINESS / adr |

> ⚠️ **編號碰撞**:兩棵樹都有編號 **57 / 58 / 59 / 60**,**同號不同樹、不同內容**:
>
> | 編號 | 本樹(remake 級) | 根樹(專案級) |
> |------|------------------|---------------|
> | 57 | `57_DOORS_TRAPS_TERRAIN` 開門 / 陷阱 / 地形法術 | `57_PM_REVIEW` PM 完成度評估 |
> | 58 | `58_MAGIC_REFERENCE` 法術參考表 | `58_DOC_AUDIT_AND_DRIFT` 文件稽核 |
> | 59 | `59_SKILL_CHECK_TRIGGERS` 技能檢定觸發點 | `59_LAYOUT_FIDELITY` 版面保真 |
> | 60 | `60_DOS_VS_REMAKE_VISUAL` 視覺差異稽核 | `60_SKILL` Skill 經驗記錄 |
>
> **引用時務必帶完整路徑**(`opendw_remake/docs/…` 或從根目錄 `../docs/…`),避免指錯樹。本 repo 既有的 inbound 連結(根 `README.md`、`docs/58_DOC_AUDIT_AND_DRIFT.md`、`docs/media/README.md`)皆已使用完整路徑,故不重編號;若日後重編,需同步更新這些連結。

---

## 引擎 / 玩法系統文件

### 玩法系統實作(56–59)

| 檔案 | 說明 |
|------|------|
| [`56_PLAYABLE_ENDING_CHAIN.md`](56_PLAYABLE_ENDING_CHAIN.md) | 可通關結局鏈:打贏終戰 Namtar → 結局序列(收官);誠實標示 Boss / 結局 = remake 設計 |
| [`57_DOORS_TRAPS_TERRAIN.md`](57_DOORS_TRAPS_TERRAIN.md) | 探索互動深度:開門 / 破密門 / 陷阱 / 戰鬥外地形法術 |
| [`58_MAGIC_REFERENCE.md`](58_MAGIC_REFERENCE.md) | 法術效果參考表(對齊 fraterrisus 攻略) |
| [`59_SKILL_CHECK_TRIGGERS.md`](59_SKILL_CHECK_TRIGGERS.md) | 非戰鬥技能檢定觸發點(逆向結果) |

### 引擎規格 / 渲染

| 檔案 | 說明 |
|------|------|
| [`VIEWPORT.md`](VIEWPORT.md) | 進入遊戲後的第一人稱 viewport(與原版一致的計畫) |
| [`VIEWPORT_COMPOSE.md`](VIEWPORT_COMPOSE.md) | 第一人稱 viewport「組景」深度逆向 |
| [`60_DOS_VS_REMAKE_VISUAL.md`](60_DOS_VS_REMAKE_VISUAL.md) | 原版 DOS 與 remake 視覺差異稽核(第一人稱已 byte-for-byte,其餘人工視覺稽核) |
| [`CONTROLS.md`](CONTROLS.md) | 操作規範(與原版說明書一致)+ 完整 headless 測試旗標表 |

### 評估 / ADR

| 檔案 | 說明 |
|------|------|
| [`REWRITE_READINESS.md`](REWRITE_READINESS.md) | 重寫可行性評估 |
| [`adr/0002-two-layer-cjk-rendering.md`](adr/0002-two-layer-cjk-rendering.md) | ADR 0002:雙層渲染(像素層整數放大 + 高解析 TTF 文字層,內外解析度解耦) |

> ADR 0001(Asset Bundle 與 ResourceProvider)在根樹:[`../../docs/adr/0001-asset-bundle-and-resource-provider.md`](../../docs/adr/0001-asset-bundle-and-resource-provider.md)。

---

## 圖 demo 目錄

remake 自身管線 headless dump 的截圖,供文件 / README 引用與視覺稽核:

| 目錄 | 內容 |
|------|------|
| [`showcase/`](showcase/) | 代表性畫面(title / menu / map / sprite),README 門面用 |
| [`screenshots/`](screenshots/) | 系統畫面總集(含 `endgame/` 終戰與結局序列、養成 / 事件 / 三語) |
| [`dos_compare/`](dos_compare/) | DOS vs remake 三向對照:`dos/` 真機擷取、`remake/` dump、`sidebyside/` 並排 |
| [`layout_compare/`](layout_compare/) | 版面對照原始 dump |
| [`assessment/`](assessment/) | 640×480 模式各畫面(menu / combat / charsheet / para) |
| [`automap_demo/`](automap_demo/) | 俯視平面地圖(`?` 鍵)與 fog-of-war seeding |
| [`combat_screens/`](combat_screens/) | 戰鬥遭遇 / 回合 / 勝利(中 / 英 / 日) |
| [`paragraph_demo/`](paragraph_demo/) | Read Paragraph 段落檢視器(跨頁、三語) |
| [`party_demo/`](party_demo/) | 角色屬性表(中 / 英 / 日) |
| [`shop_recruit/`](shop_recruit/) | 商店購買 / 酒館招募 |
| [`wrap_demo/`](wrap_demo/) | wrap 樞紐世界圖(area 0 Dilmun) |

> 散在本目錄根的 `wm_world.png` / `wm_dragonvalley.png` 為世界圖 dump,供根 README 引用。
