#!/usr/bin/env python3
"""dwemu (unicorn m68k) 實機跑 DRAGON.X 的 PKH parse(0x27fa6)+ GVRAM blit(0x2816a)。
用法: python3 emu_pkh_blit.py <DRAGON.X> <FILE.PKH>
注意: unicorn m68k 不支援 addi.l/subi.l #imm,(a7)(0x0697/0x0497),下方 CODE hook 手動補。
"""
import sys, struct
from unicorn import *
from unicorn.m68k_const import *
DX=open(sys.argv[1] if len(sys.argv)>1 else 'DRAGON.X','rb').read()
PKH=open(sys.argv[2] if len(sys.argv)>2 else 'TITLE.PKH','rb').read()
mu=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN)
mu.mem_map(0,0x100000); mu.mem_write(0,DX[0x1440:]); mu.mem_write(0x69bd6,PKH)
mu.mem_write(0x89bd4,b'\0\0\0\0'); mu.mem_map(0xC00000,0x400000)

# Handle unsupported "addi.l #imm,(a7)" (opcode 0697) and "subi.l #imm,(a7)" (0497) manually.
def hook_code(mu,addr,size,u):
    op=struct.unpack('>H',mu.mem_read(addr,2))[0]
    if op in (0x0697,0x0497):   # ADDI.L / SUBI.L  #imm,(a7)
        imm=struct.unpack('>I',mu.mem_read(addr+2,4))[0]
        a7=mu.reg_read(UC_M68K_REG_A7)
        val=struct.unpack('>I',mu.mem_read(a7,4))[0]
        if op==0x0697: val=(val+imm)&0xFFFFFFFF
        else: val=(val-imm)&0xFFFFFFFF
        mu.mem_write(a7,struct.pack('>I',val))
        mu.reg_write(UC_M68K_REG_PC,addr+6)   # skip the 6-byte instruction
mu.hook_add(UC_HOOK_CODE,hook_code)

def call(addr,args):
    sp=0x8F000
    for a in reversed(args):
        sp-=4; mu.mem_write(sp,struct.pack('>I',a&0xFFFFFFFF))
    sp-=4; mu.mem_write(sp,struct.pack('>I',0xFFFE))
    mu.reg_write(UC_M68K_REG_A7,sp)
    try: mu.emu_start(addr,0xFFFE,count=80_000_000)
    except UcError as e: print("  [stop]",e,"PC=%#x"%mu.reg_read(UC_M68K_REG_PC))
    return mu.reg_read(UC_M68K_REG_D0)

d0=call(0x27fa6,[0x69bd6,len(PKH)])
w=struct.unpack('>H',mu.mem_read(0x69bca,2))[0]
h=struct.unpack('>H',mu.mem_read(0x69bcc,2))[0]
oa=struct.unpack('>I',mu.mem_read(0x69bce,4))[0]
de=struct.unpack('>I',mu.mem_read(0x69bd2,4))[0]
print(f"parse: w={w}({w:#x}) h={h}({h:#x}) out={oa:#x} decend={de:#x} d0={d0:#x}")
print(f"  decoded words={(de-0xD83000)//2 if de>0xD83000 else 'na'}")
# table @0x69ba6 (16 words)
tab=[struct.unpack('>H',mu.mem_read(0x69ba6+2*i,2))[0] for i in range(16)]
print("  table@0x69ba6:", [hex(t) for t in tab])
dec=mu.mem_read(0xD83000, (de-0xD83000) if de>0xD83000 else 0x80000)
open('/w/decbuf_emu.bin','wb').write(bytes(dec))
d0=call(0x2816a,[0,0])
gv=mu.mem_read(0xC00000,1024*512*2)
open('/w/gvram_emu.bin','wb').write(bytes(gv))
print("GVRAM dumped, d0=%#x"%d0)
