#!/usr/bin/env python3
"""把 Dragon Wars 全螢幕圖資源(res 24-29,解壓後 32000 bytes)渲染成 PNG。
關鍵:全螢幕圖為「垂直 XOR delta 交錯」格式,需先做 title_adjust 還原(對照
opendw main.c title_adjust),否則畫面會出現紅/青交錯條紋、配色錯亂。
用法: scene_render.py <res_NN.bin(已解壓 32000B)> <out.png>
"""
import sys
from PIL import Image
P=[(0,0,0),(0,0,0xAA),(0,0xAA,0),(0,0xAA,0xAA),(0xAA,0,0),(0xAA,0,0xAA),(0xAA,0x55,0),(0xAA,0xAA,0xAA),
(0x55,0x55,0x55),(0x55,0x55,0xFF),(0x55,0xFF,0x55),(0x55,0xFF,0xFF),(0xFF,0x55,0x55),(0xFF,0x55,0xFF),(0xFF,0xFF,0x55),(0xFF,0xFF,0xFF)]
def title_adjust(raw):
    buf=bytearray(raw); src=0; dst=0xA0
    for _ in range(0x3E30):
        if src+0x9F>=len(buf) or dst+1>=len(buf): break
        ax=buf[src]|(buf[src+1]<<8); src+=2
        ax^=buf[src+0x9E]|(buf[src+0x9F]<<8)
        buf[dst]=ax&0xff; buf[dst+1]=(ax>>8)&0xff; dst+=2
    return buf
def render(raw,fn,scale=2):
    data=title_adjust(raw); img=Image.new('RGB',(320,200)); px=img.load(); i=0
    for y in range(200):
        for x in range(0,320,2):
            b=data[i] if i<len(data) else 0; i+=1
            px[x,y]=P[(b>>4)&0xF]; px[x+1,y]=P[b&0xF]
    img.resize((320*scale,200*scale),Image.NEAREST).save(fn)
if __name__=='__main__':
    render(open(sys.argv[1],'rb').read(), sys.argv[2])
    print("wrote",sys.argv[2])
