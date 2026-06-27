#include "draw.h"
#include "renderer.h"
#include "sprites.h"
#include "background.h"
#include "game.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

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

void drawPlayer(SDL_Renderer* r, const Player& p, float t, bool dying,
                int deathPhase, int deathFrame, bool frozen, Color freezeColor) {
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
    const bool moving = std::fabs(p.vx) > 1.0f;
    const bool left   = p.face < 0;
    int f;
    if (p.onGround) {
        if (moving) {
            int step = (int)(t / JACK_WALK_FRAME) % 4;
            f = (left ? JF_WALK_L0 : JF_WALK_R0) + step;
        } else {
            f = JF_IDLE;
        }
    } else if (p.vy < 0.0f) {
        f = moving ? (left ? JF_FLY_L : JF_FLY_R) : JF_FLY;
    } else {
        f = moving ? (left ? JF_FALL_L : JF_FALL_R) : JF_FALL;
    }
    const float dw = 26.0f * SPRITE_AR, dh = 28.0f;
    SDL_FRect dst{p.x + PW / 2 - dw / 2, p.y + PH - dh, dw, dh};
    if (frozen && g_jackPhase[f][0]) {
        int ph = (int)(t / 0.1f) & 3;
        SDL_Texture* tex = g_jackPhase[f][ph];
        SDL_SetTextureColorMod(tex, freezeColor.r, freezeColor.g, freezeColor.b);
        SDL_RenderTexture(r, tex, nullptr, &dst);
        SDL_SetTextureColorMod(tex, 255, 255, 255);
    } else if (g_jackTex[f]) {
        SDL_RenderTexture(r, g_jackTex[f], nullptr, &dst);
    }
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

static SpriteId enemySprite(const Enemy& e, float t, float& w, float& h) {
    bool a = std::fmod(t, 0.3f) < 0.15f;
    w = 28; h = 24;
    switch (e.kind) {
        case EK_MUMMY:
            w = 26; h = 28;
            if (e.phase == EP_FALL) return SP_MUMMY_FALL;
            if (e.phase == EP_WALK) return a ? SP_MUMMY_WALK : SP_MUMMY;
            return SP_MUMMY;
        case EK_SPHERE: w = h = 26; return a ? SP_SPHERE1 : SP_SPHERE2;
        case EK_ORB:    w = h = 26; return a ? SP_ORB1    : SP_ORB2;
        case EK_HORN:   w = h = 26; return a ? SP_HORN1   : SP_HORN2;
        case EK_CLUB:   w = h = 26; return a ? SP_CLUB1   : SP_CLUB2;
        case EK_UFO:    w = 28; h = 20; return a ? SP_UFO1 : SP_UFO2;
        default:        return a ? SP_ENEMY1 : SP_ENEMY2;
    }
}

static void drawBird(SDL_Renderer* r, const Enemy& e, float t) {
    int step = (int)(t / BIRD_FLAP_FRAME) % 3;
    int base;
    if (std::fabs(e.vx) >= std::fabs(e.vy))
        base = (e.vx < 0) ? BF_LEFT0 : BF_RIGHT0;
    else
        base = BF_VERT0;
    SDL_Texture* body = g_birdTex[base + step];
    SDL_Texture* eye  = g_birdEye[base + step];
    if (!body) return;
    const float w = 26.0f * SPRITE_AR, h = 24.0f;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, body, dst, false, 255, 255, 255);
    if (eye) drawTexTinted(r, eye, dst, false, eyePulse(t), 0, 0);
}

static void drawMummy(SDL_Renderer* r, const Enemy& e, float t) {
    int frame;
    if (e.phase == EP_FALL) {
        frame = MF_FALL;
    } else if (e.phase == EP_WALK && std::fabs(e.vx) > 1.0f) {
        int step = (int)(t / MUMMY_WALK_FRAME) % 3;
        frame = (e.vx < 0 ? MF_WALK_L0 : MF_WALK_R0) + step;
    } else {
        frame = MF_IDLE;
    }
    SDL_Texture* body = g_mummyTex[frame];
    SDL_Texture* eye  = g_mummyEye[frame];
    if (!body) return;
    const float w = 26.0f * SPRITE_AR, h = 28.0f;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, body, dst, false, 255, 255, 255);
    if (eye) drawTexTinted(r, eye, dst, false, eyePulse(t), 0, 0);
}

