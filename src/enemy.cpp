#include "enemy.h"
#include "game.h"
#include "levels_data.h"
#include <algorithm>
#include <cmath>
#include <random>

void spawnMummy(Game& g) {
    const leveldata::LevelDef& L  = levelDef(g);
    const leveldata::Layout&   lay = currentLayout(g);
    Enemy e;
    e.kind  = EK_MUMMY;
    e.phase = EP_INIT;
    e.timer = INIT_ENEMY_TIME;
    e.r     = 10.0f;
    e.x     = lay.mummyx[g.p.x < LOGW / 2 ? 1 : 0];
    float topY = FLOOR_TOP;
    for (const SDL_FRect& pl : g.platforms)
        if (e.x > pl.x && e.x < pl.x + pl.w) topY = std::min(topY, pl.y);
    e.y = std::min(MUMMY_SPAWN_Y, topY - MUMMY_HALF_H - MUMMY_DROP_GAP);
    e.vx = e.vy = 0.0f;
    e.bounces = L.bouncing;
    e.becomes = L.mummies[g.transformIdx % L.nmummies];
    std::uniform_int_distribution<int> pick(0, (int)std::size(BASIC_COLORS) - 1);
    e.spawnTint = BASIC_COLORS[pick(g.rng)];
    g.transformIdx++;
    g.enemies.push_back(e);
}

void transformMummy(Game& g, Enemy& e) {
    e.kind  = e.becomes;
    e.phase = EP_FLY;
    float s = FLY_SPEED + g.level * 2.0f;
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
}

void updateMummy(Game& g, Enemy& e, float dt) {
    if (e.phase == EP_INIT) {
        e.timer -= dt;
        if (e.timer <= 0) { e.phase = EP_APPEAR; e.timer = MUMMY_APPEAR_TIME; }
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
                    e.bounces = levelDef(g).bouncing;
                break;
            }
    }
    const SDL_FRect* ground = nullptr;
    for (const SDL_FRect& pl : g.platforms)
        if (e.x > pl.x && e.x < pl.x + pl.w && std::fabs((e.y + halfH) - pl.y) < 2.0f) {
            ground = &pl; break;
        }
    if (!ground) { e.phase = EP_FALL; return; }
    if (ground->y >= FLOOR_TOP - 1.0f) {
        e.phase = EP_DISAPPEAR;
        e.timer = MUMMY_DISAPPEAR_TIME;
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

void blockEnemy(Game& g, Enemy& e, float ox, float oy) {
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

bool flyerBlocked(const Game& g, float r, float x, float y, const Enemy* self) {
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

void updateBird(Game& g, Enemy& e, float dt, float pcx, float pcy) {
    const float s = birdSpeed(g.level);
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

void updateFlyer(Game& g, Enemy& e, float dt, float pcx, float pcy) {
    float s = FLY_SPEED + g.level * 2.0f;
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
