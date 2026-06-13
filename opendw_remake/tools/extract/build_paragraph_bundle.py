#!/usr/bin/env python3
"""把 docs/34_READ_PARAGRAPHS.md 解析成自包含的段落書 bundle 資產。

輸出(預設 locale = zh-TW):
  assets/bundle/paragraphs/<locale>/paragraphs.tsv

格式:每行 `N<TAB>繁中全文`,段落內換行以字面 `\n` 轉義(讀取端還原),
忠實保留段落內 `〔?〕` 等不確定標記。147 段(1–147)。

「Read paragraph N」防拷流程觸發時,引擎以 N 查此表顯示段落原文。
locale 可擴(日後 ja / en 各放一個子目錄即可,引擎走 --locale 選用)。

用法:
  python3 build_paragraph_bundle.py <34_READ_PARAGRAPHS.md> <out_bundle_dir> [locale]
"""
import re
import sys
import os


def parse(md_path):
    """回傳 {N: 段落正文(含換行)}。略過「> 來源:…」註記與分隔線。"""
    paras = {}
    cur = None
    buf = []
    for line in open(md_path, encoding="utf-8"):
        m = re.match(r"^##\s*段落\s*(\d+)\s*$", line.strip())
        if m:
            if cur is not None:
                paras[cur] = clean("\n".join(buf))
            cur = int(m.group(1))
            buf = []
            continue
        if cur is not None:
            ls = line.lstrip()
            # 略過轉寫註記:來源行、分隔線、以及「> (…)」這類編輯/校對說明
            # (例:「> (前段續至此頁頂端:)」「> (需人工複核)」)。
            # 注意:其餘「> …」是段落正文的續寫內容,保留(clean() 再去掉前置 >)。
            if ls.startswith("> 來源") or line.strip() == "---":
                continue
            if ls.startswith("> (") or ls.startswith("> （"):
                continue
            buf.append(line.rstrip("\n"))
    if cur is not None:
        paras[cur] = clean("\n".join(buf))
    return paras


def clean(t):
    # 去掉前置引用符號 > (保留內容)、壓掉多餘空行、首尾去空白。
    lines = [re.sub(r"^>\s?", "", ln) for ln in t.split("\n")]
    t = "\n".join(lines).strip()
    t = re.sub(r"\n{3,}", "\n\n", t)
    return t


def main():
    md = sys.argv[1] if len(sys.argv) > 1 else "docs/34_READ_PARAGRAPHS.md"
    out_bundle = sys.argv[2] if len(sys.argv) > 2 else "opendw_remake/assets/bundle/paragraphs"
    locale = sys.argv[3] if len(sys.argv) > 3 else "zh-TW"

    paras = parse(md)
    nums = sorted(paras)
    out_dir = os.path.join(out_bundle, locale)
    os.makedirs(out_dir, exist_ok=True)
    tsv_path = os.path.join(out_dir, "paragraphs.tsv")
    with open(tsv_path, "w", encoding="utf-8") as f:
        for n in nums:
            # 段落內換行轉義成字面 \n;TSV 一段一行,引擎讀取端還原。
            body = paras[n].replace("\\", "\\\\").replace("\t", " ").replace("\n", "\\n")
            f.write(f"{n}\t{body}\n")

    miss = [i for i in range(1, nums[-1] + 1) if i not in paras]
    print(f"locale={locale}  段落數={len(nums)}  範圍={nums[0]}–{nums[-1]}")
    print(f"缺號: {miss if miss else '無'}")
    print(f"輸出: {tsv_path}")


if __name__ == "__main__":
    main()
