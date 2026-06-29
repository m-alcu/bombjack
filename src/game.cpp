#include "game.h"
#include "enemy.h"
#include <algorithm>
#include <cmath>
#include <random>

const leveldata::Layout& currentLayout(const Game& g) {
    int idx = (g.level - 1) % leveldata::LEVEL_COUNT;
    return leveldata::layouts[leveldata::levels[idx].layout];
}

const leveldata::LevelDef& levelDef(const Game& g) {
    return leveldata::levels[(g.level - 1) % leveldata::LEVEL_COUNT];
}

int currentScreen(const Game& g) {
    int idx = (g.level - 1) % leveldata::LEVEL_COUNT;
    return leveldata::levels[idx].screen;
}

void initPlatforms(Game& g) {
    g.platforms.clear();
    g.platforms.push_back({0, FLOOR_TOP, LOGW, LOGH - FLOOR_TOP});
    if (currentScreen(g) == 4) return;   // California screen has no platforms
    const leveldata::Layout& lay = currentLayout(g);
    for (int i = 0; i < lay.nplats; ++i) {
        const leveldata::Plat& p = lay.plats[i];
        g.platforms.push_back({p.x, p.y, p.w, p.h});
    }
}

void spawnBombs(Game& g) {
    g.bombs.clear();
    const leveldata::Layout& lay = currentLayout(g);
    for (int i = 0; i < lay.nbombs; ++i)
        g.bombs.push_back({lay.bombs[i].x, lay.bombs[i].y, false});
    g.bombsLeft = (int)g.bombs.size();
    g.litBomb   = -1;
    g.catched   = 0;
}

static Enemy makeBird(Game& g, float ang) {
    std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);
    float speed = birdSpeed(g.level);
    Enemy e;
    e.kind = EK_BIRD;
    e.r    = 11.0f;
    e.x    = BIRD_SPAWN_L + (BIRD_SPAWN_R - BIRD_SPAWN_L) * (0.5f + 0.5f * std::cos(ang));
    e.y    = BIRD_SPAWN_T + (BIRD_SPAWN_B - BIRD_SPAWN_T) * (0.5f + 0.5f * std::sin(ang));
    e.vx   = std::cos(ang) * speed + jitter(g.rng) * 10.0f;
    e.vy   = std::sin(ang) * speed + jitter(g.rng) * 10.0f;
    return e;
}

void spawnEnemies(Game& g) {
    g.enemies.clear();
    int idx   = (g.level - 1) % leveldata::LEVEL_COUNT;
    int birds = leveldata::levels[idx].birds;
    for (int i = 0; i < birds; ++i)
        g.enemies.push_back(makeBird(g, (i + 0.5f) / birds * 6.2831853f));
}

void restartEnemies(Game& g) {
    spawnEnemies(g);
    g.mummyTimer     = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx   = 0;
    g.freezeTimer    = 0.0f;
    g.killCount      = 0;
    g.orb.active     = false;
}

