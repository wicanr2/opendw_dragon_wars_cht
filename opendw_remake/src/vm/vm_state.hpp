// vm_state — script 虛擬 CPU 狀態。
//
// 為了能「逐字移植 opendw 並逐指令差異測試」,本結構刻意鏡像 opendw engine.c 的
// 實際變數(cpu.ax/bx/cx + word_3AE2/3AE4/3AE6 + byte_3AE1 + game_state + stack),
// 僅換上語意化命名。對外仍是窄介面(Interpreter 操作它)。
#pragma once
#include <array>
#include <cstdint>
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

  // 堆疊(對照 cpu.stack[32] + sp;存 16-bit 返回位址)
  std::array<std::uint16_t, 32> stack{};
  std::uint8_t sp = 0;  // 由 0 向上;push 前進、pop 後退

  // 程式計數器:script bytes 內的 offset(對照 cpu.pc - cpu.base_pc)
  std::size_t pc = 0;
  std::vector<std::uint8_t> script;  // 當前 running_script 的 bytes

  bool halted = false;  // op_5A(結束腳本)

  // 取下一個 byte / word(LE),前進 pc。
  std::uint8_t fetch8() { return pc < script.size() ? script[pc++] : 0; }
  std::uint16_t fetch16() {
    std::uint16_t lo = fetch8();
    std::uint16_t hi = fetch8();
    return static_cast<std::uint16_t>(lo | (hi << 8));
  }

  void push(std::uint16_t v) { if (sp < stack.size()) stack[sp++] = v; }
  std::uint16_t pop() { return sp > 0 ? stack[--sp] : 0; }
};

}  // namespace dw::vm
