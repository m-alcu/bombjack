# Tools

Scripts that produce the clone's ROM-derived data (`src/data/`) and the MAME
harness used to measure the original. None of them is needed to build or play.

They need the MAME `bombjack` ROM set, which is not in this repository. The
MAME scripts read and write a working directory given by `BJTRACE` (default
`~/snap/mame/common/bjtrace`), with the ROMs in `$BJTRACE/roms/bombjack/`.

## Generated data

| File | Source | How |
|------|--------|-----|
| `src/data/rom_pens.h` | gfx ROMs + `assets/sprites.png` | `tools/gen_rom_pens.py $BJTRACE/roms/bombjack` |
| `src/data/levels_data.h` | main ROM tables, `capture_round.lua` video-RAM dumps, LOVE/Lua `levels.json` | one-off conversion, not kept; the header comment lists every ROM address |
| `src/data/demo_data.h` | main ROM 0xCE00 / 0x6D00 / 0x6E00 / 0x6F00, `capture_demo_path.lua` | one-off conversion, not kept; format in the header comment |
| `src/data/demo_runs.h` | `capture_demo_sprites.lua` over 68 demos | one-off conversion, not kept; format in the header comment |
| `src/data/sprites_full_png.h` | `assets/sprites.png` | `xxd -i assets/sprites.png \| sed s/assets_sprites_png/sprites_full_png/` |
| `src/data/screens_png.h` | `assets/screens.png` | `xxd -i assets/screens.png \| sed s/assets_screens_png/screens_png/` |

`gen_rom_pens.py` and the `xxd` commands reproduce the committed files exactly. For the three
one-off files, the scripts that turned the captures into C++ were not kept.
Each header's comment records where its data comes from and how it is encoded.

## MAME harness (`tools/mame/`)

Run a Lua script with
`mame bombjack -rompath $BJTRACE/roms -video none -sound none -nothrottle -skip_gameinfo -autoboot_script <script>`.
Scripts that start a game insert a coin at frame 1500 and press start at 1600;
play begins around frame 1950.

| Script | Captures |
|--------|----------|
| `capture_round.lua` | bombs and video RAM of the round in `$BJTRACE/round.txt` |
| `capture_transform_list.lua` | the mummy transform list (RAM 0x8268) of round `$BJROUND` |
| `capture_demo_path.lua` | Jack's path through the attract demos, unhittable |
| `capture_demo_sprites.lua` | the sprite list each demo frame |
| `trace_jack.lua` / `trace_jack.sh` | Jack's position and speed under scripted input |
| `render_sound.lua` / `render_sound.sh` | one sound command recorded to a WAV |
| `disassemble.lua` | main CPU disassembly (add `-debug -debugger none`) |

The clone has matching tools for comparing against these captures:
`--demotrace` and `--enemytrace` (see `src/debug_tools.h`).