void resetPlayer(Game& g, bool invuln) {
    g.p.x        = LOGW / 2.0f - PW / 2.0f;
    g.p.y        = LOGH / 2.0f - PH / 2.0f;
    g.p.vx       = g.p.vy = 0.0f;
    g.p.onGround = false;
    g.p.invuln   = invuln ? INVULN_TIME : 0.0f;
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

void startRound(Game& g) {
    initPlatforms(g);
    spawnBombs(g);
    spawnEnemies(g);
    resetPlayer(g, true);
    g.orb.active     = false;
    g.freezeTimer    = 0.0f;
    g.killCount      = 0;
    g.death          = {};
    g.mummyTimer     = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx   = 0;
    g.popups.clear();
    g.explosions.clear();
    g.coinPickups.clear();
    g.bonusTakens.clear();
    g.phaseStart     = g.score;
    g.multiplier     = 1;
    g.bonus.active   = false;
    g.nextBonusScore = (g.score / BONUS_LIMIT + 1) * BONUS_LIMIT;
    g.startAnim      = START_TOTAL;
}

void optionAdjust(Game& g, int d) {
    if (g.optSel == 0)
        g.startLevel = std::clamp(g.startLevel + d, 1, leveldata::LEVEL_COUNT);
    else if (g.optSel == 1)
        g.startLives = std::clamp(g.startLives + d, 1, OPT_MAX_LIVES);
    else
        g.imgStyle = (g.imgStyle + d + OPT_IMG_STYLES) % OPT_IMG_STYLES;
}

void startGame(Game& g) {
    g.score     = 0;
    g.lives     = g.startLives;
    g.level     = g.startLevel;
    g.state     = PLAYING;
    g.bCoins    = 0;
    g.nextEAt   = BONUS_E_EVERY;
    g.livesLost = 0;
    initPlatforms(g);
    startRound(g);
}

int nextLitBomb(const Game& g, int orderMin) {
    for (int j = orderMin; j < (int)g.bombs.size(); ++j)
        if (!g.bombs[j].collected) return j;
    for (int j = 0; j < (int)g.bombs.size(); ++j)
        if (!g.bombs[j].collected) return j;
    return -1;
}

int specialBonusFor(int catched) {
    switch (catched) {
        case 20: return 10000;
        case 21: return 20000;
        case 22: return 30000;
        case 23: return 50000;
        default: return 0;
    }
}

void finishLevel(Game& g) {
    int bonus = specialBonusFor(g.catched);
    if (bonus > 0) {
        g.state        = SPECIALBONUS;
        g.sb.phase     = 0;
        g.sb.timer     = SB_BEGIN_TIME;
        g.sb.remaining = bonus;
        g.sb.catched   = g.catched;
    } else {
        g.state      = ROUNDCLEAR;
        g.clearTimer = CLEAR_TIME;
    }
}

// Drive the player's owned AnimSprite: pick the pose from his current state,
// position it, and advance its own animation clock by dt. This is the PoC of
// AnimSprite::update(dt) — the sprite owns its timing rather than reading the
// global clock the way the enemy sprites do.
static void driveJack(Game& g, float dt) {
    const Player& p = g.p;
    const bool moving = std::fabs(p.vx) > 1.0f;
    const bool left   = p.face < 0;
    const char* anim;
    if (p.onGround)       anim = moving ? (left ? "walk_l" : "walk_r") : "idle";
    else if (p.vy < 0.0f) anim = moving ? (left ? "fly_l"  : "fly_r")  : "fly";
    else                  anim = moving ? (left ? "fall_l" : "fall_r") : "fall";
    g.jackSprite.play(anim);
    g.jackSprite.setPosition(p.x + PW / 2.0f, p.y + PH - JACK_DRAW_H / 2.0f);
    g.jackSprite.update(dt);
}

void updatePlaying(Game& g, const Input& in, float dt) {
    Player& p = g.p;

    // Death sequence: run the sprite phases then decrement a life.
    if (g.death.active) {
        if (g.death.phase == DP_DANCING) {
            g.death.anim += dt;
            const float step = DEATH_DANCE_TOTAL / 3.0f;
            while (g.death.anim >= step) {
                g.death.anim -= step;
                g.death.frame = (g.death.frame + 1) % 3;
                if (g.death.frame == 0) {
                    g.death.loops++;
                    if (g.death.loops >= DEATH_DANCE_LOOPS) {
                        g.death.phase = DP_FALLING;
                        g.death.frame = 0;
                        g.death.anim  = 0.0f;
                        p.vx = 0.0f;
                        p.vy = 0.0f;
                        break;
                    }
                }
            }
        } else if (g.death.phase == DP_FALLING) {
            g.death.anim += dt;
            const float step = DEATH_PLF_TOTAL / 4.0f;
            while (g.death.anim >= step) {
                g.death.anim -= step;
                g.death.frame = (g.death.frame + 1) % 4;
            }
            p.vy += GRAVITY * dt;
            if (p.vy > MAXFALL) p.vy = MAXFALL;
            p.y += p.vy * dt;
            if (p.y + PH >= FLOOR_TOP) {
                p.y = FLOOR_TOP - PH;
                p.vy = 0.0f;
                p.onGround = true;
                g.death.phase = DP_DEAD;
                g.death.frame = 0;
                g.death.anim  = 0.0f;
            } else {
                p.onGround = false;
            }
        } else if (g.death.phase == DP_DEAD) {
            g.death.anim += dt;
            const float step = DEATH_DEAD_TOTAL / 4.0f;
            while (g.death.anim >= step) {
                g.death.anim -= step;
                if (g.death.frame < 3) g.death.frame++;
                else {
                    g.death.phase = DP_WAIT;
                    g.death.timer = DEATH_WAIT_TIME;
                    break;
                }
            }
        } else if (g.death.phase == DP_WAIT) {
            g.death.timer -= dt;
            if (g.death.timer <= 0.0f) {
                g.death = {};
                g.lives--;
                g.livesLost++;
                if (g.lives <= 0) {
                    g.state    = GAMEOVER;
                    g.go.x     = (p.x + PW / 2) * (float)GAME_W / LOGW;
                    g.go.y     = HUD_H + (p.y + PH / 2) * (float)GAME_H / LOGH;
                    g.go.stage = 0;
                    g.go.timer = GAMEOVER_HOLD;
                } else {
                    resetPlayer(g, true);
                    restartEnemies(g);
                    g.startAnim = START_TOTAL;
                }
            }
        }
        return;
    }

    // Hold simulation during the "START!" intro.
    if (g.startAnim > 0.0f) {
        g.startAnim -= dt;
        driveJack(g, dt);   // keep his sprite positioned/idle during the hold
        return;
    }

    if (p.invuln > 0) p.invuln -= dt;
    const bool oldOnGround = p.onGround;
    // Tracks actions that step the Power orb colour (jumps, wall bumps, landings).
    bool orbStep = in.jumpPressed;

    // Horizontal movement.
    p.vx = (in.right ? MOVE : 0.0f) - (in.left ? MOVE : 0.0f);
    if (p.vx > 0) p.face = 1;
    else if (p.vx < 0) p.face = -1;
    p.x += p.vx * dt;
    float clampedX = std::clamp(p.x, BORDER_SOLID_X, LOGW - PW - BORDER_SOLID_X);
    if (clampedX != p.x && p.vx != 0.0f) orbStep = true;
    p.x = clampedX;

    // Jump / flutter logic.
    if (in.jumpPressed) {
        if (p.onGround) {
            p.vy = JUMP_VEL;
            p.onGround = false;
        } else if (p.vy < 0) {
            p.vy = 0;
        } else {
            p.vy -= FLUTTER;
            if (p.vy < FLUTTER_MIN) p.vy = FLUTTER_MIN;
        }
    }

    float grav;
    if (in.jumpHeld && p.vy < 0)
        grav = in.up ? GLIDE : JUMP_HOLD_GRAV;
    else if (in.jumpHeld && p.vy > 0)
        grav = GLIDE;
    else
        grav = GRAVITY;
    p.vy += grav * dt;
    if (p.vy > MAXFALL) p.vy = MAXFALL;

    float oldTop    = p.y;
    float oldBottom = p.y + PH;
    p.y += p.vy * dt;
    if (p.y < BORDER_SOLID_Y) {
        p.y = BORDER_SOLID_Y;
        if (p.vy < 0) p.vy = 0;
    }

    // Platform collision (top and bottom faces).
    p.onGround = false;
    for (const SDL_FRect& pl : g.platforms) {
        bool overlapX = (p.x + PW - 3 > pl.x) && (p.x + 3 < pl.x + pl.w);
        if (!overlapX) continue;
        if (p.vy >= 0 && oldBottom <= pl.y + 1.0f && p.y + PH >= pl.y) {
            p.y = pl.y - PH;
            p.vy = 0;
            p.onGround = true;
            break;
        }
        if (p.vy < 0 && oldTop >= pl.y + pl.h - 1.0f && p.y <= pl.y + pl.h) {
            p.y = pl.y + pl.h;
            p.vy = 0;
            orbStep = true;
            break;
        }
    }
    if (!oldOnGround && p.onGround) orbStep = true;
    if (g.orb.active && orbStep) g.orb.family = (g.orb.family + 1) % 7;

    // Bomb collection.
    for (int i = 0; i < (int)g.bombs.size(); ++i) {
        Bomb& b = g.bombs[i];
        if (b.collected) continue;
        if (b.x > p.x - BOMB_HALF_W && b.x < p.x + PW + BOMB_HALF_W &&
            b.y > p.y - BOMB_HALF_H && b.y < p.y + PH + BOMB_HALF_H) {
            b.collected = true;
            g.bombsLeft--;
            g.explosions.push_back({b.x, b.y, 0.0f});
            const bool wasLit  = (i == g.litBomb);
            const bool relight = (g.litBomb < 0) || wasLit;
            int gain;
            if (wasLit) {
                g.catched++;
                gain = 200;
                g.powerMeter += 1.0f;
            } else {
                gain = 100;
                g.powerMeter += 0.5f;
            }
            if (relight) g.litBomb = nextLitBomb(g, i + 1);
            gain *= g.multiplier;
            g.score += gain;
            g.popups.push_back({b.x, b.y - 6, 0.0f, gain});
            if (!g.orb.active && g.freezeTimer <= 0 && g.powerMeter >= POWER_NEEDED) {
                g.powerMeter  -= POWER_NEEDED;
                g.orb.active   = true;
                g.orb.x        = LOGW / 2.0f;
                g.orb.y        = LOGH / 2.0f - 16.0f;
                g.orb.vx       = ORB_SPEED * 0.5f;
                g.orb.vy       = ORB_SPEED * 0.8660254f;
                g.orb.family   = 0;
            }
        }
    }

    // Power orb movement: bounces around the play area and off platforms.
    if (g.orb.active) {
        float oldOrbX = g.orb.x;
        float oldOrbY = g.orb.y;
        g.orb.x += g.orb.vx * dt;
        g.orb.y += g.orb.vy * dt;
        const float orbL   = BORDER_SOLID_X + ORB_R;
        const float orbRgt = LOGW - BORDER_SOLID_X - ORB_R;
        const float orbT   = BORDER_SOLID_Y + ORB_R;
        if (g.orb.x < orbL)   { g.orb.x = orbL;   g.orb.vx =  std::fabs(g.orb.vx); }
        if (g.orb.x > orbRgt) { g.orb.x = orbRgt; g.orb.vx = -std::fabs(g.orb.vx); }
        if (g.orb.y < orbT)   { g.orb.y = orbT;   g.orb.vy =  std::fabs(g.orb.vy); }
        if (g.orb.y > FLOOR_TOP - ORB_R) {
            g.orb.y  = FLOOR_TOP - ORB_R;
            g.orb.vy = -std::fabs(g.orb.vy);
        }
        for (const SDL_FRect& pl : g.platforms) {
            if (pl.y >= FLOOR_TOP - 1.0f) continue;
            if (!circleOverlapsRect(g.orb.x, g.orb.y, ORB_R, pl)) continue;
            const float eps = 0.001f;
            bool hitTop    = oldOrbY + ORB_R <= pl.y + eps       && g.orb.y + ORB_R > pl.y + eps;
            bool hitBottom = oldOrbY - ORB_R >= pl.y + pl.h - eps && g.orb.y - ORB_R < pl.y + pl.h - eps;
            if (hitTop) {
                g.orb.y  = pl.y - ORB_R;
                g.orb.vy = -std::fabs(g.orb.vy);
            } else if (hitBottom) {
                g.orb.y  = pl.y + pl.h + ORB_R;
                g.orb.vy =  std::fabs(g.orb.vy);
            } else {
                bool hitLeft  = oldOrbX + ORB_R <= pl.x + eps        && g.orb.x + ORB_R > pl.x + eps;
                bool hitRight = oldOrbX - ORB_R >= pl.x + pl.w - eps && g.orb.x - ORB_R < pl.x + pl.w - eps;
                if (hitLeft) {
                    g.orb.x  = pl.x - ORB_R;
                    g.orb.vx = -std::fabs(g.orb.vx);
                } else if (hitRight) {
                    g.orb.x  = pl.x + pl.w + ORB_R;
                    g.orb.vx =  std::fabs(g.orb.vx);
                } else if (std::fabs(g.orb.vy) >= std::fabs(g.orb.vx) && oldOrbY <= pl.y) {
                    g.orb.y  = pl.y - ORB_R;
                    g.orb.vy = -std::fabs(g.orb.vy);
                } else {
                    if (oldOrbX <= pl.x) {
                        g.orb.x  = pl.x - ORB_R;
                        g.orb.vx = -std::fabs(g.orb.vx);
                    } else {
                        g.orb.x  = pl.x + pl.w + ORB_R;
                        g.orb.vx =  std::fabs(g.orb.vx);
                    }
                }
            }
            break;
        }
    }

    // Power orb pickup: freeze all enemies and make them collectable.
    if (g.orb.active) {
        if (g.orb.x > p.x - ORB_R && g.orb.x < p.x + PW + ORB_R &&
            g.orb.y > p.y - ORB_R && g.orb.y < p.y + PH + ORB_R) {
            g.orb.active  = false;
            g.freezeTimer = FREEZE_TIME;
            g.killCount   = 0;
            int idx = (g.orb.family % (int)std::size(POWER_POINTS) +
                       (int)std::size(POWER_POINTS)) % (int)std::size(POWER_POINTS);
            g.freezeColor = POWER_COLORS[idx];
            int gain = POWER_POINTS[idx] * g.multiplier;
            g.score += gain;
            g.popups.push_back({g.orb.x, g.orb.y, 0.0f, gain});
        }
    }

    // Bonus coin threshold: offer a B/E/S coin every BONUS_LIMIT points.
    if (g.score >= g.nextBonusScore) {
        g.nextBonusScore += BONUS_LIMIT;
        if (!g.bonus.active && g.freezeTimer <= 0.0f) {
            int kind = -1;
            if ((int)(g.rng() % 100) < BONUS_S_CHANCE)    kind = BK_S;
            else if (g.bCoins + g.livesLost >= g.nextEAt) kind = BK_E;
            else if (g.multiplier < MAX_MULT)              kind = BK_B;
            if (kind >= 0) spawnBonus(g, kind);
        }
    }

    // Bonus coin patrol and pickup.
    if (g.bonus.active) {
        g.bonus.anim += dt;
        g.bonus.x    += g.bonus.vx * dt;
        const float lo = g.bonus.platX + BONUS_W * 0.5f;
        const float hi = g.bonus.platX + g.bonus.platW - BONUS_W * 0.5f;
        if (g.bonus.x < lo) { g.bonus.x = lo; g.bonus.vx =  std::fabs(g.bonus.vx); }
        if (g.bonus.x > hi) { g.bonus.x = hi; g.bonus.vx = -std::fabs(g.bonus.vx); }

        if (g.bonus.x > p.x - BONUS_W * 0.5f && g.bonus.x < p.x + PW + BONUS_W * 0.5f &&
            g.bonus.y > p.y - BONUS_H * 0.5f && g.bonus.y < p.y + PH + BONUS_H * 0.5f) {
            g.bonus.active = false;
            g.bonusTakens.push_back({g.bonus.x, g.bonus.y, 0.0f});
            switch (g.bonus.kind) {
                case BK_B: {
                    int gain = BONUS_POINTS * g.multiplier;
                    g.score += gain;
                    g.popups.push_back({g.bonus.x, g.bonus.y - 6, 0.0f, gain});
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
                    g.popups.push_back({g.bonus.x, g.bonus.y - 6, 0.0f, gain});
                    g.lives++;
                    g.state      = ROUNDCLEAR;
                    g.clearTimer = CLEAR_TIME;
                    break;
                }
            }
        }
    }

    // Age out floating score popups.
    for (auto it = g.popups.begin(); it != g.popups.end();) {
        it->age += dt;
        it->y -= 18.0f * dt;
        if (it->age > 1.0f) it = g.popups.erase(it); else ++it;
    }
    for (auto it = g.coinPickups.begin(); it != g.coinPickups.end();) {
        it->age += dt;
        if (it->age >= 4 * PICKCOIN_FRAME) it = g.coinPickups.erase(it); else ++it;
    }
    for (auto it = g.bonusTakens.begin(); it != g.bonusTakens.end();) {
        it->age += dt;
        if (it->age >= 6 * BONUSTAKEN_FRAME) it = g.bonusTakens.erase(it); else ++it;
    }
    for (auto it = g.explosions.begin(); it != g.explosions.end();) {
        it->age += dt;
        if (it->age >= 3 * EXPL_FRAME) it = g.explosions.erase(it); else ++it;
    }

    if (g.bombsLeft <= 0) {
        finishLevel(g);
        return;
    }

    const bool frozen = g.freezeTimer > 0.0f;
    if (frozen) g.freezeTimer -= dt;

    // Mummy drop-in schedule.
    if (!frozen) {
        const leveldata::LevelDef& L = levelDef(g);
        g.mummyTimer += dt;
        if (g.mummyTimer >= MUMMY_SPAWN_DELAY && g.mummiesSpawned < L.nmummies) {
            g.mummyTimer = 0.0f;
            g.mummiesSpawned++;
            spawnMummy(g);
        }
    }

    // Enemy update and player-enemy collision.
    float pcx = p.x + PW / 2, pcy = p.y + PH / 2;
    for (auto it = g.enemies.begin(); it != g.enemies.end();) {
        Enemy& e = *it;
        if (!frozen) {
            if (e.kind == EK_MUMMY) updateMummy(g, e, dt);
            else                    updateFlyer(g, e, dt, pcx, pcy);
        }
        // Intangible while flashing in / appearing / disappearing (except during
        // freeze, where appear/disappear enemies are shown as collectible coins).
        bool intangible = e.phase == EP_INIT ||
            (!frozen && (e.phase == EP_APPEAR || e.phase == EP_DISAPPEAR));
        if (intangible) { ++it; continue; }

        float ex = pcx - e.x, ey = pcy - e.y;
        bool touching = ex * ex + ey * ey < (e.r + 9.0f) * (e.r + 9.0f);
        if (frozen) {
            if (touching) {
                int idx  = std::min(g.killCount, (int)(std::size(KILL_POINTS)) - 1);
                int gain = KILL_POINTS[idx] * g.multiplier;
                g.score += gain;
                g.popups.push_back({e.x, e.y - 6, 0.0f, gain});
                g.coinPickups.push_back({e.x, e.y, 0.0f});
                g.killCount++;
                it = g.enemies.erase(it);
                continue;
            }
        } else if (p.invuln <= 0 && touching) {
            g.death.active = true;
            g.death.phase  = DP_DANCING;
            g.death.frame  = 0;
            g.death.loops  = 0;
            g.death.anim   = 0.0f;
            g.death.timer  = 0.0f;
            p.invuln   = 0.0f;
            p.onGround = false;
            p.vx       = 0.0f;
            p.vy       = 0.0f;
            return;
        }
        ++it;
    }

    driveJack(g, dt);   // animate/position the player's owned sprite
}

void update(Game& g, const Input& in, float dt) {
    g.time += dt;
    switch (g.state) {
        case TITLE:
            if (in.jumpPressed) startGame(g);
            break;
        case OPTIONS:
            break;
        case PLAYING:
            updatePlaying(g, in, dt);
            break;
        case ROUNDCLEAR:
            g.clearTimer -= dt;
            if (g.clearTimer <= 0) {
                g.level++;
                startRound(g);
                g.state = PLAYING;
            }
            break;
        case SPECIALBONUS:
            g.sb.timer -= dt;
            if (g.sb.phase == 0) {
                if (g.sb.timer <= 0) { g.sb.phase = 1; g.sb.timer = SB_SCORE_TIME; }
            } else if (g.sb.phase == 1) {
                while (g.sb.phase == 1 && g.sb.timer <= 0) {
                    g.sb.remaining -= 1000;
                    g.score        += 1000;
                    if (g.sb.remaining <= 0) { g.sb.phase = 2; g.sb.timer += SB_END_TIME; }
                    else                     { g.sb.timer += SB_SCORE_TIME; }
                }
            } else {
                if (g.sb.timer <= 0) {
                    g.level++;
                    startRound(g);
                    g.state = PLAYING;
                }
            }
            break;
        case GAMEOVER: {
            const float cx   = SCREEN_W / 2.0f;
            const float cy   = SCREEN_H / 2.0f;
            const float move = GAMEOVER_SLIDE * dt;
            if (g.go.stage == 0) {
                if (g.go.y < cy) g.go.y = std::min(cy, g.go.y + move);
                else             g.go.y = std::max(cy, g.go.y - move);
                if (g.go.y == cy) g.go.stage = 1;
            } else if (g.go.stage == 1) {
                if (g.go.x < cx) g.go.x = std::min(cx, g.go.x + move);
                else             g.go.x = std::max(cx, g.go.x - move);
                if (g.go.x == cx) g.go.stage = 2;
            } else {
                g.go.timer -= dt;
                if (g.go.timer <= 0.0f) g.state = TITLE;
            }
            break;
        }
    }
}
