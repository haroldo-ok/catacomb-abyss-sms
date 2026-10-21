#!/bin/bash
# bench an alternative render.c/render_a.s pair, then restore the current ones
export PATH=$HOME/.devkitsms/bin:$PATH
cd "$(dirname "$0")/.."
cp src/render.c /tmp/ab_c.c; cp src/render_a.s /tmp/ab_s.s
cp "$1" src/render.c; cp "$2" src/render_a.s
make >/dev/null 2>&1 && tools/bench.sh
cp /tmp/ab_c.c src/render.c; cp /tmp/ab_s.s src/render_a.s
make >/dev/null 2>&1
