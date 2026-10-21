#!/bin/bash
# renderer benchmarks: cycles per render for static / turning / moving views
export PATH=$HOME/.devkitsms/bin:$PATH
cd "$(dirname "$0")/.."
for b in static:cg turn:cg2 move:cg3; do
  n=${b%%:*}; f=${b##*:}
  smstest catacomb_abyss.sms tests/bench/$n.txt >/dev/null
  python3 tools/prof2.py /tmp/$f.txt build/game.noi build/*.rst --frames 1 --top ${TOP:-60} > /tmp/bench_$n.txt 2>&1
  awk -v n=$n '/^render_draw .*calls/{d=$(NF-1); c=$(NF-2)} /^render_cast .*calls/{k=$(NF-1)} END{printf "%-7s renders %3d  draw %7d  cast %7d  total %7d  (%.2f frames/render)\n", n, c, d, k, d+k, (d+k)/59736}' /tmp/bench_$n.txt
done
