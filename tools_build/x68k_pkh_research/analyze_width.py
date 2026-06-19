def decode(data):
    out=[]; i=0; n=len(data)
    while i<n:
        ctrl=data[i]; i+=1; count=ctrl&0x7F
        if count==0: continue
        if ctrl&0x80:
            if i>=n: break
            b=data[i]; i+=1; out.extend([b&0x0F]*count)   # mask for index
        else:
            c=count
            while c>0:
                if i>=n: break
                b=data[i]; i+=1; out.append(b>>4); c-=1
                if c==0: break
                out.append(b&0x0F); c-=1
    return out
full=open('/w/out_d3/TITLE.PKH','rb').read()
px=decode(full[0x38:])
print("pixels",len(px))
# find long runs of identical value -> the gray band
runs=[]
i=0
while i<len(px):
    j=i
    while j<len(px) and px[j]==px[i]: j+=1
    if j-i>=200: runs.append((i,j-i,px[i]))
    i=j
print("long runs (start,len,val):")
for r in runs[:30]: print("  ",r)
# autocorrelation to find row period: compare px[k] vs px[k+W]
import statistics
best=[]
for W in range(128,1025):
    m=min(len(px)-W, 60000)
    match=sum(1 for k in range(0,m,3) if px[k]==px[k+W])
    cnt=len(range(0,m,3))
    best.append((match/cnt,W))
best.sort(reverse=True)
print("top autocorr widths:", [(round(s,3),w) for s,w in best[:15]])