static void drawInitEnemy(SDL_Renderer* r, const Enemy& e) {
    float prog = 1.0f - std::clamp(e.timer / INIT_ENEMY_TIME, 0.0f, 1.0f);
    int frame = std::min(3, (int)(prog * 4));
    SDL_Texture* tex = g_initEnemyTex[frame];
    if (!tex) return;
    const float w = INIT_FW * SPRITE_AR, h = (float)INIT_FH;
    SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
    drawTexTinted(r, tex, dst, false, e.spawnTint.r, e.spawnTint.g, e.spawnTint.b);
}

void drawEnemy(SDL_Renderer* r, const Enemy& e, float t, bool frozen,
               float freezeTimer) {
    if (e.phase == EP_INIT) { drawInitEnemy(r, e); return; }
    if (e.phase == EP_APPEAR && std::fmod(t, 0.12f) < 0.06f) return;
    if (frozen && freezeTimer < 1.0f && std::fmod(t, 0.16f) < 0.08f) return;
    if (frozen) {
        const Sprite& c = g_coinFrames[(int)(t * 10.0f) % 7];
        if (c.tex) {
            const float w = 24.0f * SPRITE_AR, h = 24.0f;
            SDL_FRect dst{e.x - w / 2, e.y - h / 2, w, h};
            SDL_RenderTexture(r, c.tex, nullptr, &dst);
        }
        return;
    }
    if (e.kind == EK_BIRD)  { drawBird(r, e, t);  return; }
    if (e.kind == EK_MUMMY) { drawMummy(r, e, t); return; }
    float w, h;
    SpriteId id = enemySprite(e, t, w, h);
    w *= SPRITE_AR;
    bool flip = e.vx < 0;
    drawSprite(r, id, e.x - w / 2, e.y - h / 2, w, h, flip);
}

