import struct
DX = open('/w/DRAGON.X','rb').read()
BASE=0x1440
img = DX[BASE:]
def v2f(v): return v+BASE
# find references to addresses 0x69bca (w), 0x69bcc (h), 0x69ba6 (pal table)
for label,addr in [('w 0x69bca',0x69bca),('h 0x69bcc',0x69bcc),('paltab 0x69ba6',0x69ba6),
                   ('out 0x69bce',0x69bce),('open-table 0x3638e',0x3638e),
                   ('GVRAM-scroll 0x89bd4',0x89bd4),('flag 0x5e340',0x5e340)]:
    pat = struct.pack('>I', addr)
    idx=0; hits=[]
    while True:
        p=img.find(pat,idx)
        if p<0: break
        hits.append(p); idx=p+1
    print(f"{label}: {len(hits)} refs ->", [hex(h) for h in hits[:20]])
