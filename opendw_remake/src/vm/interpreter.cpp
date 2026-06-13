#include "interpreter.hpp"

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
  s_.zf = (s_.game_state[s_.bx] & al) == 0;
  s_.cf = 0;
  s_.sf = (s_.game_state[s_.bx] & al) >= 0x80;
  std::uint16_t flags = 0;
  flags |= s_.sf << 7;
  flags |= s_.zf << 6;
  flags |= kReserved;
  flags |= s_.cf << 0;
  flags &= 0xFFFE;
  s_.flags &= kCarry;
  s_.flags |= flags;
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
  s_.cx = s_.game_state[s_.bx];
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
  s_.cx = s_.game_state[s_.bx];
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
  if (s_.game_state[s_.bx] < 0x80) {
    set_gs(s_.bx, s_.game_state[s_.bx] | 0x80);
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
  s_.cx = s_.game_state[s_.bx];
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
  set_gs(s_.bx, s_.game_state[s_.bx] | (s_.ax & 0xFF));
}

void Interpreter::op9D_test_gs_bit() {
  std::uint8_t al = s_.fetch8();
  get_bit_mask(al);
  s_.cf = 0;
  s_.zf = (s_.game_state[s_.bx] & s_.ax) == 0 ? 1 : 0;
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
// op_8A:隨機遭遇(戰鬥);remake 尚無戰鬥 → 先 halt。
void Interpreter::op8A_encounter() { s_.halted = true; }

// --- batch 5:跨資源 call / 資料資源存取 / 流程 / PRNG ---

// 用 resource_provider 取資源 idx 的 bytes;成功填入 out 回 true。
bool Interpreter::load_resource(int idx, std::vector<std::uint8_t>& out) {
  if (!s_.resource_provider) return false;
  auto r = s_.resource_provider(idx);
  if (!r) return false;
  out = std::move(*r);
  return true;
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
  auto& d = s_.data_bytes;
  if (s_.di < d.size()) d[s_.di] = al;
  if (s_.mode != save_ah) {  // byte_3AE1 != save_ah → word 模式
    al = s_.fetch8();
    s_.ax = (s_.ax & 0xFF00) | al;
    if ((std::size_t)(s_.di + 1) < d.size()) d[s_.di + 1] = al;
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

  // push 返回框(si = 當前 pc;word_3AE8;dl 待定)。
  VmState::CallFrame fr;
  fr.script = s_.script;        // 返回後仍跑當前腳本
  fr.pc = s_.pc;                // si = 返回 offset
  fr.data_bytes = s_.data_bytes;
  fr.script_res = s_.script_res;
  fr.data_res = s_.data_res;

  // 載入目標資源 bytes。tag_item = 資源 tag/section。
  std::vector<std::uint8_t> bytes;
  if (!load_resource(tag_item, bytes)) {
    // 無法解析資源:還原無變化、停在未實作(由 run() 記為 blocker)。
    // 不 push、不切換 → 標記 last_unimpl 0x58 並 halt(維持「不變成 no-op」)。
    last_unimpl_ = 0x58;
    s_.halted = true;
    return;
  }
  fr.dl = 0xFF;  // opendw:cache miss(resource_load)→ dl=0xFF;命中且 usage_type==2 亦 0xFF。
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
  VmState::CallFrame fr = std::move(s_.call_stack.back());
  s_.call_stack.pop_back();
  // opendw 的 ah!=stack[sp] 檢查屬資源旗標處理,remake 不模擬資源 flagged 狀態,略。
  s_.script = std::move(fr.script);
  s_.data_bytes = std::move(fr.data_bytes);
  s_.script_res = fr.script_res;
  s_.data_res = fr.data_res;
  s_.pc = fr.pc;  // si:返回 offset
}

// run_script(對照 run_script @0x6413):push run_script 框、切資源、從 src_offset 跑到 op_5A。
void Interpreter::run_script(int script_index, std::uint16_t src_offset) {
  VmState::ScriptFrame fr;
  fr.script = s_.script;
  fr.pc = s_.pc;
  fr.data_bytes = s_.data_bytes;
  fr.script_res = s_.script_res;
  fr.data_res = s_.data_res;
  fr.mode = s_.mode;
  s_.script_frames.push_back(std::move(fr));

  std::vector<std::uint8_t> bytes;
  if (load_resource(script_index, bytes)) {
    s_.script = bytes;
    s_.data_bytes = bytes;
    s_.script_res = script_index;
    s_.data_res = script_index;
  } else if (script_index == s_.script_res) {
    // 同一資源(level 自身),沿用當前 bytes。
  }
  s_.pc = src_offset;

  // 子迴圈:跑到 op_5A(returned=true)或 halt/越界。
  while (!s_.halted && !s_.script_frames.back().returned &&
         s_.pc < s_.script.size()) {
    std::size_t at = s_.pc;
    std::uint8_t op = s_.fetch8();
    if (trace_) trace_->record({at, op, s_.r2, s_.r4, s_.flags, s_.mode});
    Handler h = kImpl[op];
    if (!h) { last_unimpl_ = op; s_.halted = true; break; }
    (this->*h)();
  }

  // 還原上層框。
  VmState::ScriptFrame back = std::move(s_.script_frames.back());
  s_.script_frames.pop_back();
  s_.script = std::move(back.script);
  s_.data_bytes = std::move(back.data_bytes);
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

// op_62:掃描隊伍角色屬性(對照 op_scan_for_char @0x43BF)。
//   原版讀玩家資料(get_player_data_byte)+ gs[0x1F]。remake 抽取期無 party 角色資料,
//   忠實消耗 2 byte operand,並依「掃不到」語意設 carry(cpu.cf=1),讓後續流程能繼續。
void Interpreter::op62_scan_char() {
  // byte_3AE6 >>= 1(對照原版開頭)。
  std::uint8_t b6 = (s_.flags & 0xFF) >> 1;
  s_.flags = (s_.flags & 0xFF00) | b6;
  s_.fetch8();  // dl(property offset)
  s_.fetch8();  // cl(threshold)
  // 無 party 資料 → 視為掃描到底未命中:set carry(對照迴圈結束 cpu.cf=1)。
  s_.cf = 1;
  s_.flags |= kCarry;
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

  // 取資源 bytes:同 data_res 用 data_bytes、同 script_res 用 script,否則 provider。
  const std::vector<std::uint8_t>* bytes = nullptr;
  std::vector<std::uint8_t> loaded;
  if (res_idx == s_.data_res) bytes = &s_.data_bytes;
  else if (res_idx == s_.script_res) bytes = &s_.script;
  else if (load_resource(res_idx, loaded)) bytes = &loaded;

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
  auto& d = s_.data_bytes;
  std::uint16_t dest_offset = s_.r2;
  if (s_.bx < d.size()) d[s_.bx] = (std::uint8_t)(dest_offset & 0xFF);
  if (s_.mode != save_ah) {  // byte_3AE1 != save_ah
    if ((std::size_t)(s_.bx + 1) < d.size()) d[s_.bx + 1] = (dest_offset & 0xFF00) >> 8;
  }
}

// op_15:data[operand + r4] = r2(byte;word 模式再寫 +1)。
void Interpreter::op15_data_off_from_r2() {
  std::uint8_t save_ah = (s_.ax & 0xFF00) >> 8;
  s_.ax = s_.fetch8();
  s_.ax += (std::uint16_t)(s_.fetch8() << 8);
  s_.bx = s_.ax;
  auto& d = s_.data_bytes;
  s_.cx = s_.r2;
  s_.di = s_.r4;
  std::size_t idx = (std::size_t)s_.bx + s_.di;
  if (idx < d.size()) d[idx] = s_.cx & 0xFF;
  if (s_.mode != save_ah) {
    if (idx + 1 < d.size()) d[idx + 1] = (s_.cx & 0xFF00) >> 8;
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

// op_89(wait_event):等待鍵盤事件並依鍵值跳轉(後接變長 key→addr 表,以 0xFF 結束)。
//   抽取期無鍵盤輸入,無法決定分支;讀完 flags(2B)後結束該段,避免執行落入資料
//   被誤判為 opcode(原本 runaway 至 0xCE/0x79)。對照 wait_event @0x4977。
void Interpreter::op89_wait_event() {
  s_.fetch8();      // flags lo
  s_.fetch8();      // flags hi
  s_.halted = true; // 無輸入可分支 → 結束該事件段
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
  auto& d = s_.data_bytes;
  s_.cx = s_.r2;
  if (bx < d.size()) d[bx] = s_.cx & 0xFF;
  if (s_.mode != ((s_.ax & 0xFF00) >> 8)) {
    if ((std::size_t)(bx + 1) < d.size()) d[bx + 1] = (s_.cx & 0xFF00) >> 8;
  }
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
  // batch 9:gs 複製 + 資料資源字串 emit
  t[0x19] = &Interpreter::op19_gs_copy;
  t[0x7A] = &Interpreter::op7A_emit_data_string;
  t[0x7C] = &Interpreter::op7C_ui_header_data;
  t[0x88] = &Interpreter::op88_wait_escape;
  t[0x89] = &Interpreter::op89_wait_event;
  return t;
}();
#undef OP

int Interpreter::run() {
  int steps = 0;
  // 初始化資料資源:populate_3ADD_and_3ADF 後 word_3ADF == running_script(同一份)。
  // 呼叫端通常只設 s.script;此處讓 word_3ADF(data_bytes)預設指向同一份。
  if (s_.data_bytes.empty() && !s_.script.empty()) s_.data_bytes = s_.script;
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
