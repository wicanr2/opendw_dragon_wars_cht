# X68000 DRAGON.X PKH 解碼/GVRAM 關鍵常式反組譯摘錄
# vaddr↔file: file_off = vaddr + 0x1440 (HU header @0x1400)
# 全大端 (big-endian) 讀寫。由 m68k-linux-gnu-objdump -b binary -m m68k 反組譯。

## _xunpack 本體 (0x281b6): read → parse → GVRAM 展開
   281b6:	42a7           	clrl %sp@-
   281b8:	2f3c 0006 9bd6 	movel #433110,%sp@-
   281be:	2f2e 0010      	movel %fp@(16),%sp@-
   281c2:	4eb9 0000 11c4 	jsr 0x11c4
   281c8:	4fef 000c      	lea %sp@(12),%sp
   281cc:	2d40 fff8      	movel %d0,%fp@(-8)
   281d0:	2f2e fff8      	movel %fp@(-8),%sp@-
   281d4:	2f3c 0006 9bd6 	movel #433110,%sp@-
   281da:	4eb9 0002 7fa6 	jsr 0x27fa6
   281e0:	508f           	addql #8,%sp
   281e2:	2f2e 000c      	movel %fp@(12),%sp@-
   281e6:	2f2e 0008      	movel %fp@(8),%sp@-
   281ea:	4eb9 0002 816a 	jsr 0x2816a
   281f0:	508f           	addql #8,%sp
   281f2:	4e5e           	unlk %fp
   281f4:	4e75           	rts
   281f6:	4e56 fff8      	linkw %fp,#-8

## 0x27f62: big-endian 16-bit 讀 (buf[off]<<8 | buf[off+1])
   27f64:	426e fffe      	clrw %fp@(-2)
   27f68:	200e           	movel %fp,%d0
   27f6a:	d0bc ffff fffe 	addl #-2,%d0
   27f70:	2d40 fffa      	movel %d0,%fp@(-6)
   27f74:	202e fffa      	movel %fp@(-6),%d0
   27f78:	2040           	moveal %d0,%a0
   27f7a:	222e 0008      	movel %fp@(8),%d1
   27f7e:	2241           	moveal %d1,%a1
   27f80:	1091           	moveb %a1@,%a0@
   27f82:	202e fffa      	movel %fp@(-6),%d0
   27f86:	5280           	addql #1,%d0
   27f88:	2040           	moveal %d0,%a0
   27f8a:	222e 0008      	movel %fp@(8),%d1
   27f8e:	5281           	addql #1,%d1
   27f90:	2241           	moveal %d1,%a1
   27f92:	1091           	moveb %a1@,%a0@
   27f94:	302e fffe      	movew %fp@(-2),%d0
   27f98:	6000 0002      	braw 0x27f9c
   27f9c:	4e5e           	unlk %fp
   27f9e:	4e75           	rts

## 0x27fa6/0x27faa: header parse (16-word pal@buf+0x0c, w@buf+0x34, h@buf+0x36, out=0xD83000, decode=0x32c3c(out,buf+0x38,len-0x38))
   27faa:	42ae fffc      	clrl %fp@(-4)
   27fae:	202e fffc      	movel %fp@(-4),%d0
   27fb2:	b0bc 0000 0010 	cmpl #16,%d0
   27fb8:	6c30           	bges 0x27fea
   27fba:	202e fffc      	movel %fp@(-4),%d0
   27fbe:	e380           	asll #1,%d0
   27fc0:	d0ae 0008      	addl %fp@(8),%d0
   27fc4:	d0bc 0000 000c 	addl #12,%d0
   27fca:	2f00           	movel %d0,%sp@-
   27fcc:	4eb9 0002 7f62 	jsr 0x27f62
   27fd2:	588f           	addql #4,%sp
   27fd4:	222e fffc      	movel %fp@(-4),%d1
   27fd8:	e381           	asll #1,%d1
   27fda:	d2bc 0006 9ba6 	addl #433062,%d1
   27fe0:	2241           	moveal %d1,%a1
   27fe2:	3280           	movew %d0,%a1@
   27fe4:	52ae fffc      	addql #1,%fp@(-4)
   27fe8:	60c4           	bras 0x27fae
   27fea:	4279 0006 9bc6 	clrw 0x69bc6
   27ff0:	4279 0006 9bc8 	clrw 0x69bc8
   27ff6:	2f2e 0008      	movel %fp@(8),%sp@-
   27ffa:	0697 0000 0034 	addil #52,%sp@
   28000:	4eb9 0002 7f62 	jsr 0x27f62
   28006:	588f           	addql #4,%sp
   28008:	33c0 0006 9bca 	movew %d0,0x69bca
   2800e:	2f2e 0008      	movel %fp@(8),%sp@-
   28012:	0697 0000 0036 	addil #54,%sp@
   28018:	4eb9 0002 7f62 	jsr 0x27f62
   2801e:	588f           	addql #4,%sp
   28020:	33c0 0006 9bcc 	movew %d0,0x69bcc
   28026:	23fc 00d8 3000 	movel #14168064,0x69bce
   2802c:	0006 9bce 
   28030:	2f2e 000c      	movel %fp@(12),%sp@-
   28034:	0497 0000 0038 	subil #56,%sp@
   2803a:	2f2e 0008      	movel %fp@(8),%sp@-
   2803e:	0697 0000 0038 	addil #56,%sp@
   28044:	2f39 0006 9bce 	movel 0x69bce,%sp@-
   2804a:	4eb9 0003 2c3c 	jsr 0x32c3c
   28050:	4fef 000c      	lea %sp@(12),%sp
   28054:	23c0 0006 9bd2 	movel %d0,0x69bd2
   2805a:	2039 0006 9bd2 	movel 0x69bd2,%d0
   28060:	b0bc 00e0 0000 	cmpl #14680064,%d0
   28066:	6314           	blss 0x2807c
   28068:	2f39 0006 9bd2 	movel 0x69bd2,%sp@-
   2806e:	2f3c 0003 9f6c 	movel #237420,%sp@-
   28074:	4eb9 0003 3d4e 	jsr 0x33d4e
   2807a:	508f           	addql #8,%sp
   2807c:	4e5e           	unlk %fp
   2807e:	4e75           	rts

