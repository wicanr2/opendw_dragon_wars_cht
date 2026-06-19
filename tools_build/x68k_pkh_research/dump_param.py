import struct
DX=open('/w/DRAGON.X','rb').read()
BASE=0x1440
def at(v,n): 
    o=BASE+v; return DX[o:o+n]
def hexs(b): return ' '.join(f'{x:02x}' for x in b)

print("=== region 0x36340 (pushed in pre-title call @0x6b6) .. 0x36590 ===")
for v in range(0x36340, 0x36590, 16):
    print(f"  0x{v:x}: {hexs(at(v,16))}  {at(v,16).decode('latin1','replace')}")

print("\n=== words just before string table 0x36587 (parallel param array?) ===")
# string table starts 0x36587; look at 0x363c0..0x36587 as u16 array
arr=at(0x36400,0x187)
