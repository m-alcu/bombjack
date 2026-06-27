#pragma once
#include <SDL3/SDL.h>
#include "types.h"

// Background textures (defined in background.cpp).
extern SDL_Texture* g_bgTex;
extern int          g_bgCount;
extern SDL_Texture* g_gridTex[GRID_BG_COUNT];
extern SDL_Texture* g_bannerTex[BANNER_PHASES];
extern SDL_Texture* g_liveTex;

// Decode and build all background and banner textures.
void buildBackground(SDL_Renderer* ren);

// Draw the bevelled 8-layer playfield border in screen pixels.
void drawPlayfieldBorder(SDL_Renderer* r, int screen);

// Draw a platform with shaded top/bottom faces.
void drawPlatformShaded(SDL_Renderer* r, const SDL_FRect& pl, int screen,
                        const std::vector<SDL_FRect>& plats);

// Draw the rounded tab a platform pokes over the side frame.
void drawPlatformFrameTab(SDL_Renderer* r, const SDL_FRect& pl, int screen);
