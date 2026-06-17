#!/usr/bin/env python3
# Dragon Wars 腳本反組譯器(逐指令對齊 opendw src/tools/disasm.cpp + compress.c)。
# 用途:逆向 res3 戰鬥腳本控制流(本輪攻 res3@0x08b6 動作指派狀態機)。
# 線性反組譯(會在 data/string 區段 desync,但控制流區段準確;以 --start/--end 框定)。
import sys

# --- 5-bit 字串解碼(對照 compress.c extract_letter/bit_extract/alphabet)---
ALPHABET = [
    0xa0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xeb,0xec,
    0xed,0xee,0xef,0xf0,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf9,0xae,
    0xa2,0xa7,0xac,0xa1,0x8d,0xea,0xf1,0xf8,0xfa,0xb0,0xb1,0xb2,
    0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0x30,0x31,0x32,0x33,0x34,
    0x35,0x36,0x37,0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
    0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,0x52,0x53,
    0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0xa8,0xa9,0xaf,0xdc,0xa3,
    0xaa,0xbf,0xbc,0xbe,0xba,0xbb,0xad,0xa5,
]

class BE:
    def __init__(self, data, off):
        self.data=data; self.off=off; self.num_bits=0; self.bit_buffer=0; self.upper=0
    def bit_extract(self, n):
        al=0
        for _ in range(n):
            if self.num_bits==0:
                self.bit_buffer=self.data[self.off]; self.num_bits=8; self.off+=1
            tmp=self.bit_buffer
            self.bit_buffer=(self.bit_buffer<<1)&0xFF
            self.num_bits-=1
            carry=1 if tmp>self.bit_buffer else 0
            al=((al<<1)&0xFF)+carry
        return al
    def extract_letter(self):
        while True:
            ret=self.bit_extract(5)
            if ret==0: return 0,True
            if ret==0x1E:
                self.upper=(self.upper>>1)+0x80; continue
            if ret>0x1E:
                ret=self.bit_extract(6)+0x1E
            al=ALPHABET[ret-1]
            self.upper>>=1
            if self.upper>=0x40 and 0xE1<=al<=0xFA: al&=0xDF
            return al,False

def extract_string_len(data, off):
    # 回傳:解出的字串(可印),以及消耗到的 offset(對照 extract_string 回傳 bit_extractor_info.offset)
    be=BE(data,off)
    out=[]
    g8=0  # game_state.unknown[8](escape 狀態);disasm 模式不重要,用 0
    while True:
        ret,zero=be.extract_letter()
        if zero: break
        if (g8 & 0x80)==0:
            ret|=0x80; g8=ret; ret&=0x7F
        # 跳過 escape(0xAF/0xDC)複雜分支:本反組譯只需長度,簡化處理
        if ret in (0xAF,0xDC):
            # 走 escape:讀到對應 terminator
            term=ret
            while True:
                r2,z2=be.extract_letter()
                if z2: return ''.join(out), be.off
                if r2==term: break
            continue
        c=ret&0x7F
        out.append(chr(c) if 32<=c<127 else '·')
    return ''.join(out), be.off

