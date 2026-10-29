#pragma once
// The B/E/S bonus coins: B raises the multiplier, E gives an extra life, S an
// extra life and the "YOU ARE LUCKY" screen.

struct Game;

enum BonusKind { BK_B, BK_E, BK_S };

struct BonusCoin {
    bool  active = false;
    int   kind   = BK_B;
    float x = 0, y = 0, vx = 0;
    float platX = 0, platW = 0;   // the platform it patrols
    float anim  = 0;
};

constexpr int   BONUS_LIMIT    = 5000;   // a coin is offered every BONUS_LIMIT points
constexpr int   BONUS_POINTS   = 1000;
constexpr int   BONUS_S_POINTS = 5000;
constexpr int   BONUS_S_CHANCE = 6;      // percent
constexpr int   BONUS_E_EVERY  = 8;      // B coins (plus lives lost) per E coin
constexpr int   MAX_MULT       = 5;
constexpr float BONUS_W        = 26.0f;
constexpr float BONUS_H        = 26.0f;
constexpr float BONUS_SPEED    = 45.0f;

// Drop a bonus coin of `kind` onto a random platform.
void spawnBonus(Game& g, int kind);

// Offer a coin at each score threshold, patrol it along its platform and let
// Jack take it.
void updateBonus(Game& g, float dt);
