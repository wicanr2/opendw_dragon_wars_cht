#include "interpreter.hpp"

namespace dw::vm {

// --- opcode 實作(逐字對照 opendw engine.c;cpu.ax→s_.ax 等)---

void Interpreter::op00_set_word_mode() { s_.mode = 0xFF; }

void Interpreter::op01_set_byte_mode() {
  s_.r2 &= 0xFF;
  s_.mode = 0;
}

void Interpreter::op05_load_gs_r4() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  al = s_.game_state[s_.bx & 0xFF];
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.r4 = al;
}

void Interpreter::op06_imm_r4() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.r4 = al;
}

void Interpreter::op09_set_r2_arg() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.r2 = al;
  if (s_.mode != (s_.ax >> 8)) {  // word 模式:再讀高位元組
    al = s_.fetch8();
    s_.ax = (s_.ax & 0xFF00) | al;
    s_.r2 = static_cast<std::uint16_t>((al << 8) | (s_.r2 & 0xFF));
  }
}

void Interpreter::op21_r4_lo_from_r2() {
  std::uint8_t lo = s_.r2 & 0x00FF;
  s_.r4 = (s_.r4 & 0xFF00) | lo;
}

void Interpreter::op22_r2_from_r4() {
  s_.ax = s_.r4;
  s_.r2 = s_.ax;
}

void Interpreter::op3D_cmp_gs() {  // 比較 r2 vs game_state[arg],設旗標
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  s_.cx = s_.r2;
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  int cf = 0, zf = 0, sf = 0;
  if (s_.mode != ah) {  // word 比較
    std::uint16_t cv = s_.game_state[s_.bx & 0xFF];
    cv += s_.game_state[(s_.bx + 1) & 0xFF] << 8;
    if (s_.cx < cv) cf = 1;
    if (s_.cx == cv) zf = 1;
  } else {  // byte 比較
    std::uint8_t cl = s_.cx & 0xFF;
    if (cl < s_.game_state[s_.bx & 0xFF]) cf = 1;
    if (cl == s_.game_state[s_.bx & 0xFF]) zf = 1;
  }
  cf = !cf;  // 對照 engine.c:cpu.cf = !cpu.cf
  std::uint16_t flags = 0;
  flags |= sf << 7;
  flags |= zf << 6;
  flags |= kReserved;
  flags |= cf << 0;
  s_.flags = flags;
}

void Interpreter::op44_jz() {  // ZF set 才跳
  if ((s_.flags & kZero) == 0) {
    s_.fetch8();
    s_.fetch8();
    return;
  }
  std::uint16_t addr = s_.fetch16();
  s_.ax = addr;
  s_.pc = addr;
}

void Interpreter::op4B_stc() { s_.flags |= kCarry; }

void Interpreter::op4C_clc() { s_.flags &= 0xFFFE; }

void Interpreter::op52_jmp() {  // 無條件跳轉(不存返回位址)
  std::uint16_t addr = s_.fetch16();
  s_.pc = addr;
}

void Interpreter::op53_call() {  // 跳轉並推入返回位址
  std::uint16_t addr = s_.fetch16();
  std::uint16_t ret = static_cast<std::uint16_t>(s_.pc);
  s_.push(ret);
  s_.pc = addr;
}

void Interpreter::op54_ret() {  // 彈出返回位址並跳轉
  s_.pc = s_.pop();
}

void Interpreter::op99_test_r2() {  // TEST r2 自身,設 ZF/SF
  s_.cx = s_.r2;
  int zf = 0, sf = 0;
  if (s_.mode == (s_.ax >> 8)) {
    std::uint8_t cl = s_.cx & 0xFF;
    if (cl == 0) zf = 1;
    sf = (cl >= 0x80);
  } else {
    if (s_.cx == 0) zf = 1;
    sf = (s_.cx >= 0x8000);
  }
  std::uint16_t flags = 0;
  flags |= sf << 7;
  flags |= zf << 6;
  flags |= kReserved;
  flags &= 0xFFFE;
  s_.flags = (s_.flags & 0x0001) | flags;
  s_.ax = flags;
}

// --- dispatch 表 ---
#define OP(n, m) [n] = &Interpreter::m
const std::array<Interpreter::Handler, 256> Interpreter::kImpl = [] {
  std::array<Handler, 256> t{};
  t[0x00] = &Interpreter::op00_set_word_mode;
  t[0x01] = &Interpreter::op01_set_byte_mode;
  t[0x05] = &Interpreter::op05_load_gs_r4;
  t[0x06] = &Interpreter::op06_imm_r4;
  t[0x09] = &Interpreter::op09_set_r2_arg;
  t[0x21] = &Interpreter::op21_r4_lo_from_r2;
  t[0x22] = &Interpreter::op22_r2_from_r4;
  t[0x3D] = &Interpreter::op3D_cmp_gs;
  t[0x44] = &Interpreter::op44_jz;
  t[0x4B] = &Interpreter::op4B_stc;
  t[0x4C] = &Interpreter::op4C_clc;
  t[0x52] = &Interpreter::op52_jmp;
  t[0x53] = &Interpreter::op53_call;
  t[0x54] = &Interpreter::op54_ret;
  t[0x99] = &Interpreter::op99_test_r2;
  return t;
}();
#undef OP

int Interpreter::run() {
  int steps = 0;
  while (!s_.halted && s_.pc < s_.script.size()) {
    std::size_t at = s_.pc;
    std::uint8_t op = s_.fetch8();
    if (trace_) trace_->record({at, op, s_.r2, s_.r4, s_.flags, s_.mode});
    Handler h = kImpl[op];
    if (!h) { last_unimpl_ = op; s_.halted = true; break; }  // 未實作 → 停
    (this->*h)();
    ++steps;
  }
  return steps;
}

}  // namespace dw::vm
