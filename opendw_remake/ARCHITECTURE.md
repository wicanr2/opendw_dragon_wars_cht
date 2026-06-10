# OpenDW Remake — 架構設計

> **目標**：以現代 C++17 + SDL2 重寫 Dragon Wars 執行環境,內建中文化,並能脫離原始 `DATA1/DATA2/dragon.com` 自包含執行。
> **正確性 oracle**：Devin Smith 的 opendw（C 反組譯重製)— 重寫的每個模組以「與 opendw 行為一致」為驗收。
> **日期**：2026-06-10

---

## 1. 核心理念

opendw 是 16-bit x86 組語逐行翻成 C 的成果:全域變數即記憶體位址、`goto`、魔術數字遍佈(見其 `style.md` 自承)。它**可信但不可維護**。

Remake 不是「再翻譯一次組語」,而是**理解後的現代重寫**:

1. **Remake = 執行環境的重寫,不是遊戲邏輯的 clean-room 改寫。** 遊戲邏輯本身是 **script bytecode**(存在 DATA1 各 section)。Remake 用現代 C++ 寫一個乾淨的 **script VM + 渲染器 + 資產層**,去**執行原始(已萃取)的 bytecode**。
   - 好處:可**逐指令對拍**驗證 —— 同一段 bytecode 丟進 opendw 與 remake,暫存器/狀態/輸出必須一致。
   - 這讓「正確性」變成可機械驗證的事,而非靠人讀。
2. **自包含資產(使用者的洞見)。** 一旦把 DATA1/DATA2 的 bytecode + map + sprite + text 完整萃取成 remake 自有的 **asset bundle**,執行期就**不再需要原始 Dragon Wars 檔**。Remake 讀自己的 `assets/`。
3. **中文化是一等公民。** 文字層(翻譯表、Read Paragraph、CJK 字型)是模組,不是事後補丁。

---

## 2. 模組邊界(vertical slices,deep modules)

依 `rules/70-deep-modules.md`:按功能切、窄介面、隱藏複雜度。

```
src/
  vm/         script 虛擬 CPU —— remake 的心臟
    vm_state.hpp      暫存器(r3AE2/r3AE4/flags)、byte/word 模式、stack、game_state[256]
    interpreter.*     PC-based fetch-dispatch 迴圈
    opcodes.*         256 個 opcode 實作(對照 opendw targets[])
    trace.*           執行追蹤(差異測試用)
  resource/   資產層 —— 隱藏 DATA1/DATA2 與自有格式的差異
    archive.*         原始 DATA1/DATA2 reader(768B header + section)
    decompress.*      LZSS 解壓(對照 compress.c)
    text_codec.*      5-bit 文字編解碼(對照 extract_string/extract_letter)
    asset_bundle.*    NEW 自有資產格式(自包含執行)
  render/     SDL2 渲染 —— 隱藏「320×200 indexed → 現代視窗」
    framebuffer.hpp   320×200 indexed buffer(遊戲原生)
    sdl_video.*       SDL2 視窗 + 16 色盤 + pixel scaling(640×480+)
    font.*            8×8 ASCII(原生 chr_table)+ 24×24 CJK glyph
    viewport.*        3D 地城 + sprite 合成
  world/      map / level / viewport 資料
  entities/   player / party / monster / item / spell(對照 player.c 結構)
  i18n/       strings(翻譯表)、paragraphs(Read Paragraph DB)、glyph_cache
  game/       scene 狀態機:title / explore / combat / dialogue / shop
  platform/   OS adapter(file / time / input)— 只在邊界放 adapter
tools/
  extract/    DATA1/DATA2 → asset_bundle(一次性,產生自包含資產)
  verify/     差異測試:同 bytecode 對拍 opendw vs remake
assets/       萃取後的 bundle + CJK 字型 + i18n 資料(執行期讀這裡)
tests/        unit + golden test
```

**介面範例(deep module,窄對外)**:
```cpp
// resource/text_codec.hpp — 對外只露兩個函式,內部藏 5-bit/escape/大小寫切換複雜度
namespace dw::text {
  // 解碼一條 5-bit 壓縮字串,回傳 (utf8, 下一條起始 offset)
  std::pair<std::string, size_t> decode(std::span<const uint8_t> data, size_t bit_offset);
  // 反向:把(中文/ASCII)編回原格式(回寫 patch 用)
  std::vector<uint8_t> encode(std::string_view utf8);
}
```

---

## 3. VM 設計(對照 opendw)

opendw 模型(已 review):`run_engine` 載入 resource 0 → `run_script(pc)` fetch byte → `targets[256]` 分派 → handler 以 `pc++` 讀參數、改 `cpu`/`game_state`。

