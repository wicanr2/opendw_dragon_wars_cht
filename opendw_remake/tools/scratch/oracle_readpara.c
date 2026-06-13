// oracle_readpara — opendw 權威對拍:headless 驅動「Read paragraph」流程,
// 印出每個 op 執行前的 (offset, op, word_3AE2/3AE4/3AE6/byte_3AE1),並在 op_81
// (0x48C5,print_number)當下印出 word_3AE2(= 段落號 N)。
//
// 直接 #include opendw 的 engine.c 取得 static VM(cpu/targets/run loop),
// 用真實 DATA1 經 rm_init() 載入資源。硬體相關函式以本檔下方 stub 取代。
//
// 用法: oracle_readpara <area>   (cwd 需有 data1)
//   area 31 → level 資源 = area + 0x46 = 0x65;tile 0x17 script PC = 0x0695。
//
// 注意:本檔只放在 remake 樹,不修改 opendw 唯讀樹。

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// --- 硬體 / FE stub(engine.c 會呼叫,但對 N 計算無關)---
// 這些必須在 #include engine.c 之前/之後提供;放在後面(engine.c 只宣告原型於各 .h)。

// 以 HEAD engine.c(權威 VM)#include 進來;真正的 ui/vga/mouse/timers 由連結階段
// 提供(連 vga_null.c headless FE)。本檔不再自行 stub,避免型別衝突。
#include "engine_head.c"

// no-op byte handler:供 extract_string 推進 pc 用(不渲染、避開 headless UI 崩潰)。
static void noop_byte(unsigned char b) { (void)b; }

// --- 自訂 VM 迴圈:對齊 run_script 的 dispatch,但攔 op_81 印 N、攔 op_5A/未實作停止 ---
static int trace_until_op81(int max_steps)
{
  uint8_t op = 0, prev = 0;
  for (int i = 0; i < max_steps; i++) {
    uint16_t off = (uint16_t)(cpu.pc - cpu.base_pc);
    prev = op;
    op = *cpu.pc++;
    cpu.ax = op;
    cpu.bx = cpu.ax;
    fprintf(stderr, "TR %04x op=%02x r2=%04x r4=%04x fl=%04x m=%02x sp=%02x peek=%02x%02x\n",
           off, op, word_3AE2, word_3AE4, word_3AE6, byte_3AE1,
           cpu.sp, cpu.stack[(cpu.sp+1)%STACK_SIZE], cpu.stack[cpu.sp]); fflush(stderr);
    if (op == 0x81) {
      // op_81 執行前 r2 即段落號;呼叫它(會 print_number)後回報。
      fprintf(stderr, ">>> op_81 reached: word_3AE2 (paragraph N) = %u\n", word_3AE2); fflush(stderr);
      return (int)word_3AE2;
    }
    // 字串/繪圖 op 在 headless 下會崩潰,且與 N 計算無關:
    //   op_78(set_msg)/op_7B(read_header_bytes)/op_77(draw+set):用 extract_string
    //   以 no-op handler 推進 pc(對照 set_msg:pc = base_pc + extract_string(...))。
    if (op == 0x78 || op == 0x7B) {
      cpu.bx = extract_string(cpu.base_pc, cpu.pc - cpu.base_pc, noop_byte);
      cpu.pc = cpu.base_pc + cpu.bx;
      continue;
    }
    void (*fn)(void) = targets[op].func;
    if (!fn) { fprintf(stderr, ">>> unhandled op 0x%02x (prev 0x%02x) at 0x%04x\n", op, prev, off); fflush(stderr); return -1; }
    fn();
    if (op == 0x5A) { fprintf(stderr, ">>> op_5A halt\n"); fflush(stderr); return -2; }
  }
  fprintf(stderr, ">>> max steps\n"); fflush(stderr);
  return -3;
}

int main(int argc, char **argv)
{
  int area = argc > 1 ? atoi(argv[1]) : 31;
  // tile→script PC 對照(由 remake level_events 取得,皆為各關 Read-paragraph tile)。
  // area:{level_res, tile_script_pc}
  // 各關 Read-paragraph tile 的 script PC(由 remake level_events 取得)。
  struct { int area; uint16_t pc; } map[] = {
    {17, 0x06EC}, {29, 0x0507}, {31, 0x0695}, {32, 0x04BD}, {35, 0x05B6}, {36, 0x03F2},
  };
  uint16_t tile_pc = 0;
  for (unsigned i = 0; i < sizeof(map)/sizeof(map[0]); i++)
    if (map[i].area == area) tile_pc = map[i].pc;
  if (!tile_pc) { fprintf(stderr, "unknown area %d\n", area); return 2; }

  fprintf(stderr, "[oracle] calling rm_init...\n"); fflush(stderr);
  int rc = rm_init();
  fprintf(stderr, "[oracle] rm_init rc=%d\n", rc); fflush(stderr);
  if (rc != 0) { fprintf(stderr, "rm_init failed (need data1 in cwd)\n"); return 1; }

  int level_res = area + 0x46;
  fprintf(stderr, "[oracle] resource_load(0x%x)...\n", level_res); fflush(stderr);
  // 載入 level 資源(經 find_index_by_tag / resource_load),取其 index 當 word_3AE8。
  struct resource *lvl = resource_load((enum resource_section)level_res);
  fprintf(stderr, "[oracle] lvl=%p\n", (void*)lvl); fflush(stderr);
  if (!lvl) { fprintf(stderr, "load level res 0x%x failed\n", level_res); return 1; }

  // 從 tile script PC 起,掃描第一個「58 08 15 00」(op_58 tag=8 src=0x15)當進入點,
  //   跳過前面的 op_74(畫框,與 N 無關,headless vga_null 下會崩)。
  uint16_t pc = 0;
  for (uint16_t i = tile_pc; i + 4 < lvl->len; i++) {
    if (lvl->bytes[i] == 0x58 && lvl->bytes[i+1] == 0x08 &&
        lvl->bytes[i+2] == 0x15 && lvl->bytes[i+3] == 0x00) { pc = i; break; }
  }
  if (!pc) { fprintf(stderr, "op_58 tag=8 src=0x15 not found near tile_pc\n"); return 1; }

  // 初始化 VM:cpu.sp 頂端、word_3AE8/3AEA = level index、populate、pc = op_58 處。
  cpu.sp = STACK_SIZE; // 對照 reset 時 cpu.sp = STACK_SIZE(STACK_SIZE 在 engine.c 內)
  word_3AE8 = lvl->index;
  word_3AEA = lvl->index;
  fprintf(stderr, "[oracle] lvl->index=%d len=%zu bytes=%p\n", lvl->index, lvl->len, (void*)lvl->bytes); fflush(stderr);
  populate_3ADD_and_3ADF();
  fprintf(stderr, "[oracle] populate done running_script=%p bytes=%p\n", (void*)running_script, running_script?(void*)running_script->bytes:0); fflush(stderr);
  cpu.base_pc = running_script->bytes;
  cpu.pc = running_script->bytes + pc;
  byte_3AE1 = 0; word_3AE2 = 0; word_3AE4 = 0; word_3AE6 = 0x02;

  printf("=== oracle area %d level_res=0x%x lvl->index=%d tile_pc=0x%04x op58_pc=0x%04x ===\n",
         area, level_res, lvl->index, tile_pc, pc);
  int n = trace_until_op81(2000);
  fprintf(stderr, "=== RESULT area %d N=%d ===\n", area, n); fflush(stderr);
  return 0;
}
