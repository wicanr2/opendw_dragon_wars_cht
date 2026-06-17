#include "interpreter.hpp"

#include <string>
// 註:game_state 為 256-byte 區(對照 opendw struct game_state.unknown[256])。
// op_2D/op_2F 等以 cpu.bx 索引時,opendw 在 bx>255 會讀到結構外記憶體(UB,
// 正常遊玩 bx<=256)。remake 將所有 game_state 索引一律遮成 8-bit(與本檔其他
// 既有 `(bx+1) & 0xFF` 站點一致),避免在跨資源事件掃描/重放時越界 crash;
// 對 bx<=255 的合法路徑語意不變(回歸測試不受影響)。

#include "../resource/text_codec.hpp"

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
  s_.cf = 0; s_.zf = 0;  // op_3D 設 cpu.cf/zf(不動 sf,故 sf 為 sticky)
  if (s_.mode != ah) {  // word 比較
    std::uint16_t cv = s_.game_state[s_.bx & 0xFF];
    cv += s_.game_state[(s_.bx + 1) & 0xFF] << 8;
    if (s_.cx < cv) s_.cf = 1;
    if (s_.cx == cv) s_.zf = 1;
  } else {  // byte 比較
    std::uint8_t cl = s_.cx & 0xFF;
    if (cl < s_.game_state[s_.bx & 0xFF]) s_.cf = 1;
    if (cl == s_.game_state[s_.bx & 0xFF]) s_.zf = 1;
  }
  s_.cf = !s_.cf;
  std::uint16_t flags = 0;
  flags |= s_.sf << 7;  // sticky sf(對照 opendw)
  flags |= s_.zf << 6;
  flags |= kReserved;
  flags |= s_.cf << 0;
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
  s_.ax = s_.game_state[s_.bx & 0xFF];
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
  s_.ax = s_.game_state[s_.bx & 0xFF];
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
  std::uint8_t val = s_.game_state[s_.bx & 0xFF];
  val |= (s_.ax & 0xFF);
  set_gs(s_.bx, val);
}

void Interpreter::op4F_clr_gs_bit() {
  get_bit_mask(s_.r2 & 0xFF);
  std::uint8_t al = ~(s_.ax & 0xFF);
  std::uint8_t val = s_.game_state[s_.bx & 0xFF] & al;
  set_gs(s_.bx, val);
}

void Interpreter::op50_test_gs_bit() {
  get_bit_mask(s_.r2 & 0xFF);
  std::uint8_t al = s_.ax & 0xFF;
  s_.zf = (s_.game_state[s_.bx & 0xFF] & al) == 0;
  s_.cf = 0;
  s_.sf = (s_.game_state[s_.bx & 0xFF] & al) >= 0x80;
  std::uint16_t flags = 0;
  flags |= s_.sf << 7;
  flags |= s_.zf << 6;
  flags |= kReserved;
  flags |= s_.cf << 0;
  flags &= 0xFFFE;
  s_.flags &= kCarry;
  s_.flags |= flags;
}

// op_51(@0x418B):讀 2-byte operand(di = data 內偏移);掃 es[di + bl](bl 自 r4 低位
//   遞減至 0,迴圈條件 --bl != 0xFF),取最大 byte 值 → r2,該 index → r4 低位。
//   es = word_3ADF->bytes(= data_bytes)。用於戰鬥找「行動值最高的下一個 actor」。
void Interpreter::op51_argmax_data() {
  std::uint16_t di = s_.fetch8();
  di += (std::uint16_t)(s_.fetch8() << 8);
  std::uint8_t bl = (std::uint8_t)(s_.r4 & 0xFF);
  const auto& es = s_.data_bytes;
  // word_3AE2 = 0;word_3AE4 低位 = bl(對照 419E)
  s_.r2 = 0;
  s_.r4 = (s_.r4 & 0xFF00) | bl;
  // while (--bl != 0xFF):bl 先遞減,等於 0xFF(自 0 借位)時停。
  while ((std::uint8_t)(--bl) != 0xFF) {
    std::size_t idx = (std::size_t)di + bl;
    std::uint8_t al = (idx < es.size()) ? es[idx] : 0;
    if (al >= (std::uint8_t)(s_.r2 & 0xFF)) {
      s_.r2 = al;
      s_.r4 = (s_.r4 & 0xFF00) | bl;
    }
  }
}

// --- batch 3 ---
void Interpreter::set_flags() {  // 對照 opendw set_flags(讀持久 cf/zf/sf)
  s_.ax = (s_.sf << 7) | (s_.zf << 6) | kReserved | (s_.cf << 0);
  s_.ax &= 0xFFFE;        // 清新旗標的 carry
  s_.flags &= 0x0001;     // 保留舊 carry
  s_.flags |= s_.ax;
}

void Interpreter::op2F_rcr_add_gs() {
  s_.cf = s_.flags & kCarry;
  s_.flags = s_.flags >> 1;
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al; s_.bx = s_.ax;
  s_.cx = s_.game_state[s_.bx & 0xFF];
  s_.cx += (s_.game_state[(s_.bx + 1) & 0xFF] << 8);
  if (s_.mode != (s_.ax >> 8)) {
    std::uint16_t tmp = s_.r2 + s_.cx;
    s_.cf = (std::uint8_t)((unsigned)tmp << 16);  // opendw quirk:恆 0
    s_.r2 = tmp;
  } else {
    std::uint8_t b2 = s_.r2 & 0xFF;
    std::uint8_t tmp = b2 + (s_.cx & 0xFF);
    s_.cf = (std::uint8_t)((unsigned)tmp << 8);    // 恆 0
    s_.r2 = (s_.r2 & 0xFF00) | tmp;
  }
  s_.flags = (s_.flags & 0xFF00) | (((s_.flags & 0xFF) << 1) | s_.cf);
}

void Interpreter::op30_rcr_add_imm() {
  std::uint8_t cf = s_.flags & kCarry;
  s_.flags = (s_.flags & 0xFF00) | ((s_.flags & 0xFF) >> 1);
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  if (s_.mode != ah) {
    std::uint16_t ax = s_.fetch8(); ax += s_.fetch8() << 8; s_.ax = ax;
    s_.r2 += ax;
  } else {
    std::uint8_t al = s_.fetch8(); s_.ax = (s_.ax & 0xFF00) | al;
    s_.r2 += al;
  }
  s_.flags = (s_.flags & 0xFF00) | (((s_.flags & 0xFF) << 1) | cf);
}

void Interpreter::op31_rcr_sub_gs() {
  s_.cf = s_.flags & kCarry;
  s_.flags = (s_.flags & 0xFF00) | ((s_.flags & 0xFF) >> 1);
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al; s_.bx = s_.ax;
  s_.cx = s_.game_state[s_.bx & 0xFF];
  s_.cx += s_.game_state[(s_.bx + 1) & 0xFF] << 8;
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  unsigned int tmp;
  if (s_.mode != ah) {
    tmp = (unsigned int)(s_.r2 - s_.cx);
    s_.cf = (tmp & 0x10000) == 0x10000;
    s_.r2 -= s_.cx;
  } else {
    tmp = (unsigned int)(s_.r2 - (s_.cx & 0xFF));
    s_.cf = (tmp & 0x100) == 0x100;
    s_.r2 -= (s_.cx & 0xFF);
  }
  s_.cf = !s_.cf;
  s_.flags = (s_.flags & 0xFF00) | (((s_.flags & 0xFF) << 1) | s_.cf);
}

void Interpreter::op32_rcr_sub_imm() {
  s_.cf = s_.flags & kCarry;
  s_.flags = (s_.flags & 0xFF00) | ((s_.flags & 0xFF) >> 1);
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  unsigned int tmp;
  if (s_.mode != ah) {
    std::uint16_t ax = s_.fetch8(); ax += s_.fetch8() << 8; s_.ax = ax;
    tmp = (unsigned int)(s_.r2 - s_.ax);
    s_.cf = (tmp & 0x10000) == 0x10000;
    s_.r2 -= ax;
    s_.cf = !s_.cf;
    s_.flags = (s_.flags & 0xFF00) | (((s_.flags & 0xFF) << 1) | s_.cf);
  } else {
    std::uint8_t al = s_.fetch8(); s_.ax = (s_.ax & 0xFF00) | al;
    std::uint8_t b2 = s_.r2 & 0xFF;
    tmp = (unsigned int)(b2 - al);
    s_.cf = (tmp & 0x100) == 0x100;
    s_.cf = !s_.cf;
    b2 -= al;
    s_.r2 = (s_.r2 & 0xFF00) | b2;
    s_.flags = (s_.flags & 0xFF00) | (((s_.flags & 0xFF) << 1) | s_.cf);
  }
}

void Interpreter::op48_set_gs_msb() {
  s_.flags &= 0xBF;  // clear_sign_flag()(opendw 實際清 0x40)
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al; s_.bx = s_.ax;
  if (s_.game_state[s_.bx & 0xFF] < 0x80) {
    set_gs(s_.bx, s_.game_state[s_.bx & 0xFF] | 0x80);
    s_.flags |= 0x40;  // set_sign_flag()(實際設 0x40)
  }
}

void Interpreter::op49_loop() {
  std::uint8_t b = (s_.r4 & 0xFF) - 1;
  s_.r4 = (s_.r4 & 0xFF00) | b;
  if (b != 0xFF) { std::uint16_t a = s_.fetch16(); s_.pc = a; }
  else { s_.fetch8(); s_.fetch8(); }
}

void Interpreter::op4A_loop_eq() {
  std::uint8_t b = (s_.r4 & 0xFF) + 1;
  s_.r4 = (s_.r4 & 0xFF00) | b;
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  if (al == b) { s_.fetch8(); s_.fetch8(); }
  else { std::uint16_t a = s_.fetch16(); s_.ax = a; s_.pc = a; }
}

void Interpreter::op66_test_gs() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al; s_.bx = s_.ax;
  s_.zf = 0; s_.cf = 0; s_.sf = 0;
  s_.cx = s_.game_state[s_.bx & 0xFF];
  s_.cx += (s_.game_state[(s_.bx + 1) & 0xFF] << 8);
  if (s_.mode == (s_.ax >> 8)) {
    std::uint8_t cl = s_.cx & 0xFF;
    if (cl == 0) s_.zf = 1;
    if (cl >= 0x80) s_.sf = 1;
  } else {
    if (s_.cx == 0) s_.zf = 1;
    if (s_.cx >= 0x8000) s_.sf = 1;
  }
  std::uint16_t flags = (s_.sf << 7) | (s_.zf << 6) | kReserved | (s_.cf << 0);
  flags &= 0xFFFE;
  s_.flags &= 0x0001;
  s_.flags |= flags;
  s_.ax = flags;
}

void Interpreter::op9A_set_gs_ff() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al; s_.bx = s_.ax;
  al = 0xFF; s_.ax = (s_.ax & 0xFF00) | al;
  set_gs(s_.bx, al);
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  if (s_.mode != ah) set_gs(s_.bx + 1, al);
}

void Interpreter::op9B_set_gs_bit() {
  std::uint8_t al = s_.fetch8();
  get_bit_mask(al);
  set_gs(s_.bx, s_.game_state[s_.bx & 0xFF] | (s_.ax & 0xFF));
}

void Interpreter::op9D_test_gs_bit() {
  std::uint8_t al = s_.fetch8();
  get_bit_mask(al);
  s_.cf = 0;
  s_.zf = (s_.game_state[s_.bx & 0xFF] & s_.ax) == 0 ? 1 : 0;
  set_flags();
}

// --- 字串輸出 opcode ---
// 在 pc 處用 text_codec 解一條字串、推進 pc 到字串結束處(對照 opendw set_msg:
// cpu.pc = base_pc + extract_string(...)),並以 (起始 offset, 英文原文) 回呼 sink。
void Interpreter::emit_string() {
  std::size_t start = s_.pc;
  auto [str, next] = text::decode(s_.script, start);
  s_.pc = next;
  if (msg_sink_) msg_sink_(start, str);
}
void Interpreter::op78_set_msg() { emit_string(); }
void Interpreter::op7B_ui_header() { emit_string(); }            // header,文字路徑同
void Interpreter::op77_draw_and_set() { emit_string(); }          // (draw_pattern 副作用屬 render,略)

