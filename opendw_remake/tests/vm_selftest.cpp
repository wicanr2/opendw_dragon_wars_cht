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

  // --- batch 12:角色資料存取 op_5D/5E/5F/60/61/62(已對拍 opendw oracle,逐指令一致)---
  // 共用 seed:當前角色=player0(gs[6]=0)、selector=gs[0x0A]=2 → record base 0x200;
  //   op_61 用 (selector>>1)*512 = 0x200(與 5D 一致);party size gs[0x1F]=2。

  // M: op_5D 讀屬性:char[0x200+0x0C]=0x12 → r2=0x12;op_24;op_5D 0x0D 讀 0x34。
  {
    VmState s; s.game_state[6]=0; s.game_state[0x0A]=2;
    s.char_data[0x200+0x0C]=0x12; s.char_data[0x200+0x0D]=0x34;
    s.script={0x01, 0x5D,0x0C, 0x24, 0x5D,0x0D, 0x5A};
    Interpreter(s).run();
    check("op5D 讀角色屬性 0x0D=0x34", s.r2==0x34 && s.halted);
  }

  // N: op_5E 寫屬性:r2=0x99 → op_5E 0x0D 寫 char[0x200+0x0D];op_5D 0x0D 讀回 0x99。
  {
    VmState s; s.game_state[6]=0; s.game_state[0x0A]=2;
    s.script={0x01, 0x09,0x99, 0x5E,0x0D, 0x5D,0x0D, 0x5A};
    Interpreter(s).run();
    check("op5E 寫角色屬性後讀回=0x99",
          s.char_data[0x200+0x0D]==0x99 && s.r2==0x99 && s.halted);
  }

  // O: op_5F 設 bit:r2=1(mask 0x40)→ op_5F 0x21 設 char[0x200+0x21] bit6。
  {
    VmState s; s.game_state[6]=0; s.game_state[0x0A]=2;
    s.script={0x01, 0x09,0x01, 0x5F,0x21, 0x5A};
    Interpreter(s).run();
    check("op5F 設角色 bit(char[0x221]|=0x40)", s.char_data[0x200+0x21]==0x40 && s.halted);
  }

  // P: op_60 清 bit:預設 char[0x200+0x21]=0x40 → op_60 0x21 清 bit6 → 0x00。
  {
    VmState s; s.game_state[6]=0; s.game_state[0x0A]=2; s.char_data[0x200+0x21]=0x40;
    s.script={0x01, 0x09,0x01, 0x60,0x21, 0x5A};
    Interpreter(s).run();
    check("op60 清角色 bit(char[0x221]&=~0x40)", s.char_data[0x200+0x21]==0x00 && s.halted);
  }

  // Q: op_61 測 bit:char[0x200+0x20]=0x40,r2=1(mask 0x40)→ test 命中(非零)→ zf=0,cf=0。
  {
    VmState s; s.game_state[6]=0; s.game_state[0x0A]=2; s.char_data[0x200+0x20]=0x40;
    s.script={0x01, 0x09,0x01, 0x61,0x20, 0x5A};
    Interpreter(s).run();
    check("op61 測角色 bit 命中 → zf=0、cf=0",
          (s.flags & kZero)==0 && (s.flags & kCarry)==0 && s.halted);
  }

  // R: op_62 掃描:party2 人,strength(0x0C):player0(rec 0)=0、player1(rec 512)=0x07;
  //    門檻 cl=0x08 → 皆未達 → 迴圈結束 set cf;word_3AE6 不寫(僅 cpu.cf=1)。
  {
    VmState s; s.game_state[0x1F]=2;
    s.char_data[0*512+0x0C]=0x00; s.char_data[1*512+0x0C]=0x07;
    s.script={0x01, 0x62,0x0C,0x08, 0x5A};
    Interpreter(s).run();
    check("op62 全未達門檻 → cf=1、word_3AE6 不設 carry",
          s.cf==1 && (s.flags & kCarry)==0 && s.halted);
  }

  std::printf(fails ? "\n%d 項失敗\n" : "\n全部通過\n", fails);
  return fails ? 1 : 0;
}
