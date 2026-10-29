#pragma once
// The play area as the simulation left it.
#include <SDL3/SDL.h>
#include "game/game.h"

// Build Jack's living animations (call once, after buildTextures).
void buildJackSprite();

// Draw the play area in world coordinates (useWorld): backdrop, platforms,
// bombs, effects, Power ball, bonus coin, enemies, Jack, the frame and the
// score popups.
void drawPlayfield(SDL_Renderer* r, const Game& g);
