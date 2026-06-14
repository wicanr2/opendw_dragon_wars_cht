#!/usr/bin/env bash
# 重新產生 assets/fonts/cjk24.atlas:蒐集遊戲所有 CJK 文字來源的字元,
# Docker 內用 wqy-zenhei 產生 24×24 點陣字形。確保新增日文漢字/かな有字形
# (缺字形 → 渲染為空白)。
#
# 字元來源(union):
#   - assets/i18n/<locale>/*.tsv         (選單 / 事件 / 角色 / 戰鬥;譯文欄)
#   - assets/bundle/paragraphs/<loc>/*.tsv (Read paragraph 段落書全文)
#   - assets/bundle/strings/*.tsv        (VM 對話字串)
#   - 既有 atlas 內已有的字形(保留,避免漏掉其他來源如 manifest)
#
# Docker first:不污染系統 Python;一次性容器裝 pillow + fonts-wqy-zenhei。
# 用法: tools_build/gen_cjk_atlas_from_i18n.sh [repo_root]
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
REPO="$ROOT/opendw_remake"
ATLAS="$REPO/assets/fonts/cjk24.atlas"
GEN="$(cd "$(dirname "$0")" && pwd)/gen_cjk_atlas.py"

[ -d "$REPO/assets" ] || { echo "找不到 assets: $REPO/assets" >&2; exit 1; }

# 蒐集全部 CJK 字元(去重保序)。含既有 atlas 字形 codepoint,確保不退化。
CHARS=$(python3 - "$REPO" "$ATLAS" <<'PY'
import sys, os, glob, struct
repo, atlas = sys.argv[1], sys.argv[2]
seen=[]; s=set()
def add(ch):
    if (not ch.isascii()) and (not ch.isspace()) and ch not in s:
        s.add(ch); seen.append(ch)

# 既有 atlas 的字形先保留(其 codepoint 一定要保住)。
if os.path.exists(atlas):
    d=open(atlas,'rb').read()
    if d[:4]==b'CJK1':
        cnt=struct.unpack('<I',d[4:8])[0]; off=8
        for _ in range(cnt):
            cp=struct.unpack('<I',d[off:off+4])[0]; off+=4+72
            add(chr(cp))

# i18n 譯文欄(第 2 欄起)。
for tsv in sorted(glob.glob(os.path.join(repo,'assets','i18n','*','*.tsv'))):
    for line in open(tsv,encoding='utf-8',errors='ignore'):
        if line.startswith('#'): continue
        parts=line.rstrip('\n').split('\t')
        if len(parts)>=2:
            for ch in '\t'.join(parts[1:]): add(ch)

# bundle 段落書 + VM 字串(整行取非 ASCII)。
for pat in ('assets/bundle/paragraphs/*/*.tsv','assets/bundle/strings/*.tsv'):
    for tsv in sorted(glob.glob(os.path.join(repo,pat))):
        for line in open(tsv,encoding='utf-8',errors='ignore'):
            if line.startswith('#'): continue
            for ch in line: add(ch)

print(''.join(seen))
PY
)

NCHARS=$(python3 -c "import sys;print(len(sys.argv[1]))" "$CHARS")
echo "蒐集到 $NCHARS 個非 ASCII 字元,Docker 內產生 atlas..."

docker run --rm \
  -v "$REPO":/repo \
  -v "$GEN":/gen.py:ro \
  -e CHARS="$CHARS" \
  python:3.12-slim bash -c '
    set -e
    pip install --quiet pillow >/dev/null 2>&1
    apt-get update -qq >/dev/null 2>&1
    DEBIAN_FRONTEND=noninteractive apt-get install -y -qq fonts-wqy-zenhei >/dev/null 2>&1
    ls /usr/share/fonts/truetype/wqy/wqy-zenhei.ttc >/dev/null
    python3 /gen.py /repo/assets/fonts/cjk24.atlas "$CHARS"
  '

echo "atlas 已更新: $ATLAS"
