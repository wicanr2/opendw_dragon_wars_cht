# -*- coding: utf-8 -*-
"""第二階段:重寫全 repo inbound 連結到統一後的新路徑,並做死鏈檢查。

兩類引用:
 (A) markdown / doc 內的相對連結與圖片:依「含 link 檔案的舊位置」解析絕對目標,
     查映射得新目標,依「檔案新位置」重算相對路徑。
 (B) 程式碼 / CMake / skill / README 內裸寫的 repo-root 風格 `docs/...` 字串:
     直接以 repo-root 映射字串替換(含「裸編號」如 docs/44 → docs/<dir>/44...)。
"""
import os, re, sys, subprocess
from pathlib import Path
sys.path.insert(0, os.path.dirname(__file__))
from manifest import DIR_MOVES, FILE_MOVES

ROOT = Path(__file__).resolve().parents[2]

# ---- 建立 repo-root 舊路徑 -> 新路徑 的完整查找(展開目錄子樹) ----
# 先收集所有「目前在 repo 內」的檔案,反推它們的舊路徑(用 DIR/FILE moves)。
# 但更直接:用 manifest 直接生成 old->new。對 DIR_MOVES,逐一展開新樹下實體檔,
# 反算其舊路徑。
def build_path_map():
    pmap = {}  # old_repo_rel(str, posix) -> new_repo_rel(str, posix)
    # 單檔
    for o, n in FILE_MOVES.items():
        pmap[o] = n
    # 目錄:列新目錄下所有檔,反推舊路徑
    for o, n in DIR_MOVES.items():
        npath = ROOT / n
        if not npath.exists():
            continue
        for f in npath.rglob("*"):
            # 檔案與「中間子目錄」都登記,讓連到子目錄的 link 也能映射
            rel_in = f.relative_to(npath).as_posix()
            pmap[f"{o}/{rel_in}"] = f"{n}/{rel_in}"
        # 目錄本身(供「連到目錄」的 link)
        pmap[o] = n
    return pmap

PMAP = build_path_map()

# 「裸編號 / 舊基名」→ 新 repo-root 路徑(供程式碼註解 docs/44 之類)。
# key 可為 "docs/44"(裸號)、"docs/44_DATA_FORMATS_AND_MECHANICS.md"、
# "docs/gameplay/57_DOORS_TRAPS_TERRAIN.md" 等。
DIR_KEYS = set(DIR_MOVES.keys())  # 裸目錄 key,不可當字串前綴貪婪替換

def build_codecomment_map():
    m = {}
    # 完整 .md / 具體檔名映射;排除「裸目錄」key(如 docs/assessment),
    # 否則會把 (A) 已正確產生的 docs/assessment/60_... 再貪婪改成
    # docs/assessment/pm_scale_screens/60_...。
    for o, n in PMAP.items():
        if o in DIR_KEYS:
            continue
        m[o] = n
    # 裸編號 docs/NN -> 新目錄/NN_全名(取該編號的 .md 檔)。
    # 從 PMAP 找 old 形如 docs/NN_*.md 或 opendw_remake/docs/.../NN_*.md
    bynum = {}
    for o, n in PMAP.items():
        mobj = re.search(r"/(\d{2})_[^/]+\.md$", "/" + o)
        if mobj:
            num = mobj.group(1)
            bynum.setdefault(num, n)
    # src 註解裸寫 docs/NN 一律指「新路徑(去 .md)」的目錄+號;但作者寫 docs/44 意指該檔,
    # 我們替換成 新目錄/NN(不帶後綴會壞),故替換為 新完整路徑去掉 .md 的前綴形式 → 直接給目錄/NN_全名 不可行。
    # 實務:程式碼註解多寫 "docs/44"、"docs/44 §2"、"docs/42 §14"。替換成 "docs/reverse-engineering/44"。
    for num, n in bynum.items():
        new_dir = n.rsplit("/", 1)[0]  # docs/reverse-engineering
        m[f"docs/{num}"] = f"{new_dir}/{num}"
    return m, bynum

CCMAP, BYNUM = build_codecomment_map()


def relpath(from_file_repo_rel, target_repo_rel):
    """from_file 的『新位置』(repo-rel) 與 target 的『新位置』(repo-rel),回傳相對連結。"""
    from_dir = (ROOT / from_file_repo_rel).parent
    tgt = ROOT / target_repo_rel
    return os.path.relpath(tgt, from_dir).replace(os.sep, "/")


# ---- (A) markdown / doc 相對連結重寫 ----
LINK_RE = re.compile(r"(!?\[[^\]]*\]\()([^)\s]+)(\s*(?:\"[^\"]*\")?\))")
# 也處理行內 `code` 反引號內的相對路徑? 那屬字串引用,交給 (B) 的字串替換較安全。

def new_location_of(old_repo_rel):
    """檔案搬移後的新 repo-rel(若沒搬就原樣)。"""
    return PMAP.get(old_repo_rel, old_repo_rel)

