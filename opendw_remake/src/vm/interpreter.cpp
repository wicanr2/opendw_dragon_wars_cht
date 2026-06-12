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

// --- batch 2(逐字對照 opendw engine.c)---
void Interpreter::set_gs(std::uint16_t idx, std::uint8_t val) {
  if (idx < s_.game_state.size()) s_.game_state[idx] = val;
}
void Interpreter::get_bit_mask(std::uint8_t al) {  // get_bit_mask_from_table
  s_.ax = al;
  std::uint16_t di = s_.ax;
  s_.bx = s_.ax;
  std::uint8_t pcb = s_.fetch8();
  s_.bx = (s_.bx >> 3) + pcb;
  di &= 7;
  static const std::uint8_t tbl[8] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
  s_.ax = tbl[di];
}

void Interpreter::op07_r4_from_axhi() { s_.r4 = (s_.ax & 0xFF00) >> 8; }

void Interpreter::op08_gs_from_r4() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  al = s_.r4 & 0xFF;
  set_gs(s_.bx, al);
}

void Interpreter::op0A_r2_from_gs() {
  std::uint8_t idx = s_.fetch8();
  std::uint8_t al = s_.game_state[idx];
  std::uint8_t ah = s_.game_state[(idx + 1) & 0xFF];
  ah = ah & s_.mode;  // byte 模式遮罩高位
  s_.r2 = (ah << 8) | al;
}

void Interpreter::op11_gs_from_ah() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  set_gs(s_.bx, ah);
  if (s_.mode != ah) set_gs(s_.bx + 1, ah);
}

void Interpreter::op12_gs_from_r2() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  s_.cx = s_.r2;
  set_gs(s_.bx, s_.cx & 0xFF);
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) set_gs(s_.bx + 1, (s_.cx & 0xFF00) >> 8);
}

void Interpreter::op1A_gs_imm() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.di = s_.ax;
  al = s_.fetch8();
  set_gs(s_.di, al);
  s_.ax = (s_.ax & 0xFF00) | al;
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) {
    al = s_.fetch8();
    set_gs(s_.di + 1, al);
    s_.ax = (s_.ax & 0xFF00) | al;
  }
}

void Interpreter::op23_inc_gs() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.di = s_.ax;
  set_gs(s_.di, s_.game_state[s_.di] + 1);
  if (s_.game_state[s_.di] == 0)
    if (s_.mode != ((s_.ax & 0xFF00) >> 8))
      set_gs(s_.di + 1, s_.game_state[(s_.di + 1) & 0xFF] + 1);
}

void Interpreter::op24_inc_r2() {
  s_.ax = s_.r2; s_.ax++;
  std::uint8_t ah = ((s_.ax & 0xFF00) >> 8) & s_.mode;
  s_.ax = (ah << 8) | (s_.ax & 0xFF);
  s_.r2 = s_.ax;
}

void Interpreter::op25_inc_r4lo() {
  std::uint8_t lo = (s_.r4 & 0xFF) + 1;
  s_.r4 = (s_.r4 & 0xFF00) | lo;
}

void Interpreter::op26_dec_gs() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.di = s_.ax;
  s_.cx = s_.game_state[s_.di];
  s_.cx += (s_.game_state[(s_.di + 1) & 0xFF] << 8);
  s_.cx--;
  set_gs(s_.di, s_.cx & 0xFF);
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) set_gs(s_.di + 1, (s_.cx & 0xFF00) >> 8);
}

void Interpreter::op27_dec_r2() {
  s_.ax = s_.r2; s_.ax--;
  std::uint8_t ah = ((s_.ax & 0xFF00) >> 8) & s_.mode;
  s_.ax = (ah << 8) | (s_.ax & 0xFF);
  s_.r2 = s_.ax;
}

void Interpreter::op28_dec_r4lo() {
  std::uint8_t lo = (s_.r4 & 0xFF) - 1;
  s_.r4 = (s_.r4 & 0xFF00) | lo;
}