// op_79(@0x47FA,DRAGON.COM 反組譯 — opendw targets[] 標 NULL,無 C oracle):
//   反組譯(file off=0x46FA,COM @CS:0x100):
//     47FA: 56            push si
//     47FB: 0E 07         push cs / pop es        ; es = cs(程式段)
//     47FD: E8 80 EB      call 0x3380             ; draw_pattern(重繪 viewport + 設 dirty)
//     4800: 5E            pop si
//     ── 此處「落入」op_7A(@0x4801)──:
//     4801: 56 0E 07 …    push si / push cs / pop es
//     4804: 8B 1E E2 3A   mov bx, [0x3AE2]        ; bx = word_3AE2(資料資源內偏移)
//     4808: 8B 0E DF 3A   mov cx, [0x3ADF]        ; cx = word_3ADF(資料資源段)
//     480C: E8 6A D4      call 0x1C79             ; extract_string(word_3ADF->bytes, r2)
//     480F: 89 1E E2 3A   mov [0x3AE2], bx        ; word_3AE2 = 下一條起點
//     4813: 5E            pop si
//   結論:op_79 = draw_pattern + op_7A,與「op_77 = draw_pattern + op_78」對稱
//     (op_77@0x47E3 同樣 call 0x3380 後落入 op_78@0x47EC)。draw_pattern(0x3380)
//     **不消耗任何 operand、不 emit 字串**,純 render 副作用(重繪圖案、設 dirty 旗標);
//     VM 可見效應只有 op_7A 那段:從 word_3ADF->bytes 的 word_3AE2 偏移解一條字串
//     emit、r2 = 下一條起點。remake 的 op_77 既已等同 op_78(略 draw_pattern),
//     對稱地 op_79 等同 op_7A。
//   交叉驗證:主線 15 格(area 1/2/6/8/17/29)此前 halt 於 op_79;實作後事件文字
//     確實 emit(見 docs/52 重跑),語意合理 → 高信心。
void Interpreter::op79_draw_and_emit_data() { op7A_emit_data_string(); }

// op_5B(get_map_tile_data @0x427A,opendw 有 body — 對拍移植):
//   opendw op_5B_unused(engine.c:2510):
//     dl = game_state[1](Y);bl = game_state[0](X);
//     cpu.dx = (dx&0xFF00)|dl;cpu.bx = (bx&0xFF00)|bl;
//     get_map_tile_data(dl, bl);            ; 算當前格 tile 資料 → word_11C6/word_11C8
//     data_5521[word_551F + 2] = 0;          ; 清該格事件 flag 第 3 byte
//   get_map_tile_data(@0x5206):以 (X,Y) 經 wrap → di = 3*Y + data_5A04[X+1];
//     word_551F = di;word_11C6 = level_data[di] | level_data[di+1]<<8;
//     word_11C8 = level_data[di+2];若 byte_551E&0x80 → 清零並 cf=1。
//   remake VM(headless 事件抽取)無 data_5521/data_5A04/byte_551E 等 level runtime
//     狀態(level grid 不在 VmState),故無法 byte-exact 重算 tile 索引。但本 opcode
//     的「VM 可見」副作用只有 cpu.dx/cpu.bx(載入 X/Y)與 word_11C6/word_11C8、cf。
//     忠實對齊「VM 暫存器」層:dx=gs[1]、bx=gs[0];清 cf(對照 0x5234 預設 cf=0)。
//     word_11C6/11C8/word_551F 需 level grid 才能填,headless 無此資源 → 不臆造數值,
//     維持 0(與「未踩到特殊事件格」一致;area 5/27/30 各 1 格用到,僅為流程通過,
//     不依賴回填的 tile 值做後續分歧——若依賴,該格會再 halt 於讀取處而非靜默誤算)。
//   標示:get_map_tile_data 的 level-grid 重算未在 headless VM 復刻(無 level runtime
//     狀態);op_5B 在此提供 VM 暫存器層對齊,使 area 5/27/30 的事件格不再 halt。
void Interpreter::op5B_get_map_tile() {
  std::uint8_t dl = s_.game_state[1];  // Y
  std::uint8_t bl = s_.game_state[0];  // X
  s_.dx = (std::uint16_t)((s_.dx & 0xFF00) | dl);
  s_.bx = (std::uint16_t)((s_.bx & 0xFF00) | bl);
  // get_map_tile_data 的 level-grid 重算(word_551F/word_11C6/word_11C8)需 level
  //   runtime 狀態,headless VM 無此資源 → 不回填臆造值;清 carry(對照 0x5234)。
  s_.cf = 0;
  s_.flags &= 0xFFFE;
}

// --- batch 4:繪圖 / UI / 結束 ---
// op_73:al=gs[0x3F]; gs[0x3E]=al(清/設事件旗標)
void Interpreter::op73_clear_event() { set_gs(0x3E, s_.game_state[0x3F]); }
// op_74:畫框,讀 4 byte(x,y,w,h)。remake 由 framebuffer 自行畫;VM 僅消耗 operand。
void Interpreter::op74_draw_frame() { s_.fetch8(); s_.fetch8(); s_.fetch8(); s_.fetch8(); }
// op_75:ui_draw_full(無 operand);繪圖屬 render,VM 無副作用。
void Interpreter::op75_ui_full() {}
// op_76:draw_pattern(無 operand)。
void Interpreter::op76_draw_pattern() {}
// op_5A:script 結束/返回(對照 opendw run_script:op_5A 還原 saved_stack/word_3ADB/
// word_3AE8 後,run_script 迴圈 done=1 → 回到呼叫它的 C 函式)。
//   - 若目前在 op_5C 觸發的 run_script 子框內 → 標記 returned,跳出該子框迴圈(return 上層)。
//   - 否則(最外層腳本)→ halt 整個執行。
// 註:op_58/op_59 的跨資源 call 是「bytecode 內」的 call/ret,不經 run_script,
//     因此 op_5A 不負責 pop op_58 的 call_stack;那是 op_59 的事。
void Interpreter::op5A_ret() {
  if (!s_.script_frames.empty()) {
    s_.script_frames.back().returned = true;  // 跳出該 run_script 迴圈
  } else {
    s_.halted = true;
  }
}
// op_8A:隨機遭遇(對照 op_8A @0x498E → trigger_random_encounter @0x4C47)。
//   opendw 的 trigger_random_encounter 僅:byte_4F0F=怪物id、載入圖形資源、
//   init_monster_animation、byte_4F2B=0xFF —— 全是「圖形/動畫」副作用,
//   **不寫任何戰鬥數值到 game_state/char_data**(已逐行確認 engine.c:4818)。
//   故 headless 結算路徑可略過圖形:只記錄怪物 id(= word_3AE2 低位)後繼續。
//   預設(headless_encounter=false)維持 halt,既有測試/遊戲流程不變。
void Interpreter::op8A_encounter() {
  s_.ax = s_.r2;  // 對照 op_8A:cpu.ax = word_3AE2
  if (s_.headless_encounter) {
    s_.encounter_monster_id = static_cast<std::uint8_t>(s_.r2 & 0xFF);
    // 不 halt:略過圖形載入(render leaf),讓戰鬥腳本繼續跑結算。
    return;
  }
  s_.halted = true;
}

// --- batch 5:跨資源 call / 資料資源存取 / 流程 / PRNG ---

// 用 resource_provider 取資源 idx 的 bytes;成功填入 out 回 true。
bool Interpreter::load_resource(int idx, std::vector<std::uint8_t>& out) {
  if (!s_.resource_provider) return false;
  auto r = s_.resource_provider(idx);
  if (!r) return false;
  out = std::move(*r);
  return true;
}

// 取資源 index 的持久 bytes(對照 resource_get_by_index → allocations[idx])。
//   優先指向當前 data_bytes / script(同一份,寫入立即可見);否則用 res_cache
//   (miss 時以 provider 載入後快取)。回 nullptr 表無法取得。
std::vector<std::uint8_t>* Interpreter::res_bytes_by_index(int idx) {
  if (idx == s_.data_res && !s_.data_bytes.empty()) return &s_.data_bytes;
  if (idx == s_.script_res && !s_.script.empty()) return &s_.script;
  auto it = s_.res_cache.find(idx);
  if (it != s_.res_cache.end()) return &it->second;
  std::vector<std::uint8_t> loaded;
  if (!load_resource(idx, loaded)) return nullptr;
  auto [ins, ok] = s_.res_cache.emplace(idx, std::move(loaded));
  return &ins->second;
}

// op_0C:word_3AE2(r2) = word_3ADF->bytes[operand](2-byte LE),高位以 byte_3AE1(mode)遮罩。
void Interpreter::op0C_r2_from_data() {
  s_.ax = s_.fetch8();
  s_.ax += (std::uint16_t)(s_.fetch8() << 8);
  s_.bx = s_.ax;
  const auto& d = s_.data_bytes;
  std::uint16_t bx = s_.bx;
  std::uint16_t lo = (bx < d.size()) ? d[bx] : 0;
  std::uint16_t hi = ((std::size_t)(bx + 1) < d.size()) ? d[bx + 1] : 0;
  s_.ax = (std::uint16_t)(lo | (hi << 8));
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  ah &= s_.mode;  // byte_3AE1
  s_.ax = (std::uint16_t)((ah << 8) | (s_.ax & 0xFF));
  s_.r2 = s_.ax;
}

// op_1C:word_3ADF->bytes[operand] = imm;word 模式再寫一 byte 到 +1。
void Interpreter::op1C_data_store() {
  std::uint8_t save_ah = (s_.ax & 0xFF00) >> 8;
  s_.ax = s_.fetch8();
  s_.ax += (std::uint16_t)(s_.fetch8() << 8);
  s_.di = s_.ax;
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.wdata(s_.di, al);
  if (s_.mode != save_ah) {  // byte_3AE1 != save_ah → word 模式
    al = s_.fetch8();
    s_.ax = (s_.ax & 0xFF00) | al;
    s_.wdata((std::size_t)s_.di + 1, al);
  }
}

// op_40:cmp byte(r4) vs imm → 設 word_3AE6 旗標(zf/cf,反相 cf)。
void Interpreter::op40_cmp_r4_imm() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint8_t b4 = s_.r4 & 0xFF;
  int cf = ((int)b4 - (int)al) < 0;
  int zf = ((int)b4 - (int)al) == 0;
  cf = !cf;
  std::uint16_t flags = 0;
  flags |= zf << 6;
  flags |= kReserved;
  flags |= cf << 0;
  s_.flags = flags;
}

// op_43:al = flags & 0x41;若 == 1(carry set 且 zero clear)→ jmp,否則消耗 2 byte。
void Interpreter::op43_jump_above() {
  std::uint8_t al = s_.flags & 0x41;
  if (al == 1) { op52_jmp(); return; }
  s_.fetch8(); s_.fetch8();
}

// op_4D:PRNG。update_random_seed(): ax=ticks; ax+=seed; seed=ax。
//   mul = ax * r2;r2 = (mul>>16) byte;byte_3AE1!=0(word 模式)取 word。
void Interpreter::op4D_prng() {
  s_.ax = ++s_.fake_ticks;            // sys_ticks() 的可重現替身
  s_.ax += s_.random_seed;
  s_.random_seed = s_.ax;
  std::uint32_t mul = (std::uint32_t)s_.ax * (std::uint32_t)s_.r2;
  s_.r2 = (std::uint16_t)((mul & 0x00FF0000u) >> 16);
  if (s_.mode != 0)  // byte_3AE1 != 0 → word 模式
    s_.r2 = (std::uint16_t)((mul & 0xFFFF0000u) >> 16);
}

