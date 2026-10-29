#include "render/screens.h"
#include "render/background.h"
#include "render/gfx.h"
#include "render/hud.h"
#include "render/palette.h"
#include "render/textures.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

void drawStartIntro(SDL_Renderer* r, const Game& g) {
    if (g.startAnim <= 0.0f || g.demo.active || !g_startBg[0].tex) return;
    const float scale  = 1.0f;
    const float w      = 56.0f * scale, h = g_startBg[0].h * scale;
    const float finalX = (SCREEN_W - w) / 2.0f;
    const float cy     = HUD_H + (GAME_H - h) / 2.0f;
    float elapsed = START_TOTAL - g.startAnim;
    float p = std::min(elapsed / START_SLIDE, 1.0f);
    p = 1.0f - (1.0f - p) * (1.0f - p);
    float xs[2] = {-w       + (finalX - (-w))     * p,
                   SCREEN_W + (finalX - SCREEN_W) * p};

    static const Color cyc[] = {
        {220, 40, 40}, {230, 130, 30}, {220, 200, 40}, {60, 200, 70},
        {40, 180, 220}, {70, 90, 230}, {180, 60, 220}
    };
    const int N = (int)(sizeof(cyc) / sizeof(cyc[0]));
    Color bgc = cyc[(int)(g.time * 12.0f) % N];

    float fade = (elapsed <= START_SLIDE)
                     ? 1.0f
                     : std::max(0.0f, 1.0f - (elapsed - START_SLIDE) /
                                                 (START_BLACK - START_SLIDE));

    for (int i = 0; i < 2; ++i) {
        SDL_FRect d{xs[i], cy, w, h};
        drawTexTinted(r, g_startBg[i].tex,   d, ::scale(bgc, fade));
        drawTexTinted(r, g_startText[i].tex, d, {255, 255, 255});
    }
}

void drawSpecialBonus(SDL_Renderer* r, const Game& g) {
    const float w = 178.0f, h = 92.0f;
    const float x = (SCREEN_W - w) / 2.0f;
    const float y = HUD_H + (GAME_H - h) / 2.0f;
    const float cx = x + w / 2.0f;
    Color hl = colorCycle3(g.time);
    setCol(r, hl);         fillR(r, x - 2, y - 2, w + 4, h + 4);
    setCol(r, {0, 0, 28}); fillR(r, x, y, w, h);
    char buf[40];
    drawTextCentered(r, "YOU'VE GOTTEN", cx, y + 14, 1, {255, 230, 60});
    std::snprintf(buf, sizeof(buf), "%d FIRE BOMBS", g.special.fireBombs);
    drawTextCentered(r, buf, cx, y + 30, 1, hl);
    drawTextCentered(r, "SPECIAL BONUS", cx, y + 54, 1, {255, 255, 255});
    if (g.special.remaining > 0) {
        std::snprintf(buf, sizeof(buf), "%d PTS", g.special.remaining);
        drawTextCentered(r, buf, cx, y + 70, 1, hl);
    }
}

// "YOU ARE LUCKY / ONE MORE PLAY" screen (shown on collecting an S bonus).
// A hollow yellow frame with the spinning S coin in its centre pulses through
// yellow shades; the two reward lines glide in from the top corners and settle
// stacked below the frame.
void drawLucky(SDL_Renderer* r, const Game& g) {
    const float cx = SCREEN_W / 2.0f;
    const Sprite& frame = g_luckyFrame[(int)(g.time * 6.0f) % LUCKY_PHASES];
    const float fw = frame.w, fh = frame.h;                 // 64x64
    const float fx = cx - fw / 2.0f;
    const float fcy = SCREEN_H / 2.0f;                       // frame+coin centred on screen
    const float fy = fcy - fh / 2.0f;                        // frame top

    // "YOU ARE LUCKY" in red above the frame.
    drawTextCentered(r, "YOU ARE LUCKY", cx, fy - 14, 1, {255, 40, 40});

    // Frame whose bright yellow band rolls outward from the centre.
    if (frame.tex) {
        SDL_FRect dst{fx, fy, fw, fh};
        SDL_RenderTexture(r, frame.tex, nullptr, &dst);
    }

    // Spinning S coin nested in the frame's hollow centre, drawn at the same
    // on-screen size it has in-level (world sprites render at GAME_W/LOGW, 2x).
    const Sprite& coin = g_bonusS[(int)(g.time * 12.0f) % 4];
    if (coin.tex) {
        const float scale = 2.0f * (float)GAME_W / (float)LOGW;
        const float cw = coin.w * scale, ch = coin.h * scale;
        SDL_FRect dst{cx - cw / 2, fcy - ch / 2, cw, ch};
        SDL_RenderTexture(r, coin.tex, nullptr, &dst);
    }

    // Two reward lines glide from the top corners to their stacked rest spots.
    float p = g.lucky.phase == 0
                  ? std::clamp(g.lucky.timer / LUCKY_SLIDE_TIME, 0.0f, 1.0f)
                  : 1.0f;
    float e = p * p * (3.0f - 2.0f * p);                     // smoothstep ease
    const float restY1 = fy + fh + 12.0f;
    const float restY2 = restY1 + 12.0f;
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    float x1 = lerp(-40.0f, cx, e), y1 = lerp(HUD_H + 6.0f, restY1, e);
    float x2 = lerp(SCREEN_W + 40.0f, cx, e), y2 = lerp(HUD_H + 6.0f, restY2, e);
    drawTextCentered(r, "YOU CAN ENJOY", x1, y1, 1, {255, 255, 255});
    drawTextCentered(r, "ONE MORE PLAY", x2, y2, 1, {255, 255, 255});
}

