// vm_state — script 虛擬 CPU 狀態。
//
// 為了能「逐字移植 opendw 並逐指令差異測試」,本結構刻意鏡像 opendw engine.c 的
// 實際變數(cpu.ax/bx/cx + word_3AE2/3AE4/3AE6 + byte_3AE1 + game_state + stack),
// 僅換上語意化命名。對外仍是窄介面(Interpreter 操作它)。
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace dw::vm {

// word_3AE6 旗標位:對照 engine.c。
inline constexpr std::uint16_t kCarry = 1u << 0;
inline constexpr std::uint16_t kReserved = 1u << 1;  // 永遠 1
inline constexpr std::uint16_t kZero = 1u << 6;
inline constexpr std::uint16_t kSign = 1u << 7;

struct VmState {
  // 通用 scratch(對照 cpu.ax/bx/cx/dx/di/si)
  std::uint16_t ax = 0, bx = 0, cx = 0, dx = 0, di = 0, si = 0;

  // VM 真正的暫存器
  std::uint16_t r2 = 0;     // word_3AE2 主運算暫存器
  std::uint16_t r4 = 0;     // word_3AE4 次暫存器/迴圈計數
  std::uint16_t flags = 0;  // word_3AE6(見 kCarry/kZero/kSign)
  // 持久旗標(對照 cpu.cf/zf/sf):set_flags 等會讀取上一次的值
  std::uint8_t cf = 0, zf = 0, sf = 0;
  std::uint8_t mode = 0;    // byte_3AE1:0=8-bit,0xFF=16-bit

  // 256-byte 遊戲狀態(對照 game_state.unknown[])
  std::array<std::uint8_t, 256> game_state{};

  // 角色資料塊(對照 opendw player.c `data_C960[0xE00]`:7 槽 × 512B player_record;
  //   實際前 4 槽為 party 角色)。op_5D/5E/5F/60/61/62 透過此塊讀寫角色屬性。
  //   定址法(對照 get_character_data/set_character_data @engine.c:2568/2601):
  //     player_idx = game_state[6](當前角色 0..3);
  //     selector   = game_state[player_idx + 0x0A](= record_index*2,record 頁高位元);
  //     絕對 offset = (selector << 8) + property_offset。
  //   op_61(test)另用 get_player_data((selector)>>1)*512 + bx 定址(見 interpreter.cpp)。
  static constexpr std::size_t kCharDataSize = 0xE00;
  std::array<std::uint8_t, kCharDataSize> char_data{};  // = data_C960

  // 角色「擴充/回合」狀態塊(對照 engine.c data_CA4C[4096],初始全 0)。
  //   op_63(set_char_data_word)/op_69 以「selector<<8 + unknown_4456[bx] + operand」
  //   定址(per-character,stride 見 tables.c unknown_4456)。戰鬥回合的角色暫態存於此。
  static constexpr std::size_t kCharExtSize = 4096;
  std::array<std::uint8_t, kCharExtSize> char_ext{};  // = data_CA4C

  // 位元組定址堆疊(忠實對照 opendw cpu.stack[256] + sp:向下成長,
  //   push_byte: --sp; pop_byte: sp++)。op_53/54 用 push_word/pop_word(2 byte),
  //   op_04/03/56/55 用 byte/word 級存取,共用同一堆疊。
  static constexpr std::size_t kStackSize = 256;
  std::array<std::uint8_t, kStackSize> bstack{};
  std::uint16_t sp = kStackSize;  // 初始指向頂端(對照 cpu.sp = STACK_SIZE)

  // 程式計數器:script bytes 內的 offset(對照 cpu.pc - cpu.base_pc)
  std::size_t pc = 0;
  std::vector<std::uint8_t> script;  // 當前 running_script 的 bytes(對照 running_script->bytes)

  // 當前資源索引(對照 word_3AE8 = 程式資源、word_3AEA = 資料資源)。
  // populate_3ADD_and_3ADF():running_script = res[word_3AE8]、word_3ADF = res[word_3AEA]。
  int script_res = -1;  // word_3AE8(目前執行腳本的資源 tag/index)
  int data_res = -1;    // word_3AEA(word_3ADF 指向的資料資源 tag/index)

  // word_3ADF 指向的「資料資源」bytes(op_0C/op_1C 等讀寫處)。
  // 初始與 script 同一份(populate 後 word_3AE8==word_3AEA);op_58 切換後可不同。
  std::vector<std::uint8_t> data_bytes;

  bool halted = false;  // op_5A(結束腳本)

  // --- op_58/op_59 跨資源 call 的呼叫框(對照 push si / push word_3AE8 / push dl)---
  struct CallFrame {
    std::vector<std::uint8_t> script;  // 返回後的 running_script bytes
    std::size_t pc = 0;                // 返回 offset(si)
    std::vector<std::uint8_t> data_bytes;
    int script_res = -1;               // 還原 word_3AE8
    int data_res = -1;                 // 還原 word_3AEA
    std::uint8_t dl = 0;               // op_58 推入的 dl(usage_type 旗標)
  };
  std::vector<CallFrame> call_stack;

  // --- run_script 框(對照 op_5C 的遞迴 run_script;op_5A 還原)---
  // 記錄一個 run_script 進入點:op_5A 觸發時若有此框,代表「return 上層」而非整個 halt。
  struct ScriptFrame {
    std::vector<std::uint8_t> script;
    std::size_t pc = 0;
    std::vector<std::uint8_t> data_bytes;
    int script_res = -1;
    int data_res = -1;
    std::uint8_t mode = 0;
    bool returned = false;  // op_5A 設 true:跳出當前 run_script 迴圈
  };
  std::vector<ScriptFrame> script_frames;

  // 資源提供者(op_58 跨資源 call / op_5C 子 script 載入用)。
  // 給 tag/index 回傳該資源(解壓後)bytes;無法解析回 nullopt。
  using ResourceProvider =
      std::function<std::optional<std::vector<std::uint8_t>>(int tag)>;
  ResourceProvider resource_provider;

  // 偽隨機種子(op_4D PRNG;對照 random_seed=0x1234)。
  // 為求 level_events 抽取可重現,sys_ticks() 以遞增計數代替。
  std::uint16_t random_seed = 0x1234;
  std::uint16_t fake_ticks = 0;

  // 乘/除法運算工作區(對照 engine.c word_11C0..word_11CC)。
  //   op_33/35(gs 取多位元組)、op_34/36(operand)設定運算元,
  //   multiply_16bit / divide_16bit 用這組暫存,結果回存 game_state[0x37..0x3C]。
  //   屬純算術子系統(戰鬥傷害/縮放),無 I/O。
  std::uint16_t w11C0 = 0, w11C2 = 0, w11C4 = 0, w11C6 = 0, w11C8 = 0,
                w11CA = 0, w11CC = 0;

  // op_8A(隨機遭遇)行為控制。
  //   預設 halt(維持「remake 尚無戰鬥 → 停」的既有語意,既有測試依賴此)。
  //   headless_encounter=true 時:op_8A 不 halt,只記錄怪物 id(對照 opendw
  //   trigger_random_encounter 僅設 byte_4F0F/4F29/4F2B 等「圖形/動畫」狀態,
  //   不影響戰鬥數值),讓「原版戰鬥腳本」能在無圖形子系統下繼續跑結算路徑。
  bool headless_encounter = false;
  std::uint8_t encounter_monster_id = 0xFF;  // op_8A 記錄的怪物 id(= word_3AE2 低位)

  // 已載入資源的持久快取(對照 opendw resource_get_by_index → &allocations[index],
  //   engine.c:176)。op_0F(讀)/op_17(寫)以資源 index 存取任意已載入資源,
  //   且 op_17 的寫入需持久(後續再讀同一資源要看得到)。以此 map 模擬 allocations[]。
  //   key = 資源 index;value = 該資源(解壓後)bytes。
  std::map<int, std::vector<std::uint8_t>> res_cache;

  // op_89(wait_event)的 headless 鍵盤注入。
  //   opendw wait_for_event 等待鍵盤後,依 key→address 表跳轉(engine.c:4368)。
  //   headless 下無鍵盤;headless_key != 0 時 op_89 改為「以此鍵掃表跳轉」,
  //   key 為「大寫字母 | 0x80」(對照 get_key_from_buffer 的大寫化結果),
  //   例:選 Fight → 'F'|0x80 = 0xC6;Run → 'R'|0x80 = 0xD2。
  //   為 0 時維持既有行為(halt:headless 無輸入可分支)。
  std::uint8_t headless_key = 0;
  // 多重 op_89 提示的鍵序列(戰鬥流程可能連續多個選單)。非空時 op_89 依序取用;
  //   用完則回退 headless_key;再無則 halt。每個元素同樣是「大寫字母 | 0x80」。
  std::vector<std::uint8_t> headless_keys;
  std::size_t headless_key_idx = 0;

  // 取下一個 byte / word(LE),前進 pc。
  std::uint8_t fetch8() { return pc < script.size() ? script[pc++] : 0; }
  std::uint16_t fetch16() {
    std::uint16_t lo = fetch8();
    std::uint16_t hi = fetch8();
    return static_cast<std::uint16_t>(lo | (hi << 8));
  }

  // 位元組堆疊原語(對照 push_byte/pop_byte/push_word/pop_word/peek_word)。
  void push_byte(std::uint8_t v) {
    sp = (sp == 0) ? (kStackSize - 1) : (sp - 1);
    bstack[sp] = v;
  }
  std::uint8_t pop_byte() {
    std::uint8_t v = bstack[sp];
    sp = (sp + 1 >= kStackSize) ? 0 : (sp + 1);
    return v;
  }
  void push_word(std::uint16_t v) {
    push_byte((v & 0xFF00) >> 8);
    push_byte(v & 0xFF);
  }
  std::uint16_t pop_word() {
    std::uint16_t v = pop_byte();
    v += pop_byte() << 8;
    return v;
  }
  std::uint16_t peek_word() {
    std::uint16_t v = bstack[sp];
    v += bstack[(sp + 1) % kStackSize] << 8;
    return v;
  }

  // 相容舊呼叫(op_53/54):改走位元組堆疊的 word 版本。
  void push(std::uint16_t v) { push_word(v); }
  std::uint16_t pop() { return pop_word(); }
};

}  // namespace dw::vm
