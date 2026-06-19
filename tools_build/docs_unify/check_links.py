# -*- coding: utf-8 -*-
"""死鏈檢查:遍歷所有 .md 的相對連結 + 檢查殘留舊路徑。"""
import os, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKIP_DIRS = {".git", "build", "node_modules", "__pycache__", ".claude"}
LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")

def list_files(suffixes=None):
    for p in ROOT.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(ROOT)
        if any(part in SKIP_DIRS for part in rel.parts):
            continue
        if suffixes is None or p.suffix in suffixes:
            yield rel.as_posix()

def main():
    dead = []
    for rel in list_files({".md"}):
        text = (ROOT / rel).read_text(encoding="utf-8")
        base = (ROOT / rel).parent
        for m in LINK_RE.finditer(text):
            link = m.group(1)
            if re.match(r"^[a-z]+://", link) or link.startswith("#") or link.startswith("mailto:"):
                continue
            link = link.split("#", 1)[0]
            if link == "":
                continue
            tgt = os.path.normpath((base / link).as_posix())
            if not (ROOT / tgt).exists():
                dead.append((rel, link))
    print(f"=== DEAD markdown links: {len(dead)} ===")
    for f, l in dead[:80]:
        print(f"  {f}  ->  {l}")
    return len(dead)

if __name__ == "__main__":
    import sys
    sys.exit(0 if main() == 0 else 1)
