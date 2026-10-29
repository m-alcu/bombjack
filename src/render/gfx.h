#pragma once
// Drawing primitives: the arcade font, filled rectangles, tinted textures,
// and the world/screen coordinate switch.
#include <SDL3/SDL.h>
#include "core/color.h"
#include "core/constants.h"
#include "render/sprite_atlas.h"

// Horizontal stretch correction: multiplied into sprite widths so they appear
// square on the final 768x1024 window (1.5 vs 2.0 win-px per world unit).
constexpr float SPRITE_AR = 4.0f / 3.0f;

constexpr int FONT_ADV = 8;   // font advance per character

// Build the bitmap font from the sprite atlas (called once from buildTextures).
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

// Draw a texture (a white mask, usually) tinted by colour-mod, optionally
// mirrored; the convenience form takes a Color and never flips.
void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                   bool flip, Uint8 cr, Uint8 cg, Uint8 cb);
inline void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                          Color c) {
    drawTexTinted(r, tex, dst, false, c.r, c.g, c.b);
}
