#include "render/draw.h"
#include "render/gfx.h"
#include "render/textures.h"
#include "render/animsprite.h"
#include "render/background.h"
#include "game/game.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

void drawBomb(SDL_Renderer* r, const Bomb& b, bool lit, float t) {
    const float dw = BOMB_HALF_W * 2.0f;
    const float dh = BOMB_HALF_H * 2.0f;
    int frame = lit ? 1 + (int)(std::fmod(t, 0.3f) / 0.05f) % 6 : 0;
    SDL_Texture* tex = g_bombFrames[frame].tex;
    if (tex) {
        SDL_FRect dst{b.x - dw / 2, b.y - dh / 2, dw, dh};
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
}

// Populate the player's owned AnimSprite with its living poses. Called once at
// startup (after buildTextures), since the frame textures must already exist.
// Walking is a 4-frame loop per direction; the other states are single poses.
void buildJackSprite(AnimSprite& jack) {
    jack.setSize(JACK_DRAW_W, JACK_DRAW_H);
    auto single = [](int f) {
        AnimSprite::Animation a;
        a.frames.push_back({g_jackTex[f], nullptr, 0.1f});
        return a;
    };
    auto walk = [](int base) {
        AnimSprite::Animation a;
        for (int i = 0; i < 4; ++i)
            a.frames.push_back({g_jackTex[base + i], nullptr, JACK_WALK_FRAME});
        return a;
    };
    jack.addAnimation("idle",   single(JF_IDLE));
    jack.addAnimation("fly",    single(JF_FLY));
    jack.addAnimation("fly_l",  single(JF_FLY_L));
    jack.addAnimation("fly_r",  single(JF_FLY_R));
    jack.addAnimation("fall",   single(JF_FALL));
    jack.addAnimation("fall_l", single(JF_FALL_L));
    jack.addAnimation("fall_r", single(JF_FALL_R));
    jack.addAnimation("walk_l", walk(JF_WALK_L0));
    jack.addAnimation("walk_r", walk(JF_WALK_R0));
}

void drawPlayer(SDL_Renderer* r, const Player& p, const AnimSprite& jack,
                float t, bool dying, int deathPhase, int deathFrame,
                bool frozen, const Color pal14[8]) {
    if (dying) {
        const JackVarFrame* fr = nullptr;
        if (deathPhase == DP_DANCING) fr = &g_jackDance[std::clamp(deathFrame, 0, 2)];
        else if (deathPhase == DP_FALLING) fr = &g_jackPlf[std::clamp(deathFrame, 0, 3)];
        else if (deathPhase == DP_DEAD || deathPhase == DP_WAIT)
            fr = &g_jackDead[std::clamp(deathFrame, 0, 3)];
        if (fr && fr->tex) {
            const float scale = 26.0f / 16.0f;
            const float dw = fr->w * scale * SPRITE_AR;
            const float dh = fr->h * scale;
            SDL_FRect dst{p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh};
            SDL_RenderTexture(r, fr->tex, nullptr, &dst);
        }
        return;
    }
    if (p.invuln > 0 && std::fmod(t, 0.16f) < 0.08f) return;

    // Holding the Power ball (until the freeze ends) the arcade draws Jack with
    // palette 14, the one the multiplier box and power bar rotate through: each
    // ROM pen takes its colour from there. The pen masks aren't
    // part of the AnimSprite, so pick the matching pose here.
    if (frozen) {
        const bool moving = std::fabs(p.vx) > 1.0f;
        const bool left   = p.face < 0;
        int f;
        if (p.onGround)        f = moving ? (left ? JF_WALK_L0 : JF_WALK_R0) +
                                            (int)(t / JACK_WALK_FRAME) % 4 : JF_IDLE;
        else if (p.vy < 0.0f)  f = moving ? (left ? JF_FLY_L  : JF_FLY_R)  : JF_FLY;
        else                   f = moving ? (left ? JF_FALL_L : JF_FALL_R) : JF_FALL;
        SDL_FRect dst{p.x + PW / 2 - JACK_DRAW_W / 2, p.y + PH - JACK_DRAW_H,
                      JACK_DRAW_W, JACK_DRAW_H};
        for (int k = 1; k < 8; ++k)
            if (g_jackPen[f][k]) drawTexTinted(r, g_jackPen[f][k], dst, pal14[k]);
        return;
    }
    // Normal living Jack: pose, position and animation clock were set on the
    // owned AnimSprite during the simulation step (see driveJack in game.cpp).
    jack.draw(r);
}

void drawJackVictory(SDL_Renderer* r, const Player& p, float clearTimer) {
    float elapsed = CLEAR_TIME - clearTimer;
    int step = std::clamp((int)(elapsed / VICTORY_FRAME), 0, VICTORY_STEPS - 1);
    const JackVarFrame& fr = g_jackWin[VICTORY_SEQ[step]];
    if (!fr.tex) return;
    const float scale = 26.0f / 16.0f;
    const float dw = fr.w * scale * SPRITE_AR, dh = fr.h * scale;
    SDL_FRect dst{p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh};
    SDL_RenderTexture(r, fr.tex, nullptr, &dst);
}

static Color flashColor(float t);

// Flash-in / transform flash (0xBC-0xBF, 6 frames each). It uses palette 15,
// which rotates with the "SIDE-ONE" colours.
static void drawEnemyFlash(SDL_Renderer* r, const Enemy& e, float t) {
    float prog = 1.0f - std::clamp(e.timer / INIT_ENEMY_TIME, 0.0f, 1.0f);
    int frame = std::min(3, (int)(prog * 4));
    SDL_Texture* tex = g_initEnemyTex[frame];
    if (!tex) return;
    const float w = INIT_FW * SPRITE_AR, h = (float)INIT_FH;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, tex, dst, flashColor(t));
}

