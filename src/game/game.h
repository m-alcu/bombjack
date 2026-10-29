#pragma once
// Top-level game state and the state machine that steps it. Each part of the
// game lives in its own module; Game only gathers their state:
//
//   round      layout, bombs, music, clearing a round      round.h
//   player     Jack's movement and death                   player.h
//   enemy      birds, mummies and chasers                  enemy.h
//   powerball  Power ball, freeze, palette 14              powerball.h
//   bonus      B/E/S coins                                 bonus.h
//   effects    score popups and short animations           effects.h
//   hiscore    table and name entry                        hiscore.h
//   attract    title cycle and demo replay                 attract.h
#include <SDL3/SDL.h>
#include <random>
#include <vector>
#include "core/constants.h"
#include "game/attract.h"
#include "game/bonus.h"
#include "game/effects.h"
#include "game/enemy.h"
#include "game/hiscore.h"
#include "game/player.h"
#include "game/powerball.h"
#include "game/round.h"

enum State { TITLE, OPTIONS, PLAYING, ROUNDCLEAR, SPECIALBONUS, LUCKY, GAMEOVER, NAMEENTRY };

// OPTIONS screen selections.
struct Options {
    int  sel        = 0;       // highlighted row
    int  firstRound = 1;
    int  lives      = 3;
    int  imgStyle   = 0;       // 0 = CLASSIC, 1-7 = grid sets, 8 = NO background
    bool alwaysS    = false;   // spawn an S bonus every round
    bool start100k  = false;   // start a game with 100000 points
};
constexpr int OPT_COUNT      = 5;
constexpr int OPT_MAX_LIVES  = 9;
constexpr int OPT_IMG_STYLES = GRID_BG_COUNT + 2;  // CLASSIC + 7 grids + NO

// Special bonus tally after a round with 20+ fire bombs.
struct SpecialBonusState {
    int   phase     = 0;   // 0=begin hold, 1=count-up, 2=end hold
    int   remaining = 0, fireBombs = 0;
    float timer     = 0;
};
constexpr float SB_BEGIN_TIME = 2.0f;
constexpr float SB_END_TIME   = 1.5f;
constexpr float SB_SCORE_TIME = 0.1f;

// "YOU ARE LUCKY / ONE MORE PLAY" screen shown on collecting an S bonus.
struct LuckyState {
    int   phase = 0;   // 0 = text sliding in, 1 = hold before next round
    float timer = 0;   // slide progress (phase 0) then hold countdown (phase 1)
};
constexpr float LUCKY_SLIDE_TIME = 1.1f;  // corner text glides to centre
constexpr float LUCKY_HOLD_TIME  = 2.2f;  // hold "ONE MORE PLAY" before next round

// "GAME OVER" sprite gliding from where Jack died to the screen centre.
struct GameOverState {
    float x = 0, y = 0, timer = 0;   // screen pixels
    int   stage = 0;                 // 0 = to centre row, 1 = to centre column, 2 = hold
};
constexpr float GAMEOVER_SLIDE = 110.0f;
constexpr float GAMEOVER_HOLD  = 3.0f;

struct Game {
    int   state = TITLE;
    float time  = 0;     // seconds since start; drives every colour cycle
    Options opt;

    // Current game
    int   score = 0, lives = 3, round = 1;
    int   multiplier = 1;
    int   nextBonusScore = BONUS_LIMIT;
    int   bCoins = 0, nextEAt = BONUS_E_EVERY, livesLost = 0;

    // Current round
    std::vector<SDL_FRect> platforms;
    std::vector<Bomb>      bombs;
    int   bombsLeft = 0;
    int   litBomb   = -1, fireBombs = 0;   // lit bomb, and lit bombs taken
    float startAnim = 0;                   // "START!" intro time left
    float clearTimer = 0;

    Player     p{};
    DeathState death;

    std::vector<Enemy> enemies;
    int   mummiesSpawned = 0, transformIdx = 0;
    float mummyTimer = 0;
    int   killCount = 0;                   // frozen enemies taken this freeze

    PowerBall ball;
    float     powerMeter = 0, freezeTimer = 0;
    Palette14 pal14;

    BonusCoin bonus;

    std::vector<Popup>       popups;
    std::vector<TimedEffect> explosions;
    std::vector<TimedEffect> coinPickups;
    std::vector<TimedEffect> bonusTakens;

    // Screens between rounds and after the game
    SpecialBonusState special;
    LuckyState        lucky;
    GameOverState     gameOver;
    NameEntry         nameEntry;
    HiScoreEntry      hiscores[HISCORE_COUNT];

    AttractState attract;
    DemoState    demo;

    std::mt19937 rng{std::random_device{}()};
};

// Start a new game from the OPTIONS selections.
void startGame(Game& g);

// Adjust the selected OPTIONS value by d.
void optionAdjust(Game& g, int d);

// One PLAYING step: Jack, bombs, Power ball, bonus coin, effects, enemies.
void updatePlaying(Game& g, const Input& in, float dt);

// Top-level update dispatched to the current state.
void update(Game& g, const Input& in, float dt);
