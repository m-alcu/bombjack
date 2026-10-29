#include "game/enemy.h"
#include "game/game.h"
#include "core/geometry.h"
#include "platform/audio.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>

// Bird travel speed for a round.
static float birdSpeed(int round) {
    float s = 72.0f + round * 8.0f;
    return (s < 150.0f ? s : 150.0f) * 0.5f;
}

static Enemy makeBird(Game& g, float ang) {
    std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);
    float speed = birdSpeed(g.round);
    Enemy e;
    e.kind = EK_BIRD;
    e.r    = 11.0f;
    e.x    = BIRD_SPAWN_L + (BIRD_SPAWN_R - BIRD_SPAWN_L) * (0.5f + 0.5f * std::cos(ang));
    e.y    = BIRD_SPAWN_T + (BIRD_SPAWN_B - BIRD_SPAWN_T) * (0.5f + 0.5f * std::sin(ang));
    e.vx   = std::cos(ang) * speed + jitter(g.rng) * 10.0f;
    e.vy   = std::sin(ang) * speed + jitter(g.rng) * 10.0f;
    startEnemyAnim(e);
    return e;
}

void spawnBirds(Game& g) {
    g.enemies.clear();
    int birds = currentEnemyDef(g).birds;
    for (int i = 0; i < birds; ++i)
        g.enemies.push_back(makeBird(g, (i + 0.5f) / birds * 6.2831853f));
}

void spawnMummy(Game& g) {
    const leveldata::EnemyDef&   L   = currentEnemyDef(g);
    const leveldata::PlatLayout& lay = currentPlatLayout(g);
    Enemy e;
    e.kind  = EK_MUMMY;
    e.phase = EP_INIT;
    e.timer = INIT_ENEMY_TIME;
    e.r     = 10.0f;
    // Arcade rule: pick one of the layout's 4 spawn points at random, on the
    // half of the screen away from Jack.
    const bool jackLeft = g.p.x + PW / 2.0f < LOGW / 2.0f;
    int cand[4], n = 0;
    for (int i = 0; i < 4; ++i)
        if ((lay.mummySpawn[i].x < leveldata::MUMMY_SPAWN_SPLIT_X) != jackLeft) cand[n++] = i;
    const leveldata::Pt& sp = lay.mummySpawn[n ? cand[g.rng() % n] : g.rng() % 4];
    e.x = sp.x;
    float topY = FLOOR_TOP;
    for (const SDL_FRect& pl : g.platforms)
        if (e.x > pl.x && e.x < pl.x + pl.w && pl.y > sp.y) topY = std::min(topY, pl.y);
    e.y = std::min(sp.y, topY - MUMMY_HALF_H - MUMMY_DROP_GAP);
    e.vx = e.vy = 0.0f;
    e.bounces = L.bouncing;
    e.becomes = L.mummies[g.transformIdx % L.nmummies];
    g.transformIdx++;
    g.enemies.push_back(e);
    audio::play(audio::SND_MUMMY_APPEAR);
}

static void transformMummy(Game& g, Enemy& e) {
    e.kind  = e.becomes;
    e.phase = EP_FLY;
    float s = FLY_SPEED + g.round * 2.0f;
    std::uniform_int_distribution<int> coin(0, 1);
    float sx = coin(g.rng) ? 1.0f : -1.0f, sy = coin(g.rng) ? 1.0f : -1.0f;
    switch (e.kind) {
        case EK_SPHERE: e.vx = 0;            e.vy = sy * s * 0.7f;  break;
        case EK_ORB:    e.vx = sx * s * 0.7f; e.vy = 0;             break;
        case EK_CLUB:   e.vx = sx * s * 0.5f; e.vy = sy * s * 0.5f; break;
        case EK_HORN:   e.vx = sx * s * 0.6f; e.vy = sy * s * 0.6f; break;
        case EK_UFO:    e.vx = e.vy = 0;                             break;
        default:        e.vx = sx * s * 0.5f; e.vy = sy * s * 0.5f; break;
    }
    e.y -= 6.0f;
    startEnemyAnim(e);
}

