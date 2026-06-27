#include <SDL3/SDL.h>   // must precede stb_image.h (provides Uint8/Uint16)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wtype-limits"
#include "stb_image.h"
#pragma GCC diagnostic pop

#include "screens_png.h"
#include "background.h"
#include "sprites.h"     // for loadAtlas
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

SDL_Texture* g_bgTex                    = nullptr;
int          g_bgCount                  = 0;
SDL_Texture* g_gridTex[GRID_BG_COUNT]   = {};
SDL_Texture* g_bannerTex[BANNER_PHASES] = {};
SDL_Texture* g_liveTex                  = nullptr;

static const char* const GRID_BG_FILES[GRID_BG_COUNT] = {
    "magnific_places_1.png", "magnific_places_2.png", "magnific_places_3.png",
    "magnific_places_4.png", "magnific_places_5.png", "space_1.png", "space_2.png",
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const std::array<Color, 8>& borderPaletteForScreen(int screen) {
    static const std::array<Color, 8> platform1 = {{
        {255,223,0},{255,191,0},{255,159,0},{255,127,0},
        {255,95,0}, {255,63,0}, {255,63,0}, {255,31,0}
    }};
    static const std::array<Color, 8> platform2 = {{
        {0,223,0},{0,191,0},{0,191,0},{0,159,0},
        {0,127,0},{0,95,0}, {0,63,0}, {0,31,0}
    }};
    static const std::array<Color, 8> platform3 = {{
        {252,252,80},{252,252,0},{252,252,0},{216,216,0},
        {180,180,0}, {144,144,0},{108,108,0},{72,72,0}
    }};
    static const std::array<Color, 8> platform4 = {{
        {0,255,255},{0,204,255},{0,204,255},{0,170,255},
        {0,136,255},{0,0,255},  {0,0,204},  {0,0,136}
    }};
    switch (screen) {
        case 1: return platform2;
        case 2: return platform1;
        case 3: return platform3;
        case 4: return platform4;
        default: return platform1;
    }
}

struct EdgeTouch { bool left, right; };
static EdgeTouch platformEdgeTouch(int x, int w) {
    const int b = (int)std::lround(BORDER_SOLID_X);
    return { x <= b + 1, (x + w) >= LOGW - b - 1 };
}

static int platCornerPull(int j, int h) {
    int top = PLAT_TAB_NOTCH - j;
    int bot = PLAT_TAB_NOTCH - (h - 1 - j);
    return std::clamp(std::max(top, bot), 0, PLAT_TAB_EXT);
}

struct VJoin { bool joined = false; bool outerLeft = false, outerRight = false; float boxH = 0.0f; };

static VJoin findVJoin(const std::vector<SDL_FRect>& plats, const SDL_FRect& v, bool atTop) {
    const float TOL = 1.5f;
    const float vl = v.x, vr = v.x + v.w;
    const float vEdge = atTop ? v.y : v.y + v.h;
    for (const SDL_FRect& h : plats) {
        if (h.h >= h.w) continue;
        if (h.y >= FLOOR_TOP - 1.0f) continue;
        const float hl = h.x, hr = h.x + h.w, ht = h.y, hb = h.y + h.h;
        if (hr < vl - TOL || hl > vr + TOL) continue;
        if (vEdge < ht - TOL || vEdge > hb + TOL) continue;
        VJoin j;
        j.joined = true;
        j.boxH = hb - ht;
        j.outerLeft  = !(hl < vl - TOL);
        j.outerRight = !(hr > vr + TOL);
        return j;
    }
    return {};
}

static float vMiterLen(const VJoin& j, int i, int w) {
    if (!j.joined || w <= 1) return 0.0f;
    float f = 0.0f;
    if (j.outerLeft)  f = std::max(f, 1.0f - (float)i / (w - 1));
    if (j.outerRight) f = std::max(f, (float)i / (w - 1));
    return j.boxH * f;
}

static stbi_uc* loadPngFile(const char* name, int* w, int* h) {
    char buf1[512], buf2[512];
    std::snprintf(buf1, sizeof(buf1), "assets/%s", name);
    std::snprintf(buf2, sizeof(buf2), "../assets/%s", name);
    const char* candidates[] = {buf1, buf2};
    int comp = 0;
    for (const char* p : candidates) {
        size_t sz = 0;
        void* data = SDL_LoadFile(p, &sz);
        if (!data) continue;
        stbi_uc* px = stbi_load_from_memory(
            static_cast<const stbi_uc*>(data), (int)sz, w, h, &comp, 4);
        SDL_free(data);
        if (px) return px;
    }
    return nullptr;
}

static SDL_Texture* loadGridBgTex(SDL_Renderer* ren, const char* fname) {
    int w = 0, h = 0;
    stbi_uc* px = loadPngFile(fname, &w, &h);
    if (!px) { std::fprintf(stderr, "background load failed: %s\n", fname); return nullptr; }
    SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, px, w * 4);
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(s);
    stbi_image_free(px);
    return t;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void drawPlayfieldBorder(SDL_Renderer* r, int screen) {
    const auto& pal = borderPaletteForScreen(screen);
    const float x0 = 0.0f, y0 = (float)HUD_H;
    const float w = (float)GAME_W, h = (float)GAME_H;
    for (int i = 0; i < 8; ++i) {
        setCol(r, pal[i]);
        fillR(r, x0+i, y0+i, w-2.0f*i, 1.0f);
        fillR(r, x0+i, y0+i, 1.0f, h-2.0f*i);
        setCol(r, pal[7-i]);
        fillR(r, x0+i, y0+h-1.0f-i, w-2.0f*i, 1.0f);
        fillR(r, x0+w-1.0f-i, y0+i, 1.0f, h-2.0f*i);
    }
}

void drawPlatformShaded(SDL_Renderer* r, const SDL_FRect& pl, int screen,
                        const std::vector<SDL_FRect>& plats) {
    const auto& pal = borderPaletteForScreen(screen);
    const int x = (int)std::round(pl.x);
    const int y = (int)std::round(pl.y);
    const int w = std::max(1, (int)std::round(pl.w));
    const int h = std::max(1, (int)std::round(pl.h));
    if (h > w) {
        const VJoin top = findVJoin(plats, pl, true);
        const VJoin bot = findVJoin(plats, pl, false);
        for (int i = 0; i < w; ++i) {
            int band = std::min(7, (i * 8) / w);
            setCol(r, pal[band]);
            bool edge = (i == 0 || i == w - 1);
            float t = top.joined ? (float)y - vMiterLen(top, i, w)
                                 : (float)y + (edge ? 1.0f : 0.0f);
            float b = bot.joined ? (float)(y + h) + vMiterLen(bot, i, w)
                                 : (float)(y + h) - (edge ? 1.0f : 0.0f);
            if (b > t) fillR(r, (float)(x + i), t, 1.0f, b - t);
        }
        return;
    }
    const EdgeTouch touch = platformEdgeTouch(x, w);
    for (int j = 0; j < h; ++j) {
        int band = std::min(7, (j * 8) / h);
        setCol(r, pal[band]);
        bool edge = (j == 0 || j == h - 1);
        float left  = touch.left  ? BORDER_SOLID_X       : (float)x + (edge ? 1.0f : 0.0f);
        float right = touch.right ? LOGW - BORDER_SOLID_X : (float)(x + w) - (edge ? 1.0f : 0.0f);
        if (right > left) fillR(r, left, (float)(y + j), right - left, 1.0f);
    }
}

void drawPlatformFrameTab(SDL_Renderer* r, const SDL_FRect& pl, int screen) {
    const int wx = (int)std::round(pl.x);
    const int ww = std::max(1, (int)std::round(pl.w));
    const EdgeTouch touch = platformEdgeTouch(wx, ww);
    if (!touch.left && !touch.right) return;
    const auto& pal = borderPaletteForScreen(screen);
    const int sy = (int)std::round(HUD_H + pl.y * GAME_H / (float)LOGH);
    const int sh = std::max(1, (int)std::round(pl.h * GAME_H / (float)LOGH));
    const int leftEdge  = (int)std::round(pl.x * GAME_W / (float)LOGW);
    const int rightEdge = (int)std::round((pl.x + pl.w) * GAME_W / (float)LOGW);
    for (int j = 0; j < sh; ++j) {
        int band = std::min(7, (j * 8) / sh);
        int len = PLAT_TAB_EXT - platCornerPull(j, sh);
        if (len <= 0) continue;
        setCol(r, pal[band]);
        if (touch.left)  fillR(r, (float)(leftEdge - len), (float)(sy + j), (float)len, 1.0f);
        if (touch.right) fillR(r, (float)rightEdge,        (float)(sy + j), (float)len, 1.0f);
    }
}

void buildBackground(SDL_Renderer* ren) {
    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(screens_png, (int)screens_png_len, &w, &h, &comp, 4);
    if (!px) { std::fprintf(stderr, "background decode failed\n"); return; }
    SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, px, w * 4);
    g_bgTex = SDL_CreateTextureFromSurface(ren, s);
    SDL_SetTextureScaleMode(g_bgTex, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(s);
    stbi_image_free(px);
    g_bgCount = w / BG_W;

    for (int i = 0; i < GRID_BG_COUNT; ++i)
        g_gridTex[i] = loadGridBgTex(ren, GRID_BG_FILES[i]);

    // Title banner: three colour-phase variants from the atlas.
    int aw = 0, ah = 0;
    stbi_uc* apx = loadAtlas(&aw, &ah);
    if (!apx) { std::fprintf(stderr, "atlas decode failed (banner)\n"); return; }

    const int LX = 4, LY = 300;
    std::vector<Uint8> logo((size_t)BANNER_W * BANNER_H * 4);
    for (int y = 0; y < BANNER_H; ++y)
        std::memcpy(&logo[(size_t)y * BANNER_W * 4],
                    apx + ((size_t)(LY + y) * aw + LX) * 4, (size_t)BANNER_W * 4);

    // Jack life icon for the HUD.
    {
        const int LIX = 1, LIY = 3;
        std::vector<Uint8> li((size_t)LIVE_W * LIVE_H * 4);
        for (int y = 0; y < LIVE_H; ++y)
            std::memcpy(&li[(size_t)y * LIVE_W * 4],
                        apx + ((size_t)(LIY + y) * aw + LIX) * 4, (size_t)LIVE_W * 4);
        SDL_Surface* ls = SDL_CreateSurfaceFrom(LIVE_W, LIVE_H, SDL_PIXELFORMAT_RGBA32,
                                                li.data(), LIVE_W * 4);
        g_liveTex = SDL_CreateTextureFromSurface(ren, ls);
        SDL_SetTextureScaleMode(g_liveTex, SDL_SCALEMODE_NEAREST);
        SDL_DestroySurface(ls);
    }
    stbi_image_free(apx);

    const Uint8 blues[BANNER_PHASES][3] = {{0,82,255},{0,140,255},{0,189,255}};
    std::vector<Uint8> buf((size_t)BANNER_W * BANNER_H * 4);
    for (int phase = 0; phase < BANNER_PHASES; ++phase) {
        std::memcpy(buf.data(), logo.data(), buf.size());
        for (size_t p = 0; p < buf.size(); p += 4)
            for (int i = 0; i < BANNER_PHASES; ++i)
                if (buf[p]==blues[i][0] && buf[p+1]==blues[i][1] && buf[p+2]==blues[i][2]) {
                    const Uint8* c = blues[(i + BANNER_PHASES - phase) % BANNER_PHASES];
                    buf[p]=c[0]; buf[p+1]=c[1]; buf[p+2]=c[2]; break;
                }
        SDL_Surface* bs = SDL_CreateSurfaceFrom(BANNER_W, BANNER_H, SDL_PIXELFORMAT_RGBA32,
                                                buf.data(), BANNER_W * 4);
        g_bannerTex[phase] = SDL_CreateTextureFromSurface(ren, bs);
        SDL_SetTextureScaleMode(g_bannerTex[phase], SDL_SCALEMODE_NEAREST);
        SDL_DestroySurface(bs);
    }
}

void destroyBackground() {
    auto dt = [](SDL_Texture*& t) { if (t) { SDL_DestroyTexture(t); t = nullptr; } };
    dt(g_bgTex);
    dt(g_liveTex);
    for (SDL_Texture*& t : g_gridTex)   dt(t);
    for (SDL_Texture*& t : g_bannerTex) dt(t);
}