// op_58:跨資源 script call(對照 op_58 @0x4239)。
//   讀 tag(1B)+ src_offset(2B);push 返回 context(si/word_3AE8/dl);
//   依 tag 載入目標資源(find_index_by_tag/resource_load);切 running_script/word_3ADF;
//   跳到目標資源的 src_offset。
void Interpreter::op58_xcall() {
  std::uint8_t tag = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | tag;
  std::uint16_t tag_item = s_.ax;
  s_.ax = s_.fetch8();
  s_.ax += (std::uint16_t)(s_.fetch8() << 8);
  std::uint16_t src_offset = s_.ax;

  // 診斷 hook(不改行為):回報目標資源 + offset + 返回 pc。
  if (xcall_obs_) xcall_obs_(tag_item, src_offset, (std::uint16_t)s_.pc);

  // 載入目標資源 bytes。tag_item = 資源 tag/section。
  std::vector<std::uint8_t> bytes;
  if (!load_resource(tag_item, bytes)) {
    // 無法解析資源:還原無變化、停在未實作(由 run() 記為 blocker)。
    // 不 push、不切換 → 標記 last_unimpl 0x58 並 halt(維持「不變成 no-op」)。
    last_unimpl_ = 0x58;
    s_.halted = true;
    return;
  }

  // --- 對齊 opendw op_58(@0x4239)的 byte-stack 紀律 ---
  // 1) push_word(si):si = 返回 offset(當前 pc)。
  std::uint16_t si = static_cast<std::uint16_t>(s_.pc);
  s_.push_word(si);
  // 2) push_byte(word_3AE8):返回後要還原的程式資源索引。
  s_.push_byte(static_cast<std::uint8_t>(s_.script_res));
  // 3) push_byte(dl):usage_type 旗標。remake 無 usage_type 概念 → 比照
  //    cache-miss(resource_load)語意用 0xFF;對段落號 N 無影響(op_55 只把它 pop 丟棄)。
  std::uint8_t dl = 0xFF;
  s_.push_byte(dl);

  // call_stack 僅用來保存「返回後要還原的 script/data bytes vector」
  // (resource_provider 用 index 反查 section bytes 可能不可靠)。
  // si/word_3AE8/dl 的「值」已在 byte-stack 上(供 op_55 peek);
  // op_59 會先從 byte-stack pop 回它們(平衡堆疊),再從 call_stack 取回 bytes。
  VmState::CallFrame fr;
  // 跨資源 call(目標 != 當前資源):把「即將被暫停的當前資源」live bytes 種進
  //   res_cache[script_res]。讓子資源對它的「跨資源 op_17 寫入」(res_bytes_by_index
  //   命中 res_cache)落在**含當前所有自改(怪物 setup / 角色動作陣列)的同一份 buffer**
  //   上,而非 provider 全新版;返回時(op_59 cross_res 分支)再取回。
  //   同資源葉子子程式(如骰子 0x06EC)不種 → 維持 §11 自我修改碼語意。
  if ((int)tag_item != s_.script_res) {
    s_.res_cache[s_.script_res] = s_.script;
  }
  fr.script = std::move(s_.script);   // 返回後仍跑當前腳本 bytes
  fr.pc = si;                          // si = 返回 offset(備援)
  fr.data_bytes = std::move(s_.data_bytes);
  fr.script_res = s_.script_res;
  fr.data_res = s_.data_res;
  fr.dl = dl;
  s_.call_stack.push_back(std::move(fr));

  // 切到目標資源:word_3AE8 = word_3AEA = 目標 → running_script/word_3ADF 同一份。
  s_.script = bytes;
  s_.data_bytes = bytes;
  s_.script_res = tag_item;
  s_.data_res = tag_item;
  s_.pc = src_offset;
}

// op_59:op_58 的返回(對照 op_59 @0x41C8)。pop context(dl/word_3AE8/si),切回上層。
void Interpreter::op59_xret() {
  if (s_.call_stack.empty()) { s_.halted = true; return; }  // 無對應 call → 收尾

  // --- 對齊 opendw op_59(@0x41C8)的 byte-stack 紀律 ---
  // opendw:ah!=stack[sp] → resource_set_flagged(word_3AE8)。remake 不模擬
  //   資源 flagged 狀態,略此副作用,但仍須照樣 pop 以平衡堆疊。
  // cpu.ax = pop_word(); ah = ax>>8; word_3AE8 = word_3AEA = ah;(dl 在低位,丟棄)
  std::uint16_t w = s_.pop_word();
  std::uint8_t restored_script_res = (w & 0xFF00) >> 8;  // = 原 word_3AE8
  // si = pop_word();
  std::uint16_t si = s_.pop_word();

  VmState::CallFrame fr = std::move(s_.call_stack.back());
  s_.call_stack.pop_back();

  int child_res = s_.script_res;  // 即將離開的子資源
  // opendw 靠 word_3AE8 重新 resolve running_script(populate_3ADD_and_3ADF)。
  //   ── 跨資源返回(child != parent):上層在子資源執行期間可能被「跨資源 op_17」
  //      寫過(寫入落在 res_cache[parent]);故還原上層時優先取 res_cache[parent]
  //      (含 action 陣列等寫入),取出即移除避免陳舊;否則用 call frame 備份。
  //   ── 同資源返回(child == parent,如 res3→res3 葉子/骰子):用 call frame 備份還原 →
  //      維持 §11 自我修改碼「框內有效、返回丟棄」語意,verify_combat_script 不回歸。
  bool cross_res = (child_res != (int)restored_script_res);
  if (cross_res) {
    auto it = s_.res_cache.find((int)restored_script_res);
    if (it != s_.res_cache.end()) {
      s_.script = std::move(it->second);
      s_.res_cache.erase(it);
    } else {
      s_.script = std::move(fr.script);
    }
    s_.data_bytes = s_.script;
  } else {
    s_.script = std::move(fr.script);
    s_.data_bytes = std::move(fr.data_bytes);
  }
  s_.script_res = restored_script_res;  // 對齊 opendw:來自 byte-stack pop 的 word_3AE8
  s_.data_res = fr.data_res;
  s_.pc = si;  // si:返回 offset(來自 byte-stack)
}

// run_script(對照 run_script @0x6413):push run_script 框、切資源、從 src_offset 跑到 op_5A。
void Interpreter::run_script(int script_index, std::uint16_t src_offset) {
  // 對齊 opendw:resource_get_by_index 回傳「持久 allocation」(同一份 buffer)。
  //   ── 同一資源(script_index == script_res,如 res3 戰鬥 for_call 0x0761 逐角色):
  //      **沿用 live s_.script**(含自我修改,如逐角色 initiative 陣列 0x04EA),且
  //      返回時**保留**子迴圈所做的自改 → 不存/不還原 bytes vector(否則丟失寫入)。
  //   ── 不同資源:存舊 bytes、載入新資源、返回時還原舊 bytes。
  bool same_res = (script_index == s_.script_res);

  VmState::ScriptFrame fr;
  fr.pc = s_.pc;
  fr.script_res = s_.script_res;
  fr.data_res = s_.data_res;
  fr.mode = s_.mode;
  if (!same_res) {
    fr.script = s_.script;
    fr.data_bytes = s_.data_bytes;
  }
  s_.script_frames.push_back(std::move(fr));

  if (!same_res) {
    std::vector<std::uint8_t> bytes;
    if (load_resource(script_index, bytes)) {
      s_.script = std::move(bytes);
      s_.data_bytes = s_.script;
      s_.script_res = script_index;
      s_.data_res = script_index;
    }
  } else {
    s_.data_bytes = s_.script;   // 同資源:data_bytes 與 script 同一份(自改可見)
    s_.data_res = s_.script_res;
  }
  s_.pc = src_offset;

  // 子迴圈:跑到 op_5A(returned=true)或 halt/越界。
  while (!s_.halted && !s_.script_frames.back().returned &&
         s_.pc < s_.script.size()) {
    std::size_t at = s_.pc;
    std::uint8_t op = s_.fetch8();
    s_.ax = op;  // 對照 opendw run_script @0x3ACF(cpu.ax = op_code; cpu.bx = cpu.ax)
    s_.bx = op;
    if (trace_) trace_->record({at, op, s_.r2, s_.r4, s_.flags, s_.mode});
    Handler h = kImpl[op];
    if (!h) { last_unimpl_ = op; s_.halted = true; break; }
    (this->*h)();
  }

  // 還原上層框。
  VmState::ScriptFrame back = std::move(s_.script_frames.back());
  s_.script_frames.pop_back();
  if (!same_res) {
    s_.script = std::move(back.script);
    s_.data_bytes = std::move(back.data_bytes);
  } else {
    s_.data_bytes = s_.script;   // 同資源:保留 s_.script(含子迴圈自改)
  }
  s_.script_res = back.script_res;
  s_.data_res = back.data_res;
  s_.mode = back.mode;
  s_.pc = back.pc;
}

// op_5C:依 gs[0x1F](隊伍人數)重複 run_script(子 script 迴圈)。
void Interpreter::op5C_party_loop() {
  s_.mode = 0;  // set_byte_mode()
  s_.r2 &= 0xFF;
  s_.ax = s_.fetch8();
  s_.ax += (std::uint16_t)(s_.fetch8() << 8);
  std::uint16_t sub_offset = s_.ax;  // word_42D6
  std::size_t resume_pc = s_.pc;     // word_3ADB(返回此處續跑)

  if (s_.game_state[0x1F] == 0) return;  // 隊伍空 → 不跑子迴圈

  std::uint8_t saved6 = s_.game_state[6];
  s_.game_state[6] = 0;
  do {
    run_script(s_.script_res, sub_offset);  // 同一資源、子 offset
    if (s_.halted) break;
    s_.game_state[6]++;
  } while (s_.game_state[6] < s_.game_state[0x1F]);
  s_.game_state[6] = saved6;
  s_.pc = resume_pc;  // jmp 0x3AC7:回主 script 續跑
}

// op_62(op_scan_for_char @0x43BF):掃描隊伍找「屬性 dl >= 門檻 cl」的第一名角色。
//   開頭 byte_3AE6 >>= 1;讀 dl(property offset)、cl(threshold);
//   迴圈 bx=0..gs[0x1F]-1:設 gs[6]=bx;data = get_player_data_byte(bx, dl)
//                          = char_data[bx*512 + dl];
//     若 data >= cl → byte_3AE6 <<= 1(還原)、return(命中,不設 carry);
//   迴圈走完未命中 → cpu.cf = 1(注意:oracle 此分支「不」呼叫 set_flags,
//     僅設 cpu.cf;word_3AE6 維持迴圈中 >>1 後的值,carry bit 由後續指令解讀)。
void Interpreter::op62_scan_char() {
  std::uint8_t b6 = (std::uint8_t)((s_.flags & 0xFF) >> 1);  // byte_3AE6 >>= 1
  s_.flags = (std::uint16_t)((s_.flags & 0xFF00) | b6);
  std::uint8_t dl = s_.fetch8();   // property offset
  std::uint8_t cl = s_.fetch8();   // threshold
  s_.dx = dl;
  std::uint16_t bx = 0;
  std::uint8_t limit = s_.game_state[0x1F];
  for (; bx < limit; ++bx) {
    set_gs(6, (std::uint8_t)bx);
    std::uint32_t addr = (std::uint32_t)bx * 512u + dl;
    std::uint8_t data = (addr < s_.char_data.size()) ? s_.char_data[addr] : 0;
    if (data >= cl) {
      std::uint8_t b6b = (std::uint8_t)((s_.flags & 0xFF) << 1);  // byte_3AE6 <<= 1(還原)
      s_.flags = (std::uint16_t)((s_.flags & 0xFF00) | b6b);
      return;  // 命中:不設 carry
    }
  }
  s_.cf = 1;          // 未命中:僅設 cpu.cf=1。
  // 注意:oracle 此分支「不」呼叫 set_flags,也不寫 word_3AE6 → flags 維持迴圈中
  //   byte_3AE6 >>= 1 後的值(carry 只活在 cpu.cf,由後續指令解讀)。不可在此 |= kCarry。
}

// --- batch 6:byte 堆疊存取 / 資料資源讀 / 比較 / viewport ---

// op_04:push byte(word_3AE8)。配 op_03 用於暫存/還原資料資源索引。
void Interpreter::op04_push_script_res() {
  std::uint8_t al = s_.script_res & 0xFF;
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.push_byte(al);
}

// op_03:pop byte → word_3AEA(word_3ADF 指向的資料資源),重新 populate。
//   data_res 改變時用 resource_provider 取對應 bytes;同 script_res 則用當前 script。
void Interpreter::op03_pop_data_res() {
  std::uint8_t al = s_.pop_byte();
  s_.ax = (s_.ax & 0xFF00) | al;
  int new_data = al;  // word_3AEA = al(byte 索引)
  s_.data_res = new_data;
  if (new_data == s_.script_res) {
    s_.data_bytes = s_.script;  // running_script 與 word_3ADF 同一份
  } else {
    std::vector<std::uint8_t> bytes;
    if (load_resource(new_data, bytes)) s_.data_bytes = std::move(bytes);
    // 無 provider/解析失敗:保留現有 data_bytes(避免崩壞),仍記錄索引。
  }
}