static void updateMummy(Game& g, Enemy& e, float dt) {
    if (e.phase == EP_INIT) {
        e.timer -= dt;
        if (e.timer <= 0) {
            e.phase = EP_APPEAR;
            e.timer = MUMMY_APPEAR_TIME;
            startEnemyAnim(e);
        }
        return;
    }
    if (e.phase == EP_APPEAR) {
        e.timer -= dt;
        if (e.timer <= 0) {
            e.phase = EP_WALK;
            e.vx = (e.x < LOGW / 2) ? MUMMY_WALK_SPEED : -MUMMY_WALK_SPEED;
        }
        return;
    }
    if (e.phase == EP_DISAPPEAR) {
        e.timer -= dt;
        if (e.timer <= 0) transformMummy(g, e);
        return;
    }
    const float halfH = MUMMY_HALF_H, halfW = 8.0f;
    e.vy += GRAVITY * dt;
    if (e.vy > MAXFALL) e.vy = MAXFALL;
    float feetOld = e.y + halfH;
    e.y += e.vy * dt;
    float feet = e.y + halfH;
    if (e.vy >= 0) {
        for (const SDL_FRect& pl : g.platforms)
            if (e.x > pl.x && e.x < pl.x + pl.w && feetOld <= pl.y + 1 && feet >= pl.y) {
                bool landedFromFall = e.phase == EP_FALL;
                e.y = pl.y - halfH; e.vy = 0;
                if (landedFromFall && pl.y < FLOOR_TOP - 1)
                    e.bounces = currentEnemyDef(g).bouncing;
                break;
            }
    }
    const SDL_FRect* ground = nullptr;
    for (const SDL_FRect& pl : g.platforms)
        if (e.x > pl.x && e.x < pl.x + pl.w && std::fabs((e.y + halfH) - pl.y) < 2.0f) {
            ground = &pl; break;
        }
    if (!ground) {
        if (audio::tagCount("mummy_fall") == 0)
            audio::play(audio::SND_MUMMY_FALL, "mummy_fall");
        e.phase = EP_FALL;
        return;
    }
    if (e.phase == EP_FALL) {
        // Arcade: cut the fall sound, play the landing thud, and on the floor
        // (where the mummy transforms) the transform sound.
        audio::stopTag("mummy_fall");
        audio::play(audio::SND_MUMMY_LAND);
        if (ground->y >= FLOOR_TOP - 1.0f) audio::play(audio::SND_MUMMY_TRANSFORM);
    }
    if (ground->y >= FLOOR_TOP - 1.0f) {
        e.phase = EP_DISAPPEAR;
        e.timer = INIT_ENEMY_TIME;
        e.vx = e.vy = 0.0f;
        return;
    }
    e.phase = EP_WALK;
    if (e.vx == 0) e.vx = MUMMY_WALK_SPEED;
    e.x += e.vx * dt;
    const float wallL = BORDER_SOLID_X + e.r, wallR = LOGW - BORDER_SOLID_X - e.r;
    bool hitWall = (e.vx < 0 && e.x <= wallL) || (e.vx > 0 && e.x >= wallR);
    e.x = std::clamp(e.x, wallL, wallR);
    if (hitWall) {
        if (e.bounces > 0) e.bounces--;
        e.vx = -e.vx;
    } else {
        float ahead = e.x + (e.vx > 0 ? halfW : -halfW);
        bool atEdge = ahead <= ground->x || ahead >= ground->x + ground->w;
        if (atEdge && e.bounces > 0) { e.vx = -e.vx; e.bounces--; }
    }
}

static void blockEnemy(Game& g, Enemy& e, float ox, float oy) {
    const float L = BORDER_SOLID_X + e.r, R = LOGW - BORDER_SOLID_X - e.r;
    const float T = BORDER_SOLID_Y + e.r, B = FLOOR_TOP - e.r;
    if (e.x < L) { e.x = L; e.vx =  std::fabs(e.vx); }
    if (e.x > R) { e.x = R; e.vx = -std::fabs(e.vx); }
    if (e.y < T) { e.y = T; e.vy =  std::fabs(e.vy); }
    if (e.y > B) { e.y = B; e.vy = -std::fabs(e.vy); }
    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;
        if (!circleOverlapsRect(e.x, e.y, e.r, pl)) continue;
        const float eps = 0.001f;
        bool hitTop    = oy + e.r <= pl.y + eps           && e.y + e.r > pl.y + eps;
        bool hitBottom = oy - e.r >= pl.y + pl.h - eps    && e.y - e.r < pl.y + pl.h - eps;
        if (hitTop) {
            e.y = pl.y - e.r;          e.vy = -std::fabs(e.vy);
        } else if (hitBottom) {
            e.y = pl.y + pl.h + e.r;   e.vy =  std::fabs(e.vy);
        } else {
            bool hitLeft  = ox + e.r <= pl.x + eps        && e.x + e.r > pl.x + eps;
            bool hitRight = ox - e.r >= pl.x + pl.w - eps && e.x - e.r < pl.x + pl.w - eps;
            if (hitLeft) {
                e.x = pl.x - e.r;          e.vx = -std::fabs(e.vx);
            } else if (hitRight) {
                e.x = pl.x + pl.w + e.r;   e.vx =  std::fabs(e.vx);
            } else if (oy <= pl.y) {
                e.y = pl.y - e.r;          e.vy = -std::fabs(e.vy);
            } else if (ox <= pl.x) {
                e.x = pl.x - e.r;          e.vx = -std::fabs(e.vx);
            } else {
                e.x = pl.x + pl.w + e.r;   e.vx =  std::fabs(e.vx);
            }
        }
    }
}

