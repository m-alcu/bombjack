#pragma once
// Keyboard → game. Each state handles its own keys.
#include <SDL3/SDL.h>

struct Game;
struct Input;

// Act on a key press. Sets jumpEdge when the jump button went down in play.
// Returns false when the player asked to quit.
bool handleKeyDown(Game& g, SDL_Scancode sc, bool& jumpEdge);

// The keys held right now, as Jack's controls (jumpPressed left clear).
Input readHeldKeys();
