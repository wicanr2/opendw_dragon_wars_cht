// trace_harness — opendw 側差異測試 oracle(共享 bytecode,與 trace_remake.cpp 同步)。
#include <cstdio>
extern "C" void dw_trace_run(const unsigned char *code, int len);
static const unsigned char prog[] = {
/*00*/ 0x00,              /* set_word_mode */
/*01*/ 0x09,0x05,0x00,    /* r2=5 */
/*04*/ 0x12,0x10,         /* gs[10]=r2 */
/*06*/ 0x0A,0x10,         /* r2=gs[10] */
/*08*/ 0x24,              /* r2++ =6 */
/*09*/ 0x2A,              /* r2<<1 =0x0C */
/*0A*/ 0x27,              /* r2-- =0x0B */
/*0B*/ 0x38,0x0E,0x00,    /* r2&=0x0E =0x0A */
/*0E*/ 0x3A,0x01,0x00,    /* r2|=1 =0x0B */
/*11*/ 0x3C,0x03,0x00,    /* r2^=3 =0x08 */
/*14*/ 0x3E,0x08,0x00,    /* cmp r2,8 -> zf=1 */
/*17*/ 0x45,0x25,0x00,    /* jnz 0x25 (ZF set -> no jump) */
/*1A*/ 0x4B,              /* stc */
/*1B*/ 0x41,0x25,0x00,    /* jnc 0x25 (carry set -> no jump) */
/*1E*/ 0x4C,              /* clc */
/*1F*/ 0x41,0x25,0x00,    /* jnc 0x25 (carry clear -> JUMP) */
/*22*/ 0x00,0x00,0x00,    /* skipped */
/*25*/ 0x06,0x07,         /* r4=7 */
/*27*/ 0x25,              /* r4++ =8 */
/*28*/ 0x52,0x2C,0x00,    /* jmp 0x2C (end) */
/*2B*/ 0x00               /* pad */
};
int main(){ dw_trace_run(prog,(int)sizeof(prog)); return 0; }
