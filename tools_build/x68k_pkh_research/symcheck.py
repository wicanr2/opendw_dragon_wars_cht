import struct
DX = open('/w/DRAGON.X','rb').read()
# symbol table near file 0x43eb8: records 02 01 00 02 <addr:4> <name\0>
# but doc said file 0x43eb8; our file is only 284524=0x457ac bytes. find symtab.
# pattern: 0x0201 type then ... Actually doc: "02 01 00 02 <addr:4> <name\0>"
start = DX.find(b'_xunpack')
print("first _xunpack name at file off", hex(start) if start>=0 else "none")
# dump symbol records: scan for ascii names of interest
import re
for name in [b'_xunpack\x00', b'_pxunpack\x00', b'_xunpackb\x00', b'_xcomp\x00']:
    p = 0
    while True:
        i = DX.find(name, p)
        if i<0: break
        # addr is 4 bytes before the name (record: ... <addr:4> <name>)
        addr = struct.unpack('>I', DX[i-4:i])[0]
        print(f"  {name.decode().strip(chr(0)):12s} name@file0x{i:x}  addr=0x{addr:x}")
        p = i+1
