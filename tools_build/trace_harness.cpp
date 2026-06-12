// trace_harness — opendw 側差異測試 oracle:用 dw_trace_run 跑共享 bytecode。
// bytecode 必須與 opendw_remake/tools/verify/trace_remake.cpp 完全一致(KEEP IN SYNC)。
#include <cstdio>
extern "C" void dw_trace_run(const unsigned char *code, int len);
static const unsigned char prog[] = {
/*00*/ 0x00,             /* set_word_mode */
/*01*/ 0x09,0x2A,0x00,   /* r2=0x2A (word) */
/*04*/ 0x21,             /* r4lo=r2 */
/*05*/ 0x22,             /* r2=r4 */
/*06*/ 0x4B,             /* stc */
/*07*/ 0x4C,             /* clc */
/*08*/ 0x99,             /* test r2 */
/*09*/ 0x53,0x14,0x00,   /* call 0x14 */
/*0C*/ 0x09,0x63,0x00,   /* r2=0x63 (after ret) */
/*0F*/ 0x52,0x1A,0x00,   /* jmp 0x1A (end) */
/*12*/ 0x00,0x00,        /* padding */
/*14*/ 0x09,0x07,0x00,   /* sub: r2=7 */
/*17*/ 0x54,             /* ret */
/*18*/ 0x00,0x00         /* padding */
};
int main(){ dw_trace_run(prog, (int)sizeof(prog)); return 0; }
