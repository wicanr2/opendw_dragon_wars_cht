#!/usr/bin/env python3
"""第 4 輪:全程式模擬 —— 用 unicorn 跑真實 DRAGON.X 程式碼把 TITLE.PKH 自己解出來。

流程(對應反組譯):
  _xunpack(id=0x80, x=0x48, y=0x1f, 0, 0)  @0x281b6
    -> 0x11c4 open+read 整檔到 buffer 0x69bd6   (我們用 HOOK 直接餵 TITLE.PKH bytes + 回傳 len)
    -> 0x27fa6 parse:讀 buf+0x34/0x36 = w/h、buf+0x0c 16-word pal、out=0xD83000、
                jsr 0x32c3c decode(out, buf+0x38, len-0x38)
    -> 0x2816a blit dispatch -> 0x32b4c blitter:GVRAM 0xC00000 + (y<<10+x)<<1,
                每列 w word(chunky),stride 1024 word/line,h 列。

本 harness 不 stub 任何解碼/blit 邏輯(那才是「全程式模擬」),只:
  (1) 把 0x11c4 整個 hook 成「把 PKH 寫進 0x69bd6、d0=len、直接 rts」(避開 DOS trap)。
  (2) 補 unicorn m68k 不支援的 addi.l/subi.l #imm,(a7)(parse 用)。
  (3) 跑完後 dump 0xD83000 解碼暫存 + 0xC00000 GVRAM。

輸出 dump 給後續寬度求解 / 渲染。
"""
import sys, struct
from unicorn import *
from unicorn.m68k_const import *

WDIR = '/w'
PKHNAME = sys.argv[1] if len(sys.argv) > 1 else 'TITLE.PKH'
DX = open(f'{WDIR}/DRAGON.X', 'rb').read()
PKH = open(f'{WDIR}/{PKHNAME}', 'rb').read()
BASE = 0x1440  # file_off = vaddr + 0x1440

mu = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
# 主記憶體:程式 + 全域 + stack + 暫存(0xD83000 解碼暫存落在這)
mu.mem_map(0, 0x1000000)            # 0..16MB 一塊涵蓋程式、globals、0xD83000、GVRAM 0xC00000、stack
mu.mem_write(0, DX[BASE:])          # text 段映到 vaddr 0
mu.mem_write(0x89bd4, b'\0\0\0\0')  # GVRAM scroll offset = 0

OPEN = 0x11c4
BUF = 0x69bd6

# --- hook 1:整段攔截 open_read(0x11c4),直接餵檔案 ---
# _xunpack 呼叫:0x11c4(id, BUF, 0)  -> 把 PKH 寫進 BUF、d0=len、rts
def hook_code(mu, addr, size, u):
    pc = addr
    if pc == OPEN:
        mu.mem_write(BUF, PKH)
        mu.reg_write(UC_M68K_REG_D0, len(PKH))
        # 模擬 rts:讀 (a7) 當返回位址、a7+=4
        a7 = mu.reg_read(UC_M68K_REG_A7)
        ret = struct.unpack('>I', mu.mem_read(a7, 4))[0]
        mu.reg_write(UC_M68K_REG_A7, a7 + 4)
        mu.reg_write(UC_M68K_REG_PC, ret)
        return
    # 補 addi.l / subi.l #imm,(a7)
    op = struct.unpack('>H', mu.mem_read(pc, 2))[0]
    if op in (0x0697, 0x0497):
        imm = struct.unpack('>I', mu.mem_read(pc + 2, 4))[0]
        a7 = mu.reg_read(UC_M68K_REG_A7)
        val = struct.unpack('>I', mu.mem_read(a7, 4))[0]
        val = (val + imm) & 0xFFFFFFFF if op == 0x0697 else (val - imm) & 0xFFFFFFFF
        mu.mem_write(a7, struct.pack('>I', val))
        mu.reg_write(UC_M68K_REG_PC, pc + 6)
        return
    # 補 unicorn m68k 不支援的 subq.b #1,Dn (0x53xx, mode000) 與 addq.b #imm,Dn
    # 0x5300=subq.b #8? 不:0101 ddd 1 ss mmmrrr;#1 編碼 data=1->001? 實際 0x5300:
    #   bits: 0101(5) 000(data=8) 1(sub) 00(byte) 000(Dn) 000(d0) -> subq.b #8,d0? 但碼是 subq #1
    # capstone 已判為 subq.b #1,d0;data field=000 表 #8,但這裡 count 用 0x445d2 byte,
    # 直接照「Dn 低 8 bit -1」語意處理(decode 迴圈就是 d0--)。
    # subq.#data,Dn : 0101 ddd 1 ss 000rrr ; data bits11-9 (0=>8), size bits7-6, Dn bits2-0
    is_subq = ((op & 0xF000) == 0x5000 and ((op >> 8) & 1) == 1
               and (op & 0x0038) == 0 and (op & 0x00C0) != 0x00C0)
    if is_subq:
        data = (op >> 9) & 7
        if data == 0: data = 8
        size = (op >> 6) & 3   # 0=byte 1=word 2=long
        reg = op & 7
        regid = [UC_M68K_REG_D0, UC_M68K_REG_D1, UC_M68K_REG_D2, UC_M68K_REG_D3,
                 UC_M68K_REG_D4, UC_M68K_REG_D5, UC_M68K_REG_D6, UC_M68K_REG_D7][reg]
        v = mu.reg_read(regid)
        if size == 0:
            nb = ((v & 0xFF) - data) & 0xFF
            nv = (v & 0xFFFFFF00) | nb
            res = nb
        elif size == 1:
            nw = ((v & 0xFFFF) - data) & 0xFFFF
            nv = (v & 0xFFFF0000) | nw
            res = nw
        else:
            nv = (v - data) & 0xFFFFFFFF
            res = nv
        mu.reg_write(regid, nv)
        # 設 CCR Z/N(decode 迴圈用 beq/bne 判斷)
        # 注意:unicorn m68k 寫 SR 會把 a7 重設為 0(stack ptr reload quirk)→ 先存後復原
        a7save = mu.reg_read(UC_M68K_REG_A7)
        sr = mu.reg_read(UC_M68K_REG_SR)
        sr &= ~0x0F
        if res == 0: sr |= 0x04          # Z
        msb = {0:0x80,1:0x8000,2:0x80000000}[size]
        if res & msb: sr |= 0x08         # N
        mu.reg_write(UC_M68K_REG_SR, sr)
        mu.reg_write(UC_M68K_REG_A7, a7save)
        mu.reg_write(UC_M68K_REG_PC, pc + 2)
        return