## 0x32c3c: 核心解碼主迴圈 (ctrl&0x7F=count; bit7=1→run(0x32ccc); bit7=0→literal(0x32cfe))
   32c3c:	202f 0004      	movel %sp@(4),%d0
   32c40:	c0bc 00ff ffff 	andl #16777215,%d0
   32c46:	2040           	moveal %d0,%a0
   32c48:	23c0 0004 45d6 	movel %d0,0x445d6
   32c4e:	202f 0008      	movel %sp@(8),%d0
   32c52:	c0bc 00ff ffff 	andl #16777215,%d0
   32c58:	2240           	moveal %d0,%a1
   32c5a:	23c0 0004 45da 	movel %d0,0x445da
   32c60:	222f 000c      	movel %sp@(12),%d1
   32c64:	c2bc 00ff ffff 	andl #16777215,%d1
   32c6a:	d081           	addl %d1,%d0
   32c6c:	23c0 0004 45de 	movel %d0,0x445de
   32c72:	2279 0004 45da 	moveal 0x445da,%a1
   32c78:	1011           	moveb %a1@,%d0
   32c7a:	1219           	moveb %a1@+,%d1
   32c7c:	23c9 0004 45da 	movel %a1,0x445da
   32c82:	c03c 007f      	andb #127,%d0
   32c86:	13c0 0004 45d2 	moveb %d0,0x445d2
   32c8c:	0801 0007      	btst #7,%d1
   32c90:	6716           	beqs 0x32ca8
   32c92:	4eb9 0003 2ccc 	jsr 0x32ccc
   32c98:	2039 0004 45da 	movel 0x445da,%d0
   32c9e:	b0b9 0004 45de 	cmpl 0x445de,%d0
   32ca4:	641e           	bccs 0x32cc4
   32ca6:	60ca           	bras 0x32c72
   32ca8:	2039 0004 45de 	movel 0x445de,%d0
   32cae:	4eb9 0003 2cfe 	jsr 0x32cfe
   32cb4:	2039 0004 45da 	movel 0x445da,%d0
   32cba:	b0b9 0004 45de 	cmpl 0x445de,%d0
   32cc0:	6402           	bccs 0x32cc4
   32cc2:	60ae           	bras 0x32c72
   32cc4:	2039 0004 45d6 	movel 0x445d6,%d0
   32cca:	4e75           	rts

## 0x32ccc: run — 讀1 byte 重複 count 次寫 word (byte→word RLE)
   32ccc:	4280           	clrl %d0
   32cce:	4281           	clrl %d1
   32cd0:	1039 0004 45d2 	moveb 0x445d2,%d0
   32cd6:	1239 0004 45d3 	moveb 0x445d3,%d1
   32cdc:	2079 0004 45d6 	moveal 0x445d6,%a0
   32ce2:	2279 0004 45da 	moveal 0x445da,%a1
   32ce8:	1219           	moveb %a1@+,%d1
   32cea:	30c1           	movew %d1,%a0@+
   32cec:	5300           	subqb #1,%d0
   32cee:	66fa           	bnes 0x32cea
   32cf0:	23c8 0004 45d6 	movel %a0,0x445d6
   32cf6:	23c9 0004 45da 	movel %a1,0x445da
   32cfc:	4e75           	rts

