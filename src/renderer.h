#pragma once
#include <SDL3/SDL.h>
#include "types.h"

// Build the bitmap font from the sprite atlas (called once from buildSprites).
void buildFont(SDL_Renderer* ren, SDL_Surface* atlas);

// Primitive drawing helpers.
void setCol(SDL_Renderer* r, Color c, Uint8 a = 255);
void fillR(SDL_Renderer* r, float x, float y, float w, float h);

// Text rendering using the ripped arcade font.
void drawChar(SDL_Renderer* r, char ch, float x, float y, int s, Color c);
int  textWidth(const char* t, int s);
void drawText(SDL_Renderer* r, const char* t, float x, float y, int s, Color c);
void drawTextCentered(SDL_Renderer* r, const char* t, float cx, float y, int s, Color c);

// Switch between world-coordinate drawing (512x448 → play area) and raw
// screen-pixel drawing (224x256, used for HUD strips).
void useWorld(SDL_Renderer* r);
void useScreen(SDL_Renderer* r);

// 3-stage arcade colour cycle (red→yellow→blue→…), used by the GAME OVER sprite.
Color colorCycle3(float t);