void Interpreter::op2A_shl_r2() {
  s_.ax = s_.r2; s_.ax = s_.ax << 1;
  std::uint8_t ah = ((s_.ax & 0xFF00) >> 8) & s_.mode;
  s_.ax = (ah << 8) | (s_.ax & 0xFF);
  s_.r2 = s_.ax;
}

void Interpreter::op2B_shl_r4lo() {
  std::uint8_t lo = (s_.r4 & 0xFF) << 1;
  s_.r4 = (s_.r4 & 0xFF00) | lo;
}

void Interpreter::op2D_shr_r2() { s_.r2 = s_.r2 >> 1; }

void Interpreter::op2E_shr_r4lo() {
  std::uint8_t lo = (s_.r4 & 0xFF) >> 1;
  s_.r4 = (s_.r4 & 0xFF00) | lo;
}

void Interpreter::op38_and() {
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  if (s_.mode != ah) {
    std::uint8_t al = s_.fetch8(); ah = s_.fetch8();
    s_.ax = (ah << 8) | al;
    s_.r2 = s_.r2 & s_.ax;
  } else {
    std::uint8_t al = s_.fetch8();
    s_.ax = (s_.ax & 0xFF00) | al;
    s_.r2 = s_.r2 & al;
  }
}

void Interpreter::op39_or_gs() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  s_.ax = s_.game_state[s_.bx];
  s_.ax += s_.game_state[(s_.bx + 1) & 0xFF] << 8;
  s_.ax |= s_.r2;
  std::uint8_t ah = ((s_.ax & 0xFF00) >> 8) & s_.mode;
  s_.ax = (ah << 8) | (s_.ax & 0xFF);
  s_.r2 = s_.ax;
}

void Interpreter::op3A_or_imm() {
  if (s_.mode == 0) {
    std::uint8_t al = s_.fetch8();
    s_.r2 = s_.r2 | al;
  } else {
    std::uint8_t al = s_.fetch8(), ah = s_.fetch8();
    s_.ax = (ah << 8) | al;
    s_.r2 = s_.r2 | s_.ax;
  }
}

void Interpreter::op3B_xor_gs() {
  std::uint8_t al = s_.fetch8();
  s_.bx = al;
  s_.ax = s_.game_state[s_.bx];
  s_.ax += s_.game_state[(s_.bx + 1) & 0xFF] << 8;
  s_.ax = s_.ax ^ s_.r2;
  std::uint8_t ah = ((s_.ax & 0xFF00) >> 8) & s_.mode;
  s_.ax = (ah << 8) | (s_.ax & 0xFF);
  s_.r2 = s_.ax;
}

void Interpreter::op3C_xor_imm() {
  if (s_.mode == 0) {
    std::uint8_t al = s_.fetch8();
    s_.r2 = s_.r2 ^ al;
  } else {
    std::uint8_t al = s_.fetch8(), ah = s_.fetch8();
    s_.ax = (ah << 8) | al;
    s_.r2 = s_.r2 ^ s_.ax;
  }
}

void Interpreter::op3E_cmp_imm() {  // 比較 r2 vs 立即數
  std::uint8_t ah, al;
  s_.bx = s_.r2;
  ah = (s_.ax & 0xFF00) >> 8;
  int cf = 0, zf = 0;
  if (s_.mode != ah) {
    al = s_.fetch8(); ah = s_.fetch8();
    s_.ax = (ah << 8) | al;
    cf = ((int)s_.bx - (int)s_.ax) < 0;       // 整數提升→有號(對照 opendw)
    zf = ((int)s_.bx - (int)s_.ax) == 0;
  } else {
    std::uint8_t bl = s_.bx & 0xFF;
    al = s_.fetch8();
    s_.ax = (s_.ax & 0xFF00) | al;
    cf = ((int)bl - (int)al) < 0;
    zf = ((int)bl - (int)al) == 0;
  }
  cf = !cf;
  s_.flags = 0;
  s_.flags |= zf << 6;
  s_.flags |= kReserved;
  s_.flags |= cf << 0;
}

void Interpreter::op41_jnc() {  // carry clear → jump
  if ((s_.flags & kCarry) == 0) { std::uint16_t a = s_.fetch16(); s_.ax = a; s_.pc = a; }
  else { s_.fetch8(); s_.fetch8(); }
}

