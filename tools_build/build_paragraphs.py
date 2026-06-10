#!/usr/bin/env python3
"""把 34_READ_PARAGRAPHS.md 編譯成段落資料庫 paragraphs_zh.txt + paragraphs.idx。
用法: python3 build_paragraphs.py <34_READ_PARAGRAPHS.md> <out_dir>
"""
import re, sys, struct, os

def parse(md_path):
    paras = {}
    cur = None
    buf = []
    for line in open(md_path, encoding='utf-8'):
        m = re.match(r'^##\s*段落\s*(\d+)\s*$', line.strip())
        if m:
            if cur is not None:
                paras[cur] = clean('\n'.join(buf))
            cur = int(m.group(1)); buf = []
            continue
        if cur is not None:
            # 略過「> 來源:…」與「> (前段續…)」這類註記行,但保留段落正文
            if line.lstrip().startswith('> 來源') or line.strip()=='---':
                continue
            buf.append(line.rstrip('\n'))
    if cur is not None:
        paras[cur] = clean('\n'.join(buf))
    return paras

def clean(t):
    # 去掉前置引用符號 > (保留內容)、壓掉多餘空行
    lines = [re.sub(r'^>\s?','',ln) for ln in t.split('\n')]
    t = '\n'.join(lines).strip()
    t = re.sub(r'\n{3,}', '\n\n', t)
    return t

def build(paras, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    txt_path = os.path.join(out_dir, 'paragraphs_zh.txt')
    idx_path = os.path.join(out_dir, 'paragraphs.idx')
    nums = sorted(paras)
    blob = b''
    index = []  # (num, offset, length)
    for n in nums:
        data = paras[n].encode('utf-8')
        index.append((n, len(blob), len(data)))
        blob += data
    # 純文字檔(人讀/校對用)
    with open(txt_path, 'w', encoding='utf-8') as f:
        for n in nums:
            f.write(f'@{n}\n{paras[n]}\n\n')
    # 二進位索引: max_num(uint16) + 每段 (num u16, off u32, len u32)
    with open(idx_path, 'wb') as f:
        f.write(struct.pack('<H', nums[-1]))
        f.write(struct.pack('<H', len(nums)))
        for n,off,ln in index:
            f.write(struct.pack('<HII', n, off, ln))
    # blob 也獨立存一份(引擎讀這個 + idx)
    with open(os.path.join(out_dir,'paragraphs_zh.blob'),'wb') as f:
        f.write(blob)
    return nums, index

if __name__ == '__main__':
    md = sys.argv[1] if len(sys.argv)>1 else 'docs/34_READ_PARAGRAPHS.md'
    out = sys.argv[2] if len(sys.argv)>2 else 'data/paragraphs'
    paras = parse(md)
    nums, index = build(paras, out)
    miss = [i for i in range(1, nums[-1]+1) if i not in paras]
    print(f"段落數: {len(nums)}  範圍: {nums[0]}–{nums[-1]}")
    print(f"缺號: {miss if miss else '無'}")
    print(f"輸出: {out}/paragraphs_zh.txt, paragraphs.idx, paragraphs_zh.blob")
