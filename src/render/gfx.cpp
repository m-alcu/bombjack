#include "render/gfx.h"
#include "render/sprite_atlas.h"
#include <cctype>
#include <cmath>
#include <cstring>
#include <unordered_map>

static std::unordered_map<char, SDL_Texture*> g_font;

void buildFont(SDL_Renderer* ren, SDL_Surface* atlas) {
    auto add = [&](char c, int sx, int sy) {
        SDL_Surface* f = SDL_CreateSurface(FONT_W, FONT_H, SDL_PIXELFORMAT_RGBA32);
        SDL_Rect src{sx, sy, FONT_W, FONT_H};
        SDL_BlitSurface(atlas, &src, f, nullptr);
        SDL_Texture* t = SDL_CreateTextureFromSurface(ren, f);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        g_font[c] = t;
        SDL_DestroySurface(f);
    };
    using namespace atlas;
    const char* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; letters[i]; ++i) add(letters[i], FONT_X0 + FONT_DX * i, FONT_LETTER_Y);
    for (int i = 0; i < 10; ++i) add('0' + i, FONT_X0 + FONT_DX * i, FONT_DIGIT_Y);
    add('\'', 169, FONT_DIGIT_Y); add('!', 180, FONT_DIGIT_Y); add('.', 190, FONT_DIGIT_Y);
    add('-',  205, FONT_DIGIT_Y); add('(', 215, FONT_DIGIT_Y); add(')', 224, FONT_DIGIT_Y);
    // Copyright pair for the high-score footer: '@' = ©, '#' = the second mark.
    add('@', 125, 224); add('#', 137, 224);
}

void setCol(SDL_Renderer* r, Color c, Uint8 a) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a);
}

void fillR(SDL_Renderer* r, float x, float y, float w, float h) {
    SDL_FRect q{x, y, w, h};
    SDL_RenderFillRect(r, &q);
}

void drawChar(SDL_Renderer* r, char ch, float x, float y, int s, Color c) {
    auto it = g_font.find(static_cast<char>(std::toupper((unsigned char)ch)));
    if (it == g_font.end() || !it->second) return;
    SDL_SetTextureColorMod(it->second, c.r, c.g, c.b);
    SDL_FRect dst{x, y, (float)FONT_W * s, (float)FONT_H * s};
    SDL_RenderTexture(r, it->second, nullptr, &dst);
}

int textWidth(const char* t, int s) { return (int)std::strlen(t) * FONT_ADV * s; }

void drawText(SDL_Renderer* r, const char* t, float x, float y, int s, Color c) {
    for (const char* p = t; *p; ++p) {
        drawChar(r, *p, x, y, s, c);
        x += FONT_ADV * s;
    }
}

void drawTextCentered(SDL_Renderer* r, const char* t, float cx, float y, int s, Color c) {
    drawText(r, t, cx - textWidth(t, s) / 2.0f, y, s, c);
}

void useWorld(SDL_Renderer* r) {
    SDL_SetRenderScale(r, (float)GAME_W / LOGW, (float)GAME_H / LOGH);
    SDL_Rect vp{0, HUD_H * LOGH / GAME_H, LOGW, LOGH};
    SDL_SetRenderViewport(r, &vp);
}

void useScreen(SDL_Renderer* r) {
    SDL_SetRenderScale(r, 1.0f, 1.0f);
    SDL_SetRenderViewport(r, nullptr);
}

void drawTexTinted(SDL_Renderer* r, SDL_Texture* tex, const SDL_FRect& dst,
                   bool flip, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetTextureColorMod(tex, cr, cg, cb);
    SDL_RenderTextureRotated(r, tex, nullptr, &dst, 0.0, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(tex, 255, 255, 255);
}
