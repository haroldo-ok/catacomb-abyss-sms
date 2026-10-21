#!/bin/bash
# smstest with banked-call attribution for the profiler
cd "$(dirname "$0")/.."
export SMSTEST_BCALL=0x$(awk '/___sdcc_bcall_ehl/{print $1; exit}' build/game.map | sed 's/^0*//')
exec tools/smstest/smstest "$@"
