#pragma once
// Draw one frame of the current state.
#include <SDL3/SDL.h>
#include "game/game.h"

void render(SDL_Renderer* r, const Game& g);
