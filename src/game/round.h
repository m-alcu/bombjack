#pragma once
// A round: its layout from the ROM tables, setting it up, the bombs, the
// in-game music, and clearing it.
#include "core/constants.h"
#include "data/levels_data.h"

struct Game;

struct Bomb { float x, y; bool collected; };

// Bomb pickup: |dx| < 10 px and |dy| < 8 px between Jack's and the bomb's centres.
constexpr float PICK_DX = 10.0f * PX_TO_WU_X;
constexpr float PICK_DY = 8.0f * PX_TO_WU_Y;
constexpr float JACK_CENTER_ABOVE_FEET = 8.0f * PX_TO_WU_Y;  // 16 px tall arcade sprite

// "START!" intro before play begins: the sprite slides in, then holds.
constexpr float START_SPEED = 2.0f;
constexpr float START_SLIDE = 0.55f * START_SPEED;
constexpr float START_HOLD  = 0.65f * START_SPEED;
constexpr float START_BLACK = 1.0f  * START_SPEED;
constexpr float START_TOTAL = START_SLIDE + START_HOLD;

constexpr float CLEAR_TIME  = 3.2f;   // round-clear victory dance

// The current round's entries in the ROM tables (also used by enemy.cpp).
const leveldata::Round&      currentRound(const Game& g);
const leveldata::PlatLayout& currentPlatLayout(const Game& g);
const leveldata::BombLayout& currentBombLayout(const Game& g);
const leveldata::EnemyDef&   currentEnemyDef(const Game& g);

// Which of the five backdrops the current round uses.
int currentScreen(const Game& g);

// Build the platform list for the current round's layout.
void initPlatforms(Game& g);

// Initialise a new round (platforms, bombs, enemies, player).
void startRound(Game& g);

// After Jack loses a life: back to the opening birds, no ball, no freeze.
void restartEnemies(Game& g);

// Keep the round's music looping unless the Power ball's tune holds it.
void keepMusicPlaying(const Game& g);

// Take every bomb Jack touches: score it, light the next one, charge power.
void collectBombs(Game& g);

// Return the next bomb index to light (wraps through remaining bombs).
int nextLitBomb(const Game& g, int orderMin);

// Points awarded for catching n fire bombs (lit bombs) in a round.
int specialBonusFor(int fireBombs);

// All bombs taken: go to the round-clear or special-bonus screen.
void finishRound(Game& g);
