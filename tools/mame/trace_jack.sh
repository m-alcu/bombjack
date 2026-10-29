#!/bin/sh
# usage: trace_jack.sh NAME "start len field" ...   -> $BJTRACE/tr_NAME.csv
# field is Right, Left, Up, Down or Jump; frames count from the game start.
D=$(cd "$(dirname "$0")" && pwd)
export BJTRACE=${BJTRACE:-$HOME/snap/mame/common/bjtrace}
n=$1; shift
{ echo "tr_$n.csv"; for s in "$@"; do echo "$s"; done; } > "$BJTRACE/scenario.txt"
timeout 300 mame bombjack -rompath "$BJTRACE/roms" -video none -sound none -nothrottle \
    -skip_gameinfo -autoboot_script "$D/trace_jack.lua" >/dev/null 2>&1
