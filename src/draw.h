#pragma once
#include <SDL3/SDL.h>
#include "types.h"
#include "game.h"

void drawBomb(SDL_Renderer* r, const Bomb& b, bool lit, float t);

void drawPlayer(SDL_Renderer* r, const Player& p, float t, bool dying,
                int deathPhase, int deathFrame, bool frozen, Color freezeColor);

void drawJackVictory(SDL_Renderer* r, const Player& p, float clearTimer);

void drawEnemy(SDL_Renderer* r, const Enemy& e, float t, bool frozen,
               float freezeTimer);

void drawPowerOrb(SDL_Renderer* r, float x, float y, float t, int family);

// imgStyle: 0=CLASSIC, 1-7=grid sets, 8=NO background.
void drawBackground(SDL_Renderer* r, int screen, int imgStyle);

void drawHud(SDL_Renderer* r, const Game& g);

void drawStartIntro(SDL_Renderer* r, const Game& g);

void drawSpecialBonus(SDL_Renderer* r, const Game& g);

void drawOptions(SDL_Renderer* r, const Game& g);

void render(SDL_Renderer* r, const Game& g);
