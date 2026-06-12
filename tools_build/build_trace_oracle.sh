#!/bin/bash
# 建置 opendw「差異測試 oracle」harness:用 trace 跑 bytecode 逐指令印狀態。
# 用法: bash build_trace_oracle.sh <opendw路徑> <輸出dir>
set -e
OPENDW=${1:-/home/anr2/tmp/longcat/opendw}; OUT=${2:-/tmp/dwbuild/trace}
SRC=/tmp/dwsrc_oracle; TB="$(cd "$(dirname "$0")" && pwd)"
rm -rf "$SRC"; cp -r "$OPENDW" "$SRC"
cp "$TB/trace_harness.cpp" "$SRC/src/tools/trace_harness.cpp"
python3 - "$SRC" <<'PY'
import sys; SRC=sys.argv[1]; eng=f"{SRC}/src/lib/engine.c"; ui=f"{SRC}/src/lib/ui.c"
u=open(ui).read()
u=u.replace('static void draw_right_pillar();\n','',1)
u=u.replace('''static void draw_right_pillar()
{
  draw_rect.x = 1;''','''static void draw_right_pillar_unused() __attribute__((unused));
static void draw_right_pillar_unused()
{
  draw_rect.x = 1;''',1)
open(ui,'w').write(u)
s=open(eng).read(); marker='struct op_call_table targets[] = {'
if 'static void sub_2AEE' not in s:
    shim=('// build shim\nstatic void sub_2AEE(){}\n'
      '#define op_4C op_clc\n#define op_4D op_prng\n#define op_55 op_peek_and_pop\n'
      '#define op_5B op_5B_unused\n#define op_62 op_scan_for_char\n'
      'static void op_56(void); static void op_59(void); static void op_5C(void); static void op_72(void);\n'
      'static void op_43(void){} static void op_5F(void){} static void op_60(void){} static void op_63(void){}\n')
    s=s.replace(marker, shim+marker, 1)
if 'dw_trace_run' not in s:
    inject='''
static struct resource dw_trace_res;
void dw_trace_run(const unsigned char *code, int len){
  dw_trace_res.bytes=(unsigned char*)code; dw_trace_res.len=len; running_script=&dw_trace_res;
  memset(&cpu,0,sizeof(cpu)); cpu.sp=STACK_SIZE; cpu.pc=dw_trace_res.bytes; cpu.base_pc=dw_trace_res.bytes;
  byte_3AE1=0; word_3AE2=0; word_3AE4=0; word_3AE6=0; memset(&game_state,0,sizeof(game_state));
  while(cpu.pc<dw_trace_res.bytes+len){
    unsigned int off=(unsigned int)(cpu.pc-cpu.base_pc); unsigned char op=*cpu.pc++; cpu.ax=op; cpu.bx=cpu.ax;
    fprintf(stdout,"%04x op=%02x r2=%04x r4=%04x fl=%04x m=%02x\\n",off,op,word_3AE2,word_3AE4,word_3AE6,byte_3AE1);
    void(*f)(void)=targets[op].func; if(!f) break; f(); if(op==0x5A) break;
  }
}
'''
    s=s.rstrip()+'\n'+inject
open(eng,'w').write(s); print("oracle shim+inject 完成")
PY
mkdir -p "$OUT"
docker run --rm -v "$SRC":/app -v "$OUT":/out -w /app dwtools bash -c '
mkdir -p /tmp/oo; for f in src/lib/*.c; do gcc -c -O1 -w -Isrc/lib "$f" -o "/tmp/oo/$(basename ${f%.c}).o" 2>/dev/null; done
ar rcs /tmp/oo/libd.a /tmp/oo/*.o 2>/dev/null
g++ -O1 -w -std=c++14 -Isrc/lib src/tools/trace_harness.cpp /tmp/oo/libd.a -o /out/trace_opendw 2>/dev/null && echo "OK trace_opendw" || echo "FAIL"'
