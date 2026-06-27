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
    g.litBomb = -1;
    g.catched = 0;
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
    g.mummyTimer    = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx  = 0;
    g.freezeTimer   = 0.0f;
    g.killCount     = 0;
    g.orbActive     = false;
}

void resetPlayer(Game& g, bool invuln) {
    g.p.x       = LOGW / 2.0f - PW / 2.0f;
    g.p.y       = LOGH / 2.0f - PH / 2.0f;
    g.p.vx      = g.p.vy = 0.0f;
    g.p.onGround = false;
    g.p.invuln  = invuln ? INVULN_TIME : 0.0f;
}

void spawnBonus(Game& g, int kind) {
    g.bonusKind = kind;
    std::vector<const SDL_FRect*> eligible;
    for (const SDL_FRect& pl : g.platforms)
        if (pl.y < FLOOR_TOP - 1.0f) eligible.push_back(&pl);

    if (!eligible.empty()) {
        const SDL_FRect* pl = eligible[g.rng() % eligible.size()];
        g.bonusPlatX = pl->x;
        g.bonusPlatW = pl->w;
        g.bonusY     = pl->y - BONUS_H * 0.5f - 1.0f;
    } else {
        g.bonusPlatX = BORDER_SOLID_X;
        g.bonusPlatW = LOGW - 2 * BORDER_SOLID_X;
        g.bonusY     = FLOOR_TOP - BONUS_H * 0.5f - 1.0f;
    }
    g.bonusX   = g.bonusPlatX + g.bonusPlatW * 0.5f;
    g.bonusVx  = (g.rng() & 1) ? BONUS_SPEED : -BONUS_SPEED;
    g.bonusAnim = 0.0f;
    g.bonusActive = true;
}

void startRound(Game& g) {
    initPlatforms(g);
    spawnBombs(g);
    spawnEnemies(g);
    resetPlayer(g, true);
    g.orbActive      = false;
    g.freezeTimer    = 0.0f;
    g.killCount      = 0;
    g.playerDying    = false;
    g.deathTimer     = 0.0f;
    g.deathPhase     = DP_NONE;
    g.deathFrame     = 0;
    g.deathLoops     = 0;
    g.deathAnim      = 0.0f;
    g.mummyTimer     = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx   = 0;
    g.popups.clear();
    g.explosions.clear();
    g.coinPickups.clear();
    g.bonusTakens.clear();
    g.phaseStart     = g.score;
    g.multiplier     = 1;
    g.bonusActive    = false;
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
    g.score    = 0;
    g.lives    = g.startLives;
    g.level    = g.startLevel;
    g.state    = PLAYING;
    g.bCoins   = 0;
    g.nextEAt  = BONUS_E_EVERY;
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
        g.sbState      = 0;
        g.sbTimer      = SB_BEGIN_TIME;
        g.sbRemaining  = bonus;
        g.sbCatched    = g.catched;
    } else {
        g.state      = ROUNDCLEAR;
        g.clearTimer = CLEAR_TIME;
    }
}

