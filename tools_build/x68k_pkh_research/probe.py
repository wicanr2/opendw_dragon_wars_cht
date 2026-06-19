import struct, math
PKH=open('/w/TITLE.PKH','rb').read()
def ent(b):
    if not b: return 0
    from collections import Counter
    c=Counter(b); n=len(b)
    return -sum(v/n*math.log2(v/n) for v in c.values())
print(f"file size {len(PKH)}")
print(f"entropy [0:0x38]   = {ent(PKH[:0x38]):.3f}")
print(f"entropy [0x38:]    = {ent(PKH[0x38:]):.3f}")
print(f"entropy [0:64]     = {ent(PKH[:64]):.3f}")
print(f"first 0x38 bytes: {' '.join(f'{b:02x}' for b in PKH[:0x38])}")

# decode the run/literal codec from a given start offset; report pixel count
def decode(data, start):
    out=[]; i=start; n=len(data)
    while i < n:
        ctrl=data[i]; i+=1
        count=ctrl & 0x7f
        if ctrl & 0x80:   # run
            if i>=n: break
            b=data[i]; i+=1
            out += [b & 0x0f]*count
        else:             # literal: count bytes, each -> hi,lo nibble (lo only if count allows)
            for _ in range(count):
                if i>=n: break
                b=data[i]; i+=1
                out.append(b>>4); out.append(b&0x0f)
    return out

for st in (0, 0x38):
    px=decode(PKH, st)
    print(f"decode from 0x{st:x}: {len(px)} nibbles; first 32: {px[:32]}")
