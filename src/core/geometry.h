#pragma once
#include <SDL3/SDL.h>

// AABB overlap of a circle of radius r centred at (x,y) with a platform rect.
inline bool circleOverlapsRect(float x, float y, float r, const SDL_FRect& pl) {
    return x + r > pl.x && x - r < pl.x + pl.w &&
           y + r > pl.y && y - r < pl.y + pl.h;
}
