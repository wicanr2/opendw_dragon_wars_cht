from PIL import Image
PKH=open('/w/TITLE.PKH','rb').read()
def decode(data,start=0):
    out=bytearray(); i=start; n=len(data)
    while i<n:
        ctrl=data[i]; i+=1; count=ctrl&0x7f
        if ctrl&0x80:
            if i>=n: break
            b=data[i]; i+=1; out+=bytes([b&0x0f])*count
        else:
            c=count
            while c>0:
                if i>=n: break
                b=data[i]; i+=1; out.append(b>>4); c-=1
                if c<=0: break
                out.append(b&0x0f); c-=1
    return out
px=decode(PKH,0)
pal=[]
for c in [(0,0,0),(255,255,255),(200,40,40),(40,200,40),(40,40,200),(200,200,40),
      (200,40,200),(40,200,200),(128,128,128),(255,128,0),(128,64,0),(0,128,128),
      (180,120,80),(80,80,180),(60,60,60),(255,200,120)]: pal+=list(c)
pal+= [0]*(768-len(pal))
N=len(px)
# try many widths, render full, save thumbnails for the detail region
for w in (256,288,304,320,336,352,360,368,376,384,400,416,432,448,464,480,496,512,640):
    h=N//w
    if h<50: continue
    img=Image.new('P',(w,h)); img.putpalette(pal)
    img.putdata(bytes(x&0xf for x in px[:w*h]))
    img.save(f'/w/s3_w{w}.png')
print("done", N)