void drawPowerOrb(SDL_Renderer* r, float x, float y, float t, int family) {
    int fam   = (family % 7 + 7) % 7;
    int phase = (int)(t / 0.1f) & 3;
    const Sprite& f = g_orbCycle[fam][phase];
    if (f.tex) {
        const float w = 28.8f * SPRITE_AR, h = 28.8f;
        SDL_FRect dst{x - w / 2, y - h / 2, w, h};
        SDL_RenderTexture(r, f.tex, nullptr, &dst);
    }
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

void drawHud(SDL_Renderer* r, const Game& g) {
    char buf[64];
    setCol(r, {0, 0, 0});
    fillR(r, 0, 0, SCREEN_W, HUD_H);
    fillR(r, 0, HUD_H + GAME_H, SCREEN_W, HUD_H);

    const float sideX = 3.0f;
    drawText(r, "SIDE-ONE", sideX, 1, 1, {255, 230, 60});
    std::snprintf(buf, sizeof(buf), "%d", g.score - g.phaseStart);
    const float sideRight = sideX + textWidth("SIDE-ONE", 1);
    drawText(r, buf, sideRight - textWidth(buf, 1), 9, 1, {255, 255, 255});
    {
        int m = std::clamp(g.multiplier, 1, 5);
        if (g_multBorder[0] && g_multSymbol[0]) {
            static const Color MULT_PAL[] = {
                {255, 60, 60}, {255,160,  0}, {255,235,  0}, { 60,255, 60},
                {  0,220,255}, { 80,100,255}, {220, 60,255}, {255, 60,160},
            };
            constexpr int N = 8;
            int bPhase = (int)(g.time / 0.1f) % N;
            int sPhase = (bPhase + N / 2) % N;
            Color bc = MULT_PAL[bPhase], sc = MULT_PAL[sPhase];
            Color borderC = (g.freezeTimer > 0.0f) ? g.freezeColor : bc;
            auto drawMult = [&](int idx, SDL_FRect dst) {
                SDL_SetTextureColorMod(g_multSymbol[idx], sc.r, sc.g, sc.b);
                SDL_RenderTexture(r, g_multSymbol[idx], nullptr, &dst);
                SDL_SetTextureColorMod(g_multSymbol[idx], 255, 255, 255);
                SDL_SetTextureColorMod(g_multBorder[idx], borderC.r, borderC.g, borderC.b);
                SDL_RenderTexture(r, g_multBorder[idx], nullptr, &dst);
                SDL_SetTextureColorMod(g_multBorder[idx], 255, 255, 255);
            };
            drawMult(0, {96,  0, 16, 16});
            drawMult(m, {112, 0, 16, 16});

            // Side bar bands share the border color cycle.
            int step = (g.freezeTimer > 0.0f)
                ? BAR_STEPS
                : std::clamp((int)(g.powerMeter * BAR_STEPS / POWER_NEEDED), 0, BAR_STEPS);
            if (step > 0 && g_barRight[0].tex && g_barLeft[0].tex) {
                Color barC = (g.freezeTimer > 0.0f) ? g.freezeColor : bc;
                struct Group { int start, count; float margin; };
                static constexpr Group GROUPS[3] = {{0,3,0}, {3,4,8}, {7,4,16}};
                for (const auto& grp : GROUPS) {
                    if (step <= grp.start) continue;
                    int idx = std::clamp(step - 1, grp.start, grp.start + grp.count - 1);
                    float bwR = (float)g_barRight[idx].w;
                    float rx  = 128.0f + grp.margin;
                    SDL_FRect rt{rx, 0, bwR, 8}, rb{rx, 8, bwR, 8};
                    SDL_SetTextureColorMod(g_barRight[idx].tex, barC.r, barC.g, barC.b);
                    SDL_RenderTexture(r, g_barRight[idx].tex, nullptr, &rt);
                    SDL_RenderTexture(r, g_barRight[idx].tex, nullptr, &rb);
                    SDL_SetTextureColorMod(g_barRight[idx].tex, 255, 255, 255);
                    float bwL = (float)g_barLeft[idx].w;
                    float lx  = 96.0f - grp.margin - bwL;
                    SDL_FRect lt{lx, 0, bwL, 8}, lb{lx, 8, bwL, 8};
                    SDL_SetTextureColorMod(g_barLeft[idx].tex, barC.r, barC.g, barC.b);
                    SDL_RenderTexture(r, g_barLeft[idx].tex, nullptr, &lt);
                    SDL_RenderTexture(r, g_barLeft[idx].tex, nullptr, &lb);
                    SDL_SetTextureColorMod(g_barLeft[idx].tex, 255, 255, 255);
                }
            }
        }
    }

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
    if (g.startAnim <= 0.0f || !g_startBg[0].tex) return;
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
        SDL_SetTextureColorMod(g_startBg[i].tex,
                               (Uint8)(bgc.r * fade), (Uint8)(bgc.g * fade),
                               (Uint8)(bgc.b * fade));
        SDL_RenderTexture(r, g_startBg[i].tex, nullptr, &d);
        SDL_SetTextureColorMod(g_startText[i].tex, 255, 255, 255);
        SDL_RenderTexture(r, g_startText[i].tex, nullptr, &d);
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

void drawOptions(SDL_Renderer* r, const Game& g) {
    useScreen(r);
    setCol(r, {0, 0, 0});
    SDL_RenderClear(r);
    drawTextCentered(r, "GAME OPTIONS", SCREEN_W / 2.0f, HUD_H + 26, 2, {255, 230, 60});

    const char* imgNames[OPT_IMG_STYLES] = {
        "CLASSIC", "PLACES1", "PLACES2", "PLACES3", "PLACES4",
        "PLACES5", "SPACE1", "SPACE2", "NO"
    };
    const char* labels[OPT_COUNT] = {"START LEVEL", "LIVES", "NEW IMAGES"};
    const float rowY0 = HUD_H + 80;
    for (int i = 0; i < OPT_COUNT; ++i) {
        bool sel = (g.optSel == i);
        Color c = sel ? Color{255, 255, 255} : Color{150, 150, 170};
        float y = rowY0 + i * 22;
        if (sel) {
            setCol(r, Color{255, 230, 60});
            for (int k = 0; k < 7; ++k)
                fillR(r, 14, y + k, (float)(4 - std::abs(k - 3)), 1.0f);
        }
        drawText(r, labels[i], 28, y, 1, c);
        char vbuf[24];
        if (i == 0)      std::snprintf(vbuf, sizeof(vbuf), "%d", g.startLevel);
        else if (i == 1) std::snprintf(vbuf, sizeof(vbuf), "%d", g.startLives);
        else             std::snprintf(vbuf, sizeof(vbuf), "%s", imgNames[g.imgStyle]);
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

    if (g.state == TITLE) {
        useScreen(r);
        setCol(r, {0, 0, 0});
        SDL_RenderClear(r);
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

    if (g.orb.active) drawPowerOrb(r, g.orb.x, g.orb.y, g.time, g.orb.family);

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

    bool frozen = g.freezeTimer > 0.0f;
    for (const Enemy& e : g.enemies) drawEnemy(r, e, g.time, frozen, g.freezeTimer);
    if (g.state == ROUNDCLEAR)
        drawJackVictory(r, g.p, g.clearTimer);
    else if (g.state != GAMEOVER)
        drawPlayer(r, g.p, g.time, g.death.active, g.death.phase, g.death.frame,
                   frozen, g.freezeColor);

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
