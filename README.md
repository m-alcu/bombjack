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

- Play through **30 hand-authored levels** (then they loop), each with its own
  platform layout, bomb arrangement and backdrop — including the platform-less
  "California" stages where you must float to every bomb.
- Collect **every bomb** in a round to advance to the next, harder round.
- One bomb always has a **glowing fuse** — grab the lit one to raise your bonus
  multiplier (up to x5). Grabbing an unlit bomb resets the multiplier.
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
| Space / Up / W | Jump on the ground; **tap** in the air to flutter, **hold** to glide |
| R              | Restart after game over                            |
| Esc / Q        | Quit                                               |

The floaty jump is the heart of Bomb Jack: tap the jump button in mid-air to get
small upward kicks (you can hover and slowly climb), or hold it to drift down
gently.

## Build & run

Requires **SDL3** and a C++17 compiler.

### With CMake

```bash
cmake -B build
cmake --build build
./build/bombjack
```

### Quick build (no CMake)

```bash
./build.sh
./bombjack
```

### Headless tests

Run the simulation with scripted input and no window (also validates that every
sprite's pixel-map rows are rectangular):

```bash
./bombjack --selftest 600
```

Smoke-test the full render path (textures, tiled platforms, every game state)
without a display — handy for CI:

```bash
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./bombjack --rendertest
```

## Layout

- `src/main.cpp` — the entire game (physics, rendering, font, game states).
- `CMakeLists.txt` / `build.sh` — build entry points.

## Installing SDL3

- **Ubuntu/Debian:** `sudo apt install libsdl3-dev` (or build from source).
- **macOS:** `brew install sdl3`
- **From source:** https://github.com/libsdl-org/SDL
