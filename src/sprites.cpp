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
#include "sprite_atlas.h"
#include <algorithm>
#include <cstring>
#include <vector>

// Global sprite storage (defined here, declared extern in sprites.h).
Sprite g_sprites[SP_COUNT];
Sprite g_bonusFrames[4];
Sprite g_bonusE[4];
Sprite g_bonusS[4];
Sprite g_bonusTaken[6];
SDL_Texture* g_multBorder[6] = {};
SDL_Texture* g_multSymbol[6] = {};
Sprite g_barRight[BAR_STEPS];
Sprite g_barLeft[BAR_STEPS];
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

    // Jack living frames (JF_COUNT frames at 16x15). See atlas::JACK.
    auto lum = [](const Uint8* p) { return 0.30f*p[0] + 0.59f*p[1] + 0.11f*p[2]; };
    for (int i = 0; i < JF_COUNT; ++i) {
        SDL_Surface* f = SDL_CreateSurface(SIZE_16PX, JACK_FH, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{atlas::JACK[i].x, atlas::JACK[i].y, SIZE_16PX, JACK_FH};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        g_jackTex[i] = texFromSurface(ren, f);
        // Build 4 white luminance-banded frames for the freeze colour cycle.
        float lo = 1e9f, hi = -1e9f;
        for (int y = 0; y < JACK_FH; ++y)
            for (int x = 0; x < SIZE_16PX; ++x) {
                const Uint8* sp = (const Uint8*)f->pixels + y * f->pitch + x * 4;
                if (sp[3] > 0) { float L = lum(sp); lo = std::min(lo, L); hi = std::max(hi, L); }
            }
        float span = std::max(1.0f, hi - lo);
        const float bf[4] = {0.45f, 0.65f, 0.83f, 1.0f};
        for (int ph = 0; ph < 4; ++ph) {
            SDL_Surface* sv = SDL_CreateSurface(SIZE_16PX, JACK_FH, SDL_PIXELFORMAT_RGBA32);
            for (int y = 0; y < JACK_FH; ++y)
                for (int x = 0; x < SIZE_16PX; ++x) {
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

    // Death / win variable-size frames (atlas::JACK_WIN / _DANCE / _PLF / _DEAD).
    auto cropRect = [&](const AtlasRect& a, JackVarFrame& out) {
        cropJackVar(a.x, a.y, a.w, a.h, out);
    };
    for (int i = 0; i < 4; ++i) cropRect(atlas::JACK_WIN[i],  g_jackWin[i]);
    for (int i = 0; i < 3; ++i) cropRect(atlas::JACK_DANCE[i], g_jackDance[i]);
    for (int i = 0; i < 4; ++i) {
        cropRect(atlas::JACK_PLF[i],  g_jackPlf[i]);
        cropRect(atlas::JACK_DEAD[i], g_jackDead[i]);
    }

    // Font glyphs (white 7x7, two rows in the atlas).
    buildFont(ren, atlas);

    // Boxed multiplier indicators x / 1..5 (full 16x16 cells).
    // Split into two white-mask layers so border and symbol can be tinted independently.
    for (int i = 0; i < 6; ++i) {
        SDL_Surface* full = SDL_CreateSurface(SIZE_16PX, SIZE_16PX, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{atlas::MULT.x0 + i * atlas::MULT.dx, atlas::MULT.y,
                     atlas::MULT.w, atlas::MULT.h};
        SDL_BlitSurface(atlas, &src, full, nullptr);

        SDL_Surface* brd = SDL_CreateSurface(SIZE_16PX, SIZE_16PX, SDL_PIXELFORMAT_RGBA32);
        SDL_Surface* sym = SDL_CreateSurface(SIZE_16PX, SIZE_16PX, SDL_PIXELFORMAT_RGBA32);
        for (int y = 0; y < SIZE_16PX; ++y)
            for (int x = 0; x < SIZE_16PX; ++x) {
                const Uint8* s = (Uint8*)full->pixels + y * full->pitch + x * 4;
                Uint8* b = (Uint8*)brd->pixels  + y * brd->pitch  + x * 4;
                Uint8* m = (Uint8*)sym->pixels   + y * sym->pitch  + x * 4;
                bool isGreen = s[3] > 0 && s[2] == 0 && s[1] == 255; // (66,255,0)
                bool isWhite = s[3] > 0 && s[0] == 255 && s[2] == 255;
                bool isBlack = s[3] > 0 && !(s[0] | s[1] | s[2]);
                // Border layer: stored at lum=128 to match the innermost bar brightness
                b[0] = b[1] = b[2] = 200; b[3] = isGreen ? 255 : 0;
                // Symbol layer: black box + white symbol, border area transparent
                if (isBlack)       { m[0]=m[1]=m[2]=0;   m[3]=255; }
                else if (isWhite)  { m[0]=m[1]=m[2]=255; m[3]=255; }
                else               { m[0]=m[1]=m[2]=m[3]=0; }
            }
        g_multBorder[i] = texFromSurface(ren, brd);
        g_multSymbol[i] = texFromSurface(ren, sym);
        SDL_DestroySurface(brd);
        SDL_DestroySurface(sym);
        SDL_DestroySurface(full);
    }

    // GAME OVER sprite baked as white mask.
    {
        SDL_Surface* f = SDL_CreateSurface(GAMEOVER_W, GAMEOVER_H, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{atlas::GAMEOVER.x, atlas::GAMEOVER.y, GAMEOVER_W, GAMEOVER_H};
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
        SDL_Surface* f = SDL_CreateSurface(atlas::COIN.w, atlas::COIN.h, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{atlas::COIN.x0 + i * atlas::COIN.dx, atlas::COIN.y,
                     atlas::COIN.w, atlas::COIN.h};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        g_coinFrames[i] = {atlas::COIN.w, atlas::COIN.h, texFromSurface(ren, f)};
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

    auto cropR     = [&](const AtlasRect& a)  { return cropSprite(a.x, a.y, a.w, a.h); };
    auto cropStrip = [&](const AtlasStrip& s, int i) {
        return cropSprite(s.x0 + i * s.dx, s.y, s.w, s.h);
    };

    // Bonus B / E / S spin (4 frames each).
    for (int i = 0; i < 4; ++i) {
        g_bonusFrames[i] = cropR(atlas::BONUS_B[i]);
        g_bonusE[i]      = cropR(atlas::BONUS_E[i]);
        g_bonusS[i]      = cropR(atlas::BONUS_S[i]);
    }

    // Transformed chasers (2 animation frames each).
    g_sprites[SP_SPHERE1] = cropR(atlas::SPHERE1);
    g_sprites[SP_SPHERE2] = cropR(atlas::SPHERE2);
    g_sprites[SP_ORB1]    = cropR(atlas::ORB1);
    g_sprites[SP_ORB2]    = cropR(atlas::ORB2);
    g_sprites[SP_CLUB1]   = cropR(atlas::CLUB1);
    g_sprites[SP_CLUB2]   = cropR(atlas::CLUB2);
    g_sprites[SP_UFO1]    = cropR(atlas::UFO1);
    g_sprites[SP_UFO2]    = cropR(atlas::UFO2);

    // Horn frames centred into a 16x16 cell.
    auto cropCentered = [&](const AtlasRect& a) -> Sprite {
        SDL_Surface* f = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{a.x, a.y, a.w, a.h};
        SDL_Rect dst{(16 - a.w) / 2, (16 - a.h) / 2, a.w, a.h};
        SDL_BlitSurface(atlas, &src, f, &dst);
        Sprite sp{16, 16, texFromSurface(ren, f)};
        SDL_DestroySurface(f);
        return sp;
    };
    g_sprites[SP_HORN1] = cropCentered(atlas::HORN1);
    g_sprites[SP_HORN2] = cropCentered(atlas::HORN2);

    // Bonus collect flash (6 frames 32x32).
    for (int i = 0; i < 6; ++i) g_bonusTaken[i] = cropStrip(atlas::BONUS_TAKEN, i);

    // Bomb frames: 0=resting, 1-6=lit fuse.
    for (int i = 0; i < 7; ++i) g_bombFrames[i] = cropStrip(atlas::BOMB, i);

    // Bomb-clear explosion (3 growing frames).
    for (int i = 0; i < 3; ++i) g_explFrames[i] = cropR(atlas::EXPL[i]);

    // Bird (BF_COUNT) and mummy (MF_COUNT) frames, centred into 16x16 cells and
    // given a red-eye overlay mask. Same processing, different atlas tables.
    auto buildEyed = [&](const AtlasRect& a, SDL_Texture*& body, SDL_Texture*& eye) {
        SDL_Surface* f = SDL_CreateSurface(SIZE_16PX, SIZE_16PX, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{a.x, a.y, a.w, a.h};
        SDL_Rect dst{(SIZE_16PX - a.w) / 2, (SIZE_16PX - a.h) / 2, a.w, a.h};
        SDL_BlitSurface(atlas, &src, f, &dst);
        body = texFromSurface(ren, f);
        eye  = makeEyeMask(ren, f);
        SDL_DestroySurface(f);
    };
    for (int i = 0; i < BF_COUNT; ++i) buildEyed(atlas::BIRD[i],  g_birdTex[i],  g_birdEye[i]);
    for (int i = 0; i < MF_COUNT; ++i) buildEyed(atlas::MUMMY[i], g_mummyTex[i], g_mummyEye[i]);

    // Power orb: prebake per colour family × 4 cycle phases.
    {
        const int PX = atlas::ORB.x, PY = atlas::ORB.y, ow = atlas::ORB.w, oh = atlas::ORB.h;
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
    for (int i = 0; i < 4; ++i) {
        const int w = atlas::INIT[i].w, h = atlas::INIT[i].h;
        SDL_Surface* f = SDL_CreateSurface(INIT_FW, INIT_FH, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{atlas::INIT[i].x, atlas::INIT[i].y, w, h};
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
    auto splitStartHalf = [&](const AtlasRect& a, Sprite& bg, Sprite& text) {
        const int sw = a.w, sh = a.h;
        SDL_Surface* b = SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_RGBA32);
        SDL_Surface* t = SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_RGBA32);
        for (int y = 0; y < sh; ++y)
            for (int x = 0; x < sw; ++x) {
                const Uint8* sp = (const Uint8*)atlas->pixels +
                                  (a.y + y) * atlas->pitch + (a.x + x) * 4;
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
    splitStartHalf(atlas::START[0], g_startBg[0], g_startText[0]);
    splitStartHalf(atlas::START[1], g_startBg[1], g_startText[1]);

    // Coin-pickup sparkle (4 frames, white masks for yellow tint).
    for (int i = 0; i < 4; ++i) {
        const int w = atlas::PICK[i].w, h = atlas::PICK[i].h;
        SDL_Surface* f = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{atlas::PICK[i].x, atlas::PICK[i].y, w, h};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                if (p[3]) { p[0]=p[1]=p[2]=255; }
            }
        g_pickCoinFrames[i] = {w, h, texFromSurface(ren, f)};
        SDL_DestroySurface(f);
    }

    // Power bar segments: normalized to grayscale so SetTextureColorMod matches g_multBorder.
    // Pixels are (R,255,0) where R encodes brightness (66=dim → 255=bright). Black → transparent.
    for (int i = 0; i < BAR_STEPS; ++i) {
        for (int side = 0; side < 2; ++side) {
            const AtlasRect& seg = side == 0 ? atlas::BAR_RIGHT[i] : atlas::BAR_LEFT[i];
            int bw = seg.w;
            SDL_Surface* f = SDL_CreateSurface(bw, 8, SDL_PIXELFORMAT_RGBA32);
            SDL_Rect src{seg.x, seg.y, bw, 8};
            SDL_BlitSurface(atlas, &src, f, nullptr);
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < bw; ++x) {
                    Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                    bool black = p[3] > 0 && !(p[0] | p[1] | p[2]);
                    if (black || p[3] == 0) { p[0]=p[1]=p[2]=p[3]=0; }
                    else {
                        // Remap R [66..255] → [128..255] so innermost bar matches border lum=128
                        Uint8 lum = (Uint8)(200 + (int)(p[0] - 66) * 55 / 189);
                        p[0]=p[1]=p[2]=lum; p[3]=255;
                    }
                }
            Sprite sp{bw, 8, texFromSurface(ren, f)};
            SDL_DestroySurface(f);
            if (side == 0) g_barRight[i] = sp;
            else           g_barLeft[i]  = sp;
        }
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

void destroySprites() {
    auto dt = [](SDL_Texture*& t) { if (t) { SDL_DestroyTexture(t); t = nullptr; } };
    auto ds = [&](Sprite& s)      { dt(s.tex); };
    auto dv = [&](JackVarFrame& f){ dt(f.tex); };

    for (Sprite& s : g_sprites)      ds(s);
    for (Sprite& s : g_bonusFrames)  ds(s);
    for (Sprite& s : g_bonusE)       ds(s);
    for (Sprite& s : g_bonusS)       ds(s);
    for (Sprite& s : g_bonusTaken)   ds(s);
    for (SDL_Texture*& t : g_multBorder) dt(t);
    for (SDL_Texture*& t : g_multSymbol) dt(t);
    for (Sprite& s : g_barRight) ds(s);
    for (Sprite& s : g_barLeft)  ds(s);
    for (Sprite& s : g_coinFrames)   ds(s);
    for (Sprite& s : g_bombFrames)   ds(s);
    for (auto& row : g_orbCycle) for (Sprite& s : row) ds(s);
    for (Sprite& s : g_explFrames)      ds(s);
    for (Sprite& s : g_pickCoinFrames)  ds(s);
    for (Sprite& s : g_startBg)         ds(s);
    for (Sprite& s : g_startText)       ds(s);
    for (SDL_Texture*& t : g_jackTex)   dt(t);
    for (auto& row : g_jackPhase) for (SDL_Texture*& t : row) dt(t);
    for (JackVarFrame& f : g_jackDance) dv(f);
    for (JackVarFrame& f : g_jackPlf)   dv(f);
    for (JackVarFrame& f : g_jackDead)  dv(f);
    for (JackVarFrame& f : g_jackWin)   dv(f);
    dt(g_gameOverTex);
    for (SDL_Texture*& t : g_birdTex)      dt(t);
    for (SDL_Texture*& t : g_birdEye)      dt(t);
    for (SDL_Texture*& t : g_mummyTex)     dt(t);
    for (SDL_Texture*& t : g_mummyEye)     dt(t);
    for (SDL_Texture*& t : g_initEnemyTex) dt(t);
}
