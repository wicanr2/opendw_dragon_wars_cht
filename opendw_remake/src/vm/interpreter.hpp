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

  // --- batch 2:game_state 存取 / inc-dec / 位移 / 邏輯 / 比較 / 跳轉 / bit ---
  void op07_r4_from_axhi();    // 0x07
  void op08_gs_from_r4();      // 0x08
  void op0A_r2_from_gs();      // 0x0A
  void op11_gs_from_ah();      // 0x11
  void op12_gs_from_r2();      // 0x12
  void op1A_gs_imm();          // 0x1A
  void op23_inc_gs();          // 0x23
  void op24_inc_r2();          // 0x24
  void op25_inc_r4lo();        // 0x25
  void op26_dec_gs();          // 0x26
  void op27_dec_r2();          // 0x27
  void op28_dec_r4lo();        // 0x28
  void op2A_shl_r2();          // 0x2A
  void op2B_shl_r4lo();        // 0x2B
  void op2D_shr_r2();          // 0x2D
  void op2E_shr_r4lo();        // 0x2E
  void op38_and();             // 0x38
  void op39_or_gs();           // 0x39
  void op3A_or_imm();          // 0x3A
  void op3B_xor_gs();          // 0x3B
  void op3C_xor_imm();         // 0x3C
  void op3E_cmp_imm();         // 0x3E
  void op41_jnc();             // 0x41
  void op42_jc();              // 0x42
  void op45_jnz();             // 0x45
  void op46_js();              // 0x46
  void op47_jns();             // 0x47
  void op4E_set_gs_bit();      // 0x4E
  void op4F_clr_gs_bit();      // 0x4F
  void op50_test_gs_bit();     // 0x50

  // 輔助
  void set_gs(std::uint16_t idx, std::uint8_t val);
  void get_bit_mask(std::uint8_t al);
};

}  // namespace dw::vm
