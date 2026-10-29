#pragma once
// The top and bottom HUD strips: score, multiplier box and power bar, lives,
// round and high score.
#include <SDL3/SDL.h>
#include "game/game.h"

void drawHud(SDL_Renderer* r, const Game& g);