// Frozen enemy: a still coin whose pens 5 and 6 glint white in turn; it
// blinks 4 frames on / 4 off over the last FROZEN_BLINK_FRAMES of the freeze.
static void drawFrozenEnemy(SDL_Renderer* r, const Enemy& e, float t,
                            float freezeTimer) {
    const int left = (int)(freezeTimer * ARCADE_HZ);
    if (left < FROZEN_BLINK_FRAMES && (left / 4) % 2 == 0) return;
    if (!g_frozen.tex) return;
    const float w = 24.0f * SPRITE_AR, h = 24.0f;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    SDL_RenderTexture(r, g_frozen.tex, nullptr, &dst);
    const int ph = (int)(t * ARCADE_HZ) % FROZEN_GLINT_CYCLE / FROZEN_GLINT_FRAMES;
    if (ph < 2 && g_frozenGlint[ph]) SDL_RenderTexture(r, g_frozenGlint[ph], nullptr, &dst);
}

void drawEnemy(SDL_Renderer* r, const Enemy& e, float t, bool frozen,
               float freezeTimer) {
    if (e.phase == EP_INIT || (e.phase == EP_DISAPPEAR && !frozen)) {
        drawEnemyFlash(r, e, t);
        return;
    }
    if (frozen) { drawFrozenEnemy(r, e, t, freezeTimer); return; }
    // 0x30 is the 0x2F turning bird drawn mirrored.
    const int  code = e.code == 0x30 ? 0x2F : e.code & 0xFF;
    const bool flip = e.flip != (e.code == 0x30);
    SDL_Texture* tex = g_enemyTex[code];
    if (!tex) return;
    const float h = e.kind == EK_MUMMY ? 28.0f : e.kind == EK_BIRD ? 24.0f : 26.0f;
    const float w = 26.0f * SPRITE_AR;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, tex, dst, flip, 255, 255, 255);
    if (SDL_Texture* eye = g_enemyEye[code])
        drawTexTinted(r, eye, dst, flip, eyePulse(t), 0, 0);
}

// The Power ball (sprite 0x3D) is drawn in palette 14 too, pen by pen.
void drawPowerOrb(SDL_Renderer* r, float x, float y, const Color pal14[8]) {
    const float w = 32.0f * SPRITE_AR, h = 32.0f;
    SDL_FRect dst{x - w / 2, y - h / 2, w, h};
    for (int k = 1; k < 8; ++k)
        if (g_ballPen[k]) drawTexTinted(r, g_ballPen[k], dst, pal14[k]);
}