// op_0D:r2 = word_3ADF->bytes[operand + r4](2-byte LE,高位以 mode 遮罩)。
void Interpreter::op0D_r2_from_data_off() {
  s_.ax = s_.fetch8();
  s_.ax += (std::uint16_t)(s_.fetch8() << 8);
  s_.ax += s_.r4;
  s_.bx = s_.ax;
  const auto& d = s_.data_bytes;
  std::uint16_t bx = s_.bx;
  std::uint16_t lo = (bx < d.size()) ? d[bx] : 0;
  std::uint16_t hi = ((std::size_t)(bx + 1) < d.size()) ? d[bx + 1] : 0;
  s_.ax = (std::uint16_t)(lo | (hi << 8));
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  ah &= s_.mode;
  s_.ax = (std::uint16_t)((ah << 8) | (s_.ax & 0xFF));
  s_.r2 = s_.ax;
}

// op_3F:cmp byte(r4) vs gs[operand] → 設旗標(zf/cf,反相 cf)。
void Interpreter::op3F_cmp_r4_gs() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  std::uint8_t b4 = s_.r4 & 0xFF;
  std::uint8_t gv = s_.game_state[s_.bx & 0xFF];
  int zf = (b4 == gv);
  int cf = (b4 < gv);
  cf = !cf;
  std::uint16_t flags = 0;
  flags |= zf << 6;
  flags |= kReserved;
  flags |= cf << 0;
  s_.flags = flags;
}

// op_55:peek word→r2、pop byte;word 模式(ah!=byte_3AE1)再 pop byte 並取整 word。
void Interpreter::op55_peek_pop_r2() {
  s_.cx = s_.peek_word();
  s_.pop_byte();
  s_.r2 = s_.cx & 0xFF;
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  if (ah != s_.mode) {  // byte_3AE1
    s_.r2 = s_.cx;
    s_.pop_byte();
  }
}

// op_56:push r2(word 模式 push word,否則 push byte)。
void Interpreter::op56_push_r2() {
  s_.cx = s_.r2;
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  if (s_.mode != ah) s_.push_word(s_.cx);
  else s_.push_byte(s_.cx & 0xFF);
}

// op_8B:refresh_viewport(畫面更新);VM 抽取期無副作用。
void Interpreter::op8B_refresh_viewport() {}

// op_91(@0x4F2..):draw_player_status_panel()。純渲染 leaf,VM 結算狀態無副作用。
void Interpreter::op91_status_panel() {}

// op_92(@0x49FD):draw_player_status_panel + ui_draw_string + 延遲計時器輪詢輸入。
//   oracle(engine.c:5940):
//     draw_player_status_panel(); ui_draw_string();           ; 純渲染 leaf
//     bx = gs[0xDC]; if (bx != 0) exit(1);                    ; 斷言 gs[0xDC]==0(否則 opendw 自身亦 abort)
//     timer4 = data_4A5B[0];                                  ; 設延遲計時
//     while (timer4) { poll_mouse(); if (clicked!=0x80){...}  ; 滑鼠未點 → 0x4A57 return(無副作用)
//                      else { key=get_key();... } tick; delay }
//   headless:無滑鼠/鍵盤/計時器。確定性分支 = mouse「未點」(clicked!=0x80)→ 0x4A57 直接 return,
//     全程不寫任何 game_state(僅渲染 + 等待逾時)。故對 VM 結算狀態而言為 no-op。
//     gs[0xDC]!=0 時 opendw 自身會 abort(unhandled);此處標 last_unimpl 不臆造、halt。
void Interpreter::op92_status_delay() {
  if (s_.game_state[0xDC] != 0) {       // 對照 oracle:bx!=0 → exit(1)
    last_unimpl_ = 0x92;
    s_.halted = true;
    return;
  }
  // 渲染 + 延遲輪詢:headless 取「無輸入/逾時」確定性分支 → return,無 game_state 副作用。
}

// op_82(@0x48D2):讀 1 byte operand → bx;w11C6 = gs[bx]|gs[bx+1]<<8、w11C8 = gs[bx+2]|gs[bx+3]<<8;
//   print_number_9_digits()(render leaf,把 w11C6:w11C8 組成的數字 emit)。
//   VM 可見副作用:消耗 1 operand + 設 w11C6/w11C8;列印為渲染,VM 結算狀態不變。
void Interpreter::op82_print_9digits() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint16_t bx = al;
  s_.bx = bx;
  s_.w11C6 = (std::uint16_t)(s_.game_state[bx & 0xFF] |
                             (s_.game_state[(bx + 1) & 0xFF] << 8));
  s_.w11C8 = (std::uint16_t)(s_.game_state[(bx + 2) & 0xFF] |
                             (s_.game_state[(bx + 3) & 0xFF] << 8));
  // print_number_9_digits():渲染 leaf,VM 狀態不變。
}

// op_97(load_char_data @0x42FB):r2 = char_data[ base + operand + r4 ](byte/word,mode 遮罩)。
//   oracle:bx = 0xC960; bh += gs[gs[6]+0xA]; bx += operand; bx += word_3AE4;
//          cx = c960[bx-0xC960](word);r2 = cl;若 byte_3AE1 != ax 高位 → r2 = cx(word)。
//   = op_5D 同定址(char_record_base = selector<<8),但多加 r4(word_3AE4)。
void Interpreter::op97_load_char_data() {
  std::uint16_t base = char_record_base();
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint16_t addr = (std::uint16_t)(base + al + s_.r4);
  std::uint8_t cl = (addr < s_.char_data.size()) ? s_.char_data[addr] : 0;
  std::uint8_t ch = ((std::size_t)addr + 1 < s_.char_data.size()) ? s_.char_data[addr + 1] : 0;
  s_.cx = (std::uint16_t)((ch << 8) | cl);
  s_.r2 = (std::uint16_t)(s_.r2 & 0xFF00) | cl;
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) {  // byte_3AE1 != ax 高位 → word
    s_.r2 = s_.cx;
  }
}

// op_98(store_char_data @0x4348):op_97 的「寫」孿生。
//   oracle:di = gs[6];gs[di+0x18] = ax 高位(dispatch 後 = 0);
//          bx = 0xC960; bh += gs[di+0xA]; bx += operand; bx += r4; cx = r2;
//          c960[bx-0xC960] = cl;若 byte_3AE1 != ax 高位 → c960[..+1] = ch。
void Interpreter::op98_store_char_data() {
  std::uint8_t player_idx = s_.game_state[6];
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;          // dispatch 後 = 0
  s_.game_state[(player_idx + 0x18) & 0xFF] = ah;   // gs[gs[6]+0x18] = ah
  std::uint16_t base = char_record_base();
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint16_t addr = (std::uint16_t)(base + al + s_.r4);
  s_.cx = s_.r2;
  if (addr < s_.char_data.size()) s_.char_data[addr] = s_.cx & 0xFF;
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) {
    if ((std::size_t)addr + 1 < s_.char_data.size())
      s_.char_data[addr + 1] = (s_.cx & 0xFF00) >> 8;
  }
}

// --- batch 7:gamestate/資源讀 + r4 byte 堆疊 ---

// op_0B:r2 = game_state[operand + r4](2-byte,高位以 mode 遮罩)。
void Interpreter::op0B_r2_from_gs_off() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.ax += s_.r4;
  s_.bx = s_.ax;
  std::uint8_t lo = s_.game_state[s_.bx & 0xFF];
  std::uint8_t hi = s_.game_state[(s_.bx + 1) & 0xFF];
  hi &= s_.mode;
  s_.ax = (std::uint16_t)((hi << 8) | lo);
  s_.r2 = s_.ax;
}

// op_0F(extract_resource_data):
//   bx=operand;di = gs[bx] | gs[bx+1]<<8(base off);res_idx = gs[bx+2];
//   載入 res_idx,讀其 bytes[di + r4](word,mode 遮罩)→ r2。
void Interpreter::op0F_r2_from_res() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  std::uint16_t bx = s_.bx & 0xFF;
  std::uint16_t di = s_.game_state[bx];
  di += (std::uint16_t)(s_.game_state[(bx + 1) & 0xFF] << 8);
  std::uint8_t res_idx = s_.game_state[(bx + 2) & 0xFF];
  di += s_.r4;

  // 取資源 bytes:走 res_bytes_by_index(data_res/script_res/持久快取),
  //   讓 op_17 的寫入後續被 op_0F 讀到(對照 allocations[] 持久語意)。
  const std::vector<std::uint8_t>* bytes = res_bytes_by_index(res_idx);

  std::uint16_t lo = 0, hi = 0;
  if (bytes) {
    if (di < bytes->size()) lo = (*bytes)[di];
    if ((std::size_t)(di + 1) < bytes->size()) hi = (*bytes)[di + 1];
  }
  hi &= s_.mode;
  s_.ax = (std::uint16_t)((hi << 8) | lo);
  s_.r2 = s_.ax;
}

// op_93:push byte(r4 低位)。配 op_94 暫存/還原 r4。
void Interpreter::op93_push_r4() {
  std::uint8_t al = s_.r4 & 0xFF;
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.push_byte(al);
}

// op_94:pop byte → r4 低位。
void Interpreter::op94_pop_r4() {
  std::uint8_t al = s_.pop_byte();
  s_.r4 = (s_.r4 & 0xFF00) | al;
}

// --- batch 8:gs-索引資料讀寫(word_3ADF)+ gs offset 寫 ---

// op_10:di=op1;bx = gs[di] | gs[di+1]<<8 + op2;r2 = data[bx](word,mode 遮罩)。
void Interpreter::op10_r2_from_data_gs() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.di = s_.ax;
  std::uint16_t bx = s_.game_state[s_.di & 0xFF];
  bx += (std::uint16_t)(s_.game_state[(s_.di + 1) & 0xFF] << 8);
  al = s_.fetch8();
  bx += al;
  s_.bx = bx;
  const auto& d = s_.data_bytes;
  std::uint16_t lo = (bx < d.size()) ? d[bx] : 0;
  std::uint16_t hi = ((std::size_t)(bx + 1) < d.size()) ? d[bx + 1] : 0;
  s_.ax = (std::uint16_t)(lo | (hi << 8));
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  ah &= s_.mode;
  s_.ax = (std::uint16_t)((ah << 8) | (s_.ax & 0xFF));
  s_.r2 = s_.ax;
}

// op_13:gs[operand + r4] = r2(byte;word 模式再寫 +1)。
void Interpreter::op13_gs_off_from_r2() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  s_.cx = s_.r2;
  s_.bx += s_.r4;
  set_gs(s_.bx, s_.cx & 0xFF);
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) set_gs(s_.bx + 1, (s_.cx & 0xFF00) >> 8);
}

// op_14:data[operand] = r2 低位(忠實對照 opendw:低位恆寫,word 模式再寫高位)。
void Interpreter::op14_data_from_r2() {
  std::uint16_t save_ah = (s_.ax & 0xFF00) >> 8;
  std::uint16_t ax = s_.fetch8();
  ax += (std::uint16_t)(s_.fetch8() << 8);
  s_.ax = ax;
  s_.bx = s_.ax;
  std::uint16_t dest_offset = s_.r2;
  s_.wdata(s_.bx, (std::uint8_t)(dest_offset & 0xFF));  // 走 wdata:支援自我修改碼(aliased 同步 script)
  if (s_.mode != save_ah) {  // byte_3AE1 != save_ah
    s_.wdata((std::size_t)s_.bx + 1, (dest_offset & 0xFF00) >> 8);
  }
}

// op_15:data[operand + r4] = r2(byte;word 模式再寫 +1)。
void Interpreter::op15_data_off_from_r2() {
  std::uint8_t save_ah = (s_.ax & 0xFF00) >> 8;
  s_.ax = s_.fetch8();
  s_.ax += (std::uint16_t)(s_.fetch8() << 8);
  s_.bx = s_.ax;
  s_.cx = s_.r2;
  s_.di = s_.r4;
  std::size_t idx = (std::size_t)s_.bx + s_.di;
  s_.wdata(idx, s_.cx & 0xFF);
  if (s_.mode != save_ah) {
    s_.wdata(idx + 1, (s_.cx & 0xFF00) >> 8);
  }
}

