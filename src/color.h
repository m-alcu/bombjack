#pragma once
#include <SDL3/SDL.h>

// Basic RGB triple used everywhere for tints and palettes. Lives in its own
// header so low-level helpers (e.g. animsprite.h) can use it without pulling in
// the full game types, which in turn lets types.h own AnimSprite members.
struct Color { Uint8 r, g, b; };
