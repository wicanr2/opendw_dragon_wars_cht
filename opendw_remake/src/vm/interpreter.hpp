// interpreter — script 虛擬 CPU 的 fetch-dispatch 直譯器(對照 opendw run_script)。
//
// R1 batch 1:VM 核心 + 一批「無副作用」opcode(模式/算術/旗標/跳轉/呼叫/堆疊)。
// 後續 batch 補齊其餘 opcode 並全程對拍 opendw(差異測試)。
#pragma once
#include <array>
#include <functional>
#include <string>
#include "vm_state.hpp"
#include "trace.hpp"

namespace dw::vm {

class Interpreter {
public:
  explicit Interpreter(VmState& s, Trace* trace = nullptr) : s_(s), trace_(trace) {}

  // 字串輸出 sink:VM 執行 op_77/78/7B 時,以 (字串起始 offset, 解出的英文原文) 回呼。
  // 呼叫端據 (section,offset) 查 bundle 字串表 / i18n,渲染在地化文字。
  //
  // op_81(print_number)也走同一條 sink,但 offset 用哨兵值 kNumberSink:
  //   呼叫端見到此 offset 即知「字串本身就是已格式化的十進位數字(段落號 N)」,
  //   不要再拿去查字典 / i18n,直接輸出即可。
  static constexpr std::size_t kNumberSink = static_cast<std::size_t>(-1);
  using MessageSink = std::function<void(std::size_t offset, const std::string&)>;
  void set_message_sink(MessageSink sink) { msg_sink_ = std::move(sink); }

  // 跨資源 call(op_58/op_5C)的資源提供者。等同直接設 VmState::resource_provider。
  void set_resource_provider(VmState::ResourceProvider p) {
    s_.resource_provider = std::move(p);
  }

  // 執行直到 halt(op_5A)或 pc 越界。回傳執行的指令數。
  // max_steps>0 時加上指令數上限(達上限即停,halted 不設);供事件掃描/重放避免
  // 跨資源迴圈/壞跳轉造成的無限執行。<=0(預設)為無上限,行為與舊版相同。
  int run(long max_steps = 0);

  // 已實作的 opcode 集合(R1 batch 1);未實作者執行會 halt 並記錄。
  bool implemented(std::uint8_t op) const { return kImpl[op] != nullptr; }
  // 最後一個導致 halt 的未實作 opcode(0=無)。
  std::uint8_t last_unimpl() const { return last_unimpl_; }

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

  // --- batch 3:rcr+加減 / loop / set-msb / test-gs / bit ---
  void op2F_rcr_add_gs();      // 0x2F
  void op30_rcr_add_imm();     // 0x30
  void op31_rcr_sub_gs();      // 0x31
  void op32_rcr_sub_imm();     // 0x32
  void op48_set_gs_msb();      // 0x48
  void op49_loop();            // 0x49
  void op4A_loop_eq();         // 0x4A
  void op66_test_gs();         // 0x66
  void op9A_set_gs_ff();       // 0x9A
  void op9B_set_gs_bit();      // 0x9B
  void op9D_test_gs_bit();     // 0x9D

  // 字串輸出 opcode(解字串 + 推進 pc + emit 給 sink)
  void op77_draw_and_set();    // 0x77
  void op78_set_msg();         // 0x78
  void op7B_ui_header();       // 0x7B
  void emit_string();          // 共用:在 pc 解字串、推進 pc、回呼 sink

  // --- batch 4:繪圖 / UI / 結束(繪圖類在 remake 由 framebuffer 自行呈現,
  //     VM 僅正確消耗 operand;script 結束/遭遇先當 halt)---
  void op73_clear_event();     // 0x73  gs[0x3E]=gs[0x3F]
  void op74_draw_frame();      // 0x74  畫框,消耗 4 byte(x,y,w,h)
  void op75_ui_full();         // 0x75  ui_draw_full(無 operand)
  void op76_draw_pattern();    // 0x76  draw_pattern(無 operand)
  void op5A_ret();             // 0x5A  script 結束/返回(有 run_script 框→return,否則 halt)
  void op8A_encounter();       // 0x8A  隨機遭遇 → 先 halt(尚無戰鬥)

