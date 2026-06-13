// vm_selftest — R1 batch 1 VM 核心自測:手工 bytecode 驗證模式/暫存器/旗標/jmp/call-ret。
// (差異測試對拍 opendw 於後續 batch 加入。)
#include <cstdio>
#include <vector>
#include "../src/vm/interpreter.hpp"

using namespace dw::vm;

static int fails = 0;
static void check(const char* name, bool ok) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++fails;
}

static VmState run(std::vector<std::uint8_t> code) {
  VmState s;
  s.script = std::move(code);
  Interpreter ip(s);
  ip.run();
  return s;
}

int main() {
  std::printf("VM self-test (R1 batch 1):\n");

  // A: stc → carry set
  check("stc 設 carry", (run({0x4B}).flags & kCarry) != 0);

  // B: stc; clc → carry clear
  check("clc 清 carry", (run({0x4B, 0x4C}).flags & kCarry) == 0);

  // C: op06 r4=0x42 ; op22 r2=r4 → r2==r4==0x42
  {
    VmState s = run({0x06, 0x42, 0x22});
    check("op06+op22:r4→r2", s.r4 == 0x42 && s.r2 == 0x42);
  }

  // D: jmp 跳過中間的 clc,落到 stc → carry set
  //    [0]=52 jmp 5  [3]=4B(skip) [4]=4C(skip) [5]=4B stc
  check("jmp 跳過中段", (run({0x52, 0x05, 0x00, 0x4B, 0x4C, 0x4B}).flags & kCarry) != 0);

  // E: call/ret —— call 5;[5]=4B stc;[6]=54 ret→回 3;[3]=4C clc;[4]=0x02 未實作→halt
  //    驗證:確實經 ret 回到 offset 3 執行 clc(最終 carry 清除)
  {
    VmState s = run({0x53, 0x05, 0x00, 0x4C, 0x02, 0x4B, 0x54});
    check("call→stc→ret→clc", (s.flags & kCarry) == 0 && s.halted);
  }

  // F: op99 test r2==0 → ZF set
  {
    VmState s; s.script = {0x99}; s.r2 = 0; s.mode = 0; s.ax = 0;
    Interpreter(s).run();
    check("op99 r2=0→ZF", (s.flags & kZero) != 0);
  }

  // --- batch 10:op_7D / op_80 / op_8C(已對拍 opendw oracle,逐指令一致)---

  // G: op_80(advance_cursor)讀 1 byte operand,不動 r2/r4/flags/mode。
  //    [0]=01 byte mode  [1]=09 07 r2=7  [3]=80 05 advance(吃 1B)  [5]=24 inc r2→8  [6]=5A
  {
    VmState s = run({0x01, 0x09, 0x07, 0x80, 0x05, 0x24, 0x80, 0x0A, 0x5A});
    check("op80 消耗 1B operand、r2 不受影響(=8)", s.r2 == 0x08 && s.halted);
  }

  // H: op_7D(write_character_name)無 operand,headless 不動 r2/r4/flags(僅算 ax/bx)。
  {
    VmState s = run({0x01, 0x09, 0x07, 0x7D, 0x24, 0x7D, 0x5A});
    check("op7D 無 operand、r2 不受影響(=8)", s.r2 == 0x08 && s.halted);
  }

  // I: op_8C(prompt_no_yes)headless key=0 → flags = reserved|carry = 0x03(zf=0,cf=1)。
  {
    VmState s = run({0x01, 0x09, 0x07, 0x8C, 0x5A});
    check("op8C headless key=0 → flags=0x03(carry,非 zero)",
          (s.flags & kCarry) != 0 && (s.flags & kZero) == 0 &&
          (s.flags & kSign) == 0 && (s.flags & kReserved) != 0);
  }

  // --- batch 11:op_0E / op_83 / op_90(已對拍 opendw oracle,逐指令一致)---

  // J: op_0E:gs[0x10]=7、gs[0x11]=0 → base=7;op_0E 0x10 讀 data[7]=prog[7]=0x06 → r2=0x0006。
  //    [0]=01 byte mode  [1]=1A 10 07 gs[16]=7  [4]=1A 11 00 gs[17]=0
  //    [7]=06 00 r4=0  [9]=0E 10 → r2=data[7]=0x06  [11]=5A
  {
    VmState s = run({0x01, 0x1A, 0x10, 0x07, 0x1A, 0x11, 0x00, 0x06, 0x00, 0x0E, 0x10, 0x5A});
    check("op0E:r2=data[gs 索引]=0x06", s.r2 == 0x0006 && s.halted);
  }

  // K: op_83(print_char)無 operand、純輸出,不動 r2/r4/flags。
  //    op_09 r2=0x41 → op_83 印 → op_24 inc r2→0x42 → op_83 印 → r2 仍 0x42。
  {
    VmState s = run({0x01, 0x09, 0x41, 0x83, 0x24, 0x83, 0x5A});
    check("op83 純輸出、r2 不受影響(=0x42)", s.r2 == 0x42 && s.halted);
  }

  // L: op_90(sound_effect)讀 1B operand、不動 r2/r4/flags(僅音效副作用)。
  //    op_09 r2=7 → op_90 01(吃1B) → op_24 inc r2→8 → op_90 02 → r2=8。
  {
    VmState s = run({0x01, 0x09, 0x07, 0x90, 0x01, 0x24, 0x90, 0x02, 0x5A});
    check("op90 消耗 1B operand、r2 不受影響(=8)", s.r2 == 0x08 && s.halted);
  }

  std::printf(fails ? "\n%d 項失敗\n" : "\n全部通過\n", fails);
  return fails ? 1 : 0;
}
