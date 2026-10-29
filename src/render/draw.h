#pragma once
#include <SDL3/SDL.h>
#include "game/types.h"
#include "game/game.h"

void drawBomb(SDL_Renderer* r, const Bomb& b, bool lit, float t);

// Populate the player's owned AnimSprite (call once, after buildTextures).
void buildJackSprite(AnimSprite& jack);

// While frozen (Jack holds the Power ball) he is drawn in palette 14, pal14.
void drawPlayer(SDL_Renderer* r, const Player& p, const AnimSprite& jack,
                float t, bool dying, int deathPhase, int deathFrame,
                bool frozen, const Color pal14[8]);

void drawJackVictory(SDL_Renderer* r, const Player& p, float clearTimer);

void drawEnemy(SDL_Renderer* r, const Enemy& e, float t, bool frozen,
               float freezeTimer);

// pal14: palette 14 pens 1-7 (index 0 unused), shared with the HUD.
void drawPowerOrb(SDL_Renderer* r, float x, float y, const Color pal14[8]);

// imgStyle: 0=CLASSIC, 1-7=grid sets, 8=NO background.
void drawBackground(SDL_Renderer* r, int screen, int imgStyle);

void drawHud(SDL_Renderer* r, const Game& g);

void drawStartIntro(SDL_Renderer* r, const Game& g);

void drawSpecialBonus(SDL_Renderer* r, const Game& g);

void drawLucky(SDL_Renderer* r, const Game& g);

void drawHiScores(SDL_Renderer* r, const Game& g);

void drawOptions(SDL_Renderer* r, const Game& g);

void render(SDL_Renderer* r, const Game& g);
