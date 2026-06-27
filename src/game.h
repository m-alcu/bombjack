#pragma once
#include "levels_data.h"
#include "types.h"

// Level data accessors (also used by enemy.cpp).
const leveldata::Layout&   currentLayout(const Game& g);
const leveldata::LevelDef& levelDef(const Game& g);

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