## 0x32cfe: literal — 每讀1 byte 拆 hi(b>>4)/lo(b) 各寫 word (4bpp 展開; lo 取 &0x0F)
   32cfe:	4280           	clrl %d0
   32d00:	4281           	clrl %d1
   32d02:	1039 0004 45d2 	moveb 0x445d2,%d0
   32d08:	1239 0004 45d3 	moveb 0x445d3,%d1
   32d0e:	2079 0004 45d6 	moveal 0x445d6,%a0
   32d14:	2279 0004 45da 	moveal 0x445da,%a1
   32d1a:	1219           	moveb %a1@+,%d1
   32d1c:	2401           	movel %d1,%d2
   32d1e:	e88a           	lsrl #4,%d2
   32d20:	30c2           	movew %d2,%a0@+
   32d22:	5300           	subqb #1,%d0
   32d24:	b03c 0000      	cmpb #0,%d0
   32d28:	6706           	beqs 0x32d30
   32d2a:	30c1           	movew %d1,%a0@+
   32d2c:	5300           	subqb #1,%d0
   32d2e:	66ea           	bnes 0x32d1a
   32d30:	23c8 0004 45d6 	movel %a0,0x445d6
   32d36:	23c9 0004 45da 	movel %a1,0x445da
   32d3c:	4e75           	rts

## 0x2816a: GVRAM 展開派發 — push(0,h,w,out=0xD83000,(y<<10+x)<<1) 後 jsr 0x32b4c
   2816c:	42a7           	clrl %sp@-
   2816e:	3039 0006 9bcc 	movew 0x69bcc,%d0
   28174:	0280 0000 ffff 	andil #65535,%d0
   2817a:	2f00           	movel %d0,%sp@-
   2817c:	3039 0006 9bca 	movew 0x69bca,%d0
   28182:	0280 0000 ffff 	andil #65535,%d0
   28188:	2f00           	movel %d0,%sp@-
   2818a:	2f39 0006 9bce 	movel 0x69bce,%sp@-
   28190:	202e 000c      	movel %fp@(12),%d0
   28194:	720a           	moveq #10,%d1
   28196:	e3a0           	asll %d1,%d0
   28198:	d0ae 0008      	addl %fp@(8),%d0
   2819c:	e380           	asll #1,%d0
   2819e:	2f00           	movel %d0,%sp@-
   281a0:	4eb9 0003 2b4c 	jsr 0x32b4c
   281a6:	4fef 0014      	lea %sp@(20),%sp

## 0x32b4c: 真正 blit — dst=0xC00000+(arg<<...)+[0x89bd4]; src=0xD83000; 每列複製 w words;
##          dst 列步進 0x445e2=(1024-w)<<1 bytes → GVRAM stride=1024 words(2048 B)/line; 共 h 列。
   32b4c:	202f 0004      	movel %sp@(4),%d0
   32b50:	c0bc 00ff ffff 	andl #16777215,%d0
   32b56:	d0bc 00c0 0000 	addl #12582912,%d0
   32b5c:	d0b9 0008 9bd4 	addl 0x89bd4,%d0
   32b62:	2040           	moveal %d0,%a0
   32b64:	202f 0008      	movel %sp@(8),%d0
   32b68:	c0bc 00ff ffff 	andl #16777215,%d0
   32b6e:	2240           	moveal %d0,%a1
   32b70:	202f 0014      	movel %sp@(20),%d0
   32b74:	e380           	asll #1,%d0
   32b76:	23c0 0004 45e6 	movel %d0,0x445e6
   32b7c:	203c 0000 0400 	movel #1024,%d0
   32b82:	90af 000c      	subl %sp@(12),%d0
   32b86:	e380           	asll #1,%d0
   32b88:	23c0 0004 45e2 	movel %d0,0x445e2
   32b8e:	222f 0010      	movel %sp@(16),%d1
   32b92:	5381           	subql #1,%d1
   32b94:	202f 000c      	movel %sp@(12),%d0
   32b98:	5380           	subql #1,%d0
   32b9a:	2400           	movel %d0,%d2
   32b9c:	30d9           	movew %a1@+,%a0@+
   32b9e:	51ca fffc      	dbf %d2,0x32b9c
   32ba2:	d1f9 0004 45e2 	addal 0x445e2,%a0
   32ba8:	d3f9 0004 45e6 	addal 0x445e6,%a1
   32bae:	51c9 ffea      	dbf %d1,0x32b9a
   32bb2:	2008           	movel %a0,%d0
   32bb4:	4e75           	rts

