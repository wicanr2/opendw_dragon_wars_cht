from PIL import Image
def decode(data):
    out=[]; i=0; n=len(data)
    while i<n:
        ctrl=data[i]; i+=1; count=ctrl&0x7F
        if count==0: continue
        if ctrl&0x80:
            if i>=n: break
            b=data[i]; i+=1; out.extend([b]*count)
        else:
            c=count
            while c>0:
                if i>=n: break
                b=data[i]; i+=1; out.append(b>>4); c-=1
                if c==0: break
                out.append(b&0x0F); c-=1
    return out
# grayscale-ish 16-level palette for inspection
PAL=[(i*17,i*17,i*17) for i in range(16)]
full=open('/w/out_d3/TITLE.PKH','rb').read()
px=decode(full[0x38:])
print("pixels",len(px))
for W in (256,320,384,512,640,768,401,527,496,448):
    H=len(px)//W
    if H<32: continue
    img=Image.new('RGB',(W,H))
    img.putdata([PAL[p&0xF] for p in px[:W*H]])
    img.save(f'/w/try_{W}.png')
    print("saved",W,"x",H)
