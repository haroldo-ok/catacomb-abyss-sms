#!/usr/bin/env python3
"""One-off mechanical port of the Genesis game sources (orig/c4_*.c) to the
SMS layout (src/game/).  The output is then edited by hand; this script is
kept as a record and refuses to overwrite existing files unless -f."""
import os
import re
import sys

from gen_states import strip_states

HERE = os.path.dirname(os.path.abspath(__file__))
ORIG = os.path.join(HERE, '..', 'orig')
OUT = os.path.join(HERE, '..', 'src', 'game')

FILES = ['c4_main.c', 'c4_game.c', 'c4_play.c', 'c4_state.c', 'c4_wiz.c', 'c4_act1.c', 'c4_debug.c']

FUNC_RE = re.compile(r'^(?!static\b)(?!typedef\b)(?!extern\b)(?!return\b)(?!else\b)'
                     r'((?:const\s+)?[A-Za-z_]\w*(?:\s+\w+)*[\s\*]+)(\w+)\s*\(([^;{}()]*)\)\s*(;?)\s*$')


def port(src):
    src = src.replace('\r\n', '\n')
    src = re.sub(r'^#include\s+"(?!def\.h)[^"]*"\s*\n', '', src, flags=re.M)
    src = strip_states(src)
    src = re.sub(r'\bsintable\[([^\]]+)\]', r'fsin(\1)', src)
    src = re.sub(r'\bcostable\[([^\]]+)\]', r'fcos(\1)', src)
    src = re.sub(r'\brandom\(\)', 'random16()', src)
    src = re.sub(r'^\s*vid_update\(\);\s*\n', '', src, flags=re.M)
    src = re.sub(r'^\s*vid_wait_idle\(\);\s*\n', '', src, flags=re.M)
    src = re.sub(r'^\s*dbg_mark\([^;]*\);\s*\n', '', src, flags=re.M)
    out = []
    for line in src.split('\n'):
        m = FUNC_RE.match(line)
        if m and m.group(2) not in ('if', 'while', 'for', 'switch', 'sizeof'):
            ret, name, args, semi = m.groups()
            line = '%s%s (%s) B%s' % (ret, name, args, semi)
        out.append(line)
    return '\n'.join(out)


def main():
    force = '-f' in sys.argv
    os.makedirs(OUT, exist_ok=True)
    for f in FILES:
        dst = os.path.join(OUT, f)
        if os.path.exists(dst) and not force:
            print('exists, skipped', dst)
            continue
        src = open(os.path.join(ORIG, f), encoding='latin1').read()
        open(dst, 'w').write(port(src))
        print('wrote', dst)


if __name__ == '__main__':
    main()
