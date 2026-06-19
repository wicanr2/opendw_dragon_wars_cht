# 64 — X68000 .PKH 標題逆向:踩坑紀錄與已知事實(給後續 agent)

> 目的:X68000 標題(`TITLE.PKH`)的逆向經過四輪仍未完全還原。本檔把**已釘死的事實**(後續 agent 不必重推)、**屢次被推翻的錯誤假設**、**流程踩雷**集中記錄,避免重蹈覆轍。配合 `61_MULTIVERSION_ASSETS.md` §2 與 `tools_build/x68k_pkh_research/`。
>
> 現況(2026-06-19):**受阻**。PKH codec、GVRAM blit、標題載入呼叫點皆已逆出,標題**頂部 ~190px 可收斂**(天空+橄欖綠 banner),但龍/戰士藝術區 layout 對不上。X68000 主題的 title 仍回退 DOS res29。

---

## 1. 已釘死的事實(請直接採用,勿重推)

### PKH codec(已破,`tools_build/pkh_unpack.py`)
- control byte:低 7 bit = count;bit7 選 **run**(byte→word RLE)/ **literal**。
- literal:每 byte 拆 hi/lo nibble,各展成一個 16-bit word(= 4bpp 色號)。**literal count 單位是 nibble 數,不是 byte 數**(第三輪才修對;TITLE.PKH 解出 211,739 nibbles 吻合權威計數)。
- **全部 big-endian**(emu 確認)。

### GVRAM blit(已逆,#168 unicorn 驗證)
- **chunky-word**:每個 word **直接 = 4bpp 色號**,**非 plane 分離、不需 deinterleave**(第二輪推翻了「4-plane 交錯」假設)。
- **stride = 1024 words/line**;dst = GVRAM `0xC00000 + offset`、src = `0xD83000`;逐列複製 w 個 word × h 列;另有 2× 水平放大變體 `0x32bb6`。

### 標題載入鏈(已逆,#169)
- 呼叫點 **vaddr 0x708**:`_xunpack(圖號=0x80, x=0x48, y=0x1f, 0, 0)`(硬編 `#0x48`/`#0x1f` immediate)。
- 圖號→檔名表 **0x36388**(8-byte 記錄 → 字串表 `0x36587`:`TITLE.PKH\0SUBTTL.PKH\0…`);open 常式 `0x138a` 以 `id<<3` 索引。**id 0x80 = TITLE.PKH**(確認沒抽錯檔)。
- 呼叫鏈:`_xunpack → 0x11c4(open+read 整檔) → 0x27fa6(parse header) → 0x32c3c(核心解碼) → 0x2816a / 0x32b4c(GVRAM blit)`。
- parse 常式 `0x27fa6` 與 near-duplicate `0x2808c` **都從檔頭讀** `w@buf+0x34 / h@buf+0x36 / pal@buf+0x0c`;全檔對 w/h/pal 位址的寫入**只此兩處**。

### 反組譯定位輔助
- HU(Human68k `.X`)header 在 file `0x1400`;**`file_offset = 0x1440 + vaddr`**。
- 符號表 @ file `0x43eb8`(2-byte type `02 0x` + 4-byte BE addr + ASCIIZ name)。
- PKH 解壓器輸入指標走**全域 `0x6227c`**(不是 C-arg;直接呼叫餵不進輸入 —— 第一輪踩的坑)。

---

## 2. 屢次被推翻的錯誤假設(別再走這些死路)

| 輪 | 錯誤假設 | 實際 |
|---|---|---|
| #167 | .PKH = DOS Huffman codec | 不同 codec(run/literal nibble) |
| #167 | 解壓器是 `decompress(src,dst)` C 函式 | 從全域 `0x6227c` 讀輸入,stack args 餵不進 |
| #168 | GVRAM 是 4-plane 交錯,需 deinterleave | **chunky-word**(word=色號),stride 1024,不需 deinterleave |
| #169 | 有「圖號→w/h/palette 表」 | **不存在**;parse 從檔頭讀 w/h/pal |
| #169 | TITLE.PKH 檔頭 0x34/0x36/0x0c 有合理 w/h/pal | **是垃圾值**(emu 重現 w=0xfff1、h=0xf55f、pal=nibble pattern) |

**模式**:每一輪的靜態假設都被下一輪推翻。**教訓:X68000 這種複雜格式,靜態臆測屢錯 → 優先用「全程式模擬」(unicorn 跑真實 DRAGON.X 讓遊戲自己解出來),不要靠猜 w/h/layout。**

### 目前未解(下一步候選假設)
- **A**:TITLE.PKH 是別種 PKH header 變體 —— 資料從 offset 0 起、無 0x38 header(檔頭 `1f ff f1 f5 5f` 像已解 nibble);或 w/h 由呼叫端暫存器/別欄給,非 buf+0x34。
- **B**:blit 中途 GVRAM bank 切換 / 換寬(可解釋「頂部 190px 收斂、藝術區錯位」)。
- **最強路徑**:unicorn 全模擬 `_xunpack` 入口 → 跑完 → dump 模擬 GVRAM(0xC00000)→ 套程式設的 CLUT(`_apalin`/`_apalbrk`)。讓真實程式碼給出權威 w/h/palette/layout,不靜態猜。

---

## 3. 流程踩雷(footgun,務必避免)

1. **背景 sentinel 迴圈 → 空轉 16 小時**。曾有 agent 寫 `until [ -f /tmp/xxx ]; do sleep 10; done` 等一個從沒產出的檔,空轉一整夜。**嚴禁任何背景 sentinel / `&` detach 後輪詢;所有 docker 指令同步前景、等回傳再下一步。**
2. **誤入 plan mode → 0 tool use 空手退出**。spawn 後若進 plan mode 會什麼都不做就結束。**prompt 明確寫「直接執行、不要進 plan mode」。**
3. **伺服器暫時 rate limit**(非用量上限)會中斷 agent;**resume 它繼續、別留半成品掛著**;若再撞就稍等重試。
4. **有界**:這類深逆向設合理上限;**逆不出就誠實標受阻 + 精確記錄到哪一步**,不要無限嘗試或掛起。

---

## 4. 資產與工具
- 研究腳本:`tools_build/x68k_pkh_research/`(`pkh_unpack.py` codec、`emu_*.py` unicorn harness、`disasm.py`、`scan_callers.py`、`render_widths.py`、`key_routines.asm` 反組譯摘錄 + evidence PNG)。
- 原始遊戲檔(.DIM/DRAGON.X/TITLE.PKH)**一律不入庫**;只入庫工具 + 文件 + 解碼後資產。
- X68000 已成功抽的(未壓縮 .PIX):怪物 `MON.PIX` / portrait `PIC.PIX` / icon `ICON.PIX`(`61` §X68000),palette 為 DOS placeholder。