// High-score table. In the attract loop it shows the stored top-ten under a
// "BEST PLAYERS" header. During NAMEENTRY it injects the fresh score at its rank
// row and lets the player type a name inline inside a colour-cycling frame,
// under a "FANTASTIC SCORE! / RECORD YOUR NAME" header.
void drawHiScores(SDL_Renderer* r, const Game& g) {
    const bool  editing = (g.state == NAMEENTRY);
    const float cx = SCREEN_W / 2.0f;

    if (editing) {
        drawTextCentered(r, "FANTASTIC SCORE!", cx, HUD_H + 8,  1, {255, 140, 0});
        drawTextCentered(r, "RECORD YOUR NAME", cx, HUD_H + 22, 1, {255, 255, 255});
    } else {
        // Align 'B' under the last letter of the HUD's "SIDE-ONE" (drawn at x=9).
        drawText(r, "BEST PLAYERS", 9.0f + 7 * FONT_ADV, HUD_H + 14, 1, {255, 140, 0});
    }

    const float rankX = 8, scoreX = 44, nameX = 112, roundX = 150;
    const float numX  = roundX + 6 * FONT_ADV;   // after the "ROUND " word
    const float y0 = HUD_H + 40;
    const Color base = {220, 220, 230};
    char buf[24];
    for (int d = 0; d < HISCORE_COUNT; ++d) {
        const bool isEdit = editing && d == g.nameEntry.rank;
        // During editing the pending entry is injected at its rank, pushing the
        // rows below it down one place (the last stored entry drops off).
        int score, level; const char* name;
        if (isEdit)                       { score = g.score;         level = g.round;         name = g.nameEntry.text; }
        else if (editing && d > g.nameEntry.rank){ score = g.hiscores[d-1].score; level = g.hiscores[d-1].round; name = g.hiscores[d-1].name; }
        else                              { score = g.hiscores[d].score;   level = g.hiscores[d].round;   name = g.hiscores[d].name; }

        // Attract loop colour-cycles the top score/round; editing cycles the frame.
        Color hot = (!editing && d == 0) ? colorCycle3(g.time) : base;
        float y = y0 + d * 16;
        int rk = d + 1;
        const char* suf = rk == 1 ? "ST" : rk == 2 ? "ND" : rk == 3 ? "RD" : "TH";
        std::snprintf(buf, sizeof(buf), "%d%s", rk, suf); drawText(r, buf, rankX,  y, 1, base);
        std::snprintf(buf, sizeof(buf), "%d", score);     drawText(r, buf, scoreX, y, 1, hot);

        if (isEdit) {
            // Colour-cycling rounded frame around the name being typed.
            if (g_nameFrame.tex) {
                SDL_FRect dst{nameX - 5, y - 6, (float)g_nameFrame.w, (float)g_nameFrame.h};
                drawTexTinted(r, g_nameFrame.tex, dst, colorCycle3(g.time));
            }
            // Typed chars at the normal font advance (1px gap, like the table),
            // plus a blinking caret in the next slot.
            for (int k = 0; k < g.nameEntry.len; ++k)
                drawChar(r, g.nameEntry.text[k], nameX + k * FONT_ADV, y, 1, {255, 255, 255});
            if (g.nameEntry.len < NAME_MAX && std::fmod(g.time, 0.6f) < 0.3f) {
                setCol(r, {255, 255, 255});
                fillR(r, nameX + g.nameEntry.len * FONT_ADV, y + FONT_H - 1, FONT_W, 1.5f);
            }
        } else {
            drawText(r, name[0] ? name : "...", nameX, y, 1, base);
        }

        drawText(r, "ROUND", roundX, y, 1, base);
        std::snprintf(buf, sizeof(buf), "%d", level);     drawText(r, buf, numX, y, 1, hot);
    }

    // Footer "@#1984 TEHKAN LTD." (@/# are the two copyright glyphs), sitting
    // midway between the last row and the bottom, with TEHKAN colour-cycling.
    const char* footer = "@#1984 TEHKAN LTD.";
    const float fx = cx - textWidth(footer, 1) / 2.0f;
    const float fy = HUD_H + GAME_H - 20;
    drawText(r, footer, fx, fy, 1, {200, 200, 210});
    drawText(r, "TEHKAN", fx + 7 * FONT_ADV, fy, 1, colorCycle3(g.time));  // recolour word
}

