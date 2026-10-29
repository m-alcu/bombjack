#pragma once
#include <SDL3/SDL.h>

// Basic RGB triple used everywhere for tints and palettes. Lives in its own
// header so low-level helpers (e.g. animsprite.h) can use it without pulling in
// the full game types, which in turn lets types.h own AnimSprite members.
struct Color { Uint8 r, g, b; };

// Multiply every channel by a 0..1 brightness factor (used by the freeze shimmer).
constexpr Color scale(Color c, float f) {
    return {(Uint8)(c.r * f), (Uint8)(c.g * f), (Uint8)(c.b * f)};
}

// Linear blend from a to b; t in 0..1.
constexpr Color lerp(Color a, Color b, float t) {
    return {(Uint8)(a.r + (b.r - a.r) * t),
            (Uint8)(a.g + (b.g - a.g) * t),
            (Uint8)(a.b + (b.b - a.b) * t)};
}

// Four-step brightness ramp for the freeze colour-cycle shimmer.
inline constexpr float FREEZE_BRIGHT[4] = {0.45f, 0.65f, 0.83f, 1.0f};
