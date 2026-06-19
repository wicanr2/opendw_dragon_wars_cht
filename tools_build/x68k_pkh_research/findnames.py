import struct,re
DX=open('/w/DRAGON.X','rb').read()
BASE=0x1440
for s in [b'TITLE',b'.PKH',b'PKH',b'3D1',b'END1',b'SUBTTL',b'DRAGON']:
    idx=0; hits=[]
    while True:
        p=DX.find(s,idx)
        if p<0: break
        hits.append(p); idx=p+1
    if hits:
        for h in hits[:8]:
            ctx=DX[max(0,h-4):h+16]
            print(f"{s.decode():8s} file0x{h:x} vaddr0x{h-BASE:x}: {ctx}")