def resolve_old_target(old_file_repo_rel, link):
    """把 md 內相對 link 解析成『舊 repo-rel 絕對路徑』(posix),失敗回 None。"""
    if re.match(r"^[a-z]+://", link) or link.startswith("#") or link.startswith("mailto:"):
        return None, None
    frag = ""
    if "#" in link:
        link, frag = link.split("#", 1)
        frag = "#" + frag
    if link == "":
        return None, None
    old_dir = Path(old_file_repo_rel).parent
    try:
        old_target = os.path.normpath((old_dir / link).as_posix()).replace(os.sep, "/")
    except Exception:
        return None, None
    return old_target, frag

def rewrite_md(old_file_repo_rel):
    new_file_repo_rel = new_location_of(old_file_repo_rel)
    path = ROOT / new_file_repo_rel
    text = path.read_text(encoding="utf-8")
    changed = [0]
    def repl(m):
        pre, link, post = m.group(1), m.group(2), m.group(3)
        old_target, frag = resolve_old_target(old_file_repo_rel, link)
        if old_target is None:
            return m.group(0)
        # 目標的新位置(未搬移則沿用舊 repo-rel)
        new_target = PMAP.get(old_target, old_target)
        # 只有當目標『新位置真實存在』才重寫;否則保持原樣
        # (保護非路徑的 [文字](X) 與原本就壞的連結,不無端加 ../)。
        if not (ROOT / new_target).exists():
            return m.group(0)
        # 重算相對路徑(基於本檔新位置)
        newlink = relpath(new_file_repo_rel, new_target) + frag
        if newlink != link:
            changed[0] += 1
        return pre + newlink + post
    new_text = LINK_RE.sub(repl, text)
    if changed[0]:
        path.write_text(new_text, encoding="utf-8")
    return changed[0]


# ---- (B) 程式碼/CMake/skill/README 裸字串 docs/... 替換 ----
def rewrite_code_strings(file_repo_rel):
    path = ROOT / file_repo_rel
    if not path.exists():
        return 0
    text = path.read_text(encoding="utf-8")
    orig = text
    # 較長 key 先換,避免 docs/44 先吃掉 docs/44_DATA...md
    keys = sorted(CCMAP.keys(), key=len, reverse=True)
    for k in keys:
        v = CCMAP[k]
        if k == v:
            continue
        # 邊界:k 後面不可緊接英數底線(避免 docs/4 換到 docs/44);用負向前瞻。
        # k 已含 docs/ 前綴。
        pat = re.compile(re.escape(k) + r"(?![0-9A-Za-z_])")
        text = pat.sub(v, text)
    if text != orig:
        path.write_text(text, encoding="utf-8")
        return 1
    return 0


SKIP_DIRS = {".git", "build", "node_modules", "__pycache__", ".claude"}

def list_repo_files(suffixes=None):
    out = []
    for p in ROOT.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(ROOT)
        if any(part in SKIP_DIRS for part in rel.parts):
            continue
        if suffixes is None or p.suffix in suffixes:
            out.append(rel.as_posix())
    return out

def grep_docs_refs():
    out = []
    pat = re.compile(r"docs/[0-9A-Za-z_]")
    for rel in list_repo_files():
        p = ROOT / rel
        try:
            t = p.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        if pat.search(t):
            out.append(rel)
    return out

def main():
    md_files = list_repo_files({".md"})
    # md 檔的「舊 repo-rel」= 反查 PMAP(若是搬移後新位置,要找對應舊位置來解析其內 link)。
    # 但 md 內 link 是以「舊位置」寫的,需用舊位置解析。建立 new->old 反查。
    new2old = {n: o for o, n in PMAP.items()}
    total_md = 0
    for nf in md_files:
        old = new2old.get(nf, nf)  # 若該 md 沒搬,old=new
        total_md += rewrite_md(old)
    print(f"[A] markdown links rewritten: {total_md}")

    # (B) 非 md 檔
    code_files = [
        "README.md",  # README 也含 opendw_remake/docs/... 字串型 → 已由 (A) 處理過,
    ]
    # README/CONTEXT/SKILL/skills 內既有 markdown link 由 (A) 處理;
    # 但裸 inline `code` 字串(反引號內)由 (B) 補刀。對所有「含 docs/ 的非純資產檔」跑 (B)。
    targets = grep_docs_refs()
    total_code = 0
    for f in targets:
        if f.startswith("tools_build/docs_unify/"):
            continue  # 不改自己
        if f.endswith(".md"):
            # md 的反引號內字串(非 link)補做 (B);但要小心別動到已正確的 (A) link。
            # (B) 只替換明確 key,且 key 後界限保護,風險低。
            total_code += rewrite_code_strings(f)
        else:
            total_code += rewrite_code_strings(f)
    print(f"[B] code/string files touched: {total_code}")

if __name__ == "__main__":
    main()
