#!/usr/bin/env python3
"""per C line cycle totals: cline.py asm_dump.txt build/x.rst src/x.c renders"""
import re, sys
dump, rst, srcf, n = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
cyc = {}
for l in open(dump):
    m = re.match(r'^([0-9A-F]{6})\s+(\d+)\s', l)
    if m: cyc[int(m.group(1), 16)] = int(m.group(2))
cur = None; agg = {}
for l in open(rst):
    m = re.search(r';' + re.escape(srcf) + r':(\d+):', l)
    if m: cur = int(m.group(1)); continue
    m = re.match(r'^\s+([0-9A-F]{6})\s', l)
    if m and cur:
        a = int(m.group(1), 16)
        if a in cyc: agg[cur] = agg.get(cur, 0) + cyc.pop(a)
src = open(srcf).read().split('\n')
for ln, c in sorted(agg.items(), key=lambda x: -x[1])[:int(sys.argv[5]) if len(sys.argv) > 5 else 40]:
    print(f"{c // n:7d} {ln:4d} {src[ln - 1].strip()[:90]}")
print('total/render', sum(agg.values()) // n)
