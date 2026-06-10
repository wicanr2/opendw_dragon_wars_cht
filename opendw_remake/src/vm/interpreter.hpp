// interpreter — script 虛擬 CPU 的 fetch-dispatch 直譯器(對照 opendw run_script)。
//
// R1 batch 1:VM 核心 + 一批「無副作用」opcode(模式/算術/旗標/跳轉/呼叫/堆疊)。
// 後續 batch 補齊其餘 opcode 並全程對拍 opendw(差異測試)。
#pragma once
#include <array>
#include "vm_state.hpp"
#include "trace.hpp"

namespace dw::vm {

class Interpreter {
public:
  explicit Interpreter(VmState& s, Trace* trace = nullptr) : s_(s), trace_(trace) {}

  // 執行直到 halt(op_5A)或 pc 越界。回傳執行的指令數。
  int run();

  // 已實作的 opcode 集合(R1 batch 1);未實作者執行會 halt 並記錄。
  bool implemented(std::uint8_t op) const { return kImpl[op] != nullptr; }

private:
  using Handler = void (Interpreter::*)();
  static const std::array<Handler, 256> kImpl;

  VmState& s_;
  Trace* trace_;
  std::uint8_t last_unimpl_ = 0;

  // --- batch 1 opcodes(逐字對照 opendw engine.c)---
  void op00_set_word_mode();   // 0x00
  void op01_set_byte_mode();   // 0x01
  void op05_load_gs_r4();      // 0x05
  void op06_imm_r4();          // 0x06
  void op09_set_r2_arg();      // 0x09
  void op21_r4_lo_from_r2();   // 0x21
  void op22_r2_from_r4();      // 0x22
  void op3D_cmp_gs();          // 0x3D
  void op44_jz();              // 0x44
  void op4B_stc();             // 0x4B
  void op4C_clc();             // 0x4C
  void op52_jmp();             // 0x52
  void op53_call();            // 0x53
  void op54_ret();             // 0x54
  void op99_test_r2();         // 0x99
};

}  // namespace dw::vm