// --- batch 9:gs 複製 + 資料資源字串 emit ---

// op_19:gs[op2] = gs[op1](byte;word 模式連高位)。
void Interpreter::op19_gs_copy() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.di = s_.ax;
  al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  s_.bx = s_.ax;
  s_.cx = s_.game_state[s_.di & 0xFF];
  s_.cx += (std::uint16_t)(s_.game_state[(s_.di + 1) & 0xFF] << 8);
  set_gs(s_.bx, s_.cx & 0xFF);
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) set_gs(s_.bx + 1, (s_.cx & 0xFF00) >> 8);
}

// op_7A:從 word_3ADF->bytes 的 r2 偏移解一條字串 emit,r2 = 下一條起點。
//   對照 extract_string(word_3ADF->bytes, word_3AE2, ...)。
void Interpreter::op7A_emit_data_string() {
  std::size_t start = s_.r2;
  auto [str, next] = text::decode(s_.data_bytes, start);
  s_.r2 = (std::uint16_t)next;
  if (msg_sink_) msg_sink_(start, str);
}

// op_7C:set_ui_header(data, r2):同樣自 data[r2] 解字串(emit 給 sink)、r2=next。
void Interpreter::op7C_ui_header_data() {
  op7A_emit_data_string();
}

// op_88(op_wait_escape):等待 ESC 鍵;headless 抽取無輸入 → 視為段落結束。
void Interpreter::op88_wait_escape() { s_.halted = true; }

// op_89(wait_event @0x4977):等待鍵盤事件並依鍵值跳轉(後接變長 key→addr 表)。
//   對照 wait_event → wait_for_event(@0x4368)→ handle_key_event(@0x4328):
//     1. 讀 2-byte flags(word_2AA7);
//     2. 表起點 si = flags 後的 pc;每筆 3 byte:[key][addr_lo][addr_hi];
//     3. 等鍵 → 大寫化(& 0xDF)後與表中 key 比對;
//        key 型別:0x00=catch-all、0x01=數字鍵(對應隊員,選中設 gs[6])、
//        0x81=跳過(di++)、0xFF=表尾、其餘(高位 set)=直接鍵比對;
//     4. 命中 → bx = base[di+1] | base[di+2]<<8(絕對 offset),
//        cpu.pc = base + bx;word_3AE2 = key(& 0xFF)。
//   ── headless ──:無鍵盤。headless_key != 0 時以該鍵掃表(UI leaf:畫字串/mouse/
//   timer/狀態列 全略過,對選定分支無影響);為 0 時維持 halt(無輸入可分支)。
//   注意:pressed key 在 opendw 經 get_key_from_buffer 已大寫化為「大寫字母|0x80」
//   (例 'F'|0x80=0xC6);headless_key 直接帶此值。
void Interpreter::op89_wait_event() {
  std::uint8_t flags_lo = s_.fetch8();
  std::uint8_t flags_hi = s_.fetch8();
  (void)flags_lo; (void)flags_hi;       // word_2AA7;UI/輸入旗標,結算分支不需

  // 取本次注入鍵:優先用 key_provider(自適應驅動),回 0 則沿用 headless_keys 序列
  //   (逐個 op_89 取用),再用完則回退 headless_key。
  std::uint8_t key = 0;
  if (s_.key_provider) key = s_.key_provider(s_.script_res, s_.pc);
  if (key == 0) {
    if (s_.headless_key_idx < s_.headless_keys.size())
      key = s_.headless_keys[s_.headless_key_idx++];
    else
      key = s_.headless_key;
  }
  if (key == 0) { s_.halted = true; return; }  // 無注入 → 維持原行為

  const auto& base = s_.script;          // running_script bytes(= cpu.base_pc)
  std::size_t di = s_.pc;                // 表起點(flags 之後)= word_2AA2
  bool numeric_match = false;            // 命中的是「數字鍵(type 0x01)」筆

  // 掃表(對照 wait_for_event @0x29DD 迴圈)。
  while (di < base.size()) {
    std::uint8_t al = base[di];
    if (al == 0x00) {                    // catch-all:無條件命中(handle_key_event)
      break;
    }
    if (al == 0xFF) {                    // 表尾:無匹配。headless 視為段落結束(防 runaway)
      s_.halted = true;
      return;
    }
    if (al == 0x01) {                    // 數字鍵(隊員/目標選擇,對照 wait_for_event 0x29EF)。
      // 按鍵為「數字 | 0x80」(0xB1='1'…);index = key - 0xB1;< gs[0x1F](有效參戰者)→ 命中。
      if (key >= 0xB1) {
        std::uint8_t idx = (std::uint8_t)(key - 0xB1);
        if (idx < s_.game_state[0x1F]) { numeric_match = true; break; }
      }
      di += 3;
      continue;
    }
    if (al == 0x81) {                    // skip 標記(對照 0x2A24:di++)
      di += 1;
      continue;
    }
    if (al == 0x80) {                    // 0x80:特殊,非直接鍵 → 下一筆
      di += 3;
      continue;
    }
    if ((al & 0x80) == 0) {              // 範圍型(low/high 邊界);本切片未用 → 跳一筆
      di += 3;
      continue;
    }
    // 高位 set 的直接鍵:al == 按下的鍵?
    if (al == key) break;                // 命中
    di += 3;                             // 下一筆
  }
  if (di >= base.size()) { s_.halted = true; return; }

  // handle_key_event(@0x4328):di++ 後讀跳轉位址。
  std::size_t a = di + 1;
  std::uint16_t bx = (a + 1 < base.size())
                         ? (std::uint16_t)(base[a] | (base[a + 1] << 8))
                         : 0;
  // handle_key_event:若 entry 型別 al==1(數字鍵)→ 設 gs[6] = key − 0xB1(選定隊員/目標)。
  if (numeric_match) {
    s_.game_state[6] = (std::uint8_t)(key - 0xB1);
  }
  // word_3AE2 = key(對照 wait_event 結尾 word_3AE2 = ax & 0xFF)。
  s_.r2 = (std::uint16_t)(key & 0xFF);
  s_.ax = (std::uint16_t)(key & 0xFF);
  s_.bx = bx;
  s_.pc = bx;                            // cpu.pc = base_pc + bx
}

// op_81(print_number @0x48C5):cpu.ax = word_3AE2; print_number(ax)。
//   opendw 走 convert_number_to_string → string_byte_handler_func(append_string),
//   把數字字元 emit 到與文字同一條輸出流。remake 走 i18n/UTF-8,不複刻 0xB0-based
//   DOS digit 編碼,直接把 N(=r2)轉十進位字串,以哨兵 offset(kNumberSink)emit,
//   讓呼叫端可辨識「這是數字,不是字典字串」。
void Interpreter::op81_print_number() {
  s_.ax = s_.r2;
  if (msg_sink_) msg_sink_(kNumberSink, std::to_string(s_.r2));
}

// op_7D(write_character_name @0x483B / 0x3610):輸出「當前角色名」。
//   oracle:bx = game_state[6];ax = 0xC960;ah += game_state[bx+10];bx = ax;
//          player = get_player_data(val>>1);逐 byte | 0x80 輸出直到該 byte 高位為 0。
//   無 operand;不改 r2/r4/flags/game_state。VM 可見副作用僅 cpu.ax/cpu.bx。
//   remake 為 headless(無 party 角色資料)→ 忠實算 ax/bx,但不解 player 名(無資料);
//   名字輸出走 msg_sink_ 才有意義,這裡無資料可解,emit 略過(對拍只比 r2/r4/flags/gs)。
void Interpreter::op7D_char_name() {
  s_.bx = s_.game_state[6];
  s_.ax = 0xC960;
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  std::uint8_t val = s_.game_state[(s_.bx + 10) & 0xFF];
  ah += val;
  s_.ax = (std::uint16_t)((ah << 8) | (s_.ax & 0xFF));
  s_.bx = s_.ax;
  // 無 party 資料 → 不逐 byte 輸出角色名(headless)。r2/r4/flags/game_state 不變。
}

// op_80(advance_cursor @0x487F):讀 1 byte operand(欄寬/游標位置),ui_draw_string()
//   後 al += draw_rect.x、append_spaces(al)。VM 可見副作用:消耗 1 operand + cpu.ax。
//   不改 r2/r4/flags/game_state。remake 渲染由 framebuffer 自理,VM 僅正確消耗 operand。
void Interpreter::op80_advance_cursor() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  // ui_draw_string() / append_spaces 為渲染副作用,VM 狀態不變。
}

// op_8C(prompt_no_yes @0x49A5):畫「N/Y」提示、wait_for_event 取鍵,依鍵值設旗標。
//   oracle:key=='Y'(0xD9)→ cf=1,zf=1;否則 zf=0,key>0xD9 → cf=0 else cf=1。
//          最後 word_3AE6 = sf<<7 | zf<<6 | 1<<1 | cf。無 operand。
//   headless 無鍵盤 → 取「無輸入」key=0(< 0xD9)的確定性分支:zf=0、cf=1、sf=0,
//   即 word_3AE6 = 0x03(reserved|carry)。對拍時 oracle 喂同一 key=0 取得同一結果。
void Interpreter::op8C_prompt_no_yes() {
  // headless:預設無鍵盤輸入(key=0,取 No 分支,既有測試/遊戲流程依賴)。
  //   若注入了 headless_keys/headless_key(逆向 city-entry「Do you wish to enter?」
  //   的 Yes 分支用),則消耗一個鍵當作玩家輸入。鍵為「大寫字母|0x80」(對照
  //   get_key_from_buffer):'Y'|0x80=0xD9。不注入時行為與舊版完全相同。
  std::uint16_t key = 0;
  if (s_.headless_key_idx < s_.headless_keys.size())
    key = s_.headless_keys[s_.headless_key_idx++];
  else if (s_.headless_key != 0)
    key = s_.headless_key;
  if (key == 0xD9) {      // 'Y'
    s_.cf = 1;
    s_.zf = 1;
  } else {                // 'N' / 無輸入
    s_.zf = 0;
    s_.cf = ((key & 0xFF) > 0xD9) ? 0 : 1;
  }
  s_.sf = 0;
  std::uint16_t f = 0;
  f |= (std::uint16_t)(s_.sf << 7);
  f |= (std::uint16_t)(s_.zf << 6);
  f |= kReserved;
  f |= (std::uint16_t)(s_.cf << 0);
  s_.flags = f;
}

// --- batch 11:資料資源讀(gs 索引)/ 印字 / 音效 ---

// op_0E(@0x3BD0):r2 = word_3ADF->bytes[ gs[op] | gs[op+1]<<8 + word_3AE4 ](word,mode 遮罩)。
//   oracle:al=operand;bx = gs[al] | gs[al+1]<<8;bx += word_3AE4;
//          ax = es[bx] | es[bx+1]<<8(es = word_3ADF->bytes);ah &= byte_3AE1;word_3AE2 = ax。
//   與 op_0D 類似,差在 base 索引取自 game_state(gs[al] 兩 byte 組成 offset),而非直接 operand。
void Interpreter::op0E_r2_from_data_gsoff() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint16_t idx = s_.ax & 0xFF;
  std::uint16_t bx = s_.game_state[idx & 0xFF];
  bx += (std::uint16_t)(s_.game_state[(idx + 1) & 0xFF] << 8);
  bx += s_.r4;
  s_.bx = bx;
  const auto& d = s_.data_bytes;
  std::uint16_t lo = (bx < d.size()) ? d[bx] : 0;
  std::uint16_t hi = ((std::size_t)(bx + 1) < d.size()) ? d[bx + 1] : 0;
  s_.ax = (std::uint16_t)(lo | (hi << 8));
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  ah &= s_.mode;  // byte_3AE1
  s_.ax = (std::uint16_t)((ah << 8) | (s_.ax & 0xFF));
  s_.r2 = s_.ax;
}