void drawBackground(SDL_Renderer* r, int screen, int imgStyle) {
    setCol(r, {18, 16, 38});
    SDL_RenderClear(r);
    SDL_Texture* tex = nullptr;
    SDL_FRect src{};
    if (imgStyle == 0 && g_bgTex && g_bgCount > 0) {
        int idx = screen % g_bgCount;
        if (idx < 0) idx += g_bgCount;
        src = {(float)(idx * BG_W), 0, (float)BG_W, (float)BG_H};
        tex = g_bgTex;
    } else if (imgStyle >= 1 && imgStyle <= GRID_BG_COUNT) {
        SDL_Texture* gt = g_gridTex[imgStyle - 1];
        if (gt) {
            int idx = ((screen % 16) + 16) % 16;
            src = {(float)((idx % 4) * BG_W), (float)((idx / 4) * BG_H),
                   (float)BG_W, (float)BG_H};
            tex = gt;
        }
    }
    if (tex) {
        SDL_FRect dst{0, 0, (float)LOGW, (float)LOGH};
        SDL_RenderTexture(r, tex, &src, &dst);
    }
}

// Top-HUD colour cycling. The hardware rotates palette 15 ("SIDE-ONE") one
// entry every 3 frames through this table (measured from the arcade in MAME).
static constexpr Color HUD_ROT_SIDE[35] = {
    {255,255,102}, {255,255,136}, {255,255,170}, {255,255,204}, {255,255,255}, {204,204,255},
    {170,170,255}, {136,136,255}, {102,102,255}, { 68, 68,255}, { 34, 34,255}, {  0,  0,255},
    { 34,  0,255}, { 68,  0,255}, {102,  0,255}, {136,  0,255}, {170,  0,255}, {204,  0,255},
    {255,  0,255}, {255,  0,204}, {255,  0,170}, {255,  0,136}, {255,  0,102}, {255,  0, 68},
    {255,  0, 34}, {255,  0,  0}, {255, 34,  0}, {255, 68,  0}, {255,102,  0}, {255,136,  0},
    {255,170,  0}, {255,204,  0}, {255,255,  0}, {255,255, 34}, {255,255, 68},
};
constexpr int HUD_ROT_FRAMES = 3;   // arcade frames per rotation step

// Palette 14 (multiplier box, power bar, Power ball, powered-up Jack) tables,
// in the ROM's xBGR 4:4:4 words: the rainbow (0x0D48) and the Power ball's
// colour families 1-7 (0x0D9E): blue, red, magenta, green, cyan, yellow,
// each fading to white, and silver.
static constexpr uint16_t PAL14_RAINBOW[42] = {
    0x000F, 0x002F, 0x004F, 0x006F, 0x008F, 0x00AF, 0x00CF, 0x00FF,
    0x00FC, 0x00FA, 0x00F8, 0x00F6, 0x00F4, 0x00F2, 0x00F0, 0x02F0,
    0x04F0, 0x06F0, 0x08F0, 0x0AF0, 0x0CF0, 0x0FF0, 0x0FC0, 0x0FA0,
    0x0F80, 0x0F60, 0x0F40, 0x0F20, 0x0F00, 0x0F02, 0x0F04, 0x0F06,
    0x0F08, 0x0F0A, 0x0F0C, 0x0F0F, 0x0C0F, 0x0A0F, 0x080F, 0x060F,
    0x040F, 0x020F,
};
static constexpr uint16_t PAL14_FAMILY[7][8] = {
    {0x0F00, 0x0F22, 0x0F44, 0x0F66, 0x0F88, 0x0FAA, 0x0FCC, 0x0FEE},
    {0x000F, 0x022F, 0x044F, 0x066F, 0x088F, 0x0AAF, 0x0CCF, 0x0EEF},
    {0x0F0F, 0x0F2F, 0x0F4F, 0x0F6F, 0x0F8F, 0x0FAF, 0x0FCF, 0x0FEF},
    {0x00F0, 0x02F2, 0x04F4, 0x06F6, 0x08F8, 0x0AFA, 0x0CFC, 0x0EFE},
    {0x0FF0, 0x0FF2, 0x0FF4, 0x0FF6, 0x0FF8, 0x0FFA, 0x0FFC, 0x0FFE},
    {0x00FF, 0x02FF, 0x04FF, 0x06FF, 0x08FF, 0x0AFF, 0x0CFF, 0x0EFF},
    {0x0FFF, 0x0EEE, 0x0DDD, 0x0CCC, 0x0BBB, 0x0AAA, 0x0999, 0x0888},
};
static constexpr Color xbgr444(uint16_t v) {
    return {(Uint8)((v & 15) * 17), (Uint8)((v >> 4 & 15) * 17), (Uint8)((v >> 8 & 15) * 17)};
}

