import sys, struct
from unicorn import *
from unicorn.m68k_const import *
DX=open('/w/DRAGON.X','rb').read()
PKH=open('/w/TITLE.PKH','rb').read()
BASE=0x1440
mu=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN)
mu.mem_map(0,0x100000); mu.mem_write(0,DX[BASE:])
mu.mem_write(0x89bd4,b'\0\0\0\0')
mu.mem_map(0xC00000,0x400000)
try: mu.mem_map(0xD00000,0x200000)  # for out=0xd83000/0xda0000
except: pass

BUF=0x69bd6
# emulate 0x11c4 by pre-loading PKH at BUF and stubbing the jsr 0x11c4 -> just return len in d0
# We'll hook the parse path directly. Simpler: call 0x27fa6(buf=BUF, len) after writing PKH to BUF.
mu.mem_write(BUF, PKH)

# unsupported addi.l/subi.l #imm,(a7)
def hook_code(mu,addr,size,u):
    op=struct.unpack('>H',mu.mem_read(addr,2))[0]
    if op in (0x0697,0x0497):
        imm=struct.unpack('>I',mu.mem_read(addr+2,4))[0]
        a7=mu.reg_read(UC_M68K_REG_A7); val=struct.unpack('>I',mu.mem_read(a7,4))[0]
        val=(val+imm)&0xFFFFFFFF if op==0x0697 else (val-imm)&0xFFFFFFFF
        mu.mem_write(a7,struct.pack('>I',val)); mu.reg_write(UC_M68K_REG_PC,addr+6)
mu.hook_add(UC_HOOK_CODE,hook_code)

def call(addr,args):
    sp=0x8F000
    for a in reversed(args): sp-=4; mu.mem_write(sp,struct.pack('>I',a&0xFFFFFFFF))
    sp-=4; mu.mem_write(sp,struct.pack('>I',0xFFFE)); mu.reg_write(UC_M68K_REG_A7,sp)
    try: mu.emu_start(addr,0xFFFE,count=80_000_000)
    except UcError as e: print("  [stop]",e,"PC=%#x"%mu.reg_read(UC_M68K_REG_PC))
    return mu.reg_read(UC_M68K_REG_D0)

print("=== first 0x40 bytes of BUF (= TITLE.PKH header region) ===")
hdr=mu.mem_read(BUF,0x40)
print(' '.join(f'{b:02x}' for b in hdr))
print("  buf+0x0c (pal[0..3]):", [hex(struct.unpack('>H',hdr[0x0c+2*i:0x0e+2*i])[0]) for i in range(4)])
print("  buf+0x34 (w):", hex(struct.unpack('>H',hdr[0x34:0x36])[0]))
print("  buf+0x36 (h):", hex(struct.unpack('>H',hdr[0x36:0x38])[0]))

d0=call(0x27fa6,[BUF,len(PKH)])
w=struct.unpack('>H',mu.mem_read(0x69bca,2))[0]
h=struct.unpack('>H',mu.mem_read(0x69bcc,2))[0]
de=struct.unpack('>I',mu.mem_read(0x69bd2,4))[0]
print(f"\nparse -> w={w}({w:#x}) h={h}({h:#x}) decode_end={de:#x} d0={d0:#x}")
print(f"  decoded words = {(de-0xD83000)//2 if de>0xD83000 else 'na'} (out=0xd83000)")
tab=[struct.unpack('>H',mu.mem_read(0x69ba6+2*i,2))[0] for i in range(16)]
print("  pal table @0x69ba6:", [hex(t) for t in tab])
