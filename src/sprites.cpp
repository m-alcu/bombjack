#include <SDL3/SDL.h>   // must precede stb_image.h (provides Uint8/Uint16)
#include <cmath>        // stb_image implementation uses pow()
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wtype-limits"
#include "stb_image.h"
#pragma GCC diagnostic pop

#include "sprites_full_png.h"
#include "sprites.h"
#include <algorithm>
#include <cstring>
#include <vector>

// Global sprite storage (defined here, declared extern in sprites.h).
Sprite g_sprites[SP_COUNT];
Sprite g_bonusFrames[4];
Sprite g_bonusE[4];
Sprite g_bonusS[4];
Sprite g_bonusTaken[6];
SDL_Texture* g_multTex[6] = {};
Sprite g_coinFrames[7];
Sprite g_bombFrames[7];
Sprite g_orbCycle[7][4];
Sprite g_explFrames[3];
Sprite g_pickCoinFrames[4];
Sprite g_startBg[2];
Sprite g_startText[2];

SDL_Texture* g_jackTex[JF_COUNT]      = {};
SDL_Texture* g_jackPhase[JF_COUNT][4] = {};
JackVarFrame g_jackDance[3] = {};
JackVarFrame g_jackPlf[4]   = {};
JackVarFrame g_jackDead[4]  = {};
JackVarFrame g_jackWin[4]   = {};
SDL_Texture* g_gameOverTex  = nullptr;

SDL_Texture* g_birdTex[BF_COUNT] = {};
SDL_Texture* g_birdEye[BF_COUNT] = {};

SDL_Texture* g_mummyTex[MF_COUNT] = {};
SDL_Texture* g_mummyEye[MF_COUNT] = {};

SDL_Texture* g_initEnemyTex[4] = {};

// ---------------------------------------------------------------------------

static SDL_Texture* texFromSurface(SDL_Renderer* ren, SDL_Surface* s) {
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    return t;
}

static SDL_Texture* makeEyeMask(SDL_Renderer* ren, SDL_Surface* s) {
    SDL_Surface* m = SDL_CreateSurface(s->w, s->h, SDL_PIXELFORMAT_RGBA32);
    bool any = false;
    for (int y = 0; y < s->h; ++y)
        for (int x = 0; x < s->w; ++x) {
            const Uint8* sp = (const Uint8*)s->pixels + y * s->pitch + x * 4;
            Uint8* mp = (Uint8*)m->pixels + y * m->pitch + x * 4;
            bool eye = sp[0] == 255 && sp[1] == 0 && sp[2] == 0 && sp[3] == 255;
            mp[0] = mp[1] = mp[2] = 255;
            mp[3] = eye ? 255 : 0;
            any |= eye;
        }
    SDL_Texture* t = any ? texFromSurface(ren, m) : nullptr;
    SDL_DestroySurface(m);
    return t;
}

unsigned char* loadAtlas(int* w, int* h) {
    int c = 0;
    return stbi_load_from_memory(sprites_full_png, (int)sprites_full_png_len, w, h, &c, 4);
}

