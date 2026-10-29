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
#include "third_party/stb_image.h"
#pragma GCC diagnostic pop

#include "data/sprites_full_png.h"
#include "render/textures.h"
#include "render/sprite_atlas.h"
#include "data/rom_pens.h"
#include <algorithm>
#include <cstring>
#include <vector>

// Global sprite storage (defined here, declared extern in textures.h).
Sprite g_bonusFrames[4];
Sprite g_bonusE[4];
Sprite g_bonusS[4];
Sprite g_bonusTaken[6];
SDL_Texture* g_multBorder[6] = {};
SDL_Texture* g_multSymbol[6] = {};
Sprite g_bombFrames[7];
SDL_Texture* g_ballPen[8] = {};
Sprite g_explFrames[3];
Sprite g_pickCoinFrames[4];
Sprite g_startBg[2];
Sprite g_startText[2];
Sprite g_luckyFrame[LUCKY_PHASES];
Sprite g_nameFrame;

SDL_Texture* g_jackTex[JF_COUNT]      = {};
SDL_Texture* g_jackPen[JF_COUNT][8] = {};
JackVarFrame g_jackDance[3] = {};
JackVarFrame g_jackPlf[4]   = {};
JackVarFrame g_jackDead[4]  = {};
JackVarFrame g_jackWin[4]   = {};
SDL_Texture* g_gameOverTex  = nullptr;

SDL_Texture* g_enemyTex[256] = {};
SDL_Texture* g_enemyEye[256] = {};
Sprite       g_frozen;
SDL_Texture* g_frozenGlint[2] = {};


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

// White mask of the pixels drawn with ROM pen `pen` in a pen map (rom_pens.h),
// or nullptr if the pen is unused.
static SDL_Texture* makePenMask(SDL_Renderer* ren, const char* pens, int w, int h, int pen) {
    SDL_Surface* m = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    bool any = false;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const bool on = pens[y * w + x] - '0' == pen;
            Uint8* mp = (Uint8*)m->pixels + y * m->pitch + x * 4;
            mp[0] = mp[1] = mp[2] = 255;
            mp[3] = on ? 255 : 0;
            any |= on;
        }
    SDL_Texture* t = any ? texFromSurface(ren, m) : nullptr;
    SDL_DestroySurface(m);
    return t;
}

unsigned char* loadAtlas(int* w, int* h) {
    int c = 0;
    return stbi_load_from_memory(sprites_full_png, (int)sprites_full_png_len, w, h, &c, 4);
}

