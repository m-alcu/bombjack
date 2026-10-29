#pragma once
#include "data/levels_data.h"
#include "game/types.h"

// Level data accessors (also used by enemy.cpp).
const leveldata::Round&      currentRound(const Game& g);
const leveldata::PlatLayout& currentPlatLayout(const Game& g);
const leveldata::BombLayout& currentBombLayout(const Game& g);
const leveldata::EnemyDef&   levelDef(const Game& g);

// Which of the five backdrops the current level uses.
int currentScreen(const Game& g);

// Build platform list for the current level layout.
void initPlatforms(Game& g);

// Populate the bomb list from the current level layout.
void spawnBombs(Game& g);

// Spawn the initial set of bird enemies.
void spawnEnemies(Game& g);

// Tear down all enemies and restart from the opening bird set.
void restartEnemies(Game& g);

// Place Jack at his starting position.
void resetPlayer(Game& g, bool invuln);

// Drop a bonus coin (B/E/S) onto a random platform.
void spawnBonus(Game& g, int kind);

// Initialise a new round (platforms, bombs, enemies, player).
void startRound(Game& g);

// Adjust the selected OPTIONS value by d.
void optionAdjust(Game& g, int d);

// Start a new game from the OPTIONS selections.
void startGame(Game& g);

// Return the next bomb index to light (wraps through remaining bombs).
int nextLitBomb(const Game& g, int orderMin);

// Points awarded for catching n fire bombs in a row.
int specialBonusFor(int catched);

// Trigger the level-complete sequence.
void finishLevel(Game& g);

// Per-tick game simulation (PLAYING state only).
void updatePlaying(Game& g, const Input& in, float dt);

// Top-level update dispatched to the current state.
void update(Game& g, const Input& in, float dt);

// High-score table: load from disk (seeding defaults on first run), persist,
// test whether the current score qualifies, and insert a named entry.
void loadHiScores(Game& g);
void saveHiScores(const Game& g);
bool hiScoreQualifies(const Game& g);
void insertHiScore(Game& g, const char* name);

// Confirm the typed name on the NAMEENTRY screen: insert, save, return to TITLE.
void confirmNameEntry(Game& g);

// Attract mode: go to the TITLE state at the given attract step (0 = table,
// 2 = banner), start a demo of round 1 or 2, or abandon a running demo.
void enterAttract(Game& g, int step);
void startDemo(Game& g, int round);
void stopDemo(Game& g);