  // --- batch 5:跨資源 call / 資料資源存取 / 流程 / PRNG ---
  void op0C_r2_from_data();    // 0x0C  word_3AE2 = data[operand] & mode 高位遮罩
  void op1C_data_store();      // 0x1C  data[operand] = imm(byte/word)
  void op40_cmp_r4_imm();      // 0x40  cmp byte(r4), imm → 設旗標
  void op43_jump_above();      // 0x43  (flags & 0x41)==1 → jmp
  void op4D_prng();            // 0x4D  偽隨機:r2 = (ax*r2) 高位
  void op58_xcall();           // 0x58  跨資源 script call(push context、切資源、跳 src_offset)
  void op59_xret();            // 0x59  op_58 的返回(pop context)
  void op5C_party_loop();      // 0x5C  依 gs[0x1F] 重複 run_script(子 script 迴圈)
  void op62_scan_char();       // 0x62  掃描隊伍角色屬性(無 party 資料→消耗 operand、設旗標)

  // --- batch 6:byte 堆疊存取 / 資料資源讀 / 比較 / viewport ---
  void op03_pop_data_res();    // 0x03  pop byte → word_3AEA(切資料資源)
  void op04_push_script_res(); // 0x04  push byte(word_3AE8)
  void op0D_r2_from_data_off();// 0x0D  r2 = data[operand + r4](2-byte,mode 遮罩)
  void op3F_cmp_r4_gs();       // 0x3F  cmp byte(r4) vs gs[operand] → 設旗標
  void op55_peek_pop_r2();     // 0x55  peek word→r2、pop byte(word 模式再 pop)
  void op56_push_r2();         // 0x56  push r2(byte/word)
  void op8B_refresh_viewport();// 0x8B  refresh_viewport(render,VM 無副作用)

  // --- batch 7:gamestate/資源讀 + r4 byte 堆疊 ---
  void op0B_r2_from_gs_off();  // 0x0B  r2 = gs[operand + r4](2-byte,mode 遮罩)
  void op0F_r2_from_res();     // 0x0F  從 gs 指定的資源/偏移讀 word → r2
  void op93_push_r4();         // 0x93  push byte(r4)
  void op94_pop_r4();          // 0x94  pop byte → r4

  // --- batch 8:gs-索引資料讀寫(word_3ADF)+ gs offset 寫 ---
  void op10_r2_from_data_gs(); // 0x10  r2 = data[gs[op1] + op2]
  void op13_gs_off_from_r2();  // 0x13  gs[operand + r4] = r2
  void op14_data_from_r2();    // 0x14  data[operand] = r2
  void op15_data_off_from_r2();// 0x15  data[operand + r4] = r2
  void op16_data_gsoff_from_r2();// 0x16 data[gs[op]+r4] = r2

  // --- batch 9:gs 複製 + 資料資源字串 emit ---
  void op19_gs_copy();         // 0x19  gs[op2] = gs[op1](byte/word)
  void op7A_emit_data_string();// 0x7A  從 data[r2] 解字串 emit、r2=next
  void op7C_ui_header_data();  // 0x7C  set_ui_header(data, r2):同 7A 解字串 emit

  // --- 互動等待:headless 抽取無鍵盤輸入,讀完 operand 後停止該段(非整體 halt 邏輯) ---
  void op88_wait_escape();     // 0x88  wait_for_escape_key:抽取期視為段落結束
  void op89_wait_event();      // 0x89  wait_event:讀 flags(2B)後結束該段(無輸入可分支)
  void op81_print_number();    // 0x81  print_number:把 word_3AE2(N)以十進位 emit

  // --- batch 10:文字輸出 / 互動提示(逐字對照 opendw,VM 狀態效應對齊)---
  void op7D_char_name();       // 0x7D  write_character_name:輸出當前角色名(無 operand)
  void op80_advance_cursor();  // 0x80  advance_cursor:讀 1B operand、ui_draw_string + 補空白
  void op8C_prompt_no_yes();   // 0x8C  prompt_no_yes:Y/N 提示,依鍵值設 word_3AE6 旗標

  // --- batch 11:資料資源讀(gs 索引)/ 印字 / 音效 ---
  void op0E_r2_from_data_gsoff();// 0x0E  r2 = data[gs[op]|gs[op+1]<<8 + r4](word,mode 遮罩)
  void op83_print_char();      // 0x83  把 word_3AE2 以 byte/word 模式 emit(印字,無 operand)
  void op90_sound_effect();    // 0x90  op_sound_effect:讀 1B operand(音效),VM 僅消耗 operand

  // 切換 running_script / word_3ADF 到資源 idx(對照 populate_3ADD_and_3ADF)。
  // 用 resource_provider 取 bytes;成功回 true。
  bool load_resource(int idx, std::vector<std::uint8_t>& out);
  void run_script(int script_index, std::uint16_t src_offset);  // 對照 run_script

  // 輔助
  void set_gs(std::uint16_t idx, std::uint8_t val);
  void get_bit_mask(std::uint8_t al);
  void set_flags();

  MessageSink msg_sink_;
};

}  // namespace dw::vm