Remake `vm/`:
```cpp
struct VmState {
  uint16_t r2{}, r4{};            // word_3AE2(主)、word_3AE4(次/計數)
  uint8_t  mode{};                // 0=8bit, 0xFF=16bit (對照 byte_3AE1)
  struct { bool c,z,s; } flags;
  std::array<uint8_t,32> stack;   uint8_t sp{32};
  std::array<uint8_t,256> game_state{};
  const uint8_t* pc{}; const uint8_t* base_pc{};
  ResourceRef running_script;
};

class Interpreter {
public:
  explicit Interpreter(World&, ResourceManager&, Renderer&, I18n&);
  void run(ResourceId initial_script);   // = run_engine
private:
  using Handler = void(Interpreter::*)();
  static const std::array<Handler,256> kOps;   // = targets[]
  Trace* trace_{};                              // 非 null 時記錄每步(驗證)
};
```
- 每個 opcode 是 `Interpreter` 的 method,命名用語意(`op_set_msg`, `op_jump_if_carry`),不留 `op_78`。
- `kOps` 表保留與 opendw 相同的索引↔語意對應(差異測試的基準)。

---

## 4. 正確性驗證(用 opendw 當 oracle)

三層,由細到粗:

### 4.1 差異測試(differential,最強)
- 在 opendw 加一個 **trace hook**:執行 script 時輸出每步 `(pc_offset, opcode, r2, r4, flags, game_state_diff, emitted_text)` 成 `trace.jsonl`。
- Remake 跑**同一段 bytecode**(同一 resource),輸出同格式 trace。
- `tools/verify` 逐行 diff。**第一個分歧點 = remake 的 bug**。
- 覆蓋:開新遊戲、角色建立、移動、一場戰鬥、施法、存讀檔。

### 4.2 Golden 輸出測試
- 對關鍵畫面(title / 選單 / 狀態頁),擷取 opendw 的 **framebuffer hash + 顯示字串**為 golden,remake 必須吻合(中文化模式另存一組 golden)。

### 4.3 資產 round-trip
- `tools/extract` 萃取 → `asset_bundle`;驗證 remake 載入的每個 resource bytes 與 opendw `resource_load()` 輸出 **byte-for-byte 相同**(本專案已有 `resextract` 可當對照)。

> 已具備的對照素材:本 repo 的 `tools_build/`(resextract/sprite_dump)、Python 解碼器、`ALL_TEXT_FROM_SCRIPTS`、怪物/sprite/段落資料,都是現成 golden。

---

## 5. 自包含資產管線

```
DATA1 / DATA2 / dragon.com
        │  tools/extract (一次性, 對照 opendw 驗證)
        ▼
assets/
  scripts/      各 section 的 bytecode (remake VM 直接執行)
  maps/         level / viewport 資料
  sprites/      怪物/場景圖 (已可由 sprite_dump 產出)
  text/         原始英文字串 (id = section:offset)
  i18n/zh-TW/   翻譯表 + paragraphs_zh + glyph 需求集
  fonts/        wqy 24×24 子集
  palette.dat   16 色盤
```
- 執行期 remake **只讀 assets/**,不碰原始檔 → 達成使用者要的「不依賴 Dragon Wars 原始檔」。
- 註:asset 內容仍衍生自原作(版權同既有掃描資產);此處「自包含」指**執行期不需使用者提供原始磁碟檔**,非版權聲明。

---

## 6. 與中文化的整合

- VM 的字串輸出 opcode(0x77/78/7B/7A/95/96 + 待實作 0x79/7E/7F/8F)走 `I18n::lookup(section, offset)` → 若有中文則出中文,否則回退英文。
- `render/font` 自動依字元選 8×8(ASCII)或 24×24(CJK)。
- Read Paragraph:VM 偵測 `Read paragraph N` → `i18n/paragraphs` 查 `assets/i18n/zh-TW/paragraphs`(本 repo `data/paragraphs/` 已備)→ 段落檢視器 scene。

---

## 7. 分階段(每階段都對拍 opendw)

| 階段 | 內容 | 驗證 |
|------|------|------|
| R0 | 骨架 + CMake + resource/archive + text_codec | round-trip:resource bytes / 字串 == opendw |
| R1 | VM 核心 + 無副作用 opcode(算術/旗標/跳轉/game_state) | differential trace(純資料流) |
| R2 | render/framebuffer + sdl_video + 8×8 字 + title | golden:title 畫面 |
| R3 | 字串輸出 opcode + i18n + CJK 24×24 | golden:選單(中/英) |
| R4 | viewport + sprite + map 移動 | golden:地城畫面 |
| R5 | 戰鬥 / 法術 / 角色 / 存讀檔 | differential:整場戰鬥 trace |
| R6 | Read Paragraph 內嵌 + 自包含 asset bundle | 功能 + 不依賴原始檔啟動 |

---

## 8. 技術選型

- **C++17**(可選 C++20 ranges/span)、**SDL2**(視窗/輸入/音訊)、**CMake**。
- **freetype**(從 wqy 即時產 24×24 glyph,或預生成 atlas)。
- 第三方最小化:SDL2 + freetype;其餘標準庫。
- 測試:差異測試自建 harness + 簡單 golden;可選 doctest 做 unit。
- Docker first 建置(符合 repo 規範)。

---

## 9. 與既有 opendw_dragon_wars_cht 的關係

- `opendw_dragon_wars_cht/`(本 repo 現況):**逆向 + 萃取 + 文件 + 中文化資料**的成果庫 —— 成為 remake 的 **oracle + 資產來源**。
- `opendw_remake/`(本目錄):乾淨重寫的 **executable**。
- opendw 原始 C 碼:保留為 **reference**(差異測試對照組),不修改。
