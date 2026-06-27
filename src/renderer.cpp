#include "renderer.h"
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
    const char* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; letters[i]; ++i) add(letters[i], 5 + 12 * i, 212);
    for (int i = 0; i < 10; ++i) add('0' + i, 5 + 12 * i, 224);
    add('\'', 169, 224); add('!', 180, 224); add('.', 190, 224);
    add('-',  204, 224); add('(', 215, 224); add(')', 224, 224);
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

Color colorCycle3(float t) {
    const float CYCLE = 0.4f;
    int stage = (int)(t / CYCLE);
    float f = t / CYCLE - (float)stage;
    Uint8 a = (Uint8)(255.0f * f), b = (Uint8)(255.0f * (1.0f - f));
    switch (((stage % 3) + 3) % 3) {
        case 0:  return {255, a, 0};
        case 1:  return {b, b, a};
        default: return {a, 0, b};
    }
}
