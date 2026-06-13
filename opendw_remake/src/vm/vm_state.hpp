// vm_state — script 虛擬 CPU 狀態。
//
// 為了能「逐字移植 opendw 並逐指令差異測試」,本結構刻意鏡像 opendw engine.c 的
// 實際變數(cpu.ax/bx/cx + word_3AE2/3AE4/3AE6 + byte_3AE1 + game_state + stack),
// 僅換上語意化命名。對外仍是窄介面(Interpreter 操作它)。
#pragma once
#include <array>
#include <cstdint>
#include <functional>
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
