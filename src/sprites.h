#pragma once
#include <SDL3/SDL.h>
#include "types.h"
#include "renderer.h"

// Pulsing eye overlay intensity (bird/mummy): bright → black → bright.
constexpr Uint8 EYE_PULSE[] = {255, 222, 189, 156, 115, 82, 49, 0,
                               49,  82, 115, 156, 189, 222};
inline Uint8 eyePulse(float t) {
    return EYE_PULSE[(int)(t / BIRD_PULSE_STEP) % (int)(sizeof(EYE_PULSE))];
}

// Sprite IDs for the transformed chasers and legacy entries (bird/mummy use
// dedicated per-frame texture arrays below).
enum SpriteId {
    SP_BOMB, SP_ENEMY1, SP_ENEMY2,
    SP_MUMMY, SP_MUMMY_WALK, SP_MUMMY_FALL,
    SP_SPHERE1, SP_SPHERE2, SP_ORB1, SP_ORB2, SP_HORN1, SP_HORN2,
    SP_CLUB1, SP_CLUB2, SP_UFO1, SP_UFO2,
    SP_COUNT
};

struct Sprite { int w = 0, h = 0; SDL_Texture* tex = nullptr; };

extern Sprite g_sprites[SP_COUNT];
extern Sprite g_bonusFrames[4];
extern Sprite g_bonusE[4];
extern Sprite g_bonusS[4];
extern Sprite g_bonusTaken[6];
extern SDL_Texture* g_multTex[6];
extern Sprite g_coinFrames[7];
extern Sprite g_bombFrames[7];
extern Sprite g_orbCycle[7][4];
extern Sprite g_explFrames[3];
extern Sprite g_pickCoinFrames[4];
extern Sprite g_startBg[2];
extern Sprite g_startText[2];

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
extern SDL_Texture*  g_jackPhase[JF_COUNT][4];
extern JackVarFrame  g_jackDance[3];
extern JackVarFrame  g_jackPlf[4];
extern JackVarFrame  g_jackDead[4];
extern JackVarFrame  g_jackWin[4];
extern SDL_Texture*  g_gameOverTex;

// Bird wing-flap frames (left / right / vertical, 3 each).
enum BirdFrame {
    BF_LEFT0, BF_LEFT1, BF_LEFT2,
    BF_RIGHT0, BF_RIGHT1, BF_RIGHT2,
    BF_VERT0, BF_VERT1, BF_VERT2,
    BF_COUNT
};
extern SDL_Texture* g_birdTex[BF_COUNT];
extern SDL_Texture* g_birdEye[BF_COUNT];

// Mummy walk/idle/fall frames.
enum MummyFrame {
    MF_IDLE,
    MF_WALK_R0, MF_WALK_R1, MF_WALK_R2,
    MF_WALK_L0, MF_WALK_L1, MF_WALK_L2,
    MF_FALL,
    MF_COUNT
};
extern SDL_Texture* g_mummyTex[MF_COUNT];
extern SDL_Texture* g_mummyEye[MF_COUNT];

// Spawn-flash frames (baked as white masks, tinted at draw time).
extern SDL_Texture* g_initEnemyTex[4];

// Decode the full sprite atlas from the embedded PNG (caller must stbi_image_free).
unsigned char* loadAtlas(int* w, int* h);

// Load all sprite textures from the embedded atlas.
void buildSprites(SDL_Renderer* ren);

// Free all sprite textures (call before SDL_DestroyRenderer).
void destroySprites();

void drawSprite(SDL_Renderer* r, SpriteId id, float x, float y,
                float w, float h, bool flip);
void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                   bool flip, Uint8 cr, Uint8 cg, Uint8 cb);
