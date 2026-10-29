# Bomb Jack (SDL3 / C++)

A homage to the 1984 arcade game *Bomb Jack*. Float around the platforms,
collect every bomb, take the lit ones for a bonus, and dodge the enemies.

Wherever possible the clone follows the original, read from its ROMs and
measured in MAME: the round layouts, bomb order, Jack's physics, the enemies'
animation, the palette cycles, the scoring and the attract-mode demo all come
from the arcade. `tools/` holds the scripts used to get them.

The sprite atlas and the classic backdrops are embedded in the binary as PNGs
(decoded with the vendored stb_image). Sounds (`assets/sounds/`) and the
optional background sets (`assets/*.png`) are loaded at run time from the
repository, found through the `ASSET_ROOT` path the build bakes in.

![controls](https://img.shields.io/badge/SDL-3-blue)

## Gameplay

- Play through the arcade's **64 rounds** (after round 64 play continues from
  round 32, as on the cabinet), each with its own platform layout, bomb
  arrangement and backdrop — including the platform-less stages where you must
  float to every bomb. Layouts, bomb positions and the lit-bomb order are taken
  from the original ROM.
- Collect **every bomb** in a round to advance to the next one.
- After you take the first bomb, one bomb always shows a **lit fuse**, lighting
  the bombs in a fixed sequence. Fire (lit) bombs are worth 200 instead of 100
  and count toward the end-of-round **Special Bonus**: take 20–23 of them and a
  between-round screen awards 10,000–50,000 points.
- Every take-off (a jump, or walking off a ledge) scores 10 points.
- Bombs charge the **power bar** (lit bombs twice as fast). When it fills, the
  **Power ball** appears. Each take-off changes its colour; take it to
  **freeze every enemy** for about five seconds, for 100 to 2,000 points by
  colour. While the freeze lasts Jack, the ball's colours and the HUD's
  multiplier box share one rotating palette.
- While frozen, the enemies turn into coins: touch them for escalating points
  (100, 200, 300, 500, 800 …); they blink as the freeze runs out.
- Besides the birds, **mummies** drop in over time, walk and tumble down the
  platforms, and on reaching the floor **transform** into one of five chasers:
  spheres (home in horizontally), orbs (vertically), clubs (both ways), horns
  (drift diagonally) and UFOs (dive straight at you).
- **Bonus coins** appear every 5,000 points: **B** raises the score multiplier
  (up to ×5), **E** gives an extra life, and the rare **S** an extra life and
  the "YOU ARE LUCKY" screen.
- Touching an enemy when *not* frozen costs a life; you come back with brief
  invulnerability (you blink). Lose all lives and it's game over; a high
  enough score goes on the table under your initials.
- While idle the title cycles through the high-score table and demos of rounds
  1 and 2, replayed from the arcade's own recordings.

## Controls

| Key            | Action                                               |
| -------------- | ---------------------------------------------------- |
| Left / A       | Move left                                            |
| Right / D      | Move right                                           |
| Space          | Start; jump on the ground; press again in the air to stop |
| Up / W         | In the air: half gravity (jump higher, fall slower)  |
| Down / S       | In the air: double gravity (jump lower, fall faster) |
| O              | Options (from the title): start round, lives, backgrounds, … |
| R              | Restart after game over                              |
| Esc / Q        | Quit (Esc backs out of the options and name entry)   |

The jump works like the arcade's (speeds measured from the original in MAME):
Jack crouches for a moment, then every jump is full height — holding the button
changes nothing. Pressing it again mid-air cancels his vertical speed: during a
rise he stops and starts to fall, and during a fall he restarts from rest, so
tapping repeatedly makes him hover and sink slowly.

## Sound

Music and effects come from the original Bomb Jack, recorded from the arcade's
sound commands in MAME, as WAV files loaded and mixed with SDL3's audio
streams. If no audio device is available the game runs silently.

## Build & run

Requires **SDL3** and a C++17 compiler.

```bash
cmake -B build
cmake --build build
./build/bombjack
```

`build.sh` does the same (configuring on the first run).

## Source layout

The simulation (`game/`) has no drawing code (it uses SDL only for its
rectangle type) and plays sounds through `platform/audio`; the renderer
(`render/`) only reads `Game`. Each frame, `main.cpp` turns the
keyboard into an `Input`, runs `update()` in fixed 1/120 s steps, then calls
`render()`.

```
src/
  main.cpp            window and main loop
  debug_tools.*       --selftest, --rendertest, --shot, --demotrace, --enemytrace
  core/               shared by everything
    constants.h         world/screen coordinates, arcade frame rate, gravity
    color.h, geometry.h
  game/               the simulation
    game.*              Game struct and the state machine (update, updatePlaying)
    round.*             round layout from the ROM tables, setup, bombs, music, clearing
    player.*            Jack's movement, take-offs, pose, death sequence
    enemy.*             birds, mummy schedule, chasers, contact with Jack
    enemy_anim.cpp      the arcade's enemy animation tables and steps
    powerball.*         Power ball, freeze, palette 14
    bonus.*             B/E/S coins
    effects.*           score popups and short animations
    hiscore.*           high-score table and name entry
    attract.*           title cycle and demo replay
  render/             drawing
    render.*            what to draw for each state
    world.*             the play area
    hud.*               top and bottom HUD strips
    screens.*           title, table, options, bonus pages, overlays
    palette.*           colour cycles (palettes 14 and 15, enemy eyes, …)
    background.*        backdrops, platforms, the playfield frame, title banner
    textures.*          textures sliced from the sprite atlas
    sprite_atlas.h      where each sprite sits in the atlas
    gfx.*               font, rectangles, tinted textures, coordinate switch
    animsprite.h        frame-sequence sprite (Jack's living poses)
  platform/
    audio.*             sound playback, tagged channels
    input.*             keyboard → game, per state
  data/               generated from the ROMs and MAME captures (see tools/)
  third_party/        stb_image.h
tools/                ROM data generator and MAME capture harness
```

`updatePlaying` (in `game/game.cpp`) reads as the order of one arcade step:
Jack moves, bombs are collected, the Power ball and bonus coin move, effects
age, then the enemies move and meet Jack.

### Debug tools

```bash
./build/bombjack --selftest 600                    # scripted play, no window
SDL_VIDEODRIVER=dummy ./build/bombjack --rendertest [outdir]   # every state, BMPs to outdir
./build/bombjack --shot 3 round3.bmp               # a round's playfield
./build/bombjack --demotrace 1 0                   # Jack's demo path (CSV)
./build/bombjack --enemytrace 5 3000               # enemy sprites per frame (CSV)
```

They seed the random generator, so their output is the same on every run and
can be diffed before and after a change.

## Installing SDL3

- **Ubuntu/Debian:** `sudo apt install libsdl3-dev` (or build from source).
- **macOS:** `brew install sdl3`
- **From source:** https://github.com/libsdl-org/SDL
