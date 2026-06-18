#!/usr/bin/env python3
"""Dragon Wars 音訊資產萃取:把 Amiga / X68000 的真實 PCM 音效轉成 remake bundle 用的 WAV。

來源(素材不入庫,從 dragon_wars/ 解出後餵給本工具):
  Amiga  data5 / data6 — 8-bit signed mono PCM(各為一段連續音效;開頭有短靜音 lead-in)。
  X68000 DW.SND        — 8-bit signed mono PCM(2.00s;前段強擊 + 衰減尾)。

輸出(入庫:opendw_remake/assets/bundle/audio/*.wav):
  標準 WAV(mono / 16-bit signed PCM / 22050 Hz)。統一成 remake 合成輸出取樣率(22050),
  sound.cpp 直接以原生取樣率混音、不需 runtime resampler。

誠實標示(真值層級):
  - data5/data6/DW.SND 的「位元組 = 8-bit signed PCM」為觀測真值(波形能量/過零分析確認為真實音訊,非雜訊)。
  - 來源檔的「原生取樣率」原版未在反組譯中標出 → 採 remake 設計值(Amiga 11025 Hz、X68000 8000 Hz,
    皆為該平台音效常見回放率);轉檔時 resample 到 22050。此取樣率為 remake 設計,非 oracle 真值。
  - 「哪個檔對映哪個遊戲事件(門/撞牆/命中/施法)」原版無事件→樣本對照表(各檔皆單一連續音效),
    故對映由 remake 依音訊特性指定(見 sound.cpp / manifest),標 remake 設計,不謊稱原版真值。
"""
import os
import struct
import sys
import wave

OUT_RATE = 22050  # remake 合成輸出取樣率(對齊 sound.cpp kSampleRate)


def read_signed8(path):
    """讀 8-bit signed mono PCM → list[int] (-128..127)。"""
    d = open(path, "rb").read()
    return [b - 256 if b > 127 else b for b in d]


def trim_lead_silence(s, thresh=2, min_run=64):
    """去掉開頭的靜音 lead-in(|x|<=thresh 的連續段);太短不砍以免傷波形。"""
    i = 0
    n = len(s)
    while i < n and abs(s[i]) <= thresh:
        i += 1
    return s[i:] if i >= min_run else s


def resample_linear(s, src_rate, dst_rate):
    """線性內插重取樣(remake 設計;單聲道音效品質足夠)。"""
    if src_rate == dst_rate:
        return s
    n = len(s)
    out_n = int(n * dst_rate / src_rate)
    out = []
    for i in range(out_n):
        pos = i * src_rate / dst_rate
        i0 = int(pos)
        frac = pos - i0
        a = s[i0] if i0 < n else 0
        b = s[i0 + 1] if i0 + 1 < n else a
        out.append(a + (b - a) * frac)
    return out


def write_wav16(path, samples, rate):
    w = wave.open(path, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    buf = bytearray()
    for x in samples:
        v = int(round(x)) << 8  # 8-bit -> 16-bit
        v = 32767 if v > 32767 else (-32768 if v < -32768 else v)
        buf += struct.pack("<h", v)
    w.writeframes(bytes(buf))
    w.close()


# (來源檔, 輸出名, 原生取樣率[remake設計], 是否去 lead-in)
JOBS = [
    ("data5", "amiga_data5.wav", 11025, True),
    ("data6", "amiga_data6.wav", 11025, True),
    ("DW.SND", "x68k_dwsnd.wav", 8000, False),
]


def main():
    if len(sys.argv) < 3:
        print("用法: audio_extract.py <src_dir(含data5/data6/DW.SND)> <out_dir>")
        sys.exit(1)
    src_dir, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    for src, outn, rate, trim in JOBS:
        sp = os.path.join(src_dir, src)
        if not os.path.exists(sp):
            print(f"  SKIP {src}(找不到)")
            continue
        s = read_signed8(sp)
        raw_n = len(s)
        if trim:
            s = trim_lead_silence(s)
        s = resample_linear(s, rate, OUT_RATE)
        write_wav16(os.path.join(out_dir, outn), s, OUT_RATE)
        print(f"  {src:8s} {raw_n:6d}smp @{rate}Hz -> {outn} {len(s)}smp @{OUT_RATE}Hz "
              f"({len(s) / OUT_RATE:.2f}s)")


if __name__ == "__main__":
    main()
