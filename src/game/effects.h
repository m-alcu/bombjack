#pragma once
// Short-lived effects: floating score numbers, bomb explosions, the sparkle of
// a collected enemy and the flash of a taken bonus coin.

struct Game;

struct Popup { float x, y, age = 0.0f; int value = 0; };

// Shared layout for explosion flash, coin-pickup sparkle, bonus-collect flash.
// The vector name in Game says which kind it holds.
struct TimedEffect { float x, y, age = 0.0f; };

// Frame lengths; each effect lasts its frame count times these.
constexpr float EXPL_FRAME       = 0.06f;   // 3 frames
constexpr float PICKCOIN_FRAME   = 0.06f;   // 4 frames
constexpr float BONUSTAKEN_FRAME = 0.05f;   // 6 frames

// Age every effect and drop the finished ones.
void updateEffects(Game& g, float dt);
