import struct
from capstone import *
from capstone.m68k import *

DX = open('/w/DRAGON.X','rb').read()
TEXT_FILE_BASE = 0x1440          # file_off = vaddr + 0x1440
# vaddr 0 maps to file 0x1440; the loaded image starts at file 0x1440 (after 64B HU header @0x1400)
img = DX[TEXT_FILE_BASE:]        # vaddr 0 == img[0]
def v2f(v): return v + TEXT_FILE_BASE

md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)
md.detail = True

XUNPACK   = 0x281be
PXUNPACK  = 0x281ec
XUNPACKB  = 0x287da

# 1) Find all JSR to the three unpack entries (absolute long: 4eb9 <addr32>)
print("=== JSR to unpack entries (4eb9 patterns) ===")
targets = {XUNPACK:'_xunpack', PXUNPACK:'_pxunpack', XUNPACKB:'_xunpackb'}
for tgt,name in targets.items():
    pat = b'\x4e\xb9' + struct.pack('>I', tgt)
    idx = 0
    while True:
        p = img.find(pat, idx)
        if p < 0: break
        print(f"  jsr {name}(0x{tgt:x}) at vaddr 0x{p:x} (file 0x{v2f(p):x})")
        idx = p+1

# 2) Find immediate 0x48 and 0x1f near each other (title hardcoded coords)
# move.l #imm patterns or pea, or moveq. Look for byte sequences 0x0048 and 0x001f as words.
print("\n=== occurrences of word 0x0048 followed within 32 bytes by 0x001f ===")
for i in range(0, len(img)-2, 2):
    w = struct.unpack('>H', img[i:i+2])[0]
    if w == 0x0048:
        # search nearby for 0x001f
        for j in range(max(0,i-40), min(len(img)-2, i+40), 2):
            w2 = struct.unpack('>H', img[j:j+2])[0]
            if w2 == 0x001f and abs(j-i) <= 40:
                print(f"  0x48@vaddr0x{i:x} (file0x{v2f(i):x})  0x1f@vaddr0x{j:x}")
                break