# --- op 表:name + arg_count(對照 disasm.cpp op_codes[];None=未知/data byte)---
# 特殊長度由 handler 處理:0x00/01(.word/.byte 切 mode)、0x09/0x30/0x32/0x38/0x3C/0x3E(read_by_mode)、
# 0x14/0x1C/0x1A/0x11/0x9A(word_mode 依賴)、0x58(load_resource 3)、0x89(wait_event 變長)、
# 0x0F(1)、字串 op 0x77/0x78/0x7B(extract_string 變長)。
OPS = {
 0x00:('.word',0),0x01:('.byte',0),0x02:('op_02',0),0x03:('data_res=res(pop)',0),
 0x04:('push_byte(scriptidx)',0),0x05:('w3AE4=gs[',1),0x06:('set loop=',1),0x07:('w3AE4=0',0),
 0x08:('gs[',1),0x09:('w3AE2=',-1),0x0A:('w3AE2=gs[',1),0x0B:('w3AE2=gs[w3AE4+',1),
 0x0C:('op_0C',2),0x0D:('op_0D',2),0x0E:('op_0E',1),0x0F:('op_0F gs[',1),
 0x10:('op_10',2),0x11:('gs[',-2),0x12:('gs[',1),0x13:('gs[w3AE4+',1),
 0x14:('w3ADF[',-3),0x15:('op_15',2),0x16:('op_16',1),0x17:('store_data_res',1),
 0x18:('op_18',2),0x19:('op_19',2),0x1A:('gs[',-4),0x1B:('op_1B',0),
 0x1C:('w3ADF[',-5),0x1D:('memcpy0x700',0),0x1E:('op_1E',0),0x1F:('op_1F',0),
 0x20:('NOP',0),0x21:('w3AE4=w3AE2',0),0x22:('w3AE2=w3AE4',0),0x23:('inc gs[',1),
 0x24:('inc w3AE2',0),0x25:('inc[w3AE4]',0),0x26:('dec gs[',1),0x27:('dec w3AE2',0),
 0x28:('dec[mem]',0),0x29:('op_29',0),0x2A:('op_2A',0),0x2B:('shl[w3AE4]',0),
 0x2C:('op_2C',0),0x2D:('w3AE2>>=1',0),0x2E:('op_2E',0),0x2F:('op_2F',1),
 0x30:('op_30',-1),0x31:('op_31',1),0x32:('op_32',-1),0x33:('op_33',1),
 0x34:('op_34',1),0x35:('op_35',1),0x36:('op_36',1),0x37:('op_37',0),
 0x38:('w3AE2&=',-1),0x39:('op_39',1),0x3A:('op_3A',1),0x3B:('op_3B',1),
 0x3C:('op_3C',-1),0x3D:('cmp w3AE2,gs[',1),0x3E:('cmp w3AE2,',-1),0x3F:('check_gs',1),
 0x40:('cmp w3AE4,',1),0x41:('jnc',2),0x42:('jc',2),0x43:('jmp_XXX',2),
 0x44:('jz',2),0x45:('jnz',2),0x46:('js',2),0x47:('jns',2),
 0x48:('op_48',1),0x49:('loop',2),0x4A:('if',3),0x4B:('stc',0),
 0x4C:('clc',0),0x4D:('op_4D rng',0),0x4E:('op_4E',0),0x4F:('clrbit gs',1),
 0x50:('op_50',0),0x51:('op_51',2),0x52:('jmp',2),0x53:('call',2),
 0x54:('ret',0),0x55:('peek&pop',0),0x56:('push',0),0x57:('op_57_res',3),
 0x58:('load_resource',-6),0x59:('retf',0),0x5A:('ret/setbyte',0),0x5B:('op_5B',0),
 0x5C:('for_call',2),0x5D:('w3AE2=get_char_data',1),0x5E:('set_char_prop',1),0x5F:('op_5F',0),
 0x60:('op_60',0),0x61:('test_player_prop',1),0x62:('op_62',0),0x63:('op_63',2),
 0x64:('op_64',0),0x65:('op_65',0),0x66:('test gs[',1),0x67:('op_67',0),
 0x68:('op_68',1),0x69:('op_69',1),0x6A:('op_6A',4),0x6B:('op_6B',0),
 0x6C:('op_6C',0),0x6D:('op_6D',0),0x6F:('op_6F',1),0x70:('op_70',0),
 0x71:('load_world',0),0x73:('op_73',0),0x74:('draw_rect',4),0x75:('draw_ui_full',0),
 0x76:('draw_pattern',0),0x77:('draw_and_set',-7),0x78:('set_msg',-7),0x7A:('extract_string',0),
 0x7B:('ui_header',-7),0x7C:('ui_header_renc',0),0x7D:('write_char_name',0),0x7E:('op_7E',1),
 0x80:('advance_cursor',1),0x81:('op_81 w3AE2',0),0x82:('op_82',1),0x83:('write_number',0),
 0x84:('malloc',0),0x85:('res_release',0),0x86:('w3AE2=loadres',0),0x87:('write_res',0),
 0x88:('wait_escape',0),0x89:('wait_event',-8),0x8A:('random_encounter',0),0x8B:('refresh_vp',0),
 0x8C:("prompt Y/N",0),0x8D:('read_string',0),0x8F:('op_8F',0),0x90:('sound',1),
 0x91:('op_91',0),0x92:('op_92',0),0x93:('push(w3AE4&0xFF)',0),0x94:('pop byte',0),
 0x95:('ui_draw_string',0),0x96:('draw_padded_str',0),0x97:('load_char_data',1),0x98:('op_98',1),
 0x99:('test w3AE2',0),0x9A:('gs[',-9),0x9B:('op_9B',2),0x9C:('op_9C',0),
 0x9D:('op_9D',2),0x9E:('op_9E',0),0x9F:('op_9F',0),
}