// op_83(@0x48EE):把 word_3AE2 以 byte/word 模式 emit(印字)。無 operand。
//   oracle:若 byte_3AE1 != ah(word 模式)→ 先 emit word_3AE2 高位;再 emit word_3AE2 低位。
//          兩次都走 handle_byte_callback(輸出),不改 r2/r4/flags/game_state。
//   remake 走 i18n/UTF-8 字串 sink:把要印的 byte(s)轉成字串以哨兵 offset emit;
//   VM 狀態與 oracle 一致(純輸出,無狀態變更)。
void Interpreter::op83_print_char() {
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;
  std::string out;
  if (s_.mode != ah) {  // word 模式:先印高位
    out.push_back((char)((s_.r2 & 0xFF00) >> 8));
  }
  out.push_back((char)(s_.r2 & 0xFF));
  if (msg_sink_) msg_sink_(kNumberSink, out);
}

// op_90(op_sound_effect @0x49E7):讀 1 byte operand(音效編號),dispatch_sound_effect(al)。
//   VM 可見副作用:消耗 1 operand + cpu.ax;不改 r2/r4/flags/game_state。
//   operand al 即 func_5060 索引(音效編號)。VM 狀態不變;若掛了 sound sink,
//   把索引回呼出去由呼叫端轉 audio::SoundId 播放(VM 不直接相依 SDL/audio)。
void Interpreter::op90_sound_effect() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  // dispatch_sound_effect 為音效副作用,VM 狀態不變;只回呼音效索引。
  if (snd_sink_) snd_sink_(al);
}

// --- batch 12:角色資料存取(char_data = data_C960)---

// 當前角色 record 起點(char_data 內偏移)。對照 get_character_data:
//   di = game_state[6];selector = game_state[di + 0x0A];record 頁高位 = selector。
//   oracle bx = 0xC960;bh += selector → 絕對位址 (0xC9+selector)<<8 | 0x60;
//   減 0xC960 後 = (selector<<8)。即 char_data 內 record 起點 = selector << 8。
std::uint16_t Interpreter::char_record_base() {
  std::uint8_t player_idx = s_.game_state[6];
  std::uint8_t selector = s_.game_state[(player_idx + 0x0A) & 0xFF];
  return (std::uint16_t)(selector << 8);
}

// op_5D(get_character_data @0x42D8):讀當前角色屬性 → word_3AE2。
//   addr = (selector<<8) + operand;cx = char_data[addr];
//   word_3AE2 = cx & 0xFF;若 byte_3AE1 != 0(word 模式)→ word_3AE2 = cx(連高位)。
//   注意:oracle 高位來自 cx,而 cx 只讀了 1 byte → 高位實為 cx 原值高位(此處 cx=單 byte
//   讀取後高位為 0;word 模式時 word_3AE2 高位 = 0)。忠實對齊:cx 僅取 char_data[addr] 單 byte。
void Interpreter::op5D_get_char_data() {
  std::uint16_t op_pc = (std::uint16_t)(s_.pc - 1);  // op_5D 指令本身的 pc(operand 前)
  std::uint16_t base = char_record_base();
  std::uint8_t al = s_.fetch8();            // property offset
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint16_t addr = (std::uint16_t)(base + al);
  std::uint8_t cl = (addr < s_.char_data.size()) ? s_.char_data[addr] : 0;
  s_.cx = cl;                                // cx = char_data[addr](高位 0)
  s_.r2 = (std::uint16_t)(s_.cx & 0xFF);
  if (s_.mode != 0) {                        // byte_3AE1 != 0 → word 模式
    s_.r2 = (std::uint16_t)((s_.cx & 0xFF00) | (s_.r2 & 0xFF));
  }
  if (char_read_obs_) char_read_obs_(al, cl, op_pc);  // 純診斷觀測(技能檢定掃描)
}

// op_5E(set_character_data @0x4322):把 word_3AE2 寫回當前角色屬性。
//   oracle:game_state[di + 0x18] = ah(cpu.ax 高位,此時 ax 低位=gs[6]、高位=0 → 寫 0);
//   addr = (selector<<8) + operand;char_data[addr] = word_3AE2 低位;
//   若 byte_3AE1 != 0(word 模式)→ char_data[addr+1] = word_3AE2 高位。
void Interpreter::op5E_set_char_data() {
  std::uint8_t player_idx = s_.game_state[6];
  s_.ax = (s_.ax & 0xFF00) | player_idx;
  s_.di = s_.ax;
  // gs[di + 0x18] = ax 高位(此刻為 0)。忠實對齊 oracle。
  set_gs((std::uint16_t)(s_.di + 0x18), (std::uint8_t)((s_.ax & 0xFF00) >> 8));
  std::uint16_t base = char_record_base();
  std::uint8_t al = s_.fetch8();             // property offset
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint16_t addr = (std::uint16_t)(base + al);
  s_.cx = s_.r2;
  if (addr < s_.char_data.size()) s_.char_data[addr] = (std::uint8_t)(s_.cx & 0xFF);
  if (s_.mode != 0) {                         // word 模式:寫高位
    if ((std::size_t)(addr + 1) < s_.char_data.size())
      s_.char_data[addr + 1] = (std::uint8_t)((s_.cx & 0xFF00) >> 8);
  }
}

// op_5F(op_or_char_data @0x4372):設角色 bit 屬性。
//   get_bit_mask(word_3AE2):讀 1 operand → bx=byte offset、ax=bit mask;
//   cx = gs[6];ch(0xC9)+= gs[cx+0x0A] → record 頁;di = (ch<<8|0x60) = record 起點+0x60;
//   char_data[(bx + di) - 0xC960] |= mask  →  = char_data[(selector<<8) + bx] |= mask。
void Interpreter::op5F_or_char_data() {
  get_bit_mask((std::uint8_t)s_.r2);          // 吃 1 operand,設 bx/ax
  std::uint16_t base = char_record_base();    // selector<<8
  std::uint16_t addr = (std::uint16_t)(base + s_.bx);
  std::uint8_t mask = (std::uint8_t)(s_.ax & 0xFF);
  if (addr < s_.char_data.size()) s_.char_data[addr] |= mask;
}

// op_60(op_and_char_data @0x438B):清角色 bit 屬性(mask 取反後 AND)。
void Interpreter::op60_and_char_data() {
  get_bit_mask((std::uint8_t)s_.r2);
  std::uint16_t base = char_record_base();
  std::uint16_t addr = (std::uint16_t)(base + s_.bx);
  std::uint8_t mask = (std::uint8_t)(~(s_.ax & 0xFF));
  if (addr < s_.char_data.size()) s_.char_data[addr] &= mask;
}

// op_61(test_player_property @0x43A6):測角色 bit 屬性 → 設 sf/zf/cf。
//   get_bit_mask(word_3AE2) → bx=byte offset、ax=mask;
//   val = gs[gs[6] + 0x0A](= selector);player = data_C960 + (val>>1)*512;
//   test = player[bx] & mask;cf=0;sf = test>=0x80;zf = test==0;set_flags()。
void Interpreter::op61_test_char_prop() {
  get_bit_mask((std::uint8_t)s_.r2);
  std::uint8_t player_idx = s_.game_state[6];
  std::uint8_t val = s_.game_state[(player_idx + 0x0A) & 0xFF];  // selector
  std::uint32_t player_off = (std::uint32_t)(val >> 1) * 512u;   // get_player_data(val>>1)
  std::uint32_t addr = player_off + s_.bx;
  std::uint8_t test_val = (addr < s_.char_data.size()) ? s_.char_data[addr] : 0;
  std::uint8_t test_result = (std::uint8_t)(test_val & (s_.ax & 0xFF));
  s_.cf = 0;
  s_.sf = (test_result >= 0x80) ? 1 : 0;
  s_.zf = (test_result == 0) ? 1 : 0;
  set_flags();
}

// data_CA4C per-character offset table(對照 tables.c unknown_4456[],engine.c get_unknown_4456)。
static const std::uint16_t kUnknown4456[] = {
    0x0000, 0x0017, 0x002E, 0x0045, 0x005C, 0x0073, 0x008A,
    0x00A1, 0x00B8, 0x00CF, 0x00E6, 0x00FD, 0x00E8};
static std::uint16_t unknown_4456(std::uint8_t idx) {
  if (idx >= sizeof(kUnknown4456) / sizeof(kUnknown4456[0])) return 0;
  return kUnknown4456[idx];
}

// op_63(set_char_data_word @0x43F7):
//   byte_3AE1 = ax 高位(dispatch 後 ax=opcode → 高位 0 → byte 模式);r2 高位 = ah。
//   讀 2-byte word operand(→ word_4454,本切片不另用);
//   di = (0xCA4C + (selector<<8) + unknown_4456[bx=0]) - 0xCA4C = (selector<<8);
//   若 char_ext[di] != 0 → opendw 0x4430 未實作(exit);本切片戰鬥首回合 char_ext=0,
//   走 0x444C 分支:清 carry(word_3AE6 &= ~1)。為安全:!=0 時標未實作並 halt(不臆造)。
void Interpreter::op63_set_char_ext_word() {
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;  // dispatch 後 = 0
  s_.mode = ah;                              // byte_3AE1 = ah
  s_.r2 = (std::uint16_t)((ah << 8) | (s_.r2 & 0xFF));
  // es:lodsw — 讀 2-byte operand(word_4454)。
  std::uint16_t w = s_.fetch8();
  w += (std::uint16_t)(s_.fetch8() << 8);
  (void)w;
  std::uint16_t bx = 0;                      // cpu.bx=0; <<1 = 0
  std::uint8_t player_idx = s_.game_state[6];
  std::uint8_t sel = s_.game_state[(player_idx + 0x0A) & 0xFF];  // character select
  std::uint32_t di = ((std::uint32_t)sel << 8) + unknown_4456((std::uint8_t)bx);
  if (di < s_.char_ext.size() && s_.char_ext[di] != 0) {
    last_unimpl_ = 0x63;  // 對照 opendw 0x4430 未實作分支(char_ext 非 0);不臆造
    s_.halted = true;
    return;
  }
  s_.flags &= 0xFFFE;  // clear carry(對照 0x444C:word_3AE6 &= 0xFFFE)
  s_.cf = 0;
}

// op_68(@0x450A,原始 DRAGON.COM 反組譯反推 — opendw targets[] 標 NULL):
//   op_69 的「讀」孿生。讀當前角色「裝備/物品記錄」的一個位元組/字 → word_3AE2。
//   反組譯(file offset 0x440A,COM @CS:0x100):
//     bl=[0x3867]; bx<<=1;              ; 物品槽 index(= gs[7]),*2 進字表
//     al=[0x3866]; di=ax;               ; 角色 index(= gs[6])
//     ax=0xCA4C; ah += [di+0x386a];     ; char_ext 基底 + 角色頁(selector<<8)
//     ax += [bx+0x4456];                ; + 槽偏移(unknown_4456[slot] = slot*23,23B/item)
//     di=ax; lodsb; di += ax;           ; + operand(物品記錄內欄位 byte offset)
//     ax=[di]; [0x3ae2]=al;             ; r2 低位 = char_ext[di]
//     if [0x3ae1]!=0: [0x3ae3]=ah       ; word 模式才取高位
//   與 opendw op_69(engine.c:2846)同定址:selector=gs[gs[6]+0x0A]、slot=gs[7]、stride 23、
//   區段 0xCA4C(=data_CA4C=char_ext)。武器傷害(res3 0x0D68)用此讀「主傷害骰 byte[8]」等欄位,
//   餵共用骰子子程式(0x06EC)→ 武器傷害 = roll(武器主傷害骰) + floor(STR/5)(與徒手同式,只換骰源)。
void Interpreter::op68_get_char_ext() {
  std::uint16_t bx = s_.game_state[7];
  std::uint8_t player_idx = s_.game_state[6];
  std::uint8_t sel = s_.game_state[(player_idx + 0x0A) & 0xFF];
  std::uint32_t di = ((std::uint32_t)sel << 8) + unknown_4456((std::uint8_t)bx);
  std::uint8_t al = s_.fetch8();
  di += al;
  std::uint8_t lo = (di < s_.char_ext.size()) ? s_.char_ext[di] : 0;
  std::uint8_t hi = (di + 1 < s_.char_ext.size()) ? s_.char_ext[di + 1] : 0;
  s_.r2 = (std::uint16_t)(s_.r2 & 0xFF00) | lo;  // [0x3ae2] = al
  if (s_.mode != 0) {                            // byte_3AE1 != 0 → word 模式
    s_.r2 = (std::uint16_t)((hi << 8) | lo);     // [0x3ae3] = ah
  }
}