static Color flashColor(float t) {
    return HUD_ROT_SIDE[(int)(t * ARCADE_HZ / FLASH_ROT_FRAMES) % (int)std::size(HUD_ROT_SIDE)];
}

// Current rotation step, and entry `step + offset` of a cyclic table.
static int hudRotStep(const Game& g) { return (int)(g.time * ARCADE_HZ / HUD_ROT_FRAMES); }
template <int N>
static Color hudRot(const Color (&table)[N], int step, int offset) {
    return table[((step + offset) % N + N) % N];
}

// Palette 14 pen `pen` (1-7). The loaded table rotates one entry every
// HUD_ROT_FRAMES starting from its first, pen k showing entry step + k - 1.
// The box frame is pen 7, its glyphs pen 1 and the power bar bands pens 6
// down to 2.
static Color pal14Pen(const Game& g, int pen) {
    const int step = std::max(0, (int)((g.time - g.pal14Time) * ARCADE_HZ / HUD_ROT_FRAMES));
    if (g.pal14 >= 1 && g.pal14 <= 7)
        return xbgr444(PAL14_FAMILY[g.pal14 - 1][(step + pen - 1) % 8]);
    return xbgr444(PAL14_RAINBOW[(step + pen - 1) % (int)std::size(PAL14_RAINBOW)]);
}

// Multiplier box (screen x 92..131, y 0..15) and power bar, as the arcade draws
// them in palette 14: the power bar grows outward from both box sides 2 px per
// 2 power units (20 = full, as it shows while the Power ball is up or the
// freeze lasts), each 4 px band k in pen 6-k, so colour ripples outward.
static void drawMultiplierAndPower(SDL_Renderer* r, const Game& g) {
    const bool powerUp = g.orb.active || g.freezeTimer > 0.0f;

    const int units = std::clamp((int)(g.powerMeter * 2.0f + 0.001f), 0, 20);  // arcade 0..20
    const int barW  = powerUp ? 20 : 2 * (units / 2);
    for (int d = 0; d < barW; d += 4) {
        const float w = (float)std::min(4, barW - d);
        setCol(r, pal14Pen(g, 6 - d / 4));
        fillR(r, 92.0f - d - w, 0, w, 16);
        fillR(r, 132.0f + d,    0, w, 16);
    }

    const Color frameC = pal14Pen(g, 7);
    setCol(r, frameC);
    fillR(r, 92,  0, 4, 16);
    fillR(r, 128, 0, 4, 16);
    if (!g_multBorder[0] || !g_multSymbol[0]) return;
    const Color glyphC = pal14Pen(g, 1);
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
    drawText(r, "SIDE-ONE", 9, 0, 1, hudRot(HUD_ROT_SIDE, hudRotStep(g), 0));
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
    std::snprintf(buf, sizeof(buf), "-%d-", g.level);
    drawTextCentered(r, buf, 132, l2, 1, {0, 139, 255});

    drawTextCentered(r, "HI-SCORE", 192, l1, 1, {255, 230, 60});
    const float hiRight = 192 + textWidth("HI-SCORE", 1) / 2.0f;
    std::snprintf(buf, sizeof(buf), "%d", g.score);
    drawText(r, buf, hiRight - textWidth(buf, 1), l2, 1, {255, 255, 255});
}

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
    std::snprintf(buf, sizeof(buf), "%d FIRE BOMBS", g.sb.catched);
    drawTextCentered(r, buf, cx, y + 30, 1, hl);
    drawTextCentered(r, "SPECIAL BONUS", cx, y + 54, 1, {255, 255, 255});
    if (g.sb.remaining > 0) {
        std::snprintf(buf, sizeof(buf), "%d PTS", g.sb.remaining);
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
        const bool isEdit = editing && d == g.hs.rank;
        // During editing the pending entry is injected at its rank, pushing the
        // rows below it down one place (the last stored entry drops off).
        int score, level; const char* name;
        if (isEdit)                       { score = g.score;         level = g.level;         name = g.hs.entry; }
        else if (editing && d > g.hs.rank){ score = g.hi[d-1].score; level = g.hi[d-1].level; name = g.hi[d-1].name; }
        else                              { score = g.hi[d].score;   level = g.hi[d].level;   name = g.hi[d].name; }

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
            for (int k = 0; k < g.hs.len; ++k)
                drawChar(r, g.hs.entry[k], nameX + k * FONT_ADV, y, 1, {255, 255, 255});
            if (g.hs.len < NAME_MAX && std::fmod(g.time, 0.6f) < 0.3f) {
                setCol(r, {255, 255, 255});
                fillR(r, nameX + g.hs.len * FONT_ADV, y + FONT_H - 1, FONT_W, 1.5f);
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
        bool sel = (g.optSel == i);
        Color c = sel ? Color{255, 255, 255} : Color{150, 150, 170};
        float y = rowY0 + i * 18;
        if (sel) {
            setCol(r, Color{255, 230, 60});
            for (int k = 0; k < 7; ++k)
                fillR(r, 14, y + k, (float)(4 - std::abs(k - 3)), 1.0f);
        }
        drawText(r, labels[i], 28, y, 1, c);
        char vbuf[24];
        if (i == 0)      std::snprintf(vbuf, sizeof(vbuf), "%d", g.startLevel);
        else if (i == 1) std::snprintf(vbuf, sizeof(vbuf), "%d", g.startLives);
        else if (i == 2) std::snprintf(vbuf, sizeof(vbuf), "%s", imgNames[g.imgStyle]);
        else if (i == 3) std::snprintf(vbuf, sizeof(vbuf), "%s", g.alwaysS ? "ON" : "OFF");
        else             std::snprintf(vbuf, sizeof(vbuf), "%s", g.start100k ? "ON" : "OFF");
        drawText(r, vbuf, 150, y, 1, c);
    }

    drawTextCentered(r, "ARROWS SELECT / CHANGE", SCREEN_W / 2.0f, HUD_H + 162, 1,
                     {120, 200, 255});
    drawTextCentered(r, "SPACE START   ESC BACK", SCREEN_W / 2.0f, HUD_H + 176, 1,
                     {120, 200, 255});
    drawHud(r, g);
}

