# 右側隊伍狀態面板(party status panel)

進遊戲畫面 = 左側第一人稱 viewport(160×136 @ 16,8)+ **右側隊伍狀態面板** + 下方訊息。
本目錄記錄面板的資料來源、record 佈局、像素/文字分層與版面對拍。

## 預設隊伍來源

- 來源:DATA1 @ `0x2E26` 起,4 × 512B(`0x200`)player_record(opendw `player.c` 佈局)。
- 4 人:**Muskels / Theb / Elendil / Cheetah**;slot 4-6 為 `0x00`+`0xFF` 空槽(未含入隊伍)。
- 抽成自包含 bundle 資產:`assets/bundle/party/default_party.bin`(2048B),不依賴 DATA1。
- 載入:`game::Party::load_default(bundle_dir)`。

## player_record 欄位佈局(對照 opendw player.c)

| offset | 欄位 | 型別 |
|---|---|---|
| 0x00 | name | 高位元終止字串(除末位元組外皆 set bit7) |
| 0x0C/0x0D | strength / max | u8 |
| 0x0E/0x0F | dexterity / max | u8 |
| 0x10/0x11 | intel / max | u8 |
| 0x12/0x13 | spirit / max | u8 |
| 0x14/0x16 | health / max | u16 LE |
| 0x18/0x1A | stun / max | u16 LE |
| 0x1C/0x1E | power / max | u16 LE |
| 0x4C | status | bitfield(見下) |
| 0x4E | gender | u8(0 男 / 1 女) |
| 0x4F | level | u16 LE |
| 0x55 | gold | u32 LE |

status bitfield(對照 `unknown_1BC1[0..3]={02,04,80,01}` + `str_table_status`):
`0x01 dead` / `0x02 chained` / `0x04 poisoned` / `0x80 stunned`。

## 版面對拍(port 自 opendw engine.c)

`draw_player_status_panel`(0x1A72)→ `draw_player_status`(0x1ABD)→
`draw_player_stat`(6599)/`write_character_name`(0x1A40)。

原版 framebuffer 為線性 320×200(`get_line_offset(y)=y*320`),座標可直接映射到 remake 的 320×200 indexed framebuffer:

- 角色列頂 `y = i*0x10 + 0x20`(i=0..3 → y=32/48/64/80)。
- 名字 x:`draw_character` 用 `x<<3`,`x=0x1B` → 像素 216。
- 狀態條 x:`ui_draw_horizontal_line` 用 `x<<2` byte off、2px/iter,單位 = 4px;
  範圍 `0x36..0x4E` → 像素 216..312(寬 96px)。
- 三條(`draw_player_stat` y_adjust):HP `+8`、暈眩 `+0x0B`、法力 `+0x0E`,各畫雙掃描線。
- 條長度 = `(cur*23)/max + 1` 單位 × 4px;亮色段後補黑色至 312。
- 條色(`color_data[]={00,FF,CC,AA,99}&0x0F`):HP `0x0C`(亮紅)、暈眩 `0x0A`(亮綠)、法力 `0x09`(亮藍)。
- 異常狀態:原版於列頂 +8 顯示 `<name> is <status>`,不畫狀態條。

## 像素層 vs 文字層分工(雙層渲染規則)

- **像素層(Framebuffer)**:三條狀態條(HP/暈眩/法力)。
- **文字層(TextLayer TTF)**:角色名字(可 i18n;角色名通常英文)、異常狀態字。

## 驗證

- `verify_party_panel <bundle> [out.ppm]`:印出 4 角色解析欄位 + dump 像素層(狀態條)PPM。
- 完整畫面:`opendw_remake --map 1 --fp --font-ttf <wqy.ttc> --frames 1 --dump x.ppm`
  (headless 需 `SDL_VIDEODRIVER=dummy`)。
- 圖檔:
  - `ingame_fp.png` — 完整 in-game(viewport + 右側面板 4 角色 + 訊息)。
  - `party_pixel.png` — 像素層獨立 dump(狀態條位置/顏色)。
- 回歸:`vm_selftest` 全綠、`verify_fp` 4/4 byte-for-byte、`verify_compose` 4/4 — 像素層 viewport 不退化。

## 資料正確性註記

- Muskels(STR 21)、Theb(DEX 24)為戰士型 → power 0/0,面板只 2 條(HP+暈眩,無法力藍條)。
- Elendil(power 28)、Cheetah(power 26)為法系 → 面板 3 條。與 DATA1 原始數值一致。
