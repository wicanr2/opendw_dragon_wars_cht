#!/bin/bash
# 建置 batch10(op_7D/op_80/op_8C)差異測試 oracle harness。
#
# 這三個 opcode 在 opendw 會呼叫渲染 / 鍵盤 / player 資料 leaf(ui_draw_string、
# vga_update、ui_draw_chr_piece、wait_for_event、get_player_data),headless 無
# framebuffer / 鍵盤 → 直接跑會 segfault / 阻塞。
#
# 本 harness 把「不影響 VM 狀態(game_state / word_3AE2/3AE4/3AE6 / mode / cpu 暫存器)」
# 的 IO leaf 中性化,讓三個 handler 的「VM 狀態語意」能跑完並逐指令對拍 remake:
#   - vga_update / ui_draw_chr_piece / draw_character / mouse_* → no-op(只動 framebuffer)
#   - wait_for_event → 固定回 key=0(headless 無輸入;對齊 remake op_8C 的 key=0 預設)
#   - get_player_data → 回傳一個「單 byte、高位=0」的名字緩衝(讓 op_7D 名字迴圈即時收斂)
# 以上中性化都「不碰」VM 狀態欄位,故差異測試仍忠實反映 opcode 的 VM 契約。
#
# 用法: bash build_trace_oracle_batch10.sh <opendw路徑> <輸出dir>
set -e
OPENDW=${1:-/home/anr2/tmp/longcat/opendw}; OUT=${2:-/tmp/dwb10/oracle_build}
SRC=/tmp/dwsrc_b10; TB="$(cd "$(dirname "$0")" && pwd)"
rm -rf "$SRC"; cp -r "$OPENDW" "$SRC"

python3 - "$SRC" <<'PY'
import sys, re
SRC=sys.argv[1]; eng=f"{SRC}/src/lib/engine.c"; ui=f"{SRC}/src/lib/ui.c"; pl=f"{SRC}/src/lib/player.c"

# --- ui.c:去除重複 draw_right_pillar 定義(同既有 shim);中性化 ui_draw_chr_piece ---
# 注意:vga_update 在 headless(sys_ctx==NULL)本身就是 no-op,不需 patch。
# 只有 ui_draw_chr_piece 會走 draw_character/get_chr(需字型/framebuffer)→ 換成 no-op。
# 用「整段函式 body 精準替換」(已知原始碼),避免 #if 0 邊界誤吞後續定義。
u=open(ui).read()
u=u.replace('static void draw_right_pillar();\n','',1)
u=u.replace('''static void draw_right_pillar()
{
  draw_rect.x = 1;''','''static void draw_right_pillar_unused() __attribute__((unused));
static void draw_right_pillar_unused()
{
  draw_rect.x = 1;''',1)
old_chr='''void ui_draw_chr_piece(uint8_t chr)
{
  if ((chr & 0x80) == 0) {
    int16_t bx = (int16_t)draw_point.y;
    bx -= draw_rect.y;
    if (bx > 0) {
      bx = bx >> 3;

      data_2AC3[bx] = chr;
      data_2AAA[bx] = 0xFF;
    }
  }
  if (chr == 0x8D) {
    // 0x3264
    draw_point.x = draw_rect.x;
    uint8_t al = draw_point.y;
    al += 8;
    if (al > draw_rect.h) {
      //sub_343A();
      printf("BP CS:3275\\n");
      exit(1);
      al = draw_point.y;
    }
    draw_point.y = al;
    return;
  }
  // 0x3280
  draw_character(draw_point.x, draw_point.y, get_chr(chr));
  draw_point.x++;
}'''
new_chr='''void ui_draw_chr_piece(uint8_t chr)
{
  // b10:中性化渲染(draw_character/get_chr 需字型+framebuffer);VM 狀態不受影響。
  (void)chr;
}'''
assert old_chr in u, "ui_draw_chr_piece 原始 body 未匹配(opendw 版本可能變動)"
u=u.replace(old_chr, new_chr, 1)
open(ui,'w').write(u)

# --- player.c:get_player_data 回傳「高位=0 的單 byte」緩衝,讓 op_7D 名字迴圈即收斂 ---
# 精準替換整個函式 body(只此函式,不波及前後 get_player_data_base / get_player_data_byte)。
p=open(pl).read()
p=p.replace(
'''unsigned char *get_player_data(int player)
{
  size_t offset = player * SIZE_OF_PLAYER;
  return data_C960 + offset;
}''',
'''static unsigned char _b10_name_buf[2] = {0x41, 0x00}; // 'A',high-bit=0 → op_7D 名字迴圈即收斂
unsigned char *get_player_data(int player)
{
  (void)player;
  return _b10_name_buf;
}''', 1)
open(pl,'w').write(p)