// op_69(@0x453F):char_ext[(selector<<8) + unknown_4456[gs[7]] + operand] = r2(byte/word)。
void Interpreter::op69_set_char_ext() {
  std::uint16_t bx = s_.game_state[7];
  std::uint8_t player_idx = s_.game_state[6];
  std::uint8_t sel = s_.game_state[(player_idx + 0x0A) & 0xFF];
  std::uint32_t di = ((std::uint32_t)sel << 8) + unknown_4456((std::uint8_t)bx);
  std::uint8_t al = s_.fetch8();
  di += al;
  std::uint16_t ax = s_.r2;
  if (di < s_.char_ext.size()) s_.char_ext[di] = ax & 0xFF;
  if (s_.mode != 0) {
    if (di + 1 < s_.char_ext.size()) s_.char_ext[di + 1] = (ax & 0xFF00) >> 8;
  }
}

// adjust_position(@0x45D0,opendw engine.c:2915):依方向把隊伍座標 ±1。
//   gs[0](X)/gs[1](Y);DIRECTION_NORTH=0:X+1、EAST=1:Y+1、SOUTH=2:X−1、WEST=3:Y−1。
//   (對照 disasm:al==0→inc[0x3860];1→inc[0x3861];2→dec[0x3860];default(3)→dec[0x3861]。)
void Interpreter::adjust_position(std::uint8_t direction) {
  switch (direction & 0x03) {
    case 0: s_.game_state[0] = (std::uint8_t)(s_.game_state[0] + 1); break;  // NORTH
    case 1: s_.game_state[1] = (std::uint8_t)(s_.game_state[1] + 1); break;  // EAST
    case 2: s_.game_state[0] = (std::uint8_t)(s_.game_state[0] - 1); break;  // SOUTH
    case 3: s_.game_state[1] = (std::uint8_t)(s_.game_state[1] - 1); break;  // WEST
  }
}

// op_6B(@0x45A1,原始 DRAGON.COM 反組譯 — opendw targets[] 標 NULL,無 oracle handler):
//   dispatch 表 [0x3960+0x6B*2]=0x45A1。反組譯(file offset 0x44A1,COM @CS:0x100):
//     45A1  al=[0x3863]           ; al = gs[3](當前面向 facing)
//     45A4  xor al,0x2            ; al ^= 2(N↔S、E↔W:反向)
//     45A6  jmp 0x45AB            ; (與 op_6C@0x45A8「al=gs[3]」共用後段,差在這步反向)
//     45AB  push si
//     45AC  call 0x45D0           ; adjust_position(al):依方向改 gs[0]/gs[1] ±1
//     45AF  test byte [0x3883],2  ; gs[0x23] & 0x2(worldmap 模式旗標)
//     45B4  jz   0x45CC           ; 非 worldmap → 略過邊界 wrap
//     45B6  ...call 0x5559/0x5523 ; worldmap 邊界 wrap(opendw check_map_boundary_x/y,
//                                 ;   其 worldmap 分支標 unimplemented/exit)
//     45CC  pop si; jmp 0x3ACB    ; 不消耗任何 operand;回 dispatch 迴圈
//   語意:**move_party_reverse** —— 把隊伍往「面向的反方向」移動一格(= 後退一步)。
//   op_6C(0x45A8)是同段但不反向(前進一步)。
//   反組譯佐證 = opendw op_6C(engine.c:2937)body 完全一致(adjust_position(gs[3]) +
//     gs[0x23]&2 判定),只差 op_6B 多一步 `xor al,2`。
//   ── headless ──:dungeon/area 事件格(area 18 tile 0x0D 等)gs[0x23]&2==0 → 純座標
//     mutation、無 operand、不 halt → gate「選 Yes」分支可走完。worldmap 模式
//     (area 0,gs[0x23]&2!=0)opendw 自身即 exit/unimplemented 且 app 走獨立 worldmap_dest
//     進城(docs/54);此處標 last_unimpl 0x6B(不臆造邊界 wrap 數值),但**不 halt 座標
//     mutation**(座標仍 ±1,與非 worldmap 路徑同),以記錄「worldmap wrap 未復刻」。
void Interpreter::op6B_move_reverse() {
  std::uint8_t al = (std::uint8_t)(s_.game_state[3] ^ 0x02);  // 反向 facing
  s_.ax = (s_.ax & 0xFF00) | al;
  adjust_position(al);                                        // gs[0]/gs[1] ±1
  if (s_.game_state[0x23] & 0x02) {
    // worldmap 模式:opendw 邊界 wrap(0x5559/0x5523 的 worldmap 分支)未復刻。
    //   不臆造 wrap 後的座標;記錄受阻碼。座標 mutation 已套用(與 dungeon 路徑同)。
    last_unimpl_ = 0x6B;
  }
}

// op_8D(read_string_input @0x49D3,opendw engine.c:4945 → read_string_input @0x1E54):
//   玩家文字輸入常式。opendw 流程:
//     - draw_input_box;byte_1F07=0(目前長度);byte_1F08=輸入框寬上限(cap 0x10)。
//     - 迴圈 wait_for_event(ALLOW_ANY_CASE)取鍵 al:
//         '/'(0xAF)、'\'(0xDC)→ 丟棄;Backspace(0x88)→ 長度−1;
//         Enter(0x8D)→ 結束;ESC(0x9B)→ 長度歸 0 結束;
//         長度已達 byte_1F08 → 丟棄;al<0xA0(控制鍵)→ 丟棄;
//         al==0xA0(空白)且開頭 → 丟棄;否則 set_game_state(0xC6+len, al);len++。
//     - 結束時 set_game_state(0xC6+len, 0)(NUL 終止)。
//   buffer 寫到 game_state[0xC6 + i],字元為「0xA0-based DOS 字集」:空白=0xA0、
//   '0'..'9'=0xB0..0xB9、'A'..'Z'=0xC1..0xDA、'a'..'z'=0xE1..0xFA(= ASCII | 0x80)。
//   無 operand(handler 不從 script 讀 byte)。
//   ── headless ──:無鍵盤。headless_text 非空時取其字元(ASCII)逐一編碼(c|0x80)
//     寫入 gs[0xC6 + i],上限 0x10(對照 byte_1F08 cap);尾端補 NUL。空字串 = 玩家
//     直接按 Enter(gs[0xC6]=0)。讓「說暗語」puzzle(area 33 tile 0x14)say-word
//     分支不卡 prompt → 後續 script 可比對 gs[0xC6..] 與正解。不 halt。
void Interpreter::op8D_read_string() {
  constexpr std::size_t kInputBase = 0xC6;
  constexpr std::size_t kMaxLen = 0x10;  // byte_1F08 cap(engine.c:4899)
  std::size_t len = 0;
  for (char ch : s_.headless_text) {
    if (len >= kMaxLen) break;
    std::uint8_t enc = (std::uint8_t)((std::uint8_t)ch | 0x80);  // ASCII → 0xA0-based
    s_.game_state[(kInputBase + len) & 0xFF] = enc;
    ++len;
  }
  // NUL 終止(對照 0x1E99:set_game_state(0xC6+len, 0))。
  s_.game_state[(kInputBase + len) & 0xFF] = 0;
  // word_3AE2 不變;handle_byte_callback(0x8D) 為渲染副作用(送 newline),VM 狀態不依賴。
}

// op_16:bx = gs[op] | gs[op+1]<<8 + r4;data[bx] = r2(byte;word 模式再寫 +1)。
void Interpreter::op16_data_gsoff_from_r2() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint16_t index = s_.ax;
  std::uint16_t bx = s_.game_state[index & 0xFF];
  bx += (std::uint16_t)(s_.game_state[(index + 1) & 0xFF] << 8);
  bx += s_.r4;
  s_.bx = bx;
  s_.cx = s_.r2;
  s_.wdata(bx, s_.cx & 0xFF);
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) {
    s_.wdata((std::size_t)bx + 1, (s_.cx & 0xFF00) >> 8);
  }
}

// op_17(store_data_into_resource @0x3CB0):
//   offset_idx = operand;di = gs[offset_idx] | gs[offset_idx+1]<<8;
//   res_idx = gs[offset_idx+2];di += r4;res[res_idx].bytes[di] = r2 低位;
//   word 模式(byte_3AE1 != (ax>>8))再寫 +1。寫入須持久(走 res_bytes_by_index)。
void Interpreter::op17_store_into_res() {
  std::uint8_t offset_idx = s_.fetch8();
  std::uint16_t di = s_.game_state[offset_idx];
  di += (std::uint16_t)(s_.game_state[(offset_idx + 1) & 0xFF] << 8);
  std::uint8_t res_idx = s_.game_state[(offset_idx + 2) & 0xFF];
  di += s_.r4;
  s_.di = di;
  s_.cx = s_.r2;
  std::vector<std::uint8_t>* b = res_bytes_by_index(res_idx);
  if (b) {
    if (di < b->size()) (*b)[di] = s_.cx & 0xFF;
    if (s_.mode != ((s_.ax & 0xFF00) >> 8)) {
      if ((std::size_t)(di + 1) < b->size()) (*b)[di + 1] = (s_.cx & 0xFF00) >> 8;
    }
  }
}

// op_18(@0x3D1A):data[(gs[op1] | gs[op1+1]<<8) + op2] = r2(byte;word 模式再寫 +1)。
//   對照 opendw op_18:di = gs[index] | gs[index+1]<<8;di += 第二 operand;
//   word_3ADF->bytes[di] = r2 低位;若 byte_3AE1 != (ax>>8) 再寫高位。
//   注意 word 模式判定用 (ax>>8):dispatch 已把 ax 設為 opcode(高位 0),
//   讀第一 operand 後 ax 高位仍 0,故 byte 模式(mode=0)時不寫高位(與 oracle 一致)。
void Interpreter::op18_data_gsidx_from_r2() {
  std::uint8_t al = s_.fetch8();
  s_.ax = (s_.ax & 0xFF00) | al;
  std::uint16_t index = s_.ax;
  std::uint16_t di = s_.game_state[index & 0xFF];
  di += (std::uint16_t)(s_.game_state[(index + 1) & 0xFF] << 8);
  al = s_.fetch8();
  di += al;
  s_.di = di;
  s_.cx = s_.r2;
  s_.wdata(di, s_.cx & 0xFF);
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) {
    s_.wdata((std::size_t)di + 1, (s_.cx & 0xFF00) >> 8);
  }
}

// ── 乘/除法子系統(逐行對照 engine.c)──────────────────────────────
// multiply_16bit(@0x6520):11C6:11C8 = 11C2 * 11C0 (+ 11C4*11C0 高位)。
void Interpreter::mul16(std::uint16_t set_11C4) {
  s_.w11C4 = set_11C4;
  std::uint32_t result = (std::uint32_t)s_.w11C2 * s_.w11C0;
  s_.w11C6 = result & 0xFFFF;
  s_.w11C8 = (result & 0xFFFF0000u) >> 16;
  result = (std::uint32_t)s_.w11C4 * s_.w11C0;
  s_.w11C8 += result & 0xFFFF;
}

// save_gamestate_vars(@0x3F2F):gs[0x39/0x3A]=11C8、gs[0x37/0x38]=11C6,r2=11C6(byte/word)。
void Interpreter::save_gamestate_vars() {
  set_gs(57, s_.w11C8 & 0xFF);          // gs[0x39]
  set_gs(58, (s_.w11C8 & 0xFF00) >> 8); // gs[0x3A]
  std::uint16_t ax = s_.w11C6;
  set_gs(55, ax & 0xFF);                // gs[0x37]
  set_gs(56, (ax & 0xFF00) >> 8);       // gs[0x38]
  s_.r2 = (s_.r2 & 0xFF00) | (ax & 0xFF);
  if (s_.mode != 0) s_.r2 = ax;         // byte_3AE1 != 0 → word
}

// compute_division_vars(@0x3F23):11C0=r2; multiply_16bit(11C4); save。
void Interpreter::compute_division_vars() {
  s_.w11C0 = s_.r2;
  mul16(s_.w11C4);
  save_gamestate_vars();
}

