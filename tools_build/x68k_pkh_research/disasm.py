import struct, sys
from capstone import *
DX = open('/w/DRAGON.X','rb').read()
BASE = 0x1440
img = DX[BASE:]
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(vstart, vend, label):
    print(f"\n===== {label}  vaddr 0x{vstart:x}..0x{vend:x} =====")
    code = img[vstart:vend]
    for insn in md.disasm(code, vstart):
        b = ' '.join(f'{x:02x}' for x in insn.bytes)
        print(f"  {insn.address:6x}: {b:<24} {insn.mnemonic} {insn.op_str}")

regions = eval(sys.argv[1])
for (a,b,l) in regions:
    dis(a,b,l)
