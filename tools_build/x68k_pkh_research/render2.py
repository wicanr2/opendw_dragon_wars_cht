import struct
from PIL import Image
PKH=open('/w/TITLE.PKH','rb').read()

# Faithful to 0x32c3c/0x32ccc/0x32cfe:
#  main: ctrl=*in++; count = ctrl & 0x7f (a byte, stored at 445d2; 445d3 is high byte of d0/d1=0)
#  if bit7: run(0x32ccc); else literal(0x32cfe)
#  run(0x32ccc): d0=count(byte@445d2), reads ONE byte b=*in++, writes word(b) count times (b not masked-> &0xff word). loop subqb#1,d0 bne.
#       Wait: 0x32ce8 moveb (a1)+,d1 is INSIDE? re-read: 0x32ce8 reads byte to d1 ONCE before loop label 0x32cea.
#  literal(0x32cfe): d0=count; loop: read byte b; d2=b>>4 write word; subqb#1,d0; if 0 stop; write word(b)[full]; subqb#1; bne.
#     => count counts NIBBLES. each byte gives 2 nibbles (hi=b>>4, lo=b full&but stored as word; display &0xf).
def decode(data, start=0):
    out=bytearray(); i=start; n=len(data)
    while i<n:
        ctrl=data[i]; i+=1
        count=ctrl & 0x7f
        if count==0: count=0  # 0 means 0 here (byte). keep.
        if ctrl & 0x80:
            # run: read 1 byte, write 'count' words of that byte (low nibble for display)
            if i>=n: break
            b=data[i]; i+=1
            out += bytes([b & 0x0f])*count
        else:
            # literal: 'count' = number of nibbles to emit, hi then lo per byte
            c=count
            while c>0:
                if i>=n: break
                b=data[i]; i+=1
                out.append(b>>4); c-=1
                if c<=0: break
                out.append(b & 0x0f); c-=1
    return out

pal=[]
base=[(0,0,0),(255,255,255),(200,40,40),(40,200,40),(40,40,200),(200,200,40),
      (200,40,200),(40,200,200),(128,128,128),(255,128,0),(128,64,0),(0,128,128),
      (180,120,80),(80,80,180),(60,60,60),(255,200,120)]
for c in base: pal+=list(c)
px=decode(PKH,0)
print("total nibbles:",len(px))
for w in (320,384,448,512):
    h=len(px)//w
    img=Image.new('P',(w,h)); img.putpalette(pal+[0]*(768-len(pal)))
    img.putdata(bytes(x&0xf for x in px[:w*h]))
    img.save(f'/w/t2_w{w}.png')
    print(f"w={w} h={h}")
