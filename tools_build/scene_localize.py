#!/usr/bin/env python3
"""產生 Dragon Wars 全螢幕 cutscene 的中文版(英文原版另由 scene_render.py 產出)。
做法:解碼(含 title_adjust)→ 清掉烤進圖裡的英文字區(都在黑底)→ 以 wqy 渲染中文。
譯名依 CONTEXT.md(納達/波卡城/罪惡之城/瑪根地底世界)。
用法: scene_localize.py <allpic_dir(含 rNN.bin)> <out_dir>
"""
import sys
from PIL import Image, ImageDraw, ImageFont
P=[(0,0,0),(0,0,0xAA),(0,0xAA,0),(0,0xAA,0xAA),(0xAA,0,0),(0xAA,0,0xAA),(0xAA,0x55,0),(0xAA,0xAA,0xAA),
(0x55,0x55,0x55),(0x55,0x55,0xFF),(0x55,0xFF,0x55),(0x55,0xFF,0xFF),(0xFF,0x55,0x55),(0xFF,0x55,0xFF),(0xFF,0xFF,0x55),(0xFF,0xFF,0xFF)]
FONT='/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc'
def title_adjust(raw):
    buf=bytearray(raw); src=0; dst=0xA0
    for _ in range(0x3E30):
        if src+0x9F>=len(buf) or dst+1>=len(buf): break
        ax=buf[src]|(buf[src+1]<<8); src+=2
        ax^=buf[src+0x9E]|(buf[src+0x9F]<<8)
        buf[dst]=ax&0xff; buf[dst+1]=(ax>>8)&0xff; dst+=2
    return buf
def decode_rgb(raw):
    d=title_adjust(raw); img=Image.new('RGB',(320,200)); px=img.load(); i=0
    for y in range(200):
        for x in range(0,320,2):
            b=d[i] if i<len(d) else 0; i+=1
            px[x,y]=P[(b>>4)&0xF]; px[x+1,y]=P[b&0xF]
    return img
def clear(img,x,y,w,h,c=(0,0,0)): ImageDraw.Draw(img).rectangle([x,y,x+w-1,y+h-1],fill=c)
def wrap(img,text,x,y,w,size=16,color=(255,255,255)):
    d=ImageDraw.Draw(img); f=ImageFont.truetype(FONT,size); cx,cy=x,y; lh=size+2
    for ch in text:
        if ch=='\n': cx=x; cy+=lh; continue
        if cx+size>x+w: cx=x; cy+=lh
        d.text((cx,cy),ch,font=f,fill=color); cx+=size
def centered(img,text,size,color,cy):
    d=ImageDraw.Draw(img); f=ImageFont.truetype(FONT,size)
    w=len(text)*size; d.text(((320-w)//2,cy),text,font=f,fill=color)

def run(src,out):
    R=lambda n: open(f'{src}/r{n}.bin','rb').read()
    # 24 右側
    im=decode_rgb(R(24)); clear(im,196,30,124,150)
    wrap(im,"納達被奮力\n一擲,拋回\n他來時的\n深淵……",200,44,118,16); save(im,out,24)
    # 25 下方
    im=decode_rgb(R(25)); clear(im,0,140,320,60)
    wrap(im,"納達發出悽厲慘叫,墜入瑪根地底世界的最深處。他的恐怖統治就此終結!",8,144,304,16); save(im,out,25)
    # 26 下方
    im=decode_rgb(R(26)); clear(im,0,128,320,72)
    wrap(im,"納達覆滅的消息為波卡城的囚徒帶來無盡喜悅。市民們親手摧毀並燒盡這座罪惡之城,重新奪回被納達奪走的自由。",8,132,304,16); save(im,out,26)
    # 27 下方
    im=decode_rgb(R(27)); clear(im,0,116,320,84)
    wrap(im,"一個和平的新時代就此展開,大地重生。願這樣的時光永不消逝……",8,120,304,16); save(im,out,27)
    # 28 The End -> 全劇終
    im=Image.new('RGB',(320,200)); centered(im,"全劇終",44,(255,255,255),70); save(im,out,28)
    # 29 標題:**保留**原本 Dragon Wars logo 與版權,僅在底部黑帶(y188-199)加小字火龍之戰
    im=decode_rgb(R(29))
    centered(im,"火龍之戰",12,(255,255,85),187)
    save(im,out,29)

def save(img,out,n):
    img.resize((640,400),Image.NEAREST).save(f'{out}/fullscreen_{n}_zh.png')
    print(f"  fullscreen_{n}_zh.png")

if __name__=='__main__':
    print("中文版 cutscene:"); run(sys.argv[1],sys.argv[2])