static bool flyerBlocked(const Game& g, float r, float x, float y, const Enemy* self = nullptr) {
    const float L = BORDER_SOLID_X + r, R = LOGW - BORDER_SOLID_X - r;
    const float T = BIRD_SPAWN_T,       B = BIRD_SPAWN_B;
    if (x < L || x > R || y < T || y > B) return true;
    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;
        if (circleOverlapsRect(x, y, r, pl)) return true;
    }
    for (const Enemy& other : g.enemies) {
        if (&other == self || other.kind != EK_BIRD) continue;
        float bx = x - other.x, by = y - other.y;
        if (bx * bx + by * by < (r + other.r) * (r + other.r)) return true;
    }
    return false;
}

static void updateBird(Game& g, Enemy& e, float dt, float pcx, float pcy) {
    const float s = birdSpeed(g.round);
    if (++e.tick >= BIRD_DECIDE_FRAMES || (e.tgtX == 0.0f && e.tgtY == 0.0f)) {
        e.tick = 0;
        e.tgtX = pcx; e.tgtY = pcy;
        if (g.platforms.size() < 2)
            e.tgtY = std::clamp(e.tgtY, BIRD_SPAWN_T * 2.0f, BIRD_SPAWN_B);
    }
    float dx = e.tgtX - e.x, dy = e.tgtY - e.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) { e.vx = e.vy = 0.0f; return; }
    const float ux = dx / len, uy = dy / len;
    const float step = s * dt;
    const float nx = e.x + ux * step, ny = e.y + uy * step;
    if (!flyerBlocked(g, e.r, nx, ny, &e)) {
        e.x = nx; e.y = ny;
        e.vx = ux * s; e.vy = uy * s;
        return;
    }
    const int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
    int order[2][2] = {{sx, 0}, {0, sy}};
    if (std::fabs(dy) > std::fabs(dx)) {
        order[0][0] = 0;  order[0][1] = sy;
        order[1][0] = sx; order[1][1] = 0;
    }
    for (const auto& m : order) {
        if (m[0] == 0 && m[1] == 0) continue;
        const float tx = e.x + m[0] * step, ty = e.y + m[1] * step;
        if (!flyerBlocked(g, e.r, tx, ty, &e)) {
            e.x = tx; e.y = ty;
            e.vx = m[0] * s; e.vy = m[1] * s;
            return;
        }
    }
    e.vx = ux * s; e.vy = uy * s;
}

static void updateFlyer(Game& g, Enemy& e, float dt, float pcx, float pcy) {
    float s = FLY_SPEED + g.round * 2.0f;
    switch (e.kind) {
        case EK_BIRD:
            updateBird(g, e, dt, pcx, pcy); return;
        case EK_SPHERE:
            e.vx = std::clamp(e.vx + (pcx > e.x ? 1 : -1) * s*2*dt, -s*0.7f, s*0.7f); break;
        case EK_ORB:
            e.vy = std::clamp(e.vy + (pcy > e.y ? 1 : -1) * s*2*dt, -s*0.7f, s*0.7f); break;
        case EK_CLUB:
            e.vx = std::clamp(e.vx + (pcx > e.x ? 1 : -1) * s*1.5f*dt, -s*0.6f, s*0.6f);
            e.vy = std::clamp(e.vy + (pcy > e.y ? 1 : -1) * s*1.5f*dt, -s*0.6f, s*0.6f); break;
        case EK_HORN: break;
        case EK_UFO: {
            float dx = pcx - e.x, dy = pcy - e.y, len = std::sqrt(dx*dx + dy*dy) + 1e-3f;
            float k = (len < 70 ? 0.4f : 1.0f) * s;
            e.vx = dx/len * k; e.vy = dy/len * k; break;
        }
        default: break;
    }
    float ox = e.x, oy = e.y;
    e.x += e.vx * dt; e.y += e.vy * dt;
    blockEnemy(g, e, ox, oy);
}


bool updateEnemies(Game& g, float dt, bool frozen) {
    // Mummy drop-in schedule.
    if (!frozen) {
        const leveldata::EnemyDef& L = currentEnemyDef(g);
        g.mummyTimer += dt;
        if (g.mummyTimer >= MUMMY_SPAWN_DELAY && g.mummiesSpawned < L.nmummies) {
            g.mummyTimer = 0.0f;
            g.mummiesSpawned++;
            spawnMummy(g);
        }
    }

    const Player& p = g.p;
    float pcx = p.x + PW / 2, pcy = p.y + PH / 2;
    for (auto it = g.enemies.begin(); it != g.enemies.end();) {
        Enemy& e = *it;
        if (!frozen) {
            // Animate first: an enemy that transforms in its update shows its
            // first pose for a frame before stepping, as on the arcade.
            animateEnemy(e, dt);
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
                audio::play(audio::SND_KILL_ENEMY, "kill_ennemy", false, nullptr, 0.5f);
                it = g.enemies.erase(it);
                continue;
            }
        } else if (p.invuln <= 0 && touching) {
            hitJack(g);
            return true;
        }
        ++it;
    }
    return false;
}