## 0x32bb6: blit 變體 (2x 水平放大: 每 word 寫 a0 兩次 + a2=a0+2048 兩次)
   32bb6:	202f 0004      	movel %sp@(4),%d0
   32bba:	c0bc 00ff ffff 	andl #16777215,%d0
   32bc0:	d0bc 00c0 0000 	addl #12582912,%d0
   32bc6:	d0b9 0008 9bd4 	addl 0x89bd4,%d0
   32bcc:	2040           	moveal %d0,%a0
   32bce:	2440           	moveal %d0,%a2
   32bd0:	d5fc 0000 0800 	addal #2048,%a2
   32bd6:	202f 0008      	movel %sp@(8),%d0
   32bda:	c0bc 00ff ffff 	andl #16777215,%d0
   32be0:	2240           	moveal %d0,%a1
   32be2:	202f 0014      	movel %sp@(20),%d0
   32be6:	e380           	asll #1,%d0
   32be8:	23c0 0004 45e6 	movel %d0,0x445e6
   32bee:	203c 0000 0400 	movel #1024,%d0
   32bf4:	222f 000c      	movel %sp@(12),%d1
   32bf8:	e381           	asll #1,%d1
   32bfa:	9081           	subl %d1,%d0
   32bfc:	e380           	asll #1,%d0
   32bfe:	d0bc 0000 0800 	addl #2048,%d0
   32c04:	23c0 0004 45e2 	movel %d0,0x445e2
   32c0a:	222f 0010      	movel %sp@(16),%d1
   32c0e:	7000           	moveq #0,%d0
   32c10:	242f 000c      	movel %sp@(12),%d2
   32c14:	3019           	movew %a1@+,%d0
   32c16:	30c0           	movew %d0,%a0@+
   32c18:	30c0           	movew %d0,%a0@+
   32c1a:	34c0           	movew %d0,%a2@+
   32c1c:	34c0           	movew %d0,%a2@+
   32c1e:	5342           	subqw #1,%d2
   32c20:	66f2           	bnes 0x32c14
   32c22:	d1f9 0004 45e2 	addal 0x445e2,%a0
   32c28:	d5f9 0004 45e2 	addal 0x445e2,%a2
   32c2e:	d3f9 0004 45e6 	addal 0x445e6,%a1
   32c34:	5381           	subql #1,%d1
   32c36:	66d8           	bnes 0x32c10
   32c38:	2008           	movel %a0,%d0
   32c3a:	4e75           	rts

## 0x11c4: open+read 整檔到 buf (count=0 → 讀全檔; DOS _READ trap 0xff3f @0x33d6e)
    11c8:	200e           	movel %fp,%d0
    11ca:	d0bc ffff fff0 	addl #-16,%d0
    11d0:	2f00           	movel %d0,%sp@-
    11d2:	2f2e 0008      	movel %fp@(8),%sp@-
    11d6:	4eb9 0000 138a 	jsr 0x138a
    11dc:	508f           	addql #8,%sp
    11de:	2f3c 0000 0401 	movel #1025,%sp@-
    11e4:	200e           	movel %fp,%d0
    11e6:	d0bc ffff fff0 	addl #-16,%d0
    11ec:	2f00           	movel %d0,%sp@-
    11ee:	4eb9 0003 3b92 	jsr 0x33b92
    11f4:	508f           	addql #8,%sp
    11f6:	23c0 0003 d9f2 	movel %d0,0x3d9f2
    11fc:	b0bc ffff ffff 	cmpl #-1,%d0
    1202:	661a           	bnes 0x121e
    1204:	200e           	movel %fp,%d0
    1206:	d0bc ffff fff0 	addl #-16,%d0
    120c:	2f00           	movel %d0,%sp@-
    120e:	2f3c 0003 6618 	movel #222744,%sp@-
    1214:	4eb9 0000 1350 	jsr 0x1350
    121a:	508f           	addql #8,%sp
    121c:	6064           	bras 0x1282
    121e:	202e 0010      	movel %fp@(16),%d0
    1222:	6612           	bnes 0x1236
    1224:	2f39 0003 d9f2 	movel 0x3d9f2,%sp@-
    122a:	4eb9 0003 34c4 	jsr 0x334c4
    1230:	588f           	addql #4,%sp
    1232:	2d40 0010      	movel %d0,%fp@(16)
    1236:	2f2e 0010      	movel %fp@(16),%sp@-
    123a:	2f2e 000c      	movel %fp@(12),%sp@-
    123e:	2f39 0003 d9f2 	movel 0x3d9f2,%sp@-
    1244:	4eb9 0003 3d6e 	jsr 0x33d6e
    124a:	4fef 000c      	lea %sp@(12),%sp
    124e:	2d40 ffec      	movel %d0,%fp@(-20)
    1252:	2f39 0003 d9f2 	movel 0x3d9f2,%sp@-
    1258:	4eb9 0003 331c 	jsr 0x3331c
    125e:	588f           	addql #4,%sp
    1260:	202e ffec      	movel %fp@(-20),%d0
    1264:	b0ae 0010      	cmpl %fp@(16),%d0
    1268:	6418           	bccs 0x1282
    126a:	200e           	movel %fp,%d0
    126c:	d0bc ffff fff0 	addl #-16,%d0
    1272:	2f00           	movel %d0,%sp@-
    1274:	2f3c 0003 661e 	movel #222750,%sp@-
    127a:	4eb9 0000 1350 	jsr 0x1350
    1280:	508f           	addql #8,%sp
    1282:	202e ffec      	movel %fp@(-20),%d0
    1286:	6000 0002      	braw 0x128a
    128a:	4e5e           	unlk %fp
    128c:	4e75           	rts