def disasm(data, start, end):
    word_mode=False
    i=start
    while i<end and i<len(data):
        op=data[i]
        ent=OPS.get(op)
        addr=i
        i+=1
        if ent is None:
            print(f"0x{addr:04X}: .db 0x{op:02X}")
            continue
        name,ac=ent
        line=f"0x{addr:04X}: {name}"
        if op==0x00: word_mode=True; print(line+" (WORD MODE)"); continue
        if op==0x01 or op==0x5A:
            if op==0x5A: print(line);
            else: print(line+" (BYTE MODE)")
            if op==0x01: word_mode=False
            elif op==0x5A: word_mode=False
            continue
        if ac>=0:
            args=[]
            for _ in range(ac):
                args.append(data[i]); i+=1
            if op in (0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x49,0x51,0x52,0x53,0x63,0x5C,0x0C,0x0D,0x15) and ac==2:
                w=args[0]|(args[1]<<8); line+=f" 0x{w:04X}"
            else:
                line+=" "+",".join(f"0x{a:02X}" for a in args)
            print(line)
            continue
        # 特殊長度
        if ac==-1:  # read_by_mode
            if word_mode:
                w=data[i]|(data[i+1]<<8); i+=2; line+=f" 0x{w:04X}"
            else:
                line+=f" 0x{data[i]:02X}"; i+=1
            print(line); continue
        if ac==-2:  # op_11 gs[idx]=0 (word_mode → 同設 idx+1)
            line+=f"0x{data[i]:02X}]=0"; i+=1; print(line); continue
        if ac==-9:  # op_9A gs[idx]=0xFF
            line+=f"0x{data[i]:02X}]=0xFF"; i+=1; print(line); continue
        if ac==-3:  # op_14 w3ADF[word]=w3AE2
            w=data[i]|(data[i+1]<<8); i+=2; line+=f"0x{w:04X}]=w3AE2"; print(line); continue
        if ac==-4:  # op_1A gs[idx]=imm (word_mode → +1 more)
            idx=data[i]; v=data[i+1]; i+=2; line+=f"0x{idx:02X}]=0x{v:02X}"
            if word_mode: line+=f"; gs[0x{idx+1:02X}]=0x{data[i]:02X}"; i+=1
            print(line); continue
        if ac==-5:  # op_1C w3ADF[word]=imm
            w=data[i]|(data[i+1]<<8); v=data[i+2]; i+=3; line+=f"0x{w:04X}]=0x{v:02X}"
            if word_mode: line+=f"; w3ADF[0x{w+1:04X}]=0x{data[i]:02X}"; i+=1
            print(line); continue
        if ac==-6:  # load_resource res,off
            res=data[i]; off=data[i+1]|(data[i+2]<<8); i+=3
            line+=f" res:0x{res:02X} off:0x{off:04X}"; print(line); continue
        if ac==-7:  # string op:extract_string
            s,newoff=extract_string_len(data,i)
            line+=f' "{s}"'
            i=newoff
            print(line); continue
        if ac==-8:  # wait_event:2-byte flags + key table(3 byte/entry,0xFF 結尾)
            flags=data[i]|(data[i+1]<<8); i+=2
            line+=f" flags=0x{flags:04X}"; print(line)
            while i<len(data):
                k=data[i]
                if k==0xFF:
                    print(f"          [0xFF end]"); i+=1; break
                if k==0x81:
                    print(f"          [0x81 skip]"); i+=1; continue
                tgt=data[i+1]|(data[i+2]<<8); i+=3
                kd={0:'catch-all',1:'#[1-7]'}.get(k, f"'{chr(k&0x7f)}'(0x{k:02X})")
                print(f"          [{kd}] -> 0x{tgt:04X}")
            continue
        print(line)

if __name__=='__main__':
    fn=sys.argv[1]
    start=int(sys.argv[2],0) if len(sys.argv)>2 else 0
    end=int(sys.argv[3],0) if len(sys.argv)>3 else 0x10000
    data=open(fn,'rb').read()
    print(f"; {fn} ({len(data)} bytes) range 0x{start:04X}..0x{end:04X}")
    disasm(data, start, end)
