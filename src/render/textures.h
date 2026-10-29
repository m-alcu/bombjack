#pragma once
#include <SDL3/SDL.h>
#include "game/types.h"
#include "render/gfx.h"

// Red level of the enemies' pen 1 (palette 4): the arcade steps it through
// this table, one entry every EYE_PULSE_FRAMES (main ROM 0x0705).
constexpr Uint8 EYE_PULSE[] = {68, 136, 204, 255, 204, 136, 68, 0};
inline Uint8 eyePulse(float t) {
    return EYE_PULSE[(int)(t * ARCADE_HZ / EYE_PULSE_FRAMES) % (int)(sizeof(EYE_PULSE))];
}

struct Sprite { int w = 0, h = 0; SDL_Texture* tex = nullptr; };

extern Sprite g_bonusFrames[4];
extern Sprite g_bonusE[4];
extern Sprite g_bonusS[4];
extern Sprite g_bonusTaken[6];
extern SDL_Texture* g_multBorder[6];
extern SDL_Texture* g_multSymbol[6];
extern Sprite g_bombFrames[7];
extern SDL_Texture* g_ballPen[8];   // Power ball, per ROM pen 1-7 (white masks)
extern Sprite g_explFrames[3];
extern Sprite g_pickCoinFrames[4];
extern Sprite g_startBg[2];
extern Sprite g_startText[2];
// "YOU ARE LUCKY" frame: pre-baked phases whose bright yellow band travels
// outward from the centre; drawLucky cycles them over time.
constexpr int LUCKY_PHASES = 8;
extern Sprite g_luckyFrame[LUCKY_PHASES];
extern Sprite g_nameFrame;   // white mask of the high-score name-edit frame

// Jack living / death / win frames.
enum JackFrame {
    JF_IDLE,
    JF_WALK_R0, JF_WALK_R1, JF_WALK_R2, JF_WALK_R3,
    JF_WALK_L0, JF_WALK_L1, JF_WALK_L2, JF_WALK_L3,
    JF_FLY, JF_FLY_R, JF_FLY_L,
    JF_FALL, JF_FALL_R, JF_FALL_L,
    JF_COUNT
};
struct JackVarFrame { int w = 0, h = 0; SDL_Texture* tex = nullptr; };
enum JackWinPose { JW_NORMAL, JW_LEFT, JW_RIGHT, JW_UP };

extern SDL_Texture*  g_jackTex[JF_COUNT];
extern SDL_Texture*  g_jackPen[JF_COUNT][8];   // per ROM pen 1-7 (white masks)
extern JackVarFrame  g_jackDance[3];
extern JackVarFrame  g_jackPlf[4];
extern JackVarFrame  g_jackDead[4];
extern JackVarFrame  g_jackWin[4];
extern SDL_Texture*  g_gameOverTex;

// Enemy frames indexed by arcade sprite code (atlas::ENEMY_CELLS), each with
// its pen 1 mask (the red the arcade pulses, see eyePulse).
extern SDL_Texture* g_enemyTex[256];
extern SDL_Texture* g_enemyEye[256];

// Frozen enemy coin (pen 5 at rest) and its pen 5 / pen 6 glint masks.
extern Sprite       g_frozen;
extern SDL_Texture* g_frozenGlint[2];

// Spawn-flash frames (baked as white masks, tinted at draw time).
extern SDL_Texture* g_initEnemyTex[4];

// Decode the full sprite atlas from the embedded PNG (caller must stbi_image_free).
unsigned char* loadAtlas(int* w, int* h);

// Load all sprite textures from the embedded atlas.
void buildTextures(SDL_Renderer* ren);

// Free all sprite textures (call before SDL_DestroyRenderer).
void destroyTextures();

void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                   bool flip, Uint8 cr, Uint8 cg, Uint8 cb);

// Convenience: tint with a Color (no flip). Sets the colour-mod, draws, resets.
inline void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                          Color c) {
    drawTexTinted(r, tex, dst, false, c.r, c.g, c.b);
}