void updatePlaying(Game& g, const Input& in, float dt) {
    Player& p = g.p;

    // Death sequence: run the sprite phases then decrement a life.
    if (g.playerDying) {
        if (g.deathPhase == DP_DANCING) {
            g.deathAnim += dt;
            const float step = DEATH_DANCE_TOTAL / 3.0f;
            while (g.deathAnim >= step) {
                g.deathAnim -= step;
                g.deathFrame = (g.deathFrame + 1) % 3;
                if (g.deathFrame == 0) {
                    g.deathLoops++;
                    if (g.deathLoops >= DEATH_DANCE_LOOPS) {
                        g.deathPhase = DP_FALLING;
                        g.deathFrame = 0;
                        g.deathAnim  = 0.0f;
                        p.vx = 0.0f;
                        p.vy = 0.0f;
                        break;
                    }
                }
            }
        } else if (g.deathPhase == DP_FALLING) {
            g.deathAnim += dt;
            const float step = DEATH_PLF_TOTAL / 4.0f;
            while (g.deathAnim >= step) {
                g.deathAnim -= step;
                g.deathFrame = (g.deathFrame + 1) % 4;
            }
            p.vy += GRAVITY * dt;
            if (p.vy > MAXFALL) p.vy = MAXFALL;
            p.y += p.vy * dt;
            if (p.y + PH >= FLOOR_TOP) {
                p.y = FLOOR_TOP - PH;
                p.vy = 0.0f;
                p.onGround = true;
                g.deathPhase = DP_DEAD;
                g.deathFrame = 0;
                g.deathAnim  = 0.0f;
            } else {
                p.onGround = false;
            }
        } else if (g.deathPhase == DP_DEAD) {
            g.deathAnim += dt;
            const float step = DEATH_DEAD_TOTAL / 4.0f;
            while (g.deathAnim >= step) {
                g.deathAnim -= step;
                if (g.deathFrame < 3) g.deathFrame++;
                else {
                    g.deathPhase = DP_WAIT;
                    g.deathTimer = DEATH_WAIT_TIME;
                    break;
                }
            }
        } else if (g.deathPhase == DP_WAIT) {
            g.deathTimer -= dt;
            if (g.deathTimer <= 0.0f) {
                g.playerDying = false;
                g.deathPhase  = DP_NONE;
                g.lives--;
                g.livesLost++;
                if (g.lives <= 0) {
                    g.state   = GAMEOVER;
                    g.goX     = (p.x + PW / 2) * (float)GAME_W / LOGW;
                    g.goY     = HUD_H + (p.y + PH / 2) * (float)GAME_H / LOGH;
                    g.goStage = 0;
                    g.goTimer = GAMEOVER_HOLD;
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
    if (g.orbActive && orbStep) g.orbFamily = (g.orbFamily + 1) % 7;

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
            if (!g.orbActive && g.freezeTimer <= 0 && g.powerMeter >= POWER_NEEDED) {
                g.powerMeter -= POWER_NEEDED;
                g.orbActive  = true;
                g.orbX       = LOGW / 2.0f;
                g.orbY       = LOGH / 2.0f - 16.0f;
                g.orbVx      = ORB_SPEED * 0.5f;
                g.orbVy      = ORB_SPEED * 0.8660254f;
                g.orbFamily  = 0;
            }
        }
    }

    // Power orb movement: bounces around the play area and off platforms.
    if (g.orbActive) {
        float oldOrbX = g.orbX;
        float oldOrbY = g.orbY;
        g.orbX += g.orbVx * dt;
        g.orbY += g.orbVy * dt;
        const float orbL   = BORDER_SOLID_X + ORB_R;
        const float orbRgt = LOGW - BORDER_SOLID_X - ORB_R;
        const float orbT   = BORDER_SOLID_Y + ORB_R;
        if (g.orbX < orbL)   { g.orbX = orbL;   g.orbVx =  std::fabs(g.orbVx); }
        if (g.orbX > orbRgt) { g.orbX = orbRgt; g.orbVx = -std::fabs(g.orbVx); }
        if (g.orbY < orbT)   { g.orbY = orbT;   g.orbVy =  std::fabs(g.orbVy); }
        if (g.orbY > FLOOR_TOP - ORB_R) {
            g.orbY = FLOOR_TOP - ORB_R;
            g.orbVy = -std::fabs(g.orbVy);
        }
        for (const SDL_FRect& pl : g.platforms) {
            if (pl.y >= FLOOR_TOP - 1.0f) continue;
            if (!circleOverlapsRect(g.orbX, g.orbY, ORB_R, pl)) continue;
            const float eps = 0.001f;
            bool hitTop    = oldOrbY + ORB_R <= pl.y + eps       && g.orbY + ORB_R > pl.y + eps;
            bool hitBottom = oldOrbY - ORB_R >= pl.y + pl.h - eps && g.orbY - ORB_R < pl.y + pl.h - eps;
            if (hitTop) {
                g.orbY  = pl.y - ORB_R;
                g.orbVy = -std::fabs(g.orbVy);
            } else if (hitBottom) {
                g.orbY  = pl.y + pl.h + ORB_R;
                g.orbVy =  std::fabs(g.orbVy);
            } else {
                bool hitLeft  = oldOrbX + ORB_R <= pl.x + eps        && g.orbX + ORB_R > pl.x + eps;
                bool hitRight = oldOrbX - ORB_R >= pl.x + pl.w - eps && g.orbX - ORB_R < pl.x + pl.w - eps;
                if (hitLeft) {
                    g.orbX  = pl.x - ORB_R;
                    g.orbVx = -std::fabs(g.orbVx);
                } else if (hitRight) {
                    g.orbX  = pl.x + pl.w + ORB_R;
                    g.orbVx =  std::fabs(g.orbVx);
                } else if (std::fabs(g.orbVy) >= std::fabs(g.orbVx) && oldOrbY <= pl.y) {
                    g.orbY  = pl.y - ORB_R;
                    g.orbVy = -std::fabs(g.orbVy);
                } else {
                    if (oldOrbX <= pl.x) {
                        g.orbX  = pl.x - ORB_R;
                        g.orbVx = -std::fabs(g.orbVx);
                    } else {
                        g.orbX  = pl.x + pl.w + ORB_R;
                        g.orbVx =  std::fabs(g.orbVx);
                    }
                }
            }
            break;
        }
    }

    // Power orb pickup: freeze all enemies and make them collectable.
    if (g.orbActive) {
        if (g.orbX > p.x - ORB_R && g.orbX < p.x + PW + ORB_R &&
            g.orbY > p.y - ORB_R && g.orbY < p.y + PH + ORB_R) {
            g.orbActive   = false;
            g.freezeTimer = FREEZE_TIME;
            g.killCount   = 0;
            int idx = (g.orbFamily % (int)std::size(POWER_POINTS) +
                       (int)std::size(POWER_POINTS)) % (int)std::size(POWER_POINTS);
            g.freezeColor = POWER_COLORS[idx];
            int gain = POWER_POINTS[idx] * g.multiplier;
            g.score += gain;
            g.popups.push_back({g.orbX, g.orbY, 0.0f, gain});
        }
    }

    // Bonus coin threshold: offer a B/E/S coin every BONUS_LIMIT points.
    if (g.score >= g.nextBonusScore) {
        g.nextBonusScore += BONUS_LIMIT;
        if (!g.bonusActive && g.freezeTimer <= 0.0f) {
            int kind = -1;
            if ((int)(g.rng() % 100) < BONUS_S_CHANCE)    kind = BK_S;
            else if (g.bCoins + g.livesLost >= g.nextEAt) kind = BK_E;
            else if (g.multiplier < MAX_MULT)              kind = BK_B;
            if (kind >= 0) spawnBonus(g, kind);
        }
    }

    // Bonus coin patrol and pickup.
    if (g.bonusActive) {
        g.bonusAnim += dt;
        g.bonusX += g.bonusVx * dt;
        const float lo = g.bonusPlatX + BONUS_W * 0.5f;
        const float hi = g.bonusPlatX + g.bonusPlatW - BONUS_W * 0.5f;
        if (g.bonusX < lo) { g.bonusX = lo; g.bonusVx =  std::fabs(g.bonusVx); }
        if (g.bonusX > hi) { g.bonusX = hi; g.bonusVx = -std::fabs(g.bonusVx); }

        if (g.bonusX > p.x - BONUS_W * 0.5f && g.bonusX < p.x + PW + BONUS_W * 0.5f &&
            g.bonusY > p.y - BONUS_H * 0.5f && g.bonusY < p.y + PH + BONUS_H * 0.5f) {
            g.bonusActive = false;
            g.bonusTakens.push_back({g.bonusX, g.bonusY, 0.0f});
            switch (g.bonusKind) {
                case BK_B: {
                    int gain = BONUS_POINTS * g.multiplier;
                    g.score += gain;
                    g.popups.push_back({g.bonusX, g.bonusY - 6, 0.0f, gain});
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
                    g.popups.push_back({g.bonusX, g.bonusY - 6, 0.0f, gain});
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
            g.playerDying = true;
            g.deathPhase  = DP_DANCING;
            g.deathFrame  = 0;
            g.deathLoops  = 0;
            g.deathAnim   = 0.0f;
            g.deathTimer  = 0.0f;
            p.invuln   = 0.0f;
            p.onGround = false;
            p.vx       = 0.0f;
            p.vy       = 0.0f;
            return;
        }
        ++it;
    }
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
            g.sbTimer -= dt;
            if (g.sbState == 0) {
                if (g.sbTimer <= 0) { g.sbState = 1; g.sbTimer = SB_SCORE_TIME; }
            } else if (g.sbState == 1) {
                while (g.sbState == 1 && g.sbTimer <= 0) {
                    g.sbRemaining -= 1000;
                    g.score       += 1000;
                    if (g.sbRemaining <= 0) { g.sbState = 2; g.sbTimer += SB_END_TIME; }
                    else                    { g.sbTimer += SB_SCORE_TIME; }
                }
            } else {
                if (g.sbTimer <= 0) {
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
            if (g.goStage == 0) {
                if (g.goY < cy) g.goY = std::min(cy, g.goY + move);
                else            g.goY = std::max(cy, g.goY - move);
                if (g.goY == cy) g.goStage = 1;
            } else if (g.goStage == 1) {
                if (g.goX < cx) g.goX = std::min(cx, g.goX + move);
                else            g.goX = std::max(cx, g.goX - move);
                if (g.goX == cx) g.goStage = 2;
            } else {
                g.goTimer -= dt;
                if (g.goTimer <= 0.0f) g.state = TITLE;
            }
            break;
        }
    }
}