void buildSprites(SDL_Renderer* ren) {
    int sw = 0, sh = 0;
    stbi_uc* spx = loadAtlas(&sw, &sh);
    if (!spx) { std::fprintf(stderr, "full sprites atlas decode failed\n"); return; }

    SDL_Surface* atlas = SDL_CreateSurfaceFrom(sw, sh, SDL_PIXELFORMAT_RGBA32, spx, sw * 4);

    auto cropJackVar = [&](int x, int y, int w, int h, JackVarFrame& out) {
        SDL_Surface* f = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{x, y, w, h};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        out = {w, h, texFromSurface(ren, f)};
        SDL_DestroySurface(f);
    };

    // Jack living frames (JF_COUNT frames at 16x15).
    static const int jackRect[JF_COUNT][2] = {
        {4, 4},
        {24, 4}, {44, 4}, {64, 4}, {84, 4},
        {104, 4}, {124, 4}, {144, 4}, {164, 4},
        {224, 4}, {264, 4}, {304, 4},
        {204, 4}, {284, 4}, {324, 4},
    };
    auto lum = [](const Uint8* p) { return 0.30f*p[0] + 0.59f*p[1] + 0.11f*p[2]; };
    for (int i = 0; i < JF_COUNT; ++i) {
        SDL_Surface* f = SDL_CreateSurface(JACK_FW, JACK_FH, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{jackRect[i][0], jackRect[i][1], JACK_FW, JACK_FH};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        g_jackTex[i] = texFromSurface(ren, f);
        // Build 4 white luminance-banded frames for the freeze colour cycle.
        float lo = 1e9f, hi = -1e9f;
        for (int y = 0; y < JACK_FH; ++y)
            for (int x = 0; x < JACK_FW; ++x) {
                const Uint8* sp = (const Uint8*)f->pixels + y * f->pitch + x * 4;
                if (sp[3] > 0) { float L = lum(sp); lo = std::min(lo, L); hi = std::max(hi, L); }
            }
        float span = std::max(1.0f, hi - lo);
        const float bf[4] = {0.45f, 0.65f, 0.83f, 1.0f};
        for (int ph = 0; ph < 4; ++ph) {
            SDL_Surface* sv = SDL_CreateSurface(JACK_FW, JACK_FH, SDL_PIXELFORMAT_RGBA32);
            for (int y = 0; y < JACK_FH; ++y)
                for (int x = 0; x < JACK_FW; ++x) {
                    const Uint8* sp = (const Uint8*)f->pixels + y * f->pitch + x * 4;
                    Uint8* dp = (Uint8*)sv->pixels + y * sv->pitch + x * 4;
                    if (sp[3] == 0) { dp[0]=dp[1]=dp[2]=dp[3]=0; continue; }
                    int band = std::min(3, (int)((lum(sp) - lo) / span * 4.0f));
                    Uint8 v = (Uint8)(bf[(band + ph) & 3] * 255.0f);
                    dp[0] = dp[1] = dp[2] = v; dp[3] = 255;
                }
            g_jackPhase[i][ph] = texFromSurface(ren, sv);
            SDL_DestroySurface(sv);
        }
        SDL_DestroySurface(f);
    }

    // Death / win variable-size frames.
    static const int danceRect[3][4] = {
        {249, 31, 16, 17}, {269, 31, 16, 17}, {64, 34, 15, 14}
    };
    static const int plfRect[4][4] = {
        {44, 32, 15, 15}, {64, 32, 15, 16}, {84, 32, 15, 16}, {124, 32, 15, 15}
    };
    static const int deadRect[4][4] = {
        {145, 27, 16, 18}, {166, 29, 13, 16}, {184, 24, 17, 24}, {205, 27, 23, 21}
    };
    static const int winRect[4][2] = {{4, 32}, {24, 32}, {286, 32}, {44, 32}};
    for (int i = 0; i < 4; ++i)
        cropJackVar(winRect[i][0], winRect[i][1], 16, 16, g_jackWin[i]);
    for (int i = 0; i < 3; ++i)
        cropJackVar(danceRect[i][0], danceRect[i][1], danceRect[i][2], danceRect[i][3],
                    g_jackDance[i]);
    for (int i = 0; i < 4; ++i) {
        cropJackVar(plfRect[i][0], plfRect[i][1], plfRect[i][2], plfRect[i][3], g_jackPlf[i]);
        cropJackVar(deadRect[i][0], deadRect[i][1], deadRect[i][2], deadRect[i][3], g_jackDead[i]);
    }

    // Font glyphs (white 7x7, two rows in the atlas).
    buildFont(ren, atlas);

    // Boxed multiplier indicators x / 1..5.
    static const int multX[6] = {8, 24, 44, 64, 84, 104};
    for (int i = 0; i < 6; ++i) {
        SDL_Surface* f = SDL_CreateSurface(12, 12, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{multX[i], 194, 12, 12};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        g_multTex[i] = texFromSurface(ren, f);
        SDL_DestroySurface(f);
    }

    // GAME OVER sprite baked as white mask.
    {
        SDL_Surface* f = SDL_CreateSurface(GAMEOVER_W, GAMEOVER_H, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{2, 155, GAMEOVER_W, GAMEOVER_H};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        for (int y = 0; y < f->h; ++y)
            for (int x = 0; x < f->w; ++x) {
                Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                if (p[3] > 0) { p[0] = p[1] = p[2] = 255; }
            }
        g_gameOverTex = texFromSurface(ren, f);
        SDL_DestroySurface(f);
    }

    // Frozen-enemy coin spin (7 frames 14x14).
    for (int i = 0; i < 7; ++i) {
        SDL_Surface* f = SDL_CreateSurface(14, 14, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{307 + i * 14, 115, 14, 14};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        g_coinFrames[i] = {14, 14, texFromSurface(ren, f)};
        SDL_DestroySurface(f);
    }

    auto cropSprite = [&](int x, int y, int w, int h) -> Sprite {
        SDL_Surface* f = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{x, y, w, h};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        Sprite sp{w, h, texFromSurface(ren, f)};
        SDL_DestroySurface(f);
        return sp;
    };

    // Bonus B / E / S spin (4 frames each).
    static const int bonusBX[4] = {78, 98, 117, 130};
    static const int bonusEX[4] = {150, 170, 189, 202};
    static const int bonusSX[4] = {222, 242, 261, 274};
    static const int bonusW[4]  = {13, 12, 7, 12};
    for (int i = 0; i < 4; ++i) {
        g_bonusFrames[i] = cropSprite(bonusBX[i], 114, bonusW[i], 13);
        g_bonusE[i]      = cropSprite(bonusEX[i], 114, bonusW[i], 13);
        g_bonusS[i]      = cropSprite(bonusSX[i], 114, bonusW[i], 13);
    }

    // Transformed chasers (2 animation frames each).
    g_sprites[SP_SPHERE1] = cropSprite(5,   73, 14, 14);
    g_sprites[SP_SPHERE2] = cropSprite(85,  73, 14, 14);
    g_sprites[SP_ORB1]    = cropSprite(185, 74, 14, 13);
    g_sprites[SP_ORB2]    = cropSprite(245, 74, 14, 13);
    g_sprites[SP_CLUB1]   = cropSprite(305, 92, 15, 16);
    g_sprites[SP_CLUB2]   = cropSprite(345, 92, 15, 16);
    g_sprites[SP_UFO1]    = cropSprite(4,   98, 16, 10);
    g_sprites[SP_UFO2]    = cropSprite(64,  98, 16, 10);

    // Horn frames centred into a 16x16 cell.
    auto cropCentered = [&](int x, int y, int w, int h) -> Sprite {
        SDL_Surface* f = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{x, y, w, h};
        SDL_Rect dst{(16 - w) / 2, (16 - h) / 2, w, h};
        SDL_BlitSurface(atlas, &src, f, &dst);
        Sprite sp{16, 16, texFromSurface(ren, f)};
        SDL_DestroySurface(f);
        return sp;
    };
    g_sprites[SP_HORN1] = cropCentered(280, 54, 15, 14);
    g_sprites[SP_HORN2] = cropCentered(318, 52, 11, 16);

    // Bonus collect flash (6 frames 32x32).
    for (int i = 0; i < 6; ++i) g_bonusTaken[i] = cropSprite(266 + i * 33, 327, 32, 32);

    // Bomb frames: 0=resting, 1-6=lit fuse.
    for (int i = 0; i < 7; ++i) g_bombFrames[i] = cropSprite(46 + i * 20, 136, 12, 16);

    // Bomb-clear explosion (3 growing frames).
    static const int er[3][4] = {{24, 120, 8, 8}, {38, 114, 12, 12}, {56, 112, 16, 16}};
    for (int i = 0; i < 3; ++i) g_explFrames[i] = cropSprite(er[i][0], er[i][1], er[i][2], er[i][3]);

    // Bird flap frames (3 headings × 3 frames, centred into 16x16 cells).
    static const int birdRect[BF_COUNT][4] = {
        {140, 53, 16, 16}, {160, 54, 16, 13}, {180, 55, 16, 11},
        {352, 53, 16, 16}, {373, 54, 16, 13}, {393, 54, 16, 11},
        {201, 53, 15, 15}, {221, 55, 15, 12}, {241, 55, 15, 11},
    };
    for (int i = 0; i < BF_COUNT; ++i) {
        const int w = birdRect[i][2], h = birdRect[i][3];
        SDL_Surface* f = SDL_CreateSurface(BIRD_FW, BIRD_FH, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{birdRect[i][0], birdRect[i][1], w, h};
        SDL_Rect dst{(BIRD_FW - w) / 2, (BIRD_FH - h) / 2, w, h};
        SDL_BlitSurface(atlas, &src, f, &dst);
        g_birdTex[i] = texFromSurface(ren, f);
        g_birdEye[i] = makeEyeMask(ren, f);
        SDL_DestroySurface(f);
    }

    // Mummy frames (centred into 16x16 cells).
    static const int mummyRect[MF_COUNT][4] = {
        {6,  53, 12, 15},
        {45, 53, 11, 15}, {61, 53, 11, 15}, {76, 53, 13, 15},
        {92, 53, 11, 15}, {108,53, 11, 15}, {123,53, 13, 15},
        {25, 53, 14, 16},
    };
    for (int i = 0; i < MF_COUNT; ++i) {
        const int w = mummyRect[i][2], h = mummyRect[i][3];
        SDL_Surface* f = SDL_CreateSurface(MUMMY_FW, MUMMY_FH, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{mummyRect[i][0], mummyRect[i][1], w, h};
        SDL_Rect dst{(MUMMY_FW - w) / 2, (MUMMY_FH - h) / 2, w, h};
        SDL_BlitSurface(atlas, &src, f, &dst);
        g_mummyTex[i] = texFromSurface(ren, f);
        g_mummyEye[i] = makeEyeMask(ren, f);
        SDL_DestroySurface(f);
    }

    // Power orb: prebake per colour family × 4 cycle phases.
    {
        const int PX = 292, PY = 116, ow = 12, oh = 12;
        std::vector<Uint8> opx((size_t)ow * oh * 4);
        for (int y = 0; y < oh; ++y)
            std::memcpy(&opx[(size_t)y * ow * 4],
                        (Uint8*)atlas->pixels + (PY + y) * atlas->pitch + PX * 4,
                        (size_t)ow * 4);
        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < ow * oh; ++i)
            if (opx[i*4+3] > 0) { float L = lum(&opx[i*4]); lo = std::min(lo, L); hi = std::max(hi, L); }
        float span = std::max(1.0f, hi - lo);
        std::vector<int> band((size_t)ow * oh, -1);
        for (int i = 0; i < ow * oh; ++i)
            if (opx[i*4+3] > 0) band[i] = std::min(3, (int)((lum(&opx[i*4]) - lo) / span * 4.0f));
        const float bf[4] = {0.50f, 0.68f, 0.84f, 1.0f};
        for (int fam = 0; fam < 7; ++fam) {
            Color bc = POWER_COLORS[fam];
            for (int ph = 0; ph < 4; ++ph) {
                SDL_Surface* s = SDL_CreateSurface(ow, oh, SDL_PIXELFORMAT_RGBA32);
                for (int i = 0; i < ow * oh; ++i) {
                    Uint8* dp = (Uint8*)s->pixels + i * 4;
                    if (band[i] < 0) { dp[0]=dp[1]=dp[2]=dp[3]=0; continue; }
                    float f = bf[(band[i] + ph) & 3];
                    dp[0]=(Uint8)(bc.r*f); dp[1]=(Uint8)(bc.g*f);
                    dp[2]=(Uint8)(bc.b*f); dp[3]=255;
                }
                g_orbCycle[fam][ph] = {ow, oh, texFromSurface(ren, s)};
                SDL_DestroySurface(s);
            }
        }
    }

    // Spawn flash (4 frames, white masks).
    static const int initRect[4][4] = {
        {40, 158, 18, 29}, {65, 157, 29, 31}, {101, 158, 29, 29}, {138, 159, 28, 28}
    };
    for (int i = 0; i < 4; ++i) {
        const int w = initRect[i][2], h = initRect[i][3];
        SDL_Surface* f = SDL_CreateSurface(INIT_FW, INIT_FH, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{initRect[i][0], initRect[i][1], w, h};
        SDL_Rect dst{(INIT_FW - w) / 2, (INIT_FH - h) / 2, w, h};
        SDL_BlitSurface(atlas, &src, f, &dst);
        for (int y = 0; y < f->h; ++y)
            for (int x = 0; x < f->w; ++x) {
                Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                Uint8 v = (Uint8)((p[0] + p[2]) / 2);
                p[0] = p[1] = p[2] = v;
            }
        g_initEnemyTex[i] = texFromSurface(ren, f);
        SDL_DestroySurface(f);
    }

    // "START!" halves: each split into background mask and text layer.
    auto splitStartHalf = [&](int x0, Sprite& bg, Sprite& text) {
        const int sw = 56, sh = 12;
        SDL_Surface* b = SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_RGBA32);
        SDL_Surface* t = SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_RGBA32);
        for (int y = 0; y < sh; ++y)
            for (int x = 0; x < sw; ++x) {
                const Uint8* sp = (const Uint8*)atlas->pixels +
                                  (260 + y) * atlas->pitch + (x0 + x) * 4;
                Uint8* bp = (Uint8*)b->pixels + y * b->pitch + x * 4;
                Uint8* tp = (Uint8*)t->pixels + y * t->pitch + x * 4;
                bool opaque = sp[3] > 0;
                bool yellow = opaque && sp[0] > 150 && sp[1] > 150 && sp[2] < 120;
                bool box    = opaque && !yellow;
                bp[0]=bp[1]=bp[2]=255; bp[3]=box?255:0;
                tp[0]=sp[0]; tp[1]=sp[1]; tp[2]=sp[2]; tp[3]=yellow?255:0;
            }
        bg   = {sw, sh, texFromSurface(ren, b)};
        text = {sw, sh, texFromSurface(ren, t)};
        SDL_DestroySurface(b);
        SDL_DestroySurface(t);
    };
    splitStartHalf(240, g_startBg[0], g_startText[0]);
    splitStartHalf(300, g_startBg[1], g_startText[1]);

    // Coin-pickup sparkle (4 frames, white masks for yellow tint).
    static const int pickRect[4][4] = {
        {175, 159, 26, 26}, {209, 159, 30, 28}, {244, 159, 32, 29}, {282, 156, 32, 32}
    };
    for (int i = 0; i < 4; ++i) {
        const int w = pickRect[i][2], h = pickRect[i][3];
        SDL_Surface* f = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{pickRect[i][0], pickRect[i][1], w, h};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                if (p[3]) { p[0]=p[1]=p[2]=255; }
            }
        g_pickCoinFrames[i] = {w, h, texFromSurface(ren, f)};
        SDL_DestroySurface(f);
    }

    SDL_DestroySurface(atlas);
    stbi_image_free(spx);
}

void drawSprite(SDL_Renderer* r, SpriteId id, float x, float y,
                float w, float h, bool flip) {
    SDL_FRect dst{x, y, w, h};
    SDL_RenderTextureRotated(r, g_sprites[id].tex, nullptr, &dst, 0.0, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                   bool flip, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetTextureColorMod(tex, cr, cg, cb);
    SDL_RenderTextureRotated(r, tex, nullptr, &dst, 0.0, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(tex, 255, 255, 255);
}
