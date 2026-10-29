#include "render/hud.h"
#include "render/background.h"
#include "render/gfx.h"
#include "render/palette.h"
#include "render/textures.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

// Multiplier box (screen x 92..131, y 0..15) and power bar, as the arcade draws
// them in palette 14: the power bar grows outward from both box sides 2 px per
// 2 power units (20 = full, as it shows while the Power ball is up or the
// freeze lasts), each 4 px band k in pen 6-k, so colour ripples outward.
static void drawMultiplierAndPower(SDL_Renderer* r, const Game& g) {
    const bool powerUp = g.ball.active || g.freezeTimer > 0.0f;

    const int units = std::clamp((int)(g.powerMeter * 2.0f + 0.001f), 0, 20);  // arcade 0..20
    const int barW  = powerUp ? 20 : 2 * (units / 2);
    for (int d = 0; d < barW; d += 4) {
        const float w = (float)std::min(4, barW - d);
        setCol(r, pal14Pen(g.pal14, g.time, 6 - d / 4));
        fillR(r, 92.0f - d - w, 0, w, 16);
        fillR(r, 132.0f + d,    0, w, 16);
    }

    const Color frameC = pal14Pen(g.pal14, g.time, 7);
    setCol(r, frameC);
    fillR(r, 92,  0, 4, 16);
    fillR(r, 128, 0, 4, 16);
    if (!g_multBorder[0] || !g_multSymbol[0]) return;
    const Color glyphC = pal14Pen(g.pal14, g.time, 1);
    const int m = std::clamp(g.multiplier, 1, 5);
    auto cell = [&](int idx, SDL_FRect dst) {
        drawTexTinted(r, g_multSymbol[idx], dst, glyphC);
        drawTexTinted(r, g_multBorder[idx], dst, frameC);
    };
    cell(0, {96,  0, 16, 16});
    cell(m, {112, 0, 16, 16});
}

void drawHud(SDL_Renderer* r, const Game& g) {
    char buf[64];
    setCol(r, {0, 0, 0});
    fillR(r, 0, 0, SCREEN_W, HUD_H);
    fillR(r, 0, HUD_H + GAME_H, SCREEN_W, HUD_H);

    // Arcade layout: "SIDE-ONE" in 8 px character cells 1..8, the total score
    // right-aligned below it ending in cell 8 (glyphs sit 1 px into each cell).
    drawText(r, "SIDE-ONE", 9, 0, 1, pal15(g.time));
    std::snprintf(buf, sizeof(buf), "%d", g.score);
    drawText(r, buf, 65.0f - FONT_ADV * (float)(std::strlen(buf) - 1), 8, 1, {255, 255, 255});

    drawMultiplierAndPower(r, g);

    const float top = HUD_H + GAME_H;
    const float l1  = top, l2 = top + 8;

    int shown = std::clamp(g.lives - 1, 0, 7);
    for (int i = 0; i < shown; ++i) {
        SDL_FRect dst{2.0f + i * 15.0f, top + 1, 14.0f, 14};
        if (g_liveTex) SDL_RenderTexture(r, g_liveTex, nullptr, &dst);
    }

    drawTextCentered(r, "ROUND", 132, l1, 1, {80, 230, 90});
    std::snprintf(buf, sizeof(buf), "-%d-", g.round);
    drawTextCentered(r, buf, 132, l2, 1, {0, 139, 255});

    drawTextCentered(r, "HI-SCORE", 192, l1, 1, {255, 230, 60});
    const float hiRight = 192 + textWidth("HI-SCORE", 1) / 2.0f;
    std::snprintf(buf, sizeof(buf), "%d", g.score);
    drawText(r, buf, hiRight - textWidth(buf, 1), l2, 1, {255, 255, 255});
}
