#include "game/bonus.h"
#include "game/game.h"
#include "platform/audio.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Coin bounce: the arcade plays it on one channel, so a new bounce cuts the last.
static void bounceSound(float gain) {
    audio::stopTag("coin_bounce");
    audio::play(audio::SND_BONUS, "coin_bounce", false, nullptr, gain);
}

void spawnBonus(Game& g, int kind) {
    g.bonus.kind = kind;
    std::vector<const SDL_FRect*> eligible;
    for (const SDL_FRect& pl : g.platforms)
        if (pl.y < FLOOR_TOP - 1.0f) eligible.push_back(&pl);

    if (!eligible.empty()) {
        const SDL_FRect* pl = eligible[g.rng() % eligible.size()];
        g.bonus.platX = pl->x;
        g.bonus.platW = pl->w;
        g.bonus.y     = pl->y - BONUS_H * 0.5f - 1.0f;
    } else {
        g.bonus.platX = BORDER_SOLID_X;
        g.bonus.platW = LOGW - 2 * BORDER_SOLID_X;
        g.bonus.y     = FLOOR_TOP - BONUS_H * 0.5f - 1.0f;
    }
    g.bonus.x      = g.bonus.platX + g.bonus.platW * 0.5f;
    g.bonus.vx     = (g.rng() & 1) ? BONUS_SPEED : -BONUS_SPEED;
    g.bonus.anim   = 0.0f;
    g.bonus.active = true;
}

// Offer a B/E/S coin every BONUS_LIMIT points.
static void offerBonus(Game& g) {
    if (g.score < g.nextBonusScore) return;
    g.nextBonusScore += BONUS_LIMIT;
    if (g.bonus.active || g.freezeTimer > 0.0f) return;
    int kind = -1;
    if ((int)(g.rng() % 100) < BONUS_S_CHANCE)    kind = BK_S;
    else if (g.bCoins + g.livesLost >= g.nextEAt) kind = BK_E;
    else if (g.multiplier < MAX_MULT)              kind = BK_B;
    if (kind >= 0) spawnBonus(g, kind);
}

static void takeBonus(Game& g) {
    BonusCoin& c = g.bonus;
    c.active = false;
    audio::play(audio::SND_TAKE_BONUS);
    g.bonusTakens.push_back({c.x, c.y, 0.0f});
    switch (c.kind) {
        case BK_B: {
            int gain = BONUS_POINTS * g.multiplier;
            g.score += gain;
            g.popups.push_back({c.x, c.y - 6, 0.0f, gain});
            g.multiplier = std::min(g.multiplier + 1, MAX_MULT);
            g.bCoins++;
            break;
        }
        case BK_E:
            g.lives++;
            g.nextEAt += BONUS_E_EVERY;
            break;
        case BK_S: {
            int gain = BONUS_S_POINTS * g.multiplier;
            g.score += gain;
            g.popups.push_back({c.x, c.y - 6, 0.0f, gain});
            g.lives++;
            audio::stopTag("music");
            audio::stopTag("power");
            audio::play(audio::SND_GOT_SPECIAL, "special_coin");
            g.state       = LUCKY;   // "YOU ARE LUCKY / ONE MORE PLAY" screen
            g.lucky.phase = 0;
            g.lucky.timer = 0.0f;
            break;
        }
    }
}

void updateBonus(Game& g, float dt) {
    offerBonus(g);

    // In the demo the coin is replayed from the arcade run (attract.cpp).
    BonusCoin& c = g.bonus;
    if (!c.active || g.demo.active) return;
    c.anim += dt;
    c.x    += c.vx * dt;
    const float lo = c.platX + BONUS_W * 0.5f;
    const float hi = c.platX + c.platW - BONUS_W * 0.5f;
    const float bounceGain = c.kind == BK_S ? 0.5f : 1.0f;  // S bonus quieter
    if (c.x < lo) { c.x = lo; c.vx =  std::fabs(c.vx); bounceSound(bounceGain); }
    if (c.x > hi) { c.x = hi; c.vx = -std::fabs(c.vx); bounceSound(bounceGain); }

    const Player& p = g.p;
    if (c.x > p.x - BONUS_W * 0.5f && c.x < p.x + PW + BONUS_W * 0.5f &&
        c.y > p.y - BONUS_H * 0.5f && c.y < p.y + PH + BONUS_H * 0.5f)
        takeBonus(g);
}
