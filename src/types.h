#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <random>
#include <vector>
#include "constants.h"

struct Color { Uint8 r, g, b; };

// Basic colours used for the spawn flash (EP_INIT).
constexpr Color BASIC_COLORS[] = {
    {255,  60,  60}, { 60, 255,  60}, { 80, 120, 255},
    {255, 235,  60}, { 60, 235, 255}, {255, 110, 255},
    {255, 160,  50}, {255, 255, 255},
};

// Power-orb colour families (index matches POWER_POINTS):
// 0=blue, 1=red, 2=purple, 3=green, 4=turquoise, 5=yellow, 6=silver.
constexpr Color POWER_COLORS[7] = {
    { 45,  80, 240}, {235,  30,  45}, {165,  45, 225}, { 45, 205,  65},
    { 40, 215, 200}, {240, 220,  50}, {200, 205, 220},
};

enum BonusKind { BK_B, BK_E, BK_S };
enum State     { TITLE, OPTIONS, PLAYING, ROUNDCLEAR, SPECIALBONUS, GAMEOVER };
enum DeathPhase{ DP_NONE, DP_DANCING, DP_FALLING, DP_DEAD, DP_WAIT };

struct Input { bool left, right, up, down, jumpHeld, jumpPressed; };

struct Player {
    float x, y, vx, vy;
    bool  onGround;
    float invuln;
    int   face = 1;
};

struct Bomb { float x, y; bool collected; };

enum EKind  { EK_BIRD, EK_MUMMY, EK_SPHERE, EK_ORB, EK_HORN, EK_CLUB, EK_UFO };
enum EPhase { EP_INIT, EP_FLY, EP_APPEAR, EP_WALK, EP_FALL, EP_DISAPPEAR };

struct Enemy {
    float x, y, vx, vy, r;
    int   kind    = EK_BIRD;
    int   phase   = EP_FLY;
    float timer   = 0.0f;
    int   bounces = 0;
    int   becomes = EK_SPHERE;
    int   dirX = 0, dirY = 0;
    int   tick = 0;
    float tgtX = 0.0f, tgtY = 0.0f;
    Color spawnTint{255, 255, 255};
};

struct Popup     { float x, y, age = 0.0f; int value = 0; };
struct Explosion { float x, y, age = 0.0f; };
struct CoinPickup{ float x, y, age = 0.0f; };
struct BonusTaken{ float x, y, age = 0.0f; };

struct Game {
    int   state = TITLE;
    int   score = 0, lives = 3, level = 1, bombsLeft = 0;
    int   startLevel = 1;
    int   startLives = 3;
    int   imgStyle   = 0;
    int   optSel     = 0;
    int   litBomb    = -1;
    int   catched    = 0;
    int   sbState    = 0;
    float sbTimer    = 0.0f;
    int   sbRemaining= 0;
    int   sbCatched  = 0;
    int   phaseStart = 0;
    float clearTimer = 0.0f;
    float time       = 0.0f;
    float goX = 0.0f, goY = 0.0f, goTimer = 0.0f;
    int   goStage    = 0;
    float powerMeter = 0.0f;
    bool  orbActive  = false;
    float orbX = 0.0f, orbY = 0.0f;
    float orbVx = 0.0f, orbVy = 0.0f;
    int   orbFamily  = 0;
    float freezeTimer= 0.0f;
    Color freezeColor{200, 205, 220};
    int   killCount  = 0;
    bool  playerDying= false;
    float deathTimer = 0.0f;
    int   deathPhase = DP_NONE;
    int   deathFrame = 0;
    int   deathLoops = 0;
    float deathAnim  = 0.0f;
    float mummyTimer = 0.0f;
    int   mummiesSpawned = 0;
    int   transformIdx   = 0;
    int   multiplier     = 1;
    int   nextBonusScore = BONUS_LIMIT;
    bool  bonusActive    = false;
    int   bonusKind      = BK_B;
    int   bCoins         = 0;
    int   nextEAt        = BONUS_E_EVERY;
    int   livesLost      = 0;
    float bonusX = 0.0f, bonusY = 0.0f, bonusVx = 0.0f;
    float bonusPlatX = 0.0f, bonusPlatW = 0.0f;
    float bonusAnim  = 0.0f;
    float startAnim  = 0.0f;

    Player                p{};
    std::vector<SDL_FRect> platforms;
    std::vector<Bomb>      bombs;
    std::vector<Enemy>     enemies;
    std::vector<Popup>     popups;
    std::vector<Explosion> explosions;
    std::vector<CoinPickup>coinPickups;
    std::vector<BonusTaken>bonusTakens;
    std::mt19937           rng{std::random_device{}()};
};
