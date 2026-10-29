#!/bin/sh
# usage: render_sound.sh HEXCMD LENFRAMES  -> $BJTRACE/wav/r_HEXCMD.wav
# The command is injected at frame 600 (10 s) and recorded for LENFRAMES frames.
D=$(cd "$(dirname "$0")" && pwd)
export BJTRACE=${BJTRACE:-$HOME/snap/mame/common/bjtrace}
mkdir -p "$BJTRACE/wav"
printf '%s\n%s\n' "$1" "$2" > "$BJTRACE/cmd.txt"
timeout 600 mame bombjack -rompath "$BJTRACE/roms" -video none -sound none -nothrottle \
    -skip_gameinfo -autoboot_script "$D/render_sound.lua" -wavwrite "$BJTRACE/wav/r_$1.wav" >/dev/null 2>&1