void render(SDL_Renderer* r, const Game& g) {
    useWorld(r);

    if (g.state == SPECIALBONUS) {
        useScreen(r);
        setCol(r, {0, 0, 0});
        SDL_RenderClear(r);
        drawSpecialBonus(r, g);
        drawHud(r, g);
        return;
    }

    if (g.state == LUCKY) {
        useScreen(r);
        setCol(r, {0, 0, 0});
        SDL_RenderClear(r);
        drawLucky(r, g);
        drawHud(r, g);
        return;
    }

    if (g.state == NAMEENTRY) {
        useScreen(r);
        setCol(r, {0, 0, 0});
        SDL_RenderClear(r);
        drawHiScores(r, g);   // table with inline name editing
        drawHud(r, g);
        return;
    }

    if (g.state == TITLE) {
        useScreen(r);
        setCol(r, {0, 0, 0});
        SDL_RenderClear(r);
        // Attract loop alternates the banner with the high-score table.
        if (g.hs.showTable) {
            drawHiScores(r, g);
            drawHud(r, g);
            return;
        }
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
        drawHud(r, g);
        return;
    }

    if (g.state == OPTIONS) {
        drawOptions(r, g);
        return;
    }

    drawBackground(r, currentScreen(g), g.imgStyle);

    // Horizontals first, then vertical girders (so girder miter draws on top).
    for (int pass = 0; pass < 2; ++pass) {
        for (const SDL_FRect& pl : g.platforms) {
            if (pl.y >= FLOOR_TOP - 1.0f) continue;
            bool vertical = pl.h > pl.w;
            if (vertical != (pass == 1)) continue;
            drawPlatformShaded(r, pl, currentScreen(g), g.platforms);
        }
    }

    for (int i = 0; i < (int)g.bombs.size(); ++i)
        if (!g.bombs[i].collected) drawBomb(r, g.bombs[i], i == g.litBomb, g.time);

    for (const TimedEffect& ex : g.explosions) {
        int fi = std::min(2, (int)(ex.age / EXPL_FRAME));
        const Sprite& f = g_explFrames[fi];
        if (!f.tex) continue;
        const float s = 1.6f;
        SDL_FRect dst{ex.x - f.w * s / 2, ex.y - f.h * s / 2, f.w * s, f.h * s};
        SDL_RenderTexture(r, f.tex, nullptr, &dst);
    }

    for (const TimedEffect& cp : g.coinPickups) {
        int fi = std::min(3, (int)(cp.age / PICKCOIN_FRAME));
        const Sprite& f = g_pickCoinFrames[fi];
        if (!f.tex) continue;
        const float s = 2.0f;
        const float w = f.w * SPRITE_AR * s, h = f.h * s;
        SDL_FRect dst{cp.x - w / 2, cp.y - h / 2, w, h};
        drawTexTinted(r, f.tex, dst, false, 255, 215, 0);
    }

    Color pal14[8];
    for (int k = 1; k < 8; ++k) pal14[k] = pal14Pen(g, k);
    if (g.orb.active) drawPowerOrb(r, g.orb.x, g.orb.y, pal14);

    if (g.bonus.active) {
        const Sprite* set = g.bonus.kind == BK_E ? g_bonusE
                          : g.bonus.kind == BK_S ? g_bonusS : g_bonusFrames;
        const Sprite& f = set[(int)(g.bonus.anim * 12.0f) % 4];
        if (f.tex) {
            SDL_FRect dst{g.bonus.x - f.w, g.bonus.y - f.h, f.w * 2.0f, f.h * 2.0f};
            SDL_RenderTexture(r, f.tex, nullptr, &dst);
        }
    }

    for (const TimedEffect& bt : g.bonusTakens) {
        int fi = std::min(5, (int)(bt.age / BONUSTAKEN_FRAME));
        const Sprite& f = g_bonusTaken[fi];
        if (!f.tex) continue;
        const float w = f.w * SPRITE_AR * BONUSTAKEN_SCALE, h = f.h * BONUSTAKEN_SCALE;
        SDL_FRect dst{bt.x - w / 2, bt.y - h / 2, w, h};
        SDL_RenderTexture(r, f.tex, nullptr, &dst);
    }

    // The arcade's demo keeps a colour-cycling "GAME OVER" in the middle of the
    // playfield, under the sprites.
    if (g.demo.active && g_gameOverTex) {
        useScreen(r);
        Color c = colorCycle3(g.time);
        const SDL_FRect dst{(SCREEN_W - GAMEOVER_W) / 2.0f, (SCREEN_H - GAMEOVER_H) / 2.0f,
                            (float)GAMEOVER_W, (float)GAMEOVER_H};
        drawTexTinted(r, g_gameOverTex, dst, false, c.r, c.g, c.b);
        useWorld(r);
    }

    bool frozen = g.freezeTimer > 0.0f;
    for (const Enemy& e : g.enemies) drawEnemy(r, e, g.time, frozen, g.freezeTimer);
    if (g.state == ROUNDCLEAR)
        drawJackVictory(r, g.p, g.clearTimer);
    else if (g.state != GAMEOVER)
        drawPlayer(r, g.p, g.jackSprite, g.time, g.death.active, g.death.phase,
                   g.death.frame, frozen, pal14);

    useScreen(r);
    drawPlayfieldBorder(r, currentScreen(g));
    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;
        drawPlatformFrameTab(r, pl, currentScreen(g));
    }
    useWorld(r);

    for (const Popup& pp : g.popups) {
        char sb[16];
        std::snprintf(sb, sizeof(sb), "%d", pp.value);
        drawTextCentered(r, sb, pp.x, pp.y, 1, {255, 240, 140});
    }

    char buf[64];
    if (g.state == ROUNDCLEAR) {
        std::snprintf(buf, sizeof(buf), "ROUND %d CLEAR!", g.level);
        drawTextCentered(r, buf, LOGW / 2.0f, LOGH / 2.0f - 10, 3, {120, 255, 140});
    }

    useScreen(r);
    drawStartIntro(r, g);
    if (g.state == GAMEOVER && g_gameOverTex) {
        Color c = colorCycle3(g.time);
        const SDL_FRect dst{g.go.x - GAMEOVER_W / 2.0f, g.go.y - GAMEOVER_H / 2.0f,
                            (float)GAMEOVER_W, (float)GAMEOVER_H};
        drawTexTinted(r, g_gameOverTex, dst, false, c.r, c.g, c.b);
    }
    drawHud(r, g);
}
