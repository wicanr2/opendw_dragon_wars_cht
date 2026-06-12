# 重寫可行性評估(Rewrite Readiness)

> **日期**:2026-06-12
> **問題**:現在可以正式開始重寫 opendw_remake 了嗎?
> **結論**:**可以**。驗證策略已端到端證明,重寫的每一步都可機械化驗證,風險已大幅降低。

---

## 為什麼是「可以」

重寫一個遊戲引擎最大的風險是「**改完不知道對不對**」。這個風險今天已經解除:

1. **R0 資產層 — byte-for-byte 對拍 opendw**:`archive + text_codec + decompress`(含 DATA2 修正)解出的資源,與 opendw `resextract` 的輸出**逐位元組相同**(res31 怪物字串、res168 sprite 驗證通過)。
2. **R1 VM 核心 — 逐指令對拍 opendw**:差異測試 harness 已成立 —— 同一段 bytecode 丟進 opendw(oracle)與 remake VM,`(pc,op,r2,r4,flags,mode)` 逐指令比對**完全一致**。一鍵 `bash tools_build/diff_trace.sh` 從零重建兩邊並 diff。

→ 意義:之後**每加一個 opcode、每寫一個模組,都能自動證明「行為等同原版」**。這把「重寫」從「賭一把」變成「可驗證的工程」。

---

## 目前已就緒

| 層 | 狀態 |
|----|------|
| 資產層(archive/text_codec/decompress) | ✅ R0,byte-for-byte == opendw |
| VM 核心(VmState/dispatch/trace) | ✅ R1,15/256 opcode |
| 差異測試基礎設施 | ✅ 一鍵 diff_trace,逐指令對拍 |
| 可重現 docker 工具鏈 | ✅ build_opendw_tools / build_trace_oracle |
| 資產來源(scripts/sprites/maps/text/段落/翻譯) | ✅ 已萃取於上層 repo |

---

## 路線圖(每階段都 diff 對拍)

| 階段 | 內容 | 驗證方式 |
|------|------|----------|
| R1 餘 | 補齊其餘可對拍 opcode(資料/旗標/跳轉/角色資料/game_state) | diff_trace(純資料流) |
| R2 | render:framebuffer + SDL2 pixel scaling + 8×8 字 + title | golden 畫面 hash |
| R3 | 字串輸出 opcode + i18n + CJK 24×24 + 文字類未實作 opcode(0x79/7E/7F/8F) | golden(中/英) |
| R4 | viewport(3D 地城)+ sprite + map 移動 | golden 地城畫面 |
| R5 | 戰鬥/法術/角色/存讀檔 | diff_trace 整場戰鬥 |
| R6 | Read Paragraph 內嵌 + 自包含 asset bundle | 功能 + 不依賴原始磁碟檔啟動 |

---

## 風險與誠實揭露

1. **opendw 本身不完整,部分 opcode 無 oracle**:
   - opendw 未實作 **op_43 / 0x5F / 0x60 / 0x63**(targets[] 引用裸名但無實作);加上判讀出的 ~22 個 NULL opcode 中真正有效者。
   - 這些**沒有 oracle 可對拍**,需從 `dragon.asm` / `OPCODE_REFERENCE` 自行實作後**人工驗證**(跑遊戲觀察),不能純靠 diff。
2. **有副作用的 opcode(繪圖/音效/UI)無法純 trace 對拍**:需改用 golden 畫面 hash(R2 起),建置成本較高。
3. **工程量**:到「可玩中文版」是數週級工作(R2–R6)。但現在是**可切小批、每批可驗**的工程,不是無底洞。
4. **環境綁定**:整套需本機 docker + 遊戲檔 + repo,**自動夜跑不可靠**(session/重開機會中斷;雲端 agent 無本機環境)。→ 以**互動式 session、分批推進**為主。

---

## 建議

**正式開始重寫,以「opcode 批次 + 每批 diff_trace」為節奏推進 R1,然後 R2 render。**
- 每個 PR = 一批可驗證的 opcode 或一個渲染里程碑。
- 純資料 opcode 用 diff_trace;畫面用 golden;無 oracle 的用 ASM 實作 + 人工驗並標註。
- 維持 docker first、PR→main、繁中、CONTEXT.md 譯名。
