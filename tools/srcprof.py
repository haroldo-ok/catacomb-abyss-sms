#!/usr/bin/env python3
"""srcprof.py CG FUNC MODULE [N] -- per source line cycles of FUNC (needs prof2 --asm)"""
import sys, re, subprocess, glob, collections, bisect
cg, fn, mod = sys.argv[1:4]; N = int(sys.argv[4]) if len(sys.argv) > 4 else 25
out = subprocess.run(['python3', 'tools/prof2.py', cg, 'build/game.noi'] + glob.glob('build/*.rst') +
                     ['--top', '0', '--asm', fn], capture_output=True, text=True).stdout.split('\n')
rst = open('build/%s.rst' % mod).read().split('\n')
cur = None; a2s = {}; srcf = None
for l in rst:
    m = re.search(r';(src/\S+\.c):(\d+)', l)
    if m: cur = int(m.group(2)); srcf = m.group(1); continue
    m = re.match(r'\s+([0-9A-F]{6,8})\s', l)
    if m and cur: a2s[int(m.group(1), 16) & 0xFFFF] = cur
keys = sorted(a2s)
agg = collections.Counter()
for l in out:
    p = l.split()
    if len(p) < 2: continue
    try: a = int(p[0], 16) & 0xFFFF; c = int(p[1])
    except ValueError: continue
    i = bisect.bisect_right(keys, a) - 1
    if i >= 0: agg[a2s[keys[i]]] += c
src = open(srcf).read().split('\n')
for ln, c in agg.most_common(N): print('%9d %5d  %s' % (c, ln, src[ln - 1].strip()[:90]))
