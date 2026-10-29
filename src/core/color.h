#pragma once
#include <SDL3/SDL.h>

// Basic RGB triple used everywhere for tints and palettes.
struct Color { Uint8 r, g, b; };

// Multiply every channel by a 0..1 brightness factor (the "START!" fade).
constexpr Color scale(Color c, float f) {
    return {(Uint8)(c.r * f), (Uint8)(c.g * f), (Uint8)(c.b * f)};
}