void Interpreter::op42_jc() {   // carry set → jump
  if ((s_.flags & kCarry) == 0) { s_.fetch8(); s_.fetch8(); }
  else { std::uint16_t a = s_.fetch16(); s_.ax = a; s_.pc = a; }
}

void Interpreter::op45_jnz() {  // ZF clear → jump
  if ((s_.flags & kZero) != 0) { s_.fetch8(); s_.fetch8(); return; }
  std::uint16_t a = s_.fetch16(); s_.ax = a; s_.pc = a;
}

void Interpreter::op46_js() {   // SF set → jump
  if ((s_.flags & kSign) != 0) { op52_jmp(); return; }
  s_.fetch8(); s_.fetch8();
}

void Interpreter::op47_jns() {  // SF clear → jump
  if ((s_.flags & kSign) == 0) { op52_jmp(); return; }
  s_.fetch8(); s_.fetch8();
}

void Interpreter::op4E_set_gs_bit() {
  get_bit_mask(s_.r2 & 0xFF);
  std::uint8_t val = s_.game_state[s_.bx];
  val |= (s_.ax & 0xFF);
  set_gs(s_.bx, val);
}

void Interpreter::op4F_clr_gs_bit() {
  get_bit_mask(s_.r2 & 0xFF);
  std::uint8_t al = ~(s_.ax & 0xFF);
  std::uint8_t val = s_.game_state[s_.bx] & al;
  set_gs(s_.bx, val);
}

void Interpreter::op50_test_gs_bit() {
  get_bit_mask(s_.r2 & 0xFF);
  std::uint8_t al = s_.ax & 0xFF;
  int zf = (s_.game_state[s_.bx] & al) == 0;
  int cf = 0;
  int sf = (s_.game_state[s_.bx] & al) >= 0x80;
  std::uint16_t flags = 0;
  flags |= sf << 7;
  flags |= zf << 6;
  flags |= kReserved;
  flags |= cf << 0;
  flags &= 0xFFFE;
  s_.flags &= kCarry;
  s_.flags |= flags;
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
  // batch 2
  t[0x07] = &Interpreter::op07_r4_from_axhi;
  t[0x08] = &Interpreter::op08_gs_from_r4;
  t[0x0A] = &Interpreter::op0A_r2_from_gs;
  t[0x11] = &Interpreter::op11_gs_from_ah;
  t[0x12] = &Interpreter::op12_gs_from_r2;
  t[0x1A] = &Interpreter::op1A_gs_imm;
  t[0x23] = &Interpreter::op23_inc_gs;
  t[0x24] = &Interpreter::op24_inc_r2;
  t[0x25] = &Interpreter::op25_inc_r4lo;
  t[0x26] = &Interpreter::op26_dec_gs;
  t[0x27] = &Interpreter::op27_dec_r2;
  t[0x28] = &Interpreter::op28_dec_r4lo;
  t[0x2A] = &Interpreter::op2A_shl_r2;
  t[0x2B] = &Interpreter::op2B_shl_r4lo;
  t[0x2D] = &Interpreter::op2D_shr_r2;
  t[0x2E] = &Interpreter::op2E_shr_r4lo;
  t[0x38] = &Interpreter::op38_and;
  t[0x39] = &Interpreter::op39_or_gs;
  t[0x3A] = &Interpreter::op3A_or_imm;
  t[0x3B] = &Interpreter::op3B_xor_gs;
  t[0x3C] = &Interpreter::op3C_xor_imm;
  t[0x3E] = &Interpreter::op3E_cmp_imm;
  t[0x41] = &Interpreter::op41_jnc;
  t[0x42] = &Interpreter::op42_jc;
  t[0x45] = &Interpreter::op45_jnz;
  t[0x46] = &Interpreter::op46_js;
  t[0x47] = &Interpreter::op47_jns;
  t[0x4E] = &Interpreter::op4E_set_gs_bit;
  t[0x4F] = &Interpreter::op4F_clr_gs_bit;
  t[0x50] = &Interpreter::op50_test_gs_bit;
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