# --- engine.c:shim(同既有 build)+ wait_for_event 固定 key=0 + dw_trace_run inject ---
s=open(eng).read(); marker='struct op_call_table targets[] = {'
if 'static void sub_2AEE' not in s:
    shim=('// build shim\nstatic void sub_2AEE(){}\n'
      '#define op_4C op_clc\n#define op_4D op_prng\n#define op_55 op_peek_and_pop\n'
      '#define op_5B op_5B_unused\n#define op_62 op_scan_for_char\n'
      'static void op_56(void); static void op_59(void); static void op_5C(void); static void op_72(void);\n'
      'static void op_43(void){} static void op_5F(void){} static void op_60(void){} static void op_63(void){}\n')
    s=s.replace(marker, shim+marker, 1)

# batch11:dispatch_sound_effect 中性化 —— headless 無音效表(func_5060 全 NULL)會 exit(1);
# 其 VM 狀態契約(operand 消耗 + cpu.ax)在呼叫前已完成,故整個 dispatch no-op 不影響對拍。
s=re.sub(r'(static void dispatch_sound_effect\(int function_idx\)\s*\{)',
         r'\1\n  (void)function_idx; return; // b11: headless 中性化(無音效子系統)',
         s, count=1)

# wait_for_event:headless 固定回 key=0(對齊 remake op_8C 的 key=0)。
# 在函式 body 開頭直接設 cpu.ax=0 並 return,跳過鍵盤/滑鼠輪詢。
s=re.sub(r'(static void wait_for_event\(uint16_t flags, unsigned char \*src_ptr, const unsigned char \*base\)\s*\{)',
         r'\1\n  (void)flags;(void)src_ptr;(void)base; cpu.ax = 0; return; // b10: headless 固定 key=0',
         s, count=1)

if 'dw_trace_run' not in s:
    inject='''
static struct resource dw_trace_res;
void dw_trace_run(const unsigned char *code, int len){
  dw_trace_res.bytes=(unsigned char*)code; dw_trace_res.len=len; running_script=&dw_trace_res;
  word_3ADF=&dw_trace_res; // b11:word_3ADF(資料資源)指向同一份(對齊 remake data_bytes=script)
  memset(&cpu,0,sizeof(cpu)); cpu.sp=STACK_SIZE; cpu.pc=dw_trace_res.bytes; cpu.base_pc=dw_trace_res.bytes;
  byte_3AE1=0; word_3AE2=0; word_3AE4=0; word_3AE6=0; memset(&game_state,0,sizeof(game_state));
  extern void (*string_byte_handler_func)(unsigned char);
  extern void ui_draw_chr_piece(unsigned char);
  string_byte_handler_func = ui_draw_chr_piece; // b10:預設導向(已中性化)避免 NULL 函式指標
  while(cpu.pc<dw_trace_res.bytes+len){
    unsigned int off=(unsigned int)(cpu.pc-cpu.base_pc); unsigned char op=*cpu.pc++; cpu.ax=op; cpu.bx=cpu.ax;
    fprintf(stdout,"%04x op=%02x r2=%04x r4=%04x fl=%04x m=%02x\\n",off,op,word_3AE2,word_3AE4,word_3AE6,byte_3AE1);
    fflush(stdout); // 防 segfault 吞掉 buffer
    void(*f)(void)=targets[op].func; if(!f) break; f(); if(op==0x5A) break;
  }
}
'''
    s=s.rstrip()+'\n'+inject
open(eng,'w').write(s); print("batch10 oracle shim+neutralize+inject 完成")
PY

mkdir -p "$OUT"
docker run --rm -v "$SRC":/app -v "$OUT":/out -v /tmp/dwb10:/b -w /app dwtools bash -c '
mkdir -p /tmp/oo10; for f in src/lib/*.c; do gcc -c -O1 -w -Isrc/lib "$f" -o "/tmp/oo10/$(basename ${f%.c}).o" 2>/dev/null; done
ar rcs /tmp/oo10/libd.a /tmp/oo10/*.o 2>/dev/null
g++ -O1 -w -std=c++14 -Isrc/lib -I/b /b/oracle_main.cpp /tmp/oo10/libd.a -o /out/oracle 2>&1 | tail -5 && echo "OK b10 oracle" || echo "FAIL"'
