# OpenDW Remake

以 **C++20 + SDL2** 重寫的 Dragon Wars (Interplay, 1989) 執行環境,內建繁體中文化,並走向**不依賴原始磁碟檔的自包含資產**。

設計與驗證策略見 [`ARCHITECTURE.md`](ARCHITECTURE.md)。

## 定位

- **不是**把組語再翻一次。是理解後的現代重寫:乾淨的 script VM + 渲染器 + 資產層,**執行原始(已萃取並驗證)的 bytecode**。
- **正確性 oracle = opendw**(Devin Smith 的 C 反組譯)。每個模組以「與 opendw byte-for-byte / 逐指令一致」為驗收(差異測試)。
- **資產來源 / 工具 = 上層 `opendw_dragon_wars_cht/`**(逆向、萃取、中文化資料)。

## 現況(R0 進行中)

| 模組 | 狀態 |
|------|------|
| `resource/text_codec`(5-bit 字串編解碼) | ✅ 對照 opendw compress.c/engine.c |
| `resource/archive`(DATA1/DATA2 reader,含 DATA2 fallback 修正) | ✅ |
| `resource/decompress`(Huffman 樹解壓) | ✅ round-trip byte-for-byte == opendw(res31/res168 驗證) |
| `tools/verify`(verify_r0 / verify_decompress 對拍 opendw) | ✅ |
| VM / render / i18n / game | ⏳ 見 ARCHITECTURE §7 階段表 |

**R0(資產層)完成**:archive + text_codec + decompress 全數對拍 opendw 一致。

| `vm/`(script 虛擬 CPU) | 🚧 R1:56/256 opcode(模式/算術/旗標/邏輯/比較/跳轉/loop/game_state/bit) |
| **差異測試 harness** | ✅ remake VM trace == opendw oracle(逐指令一致)。`bash tools_build/diff_trace.sh` |
| `render/`(framebuffer + 全螢幕圖 + 8×8 字) | ✅ R2 batch 1:framebuffer + title_adjust(title golden 通過,`render_golden.sh`)。✅ R2 batch 2:遊戲原生 8×8 字(chr_table @ dragon.com 0xBE52,MSB-first),視覺驗證 alphabet/數字/符號正確 |

**R1 進行中**:VM 核心 + 差異測試 harness 已立(`diff_trace.sh` 證明 remake 對拍 opendw 逐指令一致)。
逐 batch 補齊 256 opcode,每批用差異測試驗。
> 發現:opendw 未實作 op_43/5F/60/63(targets[] 引用裸名但無實作),這幾個 opcode **無 oracle**,需另從 ASM/spec 實作後人工驗。

## 建置(docker first)

```bash
docker build -t dwr -f - . <<'EOF'
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y g++ cmake && rm -rf /var/lib/apt/lists/*
EOF
docker run --rm -v "$PWD":/app -w /app dwr bash -c \
  "cmake -S . -B build && cmake --build build -j"

# 驗證 R0(需準備含 data1 的目錄)
./build/verify_r0 /path/to/data_dir
# 預期印出: Interplay / Do you wish to.. / Begin a new game / Continue an old game ...
```

## 授權

重寫程式碼(本目錄)為原創,採 BSD。執行所需資產衍生自 Interplay Dragon Wars (1989),屬保存/中文化範疇。
