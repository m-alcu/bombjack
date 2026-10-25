# Bomb Jack (minimal SDL3 / C++)

A small, single-file homage to the 1984 arcade game *Bomb Jack*. Float around
floating platforms, collect every bomb, grab the lit one to build a bonus, and
dodge the chasers.

The character sprites (Jack's stand/walk/float frames, the fuse bomb, the
flapping bird enemies) are cropped from the **official arcade Bomb Jack sprite
sheet**; the girder platform is a small pixel map. The round backdrops are the
five classic arcade scenes (Egypt, Greece, castle, city, night pyramid). Both
the sprite sheet and the backdrop strip are embedded into the binary as PNGs and
decoded at startup with the vendored, public-domain stb_image, so — together
with the bitmap font — there are **no runtime asset files** to ship. Rounds
cycle through the backdrops: round 1 shows the first, then each round advances to
the next, wrapping after the fifth.

![controls](https://img.shields.io/badge/SDL-3-blue)

## Gameplay

- Play through the arcade's **64 rounds** (after round 64 play continues from
  round 32, as on the cabinet), each with its own platform layout, bomb
  arrangement and backdrop — including the platform-less stages where you must
  float to every bomb. Layouts, bomb positions and the lit-bomb order are taken
  from the original ROM.
- Collect **every bomb** in a round to advance to the next, harder round.
- After you grab the first bomb, one bomb always shows a **glowing fuse** that
  lights the bombs in a fixed sequence. Grab the lit ("fire") bomb to keep the
  chain going — fire bombs are worth 200 (vs 100) and count toward the
  end-of-round **Special Bonus**: catch 20–23 of them and a between-round screen
  awards 10,000–50,000 bonus points, counting up onto your score.
- Catching bombs charges the **POWER** gauge (lit bombs charge it faster). When
  it fills, a colour-cycling **P orb** appears — grab it to **freeze every
  chaser** for a few seconds.
- While frozen, touch the chasers to **destroy them** for escalating points
  (100, 200, 300, 500, 800 …); they blink as the freeze runs out.
- Beyond the patrolling birds, **mummies** drop in over time, walk and tumble
  down the platforms, and on reaching the ground **transform** into one of five
  flying chasers — spheres (home horizontally), orbs (home vertically), clubs
  (home both ways), horns (constant diagonal drift) and UFOs (dive straight at
  you). Each level's transform sequence comes from the original game data.
- Touching a chaser when *not* frozen costs a life; you respawn with brief
  invulnerability (you blink). Lose all lives and it's game over. (A mummy is
  harmless while it's still popping in.)
- Floating score popups show every point gained.
- Rounds are endless and the chasers get faster and more numerous — chase a high
  score.

## Controls

| Key            | Action                                             |
| -------------- | -------------------------------------------------- |
| Left / A       | Move left                                          |
| Right / D      | Move right                                         |
| Space          | Jump on the ground; press again in the air to stop |
| Up / W         | In the air: half gravity (jump higher, fall slower) |
| Down / S       | In the air: double gravity (jump lower, fall faster) |
| R              | Restart after game over                            |
| Esc / Q        | Quit                                               |

The jump works like the arcade's (speeds measured from the original in MAME):
Jack crouches for a moment, then every jump is full height — holding the button
changes nothing. Pressing it again mid-air cancels his vertical speed: during a
rise he stops and starts to fall, and during a fall he restarts from rest, so
tapping repeatedly makes him hover and sink slowly.

## Sound

Music and effects are ported straight from the original Bomb Jack: jump,
footsteps, bomb pickups, the power-ball appear/collect jingles, enemy hits and
kills, bonus coins, level-clear, game-over and the in-game music loop. They live
in `assets/sounds/` (`.wav` effects + `.mp3` music, the same files as the Lua
original).

No extra library is needed — playback uses SDL3's built-in audio-stream mixing,
and MP3s are decoded with the vendored single-header [`dr_mp3.h`](src/dr_mp3.h)
(same style as `stb_image.h`). If no audio device is available the game simply
runs silently.

## Build & run

Requires **SDL3** and a C++17 compiler.

### With CMake

```bash
cmake -B build
cmake --build build
./build/bombjack
```

### Quick build

`build.sh` configures (first run) and builds via CMake:

```bash
./build.sh
./build/bombjack
```

### Headless tests

Run the simulation with scripted input and no window (also validates that every
sprite's pixel-map rows are rectangular):

```bash
./build/bombjack --selftest 600
```

Smoke-test the full render path (textures, tiled platforms, every game state)
without a display — handy for CI:

```bash
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/bombjack --rendertest
```

## Layout

- `src/main.cpp` — the entire game (physics, rendering, font, game states).
- `CMakeLists.txt` / `build.sh` — build entry points.

## Installing SDL3

- **Ubuntu/Debian:** `sudo apt install libsdl3-dev` (or build from source).
- **macOS:** `brew install sdl3`
- **From source:** https://github.com/libsdl-org/SDL
