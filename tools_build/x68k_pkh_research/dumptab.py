import struct
DX = open('/w/DRAGON.X','rb').read()
BASE=0x1440
def rd_l(v): return struct.unpack('>I', DX[BASE+v:BASE+v+4])[0]
def rd_w(v): return struct.unpack('>H', DX[BASE+v:BASE+v+2])[0]
def cstr(v):
    o=BASE+v; s=b''
    while DX[o] not in (0,) and len(s)<40: s+=bytes([DX[o]]); o+=1
    return s

print("=== table @0x3638e, 8-byte entries, around id 0x80 ===")
for i in range(0x7c, 0x88):
    base = 0x3638e + i*8
    e0=rd_l(base); e1=rd_l(base+4)
    # e0 might be a pointer to filename string
    extra=''
    if 0 < e0 < 0x50000:
        try: extra = cstr(e0).decode('latin1','replace')
        except: pass
    print(f"  id 0x{i:02x}: @0x{base:x}  [0]={e0:#010x} [1]={e1:#010x}  str(@[0])='{extra}'")

print("\n=== raw bytes of table region 0x3678e..0x367ce (id 0x80..0x87) ===")
o=BASE+0x3678e
print(' '.join(f'{DX[o+k]:02x}' for k in range(0x40)))
