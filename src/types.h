#pragma once
#include <SDL3/SDL.h>
#include <random>
#include <vector>
#include "constants.h"
#include "color.h"
#include "animsprite.h"

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

enum BonusKind  { BK_B, BK_E, BK_S };
enum State      { TITLE, OPTIONS, PLAYING, ROUNDCLEAR, SPECIALBONUS, GAMEOVER };
enum DeathPhase { DP_NONE, DP_DANCING, DP_FALLING, DP_DEAD, DP_WAIT };

struct Input  { bool left, right, up, down, jumpHeld, jumpPressed; };

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

struct Popup { float x, y, age = 0.0f; int value = 0; };

// Shared layout for explosion flash, coin-pickup sparkle, bonus-collect flash.
// The vector name in Game says which kind it holds.
struct TimedEffect { float x, y, age = 0.0f; };

// ---------------------------------------------------------------------------
// Game-system sub-states (kept out of the flat Game struct for clarity)
// ---------------------------------------------------------------------------

struct DeathState {
    bool  active = false;
    int   phase  = DP_NONE;
    int   frame  = 0, loops = 0;
    float anim   = 0.0f, timer = 0.0f;
};

struct OrbState {
    bool  active = false;
    float x = 0, y = 0, vx = 0, vy = 0;
    int   family = 0;
};

struct BonusState {
    bool  active = false;
    int   kind   = BK_B;
    float x = 0, y = 0, vx = 0;
    float platX = 0, platW = 0;
    float anim  = 0;
};

struct SBState {
    int   phase     = 0;   // 0=begin hold, 1=count-up, 2=end hold
    int   remaining = 0, catched = 0;
    float timer     = 0;
};

struct GameOverState {
    float x = 0, y = 0, timer = 0;
    int   stage = 0;
};

// ---------------------------------------------------------------------------
// God struct: top-level game state
// ---------------------------------------------------------------------------
struct Game {
    int   state = TITLE;
    int   score = 0, lives = 3, level = 1, bombsLeft = 0;
    int   startLevel = 1, startLives = 3;
    int   imgStyle = 0, optSel = 0;
    int   litBomb  = -1, catched = 0;
    int   phaseStart = 0, multiplier = 1;
    int   nextBonusScore = BONUS_LIMIT;
    int   bCoins = 0, nextEAt = BONUS_E_EVERY, livesLost = 0;
    int   killCount = 0;
    int   mummiesSpawned = 0, transformIdx = 0;
    float time = 0, clearTimer = 0, startAnim = 0;
    float powerMeter = 0, freezeTimer = 0, mummyTimer = 0;
    Color freezeColor{200, 205, 220};

    DeathState    death;
    OrbState      orb;
    BonusState    bonus;
    SBState       sb;
    GameOverState go;

    Player p{};
    AnimSprite jackSprite;   // owned animated sprite for the living player
    std::vector<SDL_FRect>   platforms;
    std::vector<Bomb>        bombs;
    std::vector<Enemy>       enemies;
    std::vector<Popup>       popups;
    std::vector<TimedEffect> explosions;
    std::vector<TimedEffect> coinPickups;
    std::vector<TimedEffect> bonusTakens;
    std::mt19937 rng{std::random_device{}()};
};