# ============================================================================
# 2026-06-19 追加:標題載入呼叫點 + open 常式 + 圖號→檔名表 + 第二 parse 路徑
# ============================================================================

## 標題載入呼叫點 (vaddr 0x708):_xunpack(id=0x80, x=0x48, y=0x1f, 0, 0)
##   圖號(figure id)= 0x80;x=0x48、y=0x1f 為硬編座標。_xunpack 入口 = 0x281b4(symtab)。
   6f2:	42a7           	clr.l -(a7)          ; arg5 = 0
   6f4:	42a7           	clr.l -(a7)          ; arg4 = 0
   6f6:	2f3c 0000 001f 	move.l #$1f, -(a7)   ; arg3 = y = 0x1f
   6fc:	2f3c 0000 0048 	move.l #$48, -(a7)   ; arg2 = x = 0x48
   702:	2f3c 0000 0080 	move.l #$80, -(a7)   ; arg1 = figure id = 0x80
   708:	4eb9 0002 81b4 	jsr 0x281b4         ; _xunpack
   70e:	4fef 0014      	lea $14(a7), a7      ; 清 5 args

## open 常式 0x138a:以 figure id 索引「圖號→檔名表」(id<<3 + 0x3638e,旗標路徑;或一般路徑取字串表)
   139a:	202e 0008      	move.l $8(a6), d0    ; d0 = id
   139e:	e788           	lsl.l #3, d0         ; id << 3 (8-byte 記錄)
   13a0:	d0bc 0003 638e 	add.l #$3638e, d0    ; + table base 0x3638e
   13a6:	2040           	movea.l d0, a0
   13a8:	4a90           	tst.l (a0)
   ...                   ; 之後組出 "<name>:" 字串(寫 0x3a=':' 當磁碟分隔)

## 圖號→檔名表:vaddr 0x36388,8-byte 記錄 {flag:u16, ?:u16, str_ptr:u32}
##   字串表 @vaddr 0x36587:
##   "TITLE.PKH\0SUBTTL.PKH\0ICON.PIX\0PROG1..3.PKH\03D1..4.PKH\0END1..5.PKH\0"
##   前段另有存檔名 "C.1/M.1/F.1/A.1...\0TEMPROST.DAT\0MAP\0PIC.PIX\0MONS\0ITEM\0MON.PIX\0..."
##   => 此表只映射「圖號→檔名」,不含 w/h/palette。

## 第二 parse 路徑(near-duplicate of 0x27fa6,vaddr 0x2808c body):
##   同樣 pal@buf+0x0c(16 word)→0x69ba6、w@buf+0x34→0x69bca、h@buf+0x36→0x69bcc;
##   僅 out=0xda0000(vs 0x27fa6 的 0xd83000)。=> 兩條 parse 都從檔頭讀 w/h/pal,無第三條從圖號表灌入。
   280ea:	33c0 0006 9bca 	move.w d0, 0x69bca   ; w  <- buf+0x34
   28102:	33c0 0006 9bcc 	move.w d0, 0x69bcc   ; h  <- buf+0x36
   28108:	23fc 00da 0000 0006 9bce 	move.l #$da0000, 0x69bce  ; out = 0xda0000
