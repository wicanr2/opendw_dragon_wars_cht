import struct
PKH=open('/w/TITLE.PKH','rb').read()
# instrument decode: record (in_offset, out_nibble_index, ctrl, kind) ops
ops=[]; i=0; n=len(PKH); out_idx=0
while i<n:
    in_off=i; ctrl=PKH[i]; i+=1; count=ctrl&0x7f
    if ctrl&0x80:
        if i>=n: break
        b=PKH[i]; i+=1; kind='RUN'; produced=count
        out_idx+=count
    else:
        kind='LIT'; produced=count; out_idx+=count
        # consume ceil(count/2) bytes
        consumed=(count+1)//2
        i+=consumed
    ops.append((in_off,out_idx,kind,count))
print("total ops",len(ops),"final out_idx",out_idx)
# The coherent top is ~190 rows. At w=512 that's ~190*512=97280 nibbles. find ops near there.
# Find the largest single RUN (the long gray band) 
runs=[(o[3],o[0],o[1]) for o in ops if o[2]=='RUN']
runs.sort(reverse=True)
print("top runs (count, in_off, out_idx_after):", runs[:5])
# value 5 gray band len 38784 mentioned. find run with count near max repeated -> actually run count max=0x7f=127
# long band = many consecutive RUN ops of same byte. Detect transition: where do LIT ops dominate (noise)?
# bucket out_idx into rows of 512, count LIT vs RUN per 5000-nibble window
import collections
W=5000; lit=collections.Counter(); run=collections.Counter()
for o in ops:
    bucket=o[1]//W
    if o[2]=='LIT': lit[bucket]+=1
    else: run[bucket]+=1
print("\nwindow(5000 nib): row~ | RUNops LITops")
for b in range(0, out_idx//W+1):
    r=run[b]; l=lit[b]
    flag=' <== noise starts' if l>r*3 and b>5 else ''
    if b<60: print(f"  out~{b*W:7d} (row@512~{b*W//512:3d}): RUN={r:4d} LIT={l:4d}{flag}")
