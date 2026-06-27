#pragma once
#include "types.h"

// Bird travel speed for a level.
inline float birdSpeed(int level) {
    float s = 72.0f + level * 8.0f;
    return (s < 150.0f ? s : 150.0f) * 0.5f;
}

// AABB overlap of a circle of radius r centred at (x,y) with a platform rect.
inline bool circleOverlapsRect(float x, float y, float r, const SDL_FRect& pl) {
    return x + r > pl.x && x - r < pl.x + pl.w &&
           y + r > pl.y && y - r < pl.y + pl.h;
}

void spawnMummy(Game& g);
void transformMummy(Game& g, Enemy& e);
void updateMummy(Game& g, Enemy& e, float dt);

void blockEnemy(Game& g, Enemy& e, float ox, float oy);
bool flyerBlocked(const Game& g, float r, float x, float y,
                  const Enemy* self = nullptr);
void updateBird(Game& g, Enemy& e, float dt, float pcx, float pcy);
void updateFlyer(Game& g, Enemy& e, float dt, float pcx, float pcy);
