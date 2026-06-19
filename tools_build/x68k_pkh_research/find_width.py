#!/usr/bin/env python3
"""對權威解碼後的 index 流做寬度搜尋:autocorrelation(相鄰列相符率)。"""
import sys
px = open(sys.argv[1], 'rb').read()  # .idx (1 byte/pixel, 0..15)
n = len(px)
print(f"pixels={n}")
best = []
for w in range(128, 1025):
    # 取中段 sample 避免 header/trailer 影響
    lines = n // w
    if lines < 40:
        continue
    s = (lines // 4) * w        # 從 1/4 處取
    cnt = 0; tot = 0
    for k in range(s, min(s + 60 * w, n - w)):
        if px[k] == px[k + w]:
            cnt += 1
        tot += 1
    if tot:
        best.append((cnt / tot, w))
best.sort(reverse=True)
print("top widths by vertical autocorrelation:")
for r, w in best[:25]:
    print(f"  w={w:4}  match={r:.4f}")
