#pragma once
// Full-screen pages and overlays drawn in screen pixels (useScreen).
#include <SDL3/SDL.h>
#include "game/game.h"

void drawTitle(SDL_Renderer* r, const Game& g);         // banner and hints
void drawHiScores(SDL_Renderer* r, const Game& g);      // table, or name entry
void drawOptions(SDL_Renderer* r, const Game& g);       // OPTIONS (clears, draws the HUD)
void drawSpecialBonus(SDL_Renderer* r, const Game& g);
void drawLucky(SDL_Renderer* r, const Game& g);         // "YOU ARE LUCKY / ONE MORE PLAY"
void drawStartIntro(SDL_Renderer* r, const Game& g);    // "START!" over the play area
void drawGameOver(SDL_Renderer* r, const Game& g);      // "GAME OVER" over the play area