void drawOptions(SDL_Renderer* r, const Game& g) {
    useScreen(r);
    setCol(r, {0, 0, 0});
    SDL_RenderClear(r);
    drawTextCentered(r, "GAME OPTIONS", SCREEN_W / 2.0f, HUD_H + 26, 2, {255, 230, 60});

    const char* imgNames[OPT_IMG_STYLES] = {
        "CLASSIC", "PLACES1", "PLACES2", "PLACES3", "PLACES4",
        "PLACES5", "SPACE1", "SPACE2", "NO"
    };
    const char* labels[OPT_COUNT] = {"START LEVEL", "LIVES", "NEW IMAGES",
                                     "S BONUS", "100K START"};
    const float rowY0 = HUD_H + 80;
    for (int i = 0; i < OPT_COUNT; ++i) {
        bool sel = (g.opt.sel == i);
        Color c = sel ? Color{255, 255, 255} : Color{150, 150, 170};
        float y = rowY0 + i * 18;
        if (sel) {
            setCol(r, Color{255, 230, 60});
            for (int k = 0; k < 7; ++k)
                fillR(r, 14, y + k, (float)(4 - std::abs(k - 3)), 1.0f);
        }
        drawText(r, labels[i], 28, y, 1, c);
        char vbuf[24];
        if (i == 0)      std::snprintf(vbuf, sizeof(vbuf), "%d", g.opt.firstRound);
        else if (i == 1) std::snprintf(vbuf, sizeof(vbuf), "%d", g.opt.lives);
        else if (i == 2) std::snprintf(vbuf, sizeof(vbuf), "%s", imgNames[g.opt.imgStyle]);
        else if (i == 3) std::snprintf(vbuf, sizeof(vbuf), "%s", g.opt.alwaysS ? "ON" : "OFF");
        else             std::snprintf(vbuf, sizeof(vbuf), "%s", g.opt.start100k ? "ON" : "OFF");
        drawText(r, vbuf, 150, y, 1, c);
    }

    drawTextCentered(r, "ARROWS SELECT / CHANGE", SCREEN_W / 2.0f, HUD_H + 162, 1,
                     {120, 200, 255});
    drawTextCentered(r, "SPACE START   ESC BACK", SCREEN_W / 2.0f, HUD_H + 176, 1,
                     {120, 200, 255});
    drawHud(r, g);
}

// Title banner with the start and options hints.
void drawTitle(SDL_Renderer* r, const Game& g) {
    const float bx = (SCREEN_W - BANNER_W) / 2.0f;
    const float by = HUD_H + (GAME_H / 4.0f) - (BANNER_H / 2.0f);
    int phase = (int)(g.time / BANNER_ROT) % BANNER_PHASES;
    if (g_bannerTex[phase]) {
        SDL_FRect dst{bx, by, (float)BANNER_W, (float)BANNER_H};
        SDL_RenderTexture(r, g_bannerTex[phase], nullptr, &dst);
    }
    drawTextCentered(r, "PRESS SPACE TO START", SCREEN_W / 2.0f, by + BANNER_H + 14,
                     1, std::fmod(g.time, 1.0f) < 0.5f ? Color{230, 230, 230}
                                                        : Color{120, 120, 120});
    drawTextCentered(r, "ARROWS MOVE   SPACE FLOAT", SCREEN_W / 2.0f,
                     by + BANNER_H + 28, 1, {150, 150, 170});
    drawTextCentered(r, "PRESS O FOR OPTIONS", SCREEN_W / 2.0f,
                     by + BANNER_H + 42, 1, {255, 200, 60});
}

// "GAME OVER" gliding to the centre of the screen.
void drawGameOver(SDL_Renderer* r, const Game& g) {
    if (!g_gameOverTex) return;
    Color c = colorCycle3(g.time);
    const SDL_FRect dst{g.gameOver.x - GAMEOVER_W / 2.0f, g.gameOver.y - GAMEOVER_H / 2.0f,
                        (float)GAMEOVER_W, (float)GAMEOVER_H};
    drawTexTinted(r, g_gameOverTex, dst, false, c.r, c.g, c.b);
}