mu.hook_add(UC_HOOK_CODE, hook_code)

# trace blitter dst writes:hook MEM_WRITE 落在 GVRAM 的最小/最大位址,觀察 bank
gv_lo = [0xFFFFFFFF]; gv_hi = [0]
def hook_mem(mu, access, address, size, value, u):
    if 0xC00000 <= address < 0x1000000:
        if address < gv_lo[0]: gv_lo[0] = address
        if address > gv_hi[0]: gv_hi[0] = address
mu.hook_add(UC_HOOK_MEM_WRITE, hook_mem)

# --- 呼叫 _xunpack(id=0x80, x=0x48, y=0x1f, 0, 0) ---
# _xunpack body 用 link a6,#-8 取 $8(a6)=id, $c(a6)=x, $10(a6)=y? 看 281be: $10(a6)送 open(=id)
# 281e2: $c(a6) push 給 blit (=y) ; 281e6: $8(a6) push (=x)
# 即 stack 上自低到高:retaddr, arg1, arg2, arg3 -> $8=arg1, $c=arg2, $10=arg3
# 呼叫點 0x6f2..708 push 順序:clr,clr,#1f,#48,#80 然後 jsr -> 最後 push 的 #80 在最低位 = arg1=$8(a6)?
# 但 open 取 $10(a6)=id。故 stack 佈局:$8=x?,$c=y?,$10=id. 對照呼叫點:
#   push 0,0,y=1f,x=48,id=80 (id 最後 push=最低位址)=> $8(a6)=id=0x80? open 取 $10..
# 直接照呼叫點順序壓:[id,x,y,0,0] 反序壓堆疊讓 id 在最低。
def call_xunpack():
    sp = 0x8F000
    args = [0x80, 0x48, 0x1f, 0, 0]   # id, x, y, 0, 0(呼叫點壓入順序)
    for a in reversed(args):
        sp -= 4; mu.mem_write(sp, struct.pack('>I', a & 0xFFFFFFFF))
    sp -= 4; mu.mem_write(sp, struct.pack('>I', 0xFFFE))  # 假返回位址
    mu.reg_write(UC_M68K_REG_A7, sp)
    try:
        mu.emu_start(0x281b4, 0xFFFE, count=200_000_000)
        print("[ok] _xunpack returned cleanly")
    except UcError as e:
        print("[stop]", e, "PC=%#x" % mu.reg_read(UC_M68K_REG_PC))

call_xunpack()

# --- dump globals ---
def rw(a): return struct.unpack('>H', mu.mem_read(a, 2))[0]
def rl(a): return struct.unpack('>I', mu.mem_read(a, 4))[0]
w = rw(0x69bca); h = rw(0x69bcc)
de = rl(0x69bd2)
out0 = 0xD83000
pal = [rw(0x69ba6 + 2*i) for i in range(16)]
print(f"parse  -> w={w}({w:#x}) h={h}({h:#x})")
print(f"decode_end=0x{de:x}  decoded_words={(de-out0)//2 if de>out0 else 'na'}")
print(f"pal@0x69ba6: {[hex(p) for p in pal]}")
print(f"GVRAM writes: lo={gv_lo[0]:#x} hi={gv_hi[0]:#x}" if gv_hi[0] else "GVRAM writes: NONE")

# dump 解碼暫存(到 decode_end,若太離譜上限 256KB)
nwords = (de - out0)//2 if out0 < de < out0 + 0x40000 else 130000
dec = mu.mem_read(out0, nwords*2)
open(f'{WDIR}/dec_{PKHNAME}.bin', 'wb').write(dec)
# 轉成 1 byte/pixel(每 word 取低 byte = 4bpp index 0..15)
px = bytes(dec[2*i+1] & 0xFF for i in range(nwords))
open(f'{WDIR}/px_{PKHNAME}.bin', 'wb').write(px)
print(f"dumped {nwords} words -> dec_{PKHNAME}.bin, px_{PKHNAME}.bin")

# dump GVRAM 一塊(0xC00000 起 2048*512 bytes = 512 lines @1024 words)
gv = mu.mem_read(0xC00000, 1024*2*400)
open(f'{WDIR}/gvram_{PKHNAME}.bin', 'wb').write(gv)
print(f"dumped GVRAM 0xC00000 (1024w x 400 lines) -> gvram_{PKHNAME}.bin")
