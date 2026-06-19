#!/usr/bin/env python3
"""權威解碼:逐指令對照 DRAGON.X 0x32c3c / 0x32ccc / 0x32cfe 的 Python 轉寫。

每個 op 都對應反組譯(見 key_routines.asm / 本輪 disasm):
  主迴圈 0x32c3c:
    ctrl = *in++; count = ctrl & 0x7F (-> 0x445d2)
    *in++ 之前先 move.b (a1),d0; move.b (a1)+,d1 -> ctrl 同一 byte 讀兩次,d1 留 bit7 判 run/lit
    btst #7,d1: !=0 run(0x32ccc) else literal(0x32cfe)
  run 0x32ccc:  count=0x445d2; b=*in++; repeat count: *out++ = (word) b   (movew d1,(a0)+, d1=byte)
  literal 0x32cfe: count=0x445d2;
    loop: b=*in++; d2=b>>4; *out++=word(d2); count--; if count==0 break;
          *out++=word(b);  count--; if count!=0 continue   (movew d1,(a0)+ = 整 byte d1)

注意:run/literal 寫的是「整個 byte 當 word」(movew d1,(a0)+),
顯示成 4bpp index 時取低 nibble(&0x0F);hi-nibble word(d2=b>>4)本身就 <16。
本腳本同時輸出「word 值」與「&0x0F index」兩種,供比對。
"""
import sys, struct, collections

def decode(data, start):
    """逐指令對齊 DRAGON.X 的解碼器(byte 計數器 + do-while + underflow)。

    關鍵更正(本輪 emu 驗證):count = ctrl & 0x7F 是 **byte 計數器**,
    run/literal 都用 `subq.b #1` 配 do-while,所以 **count==0 = 256 次**(byte 0→0xFF→…→0),
    不是「跳過」。run(0x32ccc)讀 1 byte 後寫 count 次;literal(0x32cfe)交替寫
    hi-nibble word 與整 byte word,每寫一個就 subq.b,為 0 即停。
    """
    out = []
    i = start
    n = len(data)
    while i < n:
        ctrl = data[i]; i += 1
        count = ctrl & 0x7F
        run = (ctrl & 0x80) != 0
        if run:
            # run 0x32ccc:b=*in++;do{*out++=word(b)}while(--count_byte)
            if i >= n: break
            b = data[i]; i += 1
            reps = count if count != 0 else 256
            out.extend([b] * reps)
        else:
            # literal 0x32cfe:do-while,d0 為 byte 計數器
            d0 = count            # byte counter
            while True:
                if i >= n: break
                b = data[i]; i += 1
                out.append(b >> 4)            # hi nibble word
                d0 = (d0 - 1) & 0xFF
                if d0 == 0:
                    break
                out.append(b)                 # 整 byte word(顯示取低 nibble)
                d0 = (d0 - 1) & 0xFF
                if d0 == 0:
                    break
    return out


if __name__ == '__main__':
    fn = sys.argv[1]
    start = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x38
    data = open(fn, 'rb').read()
    words = decode(data, start)
    print(f"{fn}: start=0x{start:x} in_len={len(data)} decoded_words={len(words)}")
    idx = [w & 0x0F for w in words]
    hist = collections.Counter(idx)
    print("index histogram (&0x0F):", dict(sorted(hist.items())))
    # word 值分布(看是否真有 >15 的整 byte 寫入)
    big = sum(1 for w in words if w > 15)
    print(f"words with value>15 (整 byte run/lit-lo): {big} / {len(words)} ({100*big/len(words):.1f}%)")
    # 存兩版:idx(&0x0F)與 raw word low byte
    open(fn + '.idx', 'wb').write(bytes(idx))
    open(fn + '.wlo', 'wb').write(bytes(w & 0xFF for w in words))
    # 因數分解找可能寬度
    n = len(words)
    print("divisors near 200-700:", [d for d in range(200, 700) if n % d == 0][:20])
