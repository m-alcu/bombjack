#include "render/world.h"
#include "render/animsprite.h"
#include "render/background.h"
#include "render/gfx.h"
#include "render/palette.h"
#include "render/textures.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// On-screen draw size of the living Jack sprite.
constexpr float JACK_DRAW_W = 26.0f * SPRITE_AR;
constexpr float JACK_DRAW_H = 28.0f;
constexpr float JACK_WALK_FRAME = 0.125f;

constexpr float BOMB_HALF_W = 6.0f * (float)LOGW / (float)GAME_W;
constexpr float BOMB_HALF_H = 8.0f * (float)LOGH / (float)GAME_H;

constexpr float BONUSTAKEN_SCALE = 1.8f;

// Round-clear victory dance
constexpr int   VICTORY_STEPS = 16;
constexpr float VICTORY_FRAME = CLEAR_TIME / VICTORY_STEPS;
constexpr int VICTORY_SEQ[VICTORY_STEPS] = {
    JW_NORMAL, JW_LEFT, JW_NORMAL, JW_RIGHT, JW_NORMAL, JW_UP, JW_NORMAL, JW_UP,
    JW_NORMAL, JW_LEFT, JW_NORMAL, JW_RIGHT, JW_NORMAL, JW_UP, JW_NORMAL, JW_UP,
};

static void drawBomb(SDL_Renderer* r, const Bomb& b, bool lit, float t) {
    const float dw = BOMB_HALF_W * 2.0f;
    const float dh = BOMB_HALF_H * 2.0f;
    int frame = lit ? 1 + (int)(std::fmod(t, 0.3f) / 0.05f) % 6 : 0;
    SDL_Texture* tex = g_bombFrames[frame].tex;
    if (tex) {
        SDL_FRect dst{b.x - dw / 2, b.y - dh / 2, dw, dh};
        SDL_RenderTexture(r, tex, nullptr, &dst);
    }
}

// Jack's living poses, indexed by JackPose. Built once at startup (after
// buildTextures), since the frame textures must already exist. Walking is a
// 4-frame loop per direction; the other states are single poses.
static AnimSprite s_jack;
static const char* const JACK_ANIM[] = {
    "idle", "walk_l", "walk_r", "fly", "fly_l", "fly_r", "fall", "fall_l", "fall_r",
};

void buildJackSprite() {
    AnimSprite& jack = s_jack;
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

// While frozen (Jack holds the Power ball) he is drawn in palette 14, pal14.
static void drawPlayer(SDL_Renderer* r, const Player& p, float t,
                       bool dying, int deathPhase, int deathFrame,
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
    // Normal living Jack: the simulation picked the pose and its clock
    // (updateJackPose).
    s_jack.play(JACK_ANIM[p.pose]);
    s_jack.setClock(p.poseTime);
    s_jack.setPosition(p.x + PW / 2.0f, p.y + PH - JACK_DRAW_H / 2.0f);
    s_jack.draw(r);
}

static void drawJackVictory(SDL_Renderer* r, const Player& p, float clearTimer) {
    float elapsed = CLEAR_TIME - clearTimer;
    int step = std::clamp((int)(elapsed / VICTORY_FRAME), 0, VICTORY_STEPS - 1);
    const JackVarFrame& fr = g_jackWin[VICTORY_SEQ[step]];
    if (!fr.tex) return;
    const float scale = 26.0f / 16.0f;
    const float dw = fr.w * scale * SPRITE_AR, dh = fr.h * scale;
    SDL_FRect dst{p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh};
    SDL_RenderTexture(r, fr.tex, nullptr, &dst);
}

// Flash-in / transform flash (0xBC-0xBF, 6 frames each). It uses palette 15,
// which rotates with the "SIDE-ONE" colours.
static void drawEnemyFlash(SDL_Renderer* r, const Enemy& e, float t) {
    float prog = 1.0f - std::clamp(e.timer / INIT_ENEMY_TIME, 0.0f, 1.0f);
    int frame = std::min(3, (int)(prog * 4));
    SDL_Texture* tex = g_initEnemyTex[frame];
    if (!tex) return;
    const float w = INIT_FW * SPRITE_AR, h = (float)INIT_FH;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, tex, dst, pal15(t));
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

static void drawEnemy(SDL_Renderer* r, const Enemy& e, float t, bool frozen,
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

// The Power ball (sprite 0x3D) is drawn in palette 14 too, pen by pen
// (pal14: pens 1-7, index 0 unused).
static void drawPowerBall(SDL_Renderer* r, float x, float y, const Color pal14[8]) {
    const float w = 32.0f * SPRITE_AR, h = 32.0f;
    SDL_FRect dst{x - w / 2, y - h / 2, w, h};
    for (int k = 1; k < 8; ++k)
        if (g_ballPen[k]) drawTexTinted(r, g_ballPen[k], dst, pal14[k]);
}

// The play area: backdrop, platforms, bombs, effects, Power ball, bonus coin,
// enemies, Jack, the frame and the score popups.
void drawPlayfield(SDL_Renderer* r, const Game& g) {
    drawBackground(r, currentScreen(g), g.opt.imgStyle);

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
    for (int k = 1; k < 8; ++k) pal14[k] = pal14Pen(g.pal14, g.time, k);
    if (g.ball.active) drawPowerBall(r, g.ball.x, g.ball.y, pal14);

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
        drawPlayer(r, g.p, g.time, g.death.active, g.death.phase,
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
        std::snprintf(buf, sizeof(buf), "ROUND %d CLEAR!", g.round);
        drawTextCentered(r, buf, LOGW / 2.0f, LOGH / 2.0f - 10, 3, {120, 255, 140});
    }
}
