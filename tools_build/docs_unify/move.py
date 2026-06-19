# -*- coding: utf-8 -*-
"""第一階段:用 git mv 把檔案/目錄搬到統一後的位置(保留歷史)。"""
import subprocess, sys, os
from pathlib import Path
sys.path.insert(0, os.path.dirname(__file__))
from manifest import DIR_MOVES, FILE_MOVES

ROOT = Path(__file__).resolve().parents[2]

def git_mv(src, dst):
    s, d = ROOT / src, ROOT / dst
    if not s.exists():
        print(f"  SKIP (missing): {src}")
        return
    d.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "mv", str(src), str(dst)], cwd=ROOT, check=True)
    print(f"  mv {src} -> {dst}")

def main():
    # 目錄級:先搬「會被 FILE_MOVES 寫入同名根目錄」的(如 docs/assessment),
    # 但 git mv 目錄到自身子目錄會衝突,需特判:docs/assessment -> docs/assessment/pm_scale_screens。
    print("== DIR moves ==")
    for src, dst in DIR_MOVES.items():
        if (ROOT / dst).resolve().is_relative_to((ROOT / src).resolve()):
            # 搬到自身子目錄:先搬到暫存再搬回
            tmp = src + "__tmp_unify"
            subprocess.run(["git", "mv", src, tmp], cwd=ROOT, check=True)
            (ROOT / dst).parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["git", "mv", tmp, dst], cwd=ROOT, check=True)
            print(f"  mv(self-nest) {src} -> {dst}")
        else:
            git_mv(src, dst)
    print("== FILE moves ==")
    for src, dst in FILE_MOVES.items():
        git_mv(src, dst)

if __name__ == "__main__":
    main()
