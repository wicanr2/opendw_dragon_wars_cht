#!/usr/bin/env python3
"""從 DATA1 萃取怪物 sprite 進 asset bundle:.spr(indexed)+ .png(編輯用)+ manifest。
.spr 格式: "DWSP" + u16 w + u16 h + u8 palN + palN*(r,g,b) + w*h*index(LE)。
用法: extract_sprite.py <sprite_dump_ppm> <res_id> <name> <bundle_dir>
(ppm 由 sprite_dump 產生:已是 DOS 16 色渲染)
"""
import sys, struct, json, os
from PIL import Image
DOS=[(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
(85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
def nearest(c): return min(range(16),key=lambda i:sum((c[j]-DOS[i][j])**2 for j in range(3)))
def main():
    ppm,rid,name,bundle=sys.argv[1],int(sys.argv[2]),sys.argv[3],sys.argv[4]
    img=Image.open(ppm).convert('RGB'); w,h=img.size
    idx=bytes(nearest(p) for p in img.getdata())
    os.makedirs(f'{bundle}/sprites',exist_ok=True)
    spr=struct.pack('<4sHHB',b'DWSP',w,h,16)+b''.join(struct.pack('BBB',*c) for c in DOS)+idx
    base=f'{rid:03d}_{name}'
    open(f'{bundle}/sprites/{base}.spr','wb').write(spr)
    # 用 DOS palette 量化的 indexed PNG(編輯用,改完轉回 .spr)
    pal=Image.new('P',(1,1)); flat=[]
    for c in DOS: flat+=list(c)
    flat+=[0]*(768-len(flat)); pal.putpalette(flat)
    img.quantize(palette=pal,dither=Image.NONE).save(f'{bundle}/sprites/{base}.png')
    # manifest
    mf=f'{bundle}/sprites/manifest.json'
    m=json.load(open(mf)) if os.path.exists(mf) else {}
    m[str(rid)]={'name':name,'spr':f'{base}.spr','png':f'{base}.png','w':w,'h':h,'source':'dos'}
    json.dump(m,open(mf,'w'),ensure_ascii=False,indent=2)
    print(f"res {rid} {name}: {base}.spr ({len(spr)}B) + .png + manifest")
if __name__=='__main__': main()