void buildTextures(SDL_Renderer* ren) {
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
    for (int i = 0; i < JF_COUNT; ++i) {
        SDL_Surface* f = SDL_CreateSurface(SIZE_16PX, JACK_FH, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{atlas::JACK[i].x, atlas::JACK[i].y, SIZE_16PX, JACK_FH};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        g_jackTex[i] = texFromSurface(ren, f);
        // White mask per ROM pen, recoloured with palette 14 while Jack holds
        // the Power ball (see drawPlayer).
        for (int k = 1; k < 8; ++k)
            g_jackPen[i][k] = makePenMask(ren, rompens::kJack[i], SIZE_16PX, JACK_FH, k);
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
                // Border layer: full white so the tint shows the exact HUD colour
                b[0] = b[1] = b[2] = 255; b[3] = isGreen ? 255 : 0;
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

    // Frozen enemy coin: pen 5 put back to its resting colour, plus white
    // masks of pens 5 and 6 for the glint.
    {
        const AtlasRect& a = atlas::FROZEN;
        SDL_Surface* f = SDL_CreateSurface(a.w, a.h, SDL_PIXELFORMAT_RGBA32);
        SDL_Surface* m[2];
        SDL_Rect src{a.x, a.y, a.w, a.h};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        for (int k = 0; k < 2; ++k) m[k] = SDL_CreateSurface(a.w, a.h, SDL_PIXELFORMAT_RGBA32);
        for (int y = 0; y < a.h; ++y)
            for (int x = 0; x < a.w; ++x) {
                Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                const unsigned rgb = p[3] ? (unsigned)p[0] << 16 | p[1] << 8 | p[2] : ~0u;
                for (int k = 0; k < 2; ++k) {
                    Uint8* q = (Uint8*)m[k]->pixels + y * m[k]->pitch + x * 4;
                    q[0] = q[1] = q[2] = 255;
                    q[3] = rgb == (k ? atlas::FROZEN_PEN6 : atlas::FROZEN_PEN5) ? 255 : 0;
                }
                if (rgb == atlas::FROZEN_PEN5) {
                    p[0] = atlas::FROZEN_PEN5_REST >> 16;
                    p[1] = atlas::FROZEN_PEN5_REST >> 8 & 0xFF;
                    p[2] = atlas::FROZEN_PEN5_REST & 0xFF;
                }
            }
        g_frozen = {a.w, a.h, texFromSurface(ren, f)};
        for (int k = 0; k < 2; ++k) {
            g_frozenGlint[k] = texFromSurface(ren, m[k]);
            SDL_DestroySurface(m[k]);
        }
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

    // "YOU ARE LUCKY" frame: bake LUCKY_PHASES variants where the bright yellow
    // band sits at a different radius, so cycling them animates a colour wave
    // rolling outward from the centre. The transparent centre is preserved.
    {
        SDL_Surface* base = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{atlas::LUCKY_FRAME.x, atlas::LUCKY_FRAME.y, 64, 64};
        SDL_BlitSurface(atlas, &src, base, nullptr);
        for (int p = 0; p < LUCKY_PHASES; ++p) {
            float off = (float)p / LUCKY_PHASES * 6.2831853f;
            SDL_Surface* sv = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA32);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) {
                    const Uint8* sp = (const Uint8*)base->pixels + y * base->pitch + x * 4;
                    Uint8* dp = (Uint8*)sv->pixels + y * sv->pitch + x * 4;
                    if (sp[3] == 0) { dp[0] = dp[1] = dp[2] = dp[3] = 0; continue; }
                    float d = std::max(std::fabs(x - 31.5f), std::fabs(y - 31.5f));
                    int v = (int)(160.0f + 95.0f * std::sin(d * 0.5f - off));
                    dp[0] = dp[1] = (Uint8)std::clamp(v, 0, 255); dp[2] = 0; dp[3] = 255;
                }
            g_luckyFrame[p] = {64, 64, texFromSurface(ren, sv)};
            SDL_DestroySurface(sv);
        }
        SDL_DestroySurface(base);
    }

    // High-score name-edit frame, baked to a white mask so it can be tinted with
    // the colour cycle at draw time. The transparent centre hole is preserved.
    {
        const AtlasRect& nf = atlas::NAME_FRAME;
        SDL_Surface* f = SDL_CreateSurface(nf.w, nf.h, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{nf.x, nf.y, nf.w, nf.h};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        for (int y = 0; y < nf.h; ++y)
            for (int x = 0; x < nf.w; ++x) {
                Uint8* p = (Uint8*)f->pixels + y * f->pitch + x * 4;
                if (p[3] > 0) { p[0] = p[1] = p[2] = 255; }
            }
        g_nameFrame = {nf.w, nf.h, texFromSurface(ren, f)};
        SDL_DestroySurface(f);
    }

    // Bonus collect flash (6 frames 32x32).
    for (int i = 0; i < 6; ++i) g_bonusTaken[i] = cropStrip(atlas::BONUS_TAKEN, i);

    // Bomb frames: 0=resting, 1-6=lit fuse.
    for (int i = 0; i < 7; ++i) g_bombFrames[i] = cropStrip(atlas::BOMB, i);

    // Bomb-clear explosion (3 growing frames).
    for (int i = 0; i < 3; ++i) g_explFrames[i] = cropR(atlas::EXPL[i]);

    // Enemy frames by arcade sprite code, each with its pen 1 (red) mask.
    for (const atlas::CodeCell& c : atlas::ENEMY_CELLS) {
        SDL_Surface* f = SDL_CreateSurface(SIZE_16PX, SIZE_16PX, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{c.x, c.y, SIZE_16PX, SIZE_16PX};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        g_enemyTex[c.code] = texFromSurface(ren, f);
        g_enemyEye[c.code] = makeEyeMask(ren, f);
        SDL_DestroySurface(f);
    }

    // Power ball: one white mask per ROM pen, recoloured with palette 14.
    for (int k = 1; k < 8; ++k)
        g_ballPen[k] = makePenMask(ren, rompens::kPowerBall, SIZE_16PX, SIZE_16PX, k);

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

    SDL_DestroySurface(atlas);
    stbi_image_free(spx);
}

void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                   bool flip, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetTextureColorMod(tex, cr, cg, cb);
    SDL_RenderTextureRotated(r, tex, nullptr, &dst, 0.0, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(tex, 255, 255, 255);
}

void destroyTextures() {
    auto dt = [](SDL_Texture*& t) { if (t) { SDL_DestroyTexture(t); t = nullptr; } };
    auto ds = [&](Sprite& s)      { dt(s.tex); };
    auto dv = [&](JackVarFrame& f){ dt(f.tex); };

    for (Sprite& s : g_bonusFrames)  ds(s);
    for (Sprite& s : g_bonusE)       ds(s);
    for (Sprite& s : g_bonusS)       ds(s);
    for (Sprite& s : g_bonusTaken)   ds(s);
    for (SDL_Texture*& t : g_multBorder) dt(t);
    for (SDL_Texture*& t : g_multSymbol) dt(t);
    for (Sprite& s : g_bombFrames)   ds(s);
    for (SDL_Texture*& t : g_ballPen) dt(t);
    for (Sprite& s : g_explFrames)      ds(s);
    for (Sprite& s : g_pickCoinFrames)  ds(s);
    for (Sprite& s : g_startBg)         ds(s);
    for (Sprite& s : g_startText)       ds(s);
    for (Sprite& s : g_luckyFrame)      ds(s);
    ds(g_nameFrame);
    for (SDL_Texture*& t : g_jackTex)   dt(t);
    for (auto& row : g_jackPen) for (SDL_Texture*& t : row) dt(t);
    for (JackVarFrame& f : g_jackDance) dv(f);
    for (JackVarFrame& f : g_jackPlf)   dv(f);
    for (JackVarFrame& f : g_jackDead)  dv(f);
    for (JackVarFrame& f : g_jackWin)   dv(f);
    dt(g_gameOverTex);
    for (SDL_Texture*& t : g_enemyTex)     dt(t);
    for (SDL_Texture*& t : g_enemyEye)     dt(t);
    ds(g_frozen);
    for (SDL_Texture*& t : g_frozenGlint)  dt(t);
    for (SDL_Texture*& t : g_initEnemyTex) dt(t);
}