// divide_16bit(@0x6539):32-bit long division(11C6:11C8 / 11C0 → 商 11C6、餘 11CA:11CC)。
void Interpreter::div16() {
  int old_carry = 0, carry = 0;
  s_.w11CA = 0; s_.w11CC = 0;
  for (int i = 0; i < 0x20; ++i) {
    carry = (s_.w11C6 & 0x8000) ? 1 : 0;
    s_.w11C6 = (std::uint16_t)(s_.w11C6 << 1);
    old_carry = carry;
    carry = (s_.w11C8 & 0x8000) ? 1 : 0;
    s_.w11C8 = (std::uint16_t)((s_.w11C8 << 1) + old_carry);
    old_carry = carry;
    carry = (s_.w11CA & 0x8000) ? 1 : 0;
    s_.w11CA = (std::uint16_t)((s_.w11CA << 1) + old_carry);
    old_carry = carry;
    carry = (s_.w11CC & 0x8000) ? 1 : 0;
    s_.w11CC = (std::uint16_t)((s_.w11CC << 1) + old_carry);
    old_carry = carry;

    std::uint16_t ax = s_.w11CA, old16 = ax;
    ax = (std::uint16_t)(ax - s_.w11C0);
    carry = (ax > old16) ? 1 : 0;
    std::uint16_t bx = s_.w11CC; old16 = bx;
    bx = (std::uint16_t)(bx - carry);
    carry = (bx > old16) ? 1 : 0;
    if (carry != 1) { s_.w11CA = ax; s_.w11CC = bx; s_.w11C6++; }
  }
}

// divide_and_save_results(@0x...):divide; gs[0x3B/0x3C]=11CA; save。
void Interpreter::divide_and_save_results() {
  div16();
  std::uint16_t ax = s_.w11CA;
  set_gs(0x3B, ax & 0xFF);
  set_gs(0x3C, (ax & 0xFF00) >> 8);
  save_gamestate_vars();
}

// op_33(@0x3EF8):11C2=gs[op](word),11C4=gs[op+2](word),11C0=r2 → multiply → save。
void Interpreter::op33_mul_gs() {
  std::uint8_t al = s_.fetch8();
  std::uint16_t bx = al;
  s_.bx = bx;
  s_.w11C2 = (std::uint16_t)(s_.game_state[bx & 0xFF] |
                             (s_.game_state[(bx + 1) & 0xFF] << 8));
  s_.w11C4 = (std::uint16_t)(s_.game_state[(bx + 2) & 0xFF] |
                             (s_.game_state[(bx + 3) & 0xFF] << 8));
  s_.w11C0 = s_.r2;
  mul16(s_.w11C4);          // 對照 op_33:multiply_16bit(word_11C4)
  save_gamestate_vars();
}

// op_34(@0x3F3F):11C2=operand(byte;word 模式再讀高位),11C4=高位 → compute_division_vars。
void Interpreter::op34_mul_imm() {
  std::uint8_t al = s_.fetch8();
  s_.w11C2 = (s_.w11C2 & 0xFF00) | al;
  std::uint8_t ah = (s_.ax & 0xFF00) >> 8;  // dispatch 已設 ax=opcode → ah=0
  std::uint8_t hi = ah;
  if (s_.mode != ah) al = s_.fetch8();      // byte_3AE1 != ah → 讀高位
  s_.w11C2 = (std::uint16_t)((al << 8) | (s_.w11C2 & 0xFF));
  al = hi;
  s_.w11C4 = (std::uint16_t)((hi << 8) | al);
  compute_division_vars();
}

// op_35(@0x3F66):11C6=gs[op](word),11C8=gs[op+2](word),11C0=r2 → divide → save。
void Interpreter::op35_div_gs() {
  std::uint8_t al = s_.fetch8();
  std::uint16_t bx = al;
  s_.bx = bx;
  s_.w11C6 = (std::uint16_t)(s_.game_state[bx & 0xFF] |
                             (s_.game_state[(bx + 1) & 0xFF] << 8));
  s_.w11C8 = (std::uint16_t)(s_.game_state[(bx + 2) & 0xFF] |
                             (s_.game_state[(bx + 3) & 0xFF] << 8));
  s_.w11C0 = s_.r2;
  divide_and_save_results();
}

// op_36(@0x3F8D):11C6=r2,11C8=0,11C0=operand(byte;word 模式再讀高位)→ divide → save。
void Interpreter::op36_div_imm() {
  s_.cx = s_.r2;
  s_.w11C6 = s_.cx;
  std::uint8_t ah = 0;
  s_.w11C8 = 0;
  std::uint8_t al = s_.fetch8();
  if (s_.mode != 0) ah = s_.fetch8();  // byte_3AE1 != 0 → word
  s_.w11C0 = (std::uint16_t)((ah << 8) | al);
  divide_and_save_results();
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
  t[0x51] = &Interpreter::op51_argmax_data;
  // batch 3
  t[0x2F] = &Interpreter::op2F_rcr_add_gs;
  t[0x30] = &Interpreter::op30_rcr_add_imm;
  t[0x31] = &Interpreter::op31_rcr_sub_gs;
  t[0x32] = &Interpreter::op32_rcr_sub_imm;
  t[0x48] = &Interpreter::op48_set_gs_msb;
  t[0x49] = &Interpreter::op49_loop;
  t[0x4A] = &Interpreter::op4A_loop_eq;
  t[0x66] = &Interpreter::op66_test_gs;
  t[0x9A] = &Interpreter::op9A_set_gs_ff;
  t[0x9B] = &Interpreter::op9B_set_gs_bit;
  t[0x9D] = &Interpreter::op9D_test_gs_bit;
  // 字串輸出
  t[0x77] = &Interpreter::op77_draw_and_set;
  t[0x78] = &Interpreter::op78_set_msg;
  t[0x79] = &Interpreter::op79_draw_and_emit_data;  // DRAGON.COM 反組譯:draw_pattern + op_7A
  t[0x7B] = &Interpreter::op7B_ui_header;
  // batch 4:繪圖 / UI / 結束
  t[0x73] = &Interpreter::op73_clear_event;
  t[0x74] = &Interpreter::op74_draw_frame;
  t[0x75] = &Interpreter::op75_ui_full;
  t[0x76] = &Interpreter::op76_draw_pattern;
  t[0x5A] = &Interpreter::op5A_ret;
  t[0x8A] = &Interpreter::op8A_encounter;
  // batch 5:跨資源 call / 資料資源存取 / 流程 / PRNG
  t[0x0C] = &Interpreter::op0C_r2_from_data;
  t[0x1C] = &Interpreter::op1C_data_store;
  t[0x40] = &Interpreter::op40_cmp_r4_imm;
  t[0x43] = &Interpreter::op43_jump_above;
  t[0x4D] = &Interpreter::op4D_prng;
  t[0x58] = &Interpreter::op58_xcall;
  t[0x59] = &Interpreter::op59_xret;
  t[0x5B] = &Interpreter::op5B_get_map_tile;  // get_map_tile_data(opendw 對拍移植)
  t[0x5C] = &Interpreter::op5C_party_loop;
  t[0x62] = &Interpreter::op62_scan_char;
  // batch 6:byte 堆疊 / 資料資源讀 / 比較 / viewport
  t[0x03] = &Interpreter::op03_pop_data_res;
  t[0x04] = &Interpreter::op04_push_script_res;
  t[0x0D] = &Interpreter::op0D_r2_from_data_off;
  t[0x3F] = &Interpreter::op3F_cmp_r4_gs;
  t[0x55] = &Interpreter::op55_peek_pop_r2;
  t[0x56] = &Interpreter::op56_push_r2;
  t[0x8B] = &Interpreter::op8B_refresh_viewport;
  t[0x92] = &Interpreter::op92_status_delay;
  t[0x82] = &Interpreter::op82_print_9digits;
  t[0x97] = &Interpreter::op97_load_char_data;
  t[0x98] = &Interpreter::op98_store_char_data;
  t[0x91] = &Interpreter::op91_status_panel;
  // batch 7:gamestate/資源讀 + r4 byte 堆疊
  t[0x0B] = &Interpreter::op0B_r2_from_gs_off;
  t[0x0F] = &Interpreter::op0F_r2_from_res;
  t[0x93] = &Interpreter::op93_push_r4;
  t[0x94] = &Interpreter::op94_pop_r4;
  // batch 8:gs-索引資料讀寫 + gs offset 寫
  t[0x10] = &Interpreter::op10_r2_from_data_gs;
  t[0x13] = &Interpreter::op13_gs_off_from_r2;
  t[0x14] = &Interpreter::op14_data_from_r2;
  t[0x15] = &Interpreter::op15_data_off_from_r2;
  t[0x16] = &Interpreter::op16_data_gsoff_from_r2;
  t[0x17] = &Interpreter::op17_store_into_res;
  t[0x18] = &Interpreter::op18_data_gsidx_from_r2;
  t[0x33] = &Interpreter::op33_mul_gs;
  t[0x34] = &Interpreter::op34_mul_imm;
  t[0x35] = &Interpreter::op35_div_gs;
  t[0x36] = &Interpreter::op36_div_imm;
  // batch 9:gs 複製 + 資料資源字串 emit
  t[0x19] = &Interpreter::op19_gs_copy;
  t[0x7A] = &Interpreter::op7A_emit_data_string;
  t[0x7C] = &Interpreter::op7C_ui_header_data;
  t[0x88] = &Interpreter::op88_wait_escape;
  t[0x89] = &Interpreter::op89_wait_event;
  t[0x81] = &Interpreter::op81_print_number;
  // batch 10:文字輸出 / 互動提示(逐字對照 opendw)
  t[0x7D] = &Interpreter::op7D_char_name;
  t[0x80] = &Interpreter::op80_advance_cursor;
  t[0x8C] = &Interpreter::op8C_prompt_no_yes;
  // batch 11:資料資源讀(gs 索引)/ 印字 / 音效
  t[0x0E] = &Interpreter::op0E_r2_from_data_gsoff;
  t[0x83] = &Interpreter::op83_print_char;
  t[0x90] = &Interpreter::op90_sound_effect;
  // batch 12:角色資料存取(char_data = data_C960)
  t[0x5D] = &Interpreter::op5D_get_char_data;
  t[0x5E] = &Interpreter::op5E_set_char_data;
  t[0x5F] = &Interpreter::op5F_or_char_data;
  t[0x60] = &Interpreter::op60_and_char_data;
  t[0x61] = &Interpreter::op61_test_char_prop;
  t[0x63] = &Interpreter::op63_set_char_ext_word;
  t[0x68] = &Interpreter::op68_get_char_ext;
  t[0x69] = &Interpreter::op69_set_char_ext;
  t[0x6B] = &Interpreter::op6B_move_reverse;  // DRAGON.COM 0x45A1:move_party_reverse
  t[0x8D] = &Interpreter::op8D_read_string;   // opendw 0x49D3:read_string_input
  return t;
}();
#undef OP

int Interpreter::run(long max_steps) {
  int steps = 0;
  // 初始化資料資源:populate_3ADD_and_3ADF 後 word_3ADF == running_script(同一份)。
  // 呼叫端通常只設 s.script;此處讓 word_3ADF(data_bytes)預設指向同一份。
  if (s_.data_bytes.empty() && !s_.script.empty()) s_.data_bytes = s_.script;
  while (!s_.halted && s_.pc < s_.script.size()) {
    std::size_t at = s_.pc;
    std::uint8_t op = s_.fetch8();
    // 對照 opendw run_script(@0x3ACF):每次 dispatch 都 `cpu.ax = op_code; cpu.bx = cpu.ax`
    //   (opcode 進 al,高位元組清 0)。多個 opcode(如 op_09 的 byte/word 模式判定
    //   `byte_3AE1 != (ax>>8)`)依賴此清零;漏設會讓 ax 高位殘留上一指令的值,
    //   進而多吃一個 operand 而 desync(戰鬥腳本 res3 @0x6aa 即因此誤判)。
    s_.ax = op;
    s_.bx = op;
    if (trace_) trace_->record({at, op, s_.r2, s_.r4, s_.flags, s_.mode});
    Handler h = kImpl[op];
    if (!h) { last_unimpl_ = op; s_.halted = true; break; }  // 未實作 → 停
    (this->*h)();
    ++steps;
    if (max_steps > 0 && steps >= max_steps) break;  // 上限保護(掃描/重放用)
  }
  return steps;
}

}  // namespace dw::vm
