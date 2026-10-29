#include "enemy.h"
#include "audio.h"
#include "game.h"
#include "levels_data.h"
#include <algorithm>
#include <cmath>
#include <random>

void spawnMummy(Game& g) {
    const leveldata::EnemyDef&   L   = levelDef(g);
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
    startEnemyAnim(e);
}

void updateMummy(Game& g, Enemy& e, float dt) {
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
                    e.bounces = levelDef(g).bouncing;
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

// ---------------------------------------------------------------------------
// Enemy animation, as the arcade steps it (main ROM). Every enemy carries the
// ROM sprite code it shows; these tables and per-frame steps pick it.

// Mummy (0x3728 walk, 0x377A fall; tables at 0x3E67): a new pose every 6
// frames walking, 4 falling. A change of walking direction (and the first
// step after appearing) shows the standing pose 0x04 for 6 frames first.
static constexpr uint8_t MUMMY_FALL[4]    = {0x04, 0x05, 0x04, 0x05};
static constexpr uint8_t MUMMY_WALK[2][4] = {{0x06, 0x07, 0x08, 0x07},   // right
                                             {0x09, 0x0A, 0x0B, 0x0A}};  // left

// Bird (0x3B7A; 0x3ECC): flap one pose every 6 frames, sideways or up/down.
static constexpr uint8_t BIRD_FLAP[2][3] = {{0x29, 0x2A, 0x2B}, {0x2C, 0x2D, 0x2E}};

// Bird heading change (0x34CB; sequences at 0x3E78), [from][to] for headings
// 0 right, 1 left, 2 up, 3 down: flip, poses shown 6 frames each, 0xFF, then
// the flip and pose it flies on with. Flip is 0x80 (attribute bit 7).
static constexpr uint8_t BIRD_TURN[4][4][7] = {
    {{0},                                       {0x00, 0x30, 0x2D, 0x2F, 0xFF, 0x00, 0x29},
     {0x00, 0x30, 0x2D, 0x2E, 0xFF, 0x00, 0x2C}, {0x00, 0x30, 0x2D, 0x2E, 0xFF, 0x00, 0x2C}},
    {{0x80, 0x30, 0x2D, 0x2F, 0xFF, 0x80, 0x29}, {0},
     {0x80, 0x30, 0x2D, 0x2E, 0xFF, 0x00, 0x2C}, {0x80, 0x30, 0x2D, 0x2E, 0xFF, 0x00, 0x2C}},
    {{0x80, 0x2F, 0x29, 0x2A, 0xFF, 0x80, 0x2B}, {0x00, 0x2F, 0x29, 0x2A, 0xFF, 0x00, 0x2B},
     {0},                                       {0x00, 0x2C, 0x2D, 0x2E, 0xFF, 0x00, 0x2D}},
    {{0x80, 0x2F, 0x29, 0x2A, 0xFF, 0x80, 0x2B}, {0x00, 0x2F, 0x29, 0x2A, 0xFF, 0x00, 0x2B},
     {0x00, 0x2C, 0x2D, 0x2E, 0xFF, 0x00, 0x2D}, {0}},
};

// Transformed chasers (0x2AAA; tables at 0x2EA6): frames per pose, the pose
// shown before the first step (0x3ED2), and the cycle moving left / right.
// Some flip the left cycle to go right, others play it backwards.
struct ChaserAnim {
    int     period, n;
    uint8_t first;
    uint8_t left[7], right[7];
    bool    flipRight;
};
static const ChaserAnim& chaserAnim(int kind) {
    static constexpr ChaserAnim HORN   = {7, 4, 0x50, {0x50, 0x51, 0x52, 0x53},
                                          {0x50, 0x51, 0x52, 0x53}, true};
    static constexpr ChaserAnim SPHERE = {2, 5, 0x34, {0x67, 0x68, 0x69, 0x6A, 0x6B},
                                          {0x67, 0x68, 0x69, 0x6A, 0x6B}, false};
    static constexpr ChaserAnim ORB    = {3, 7, 0x54, {0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A},
                                          {0x5A, 0x59, 0x58, 0x57, 0x56, 0x55, 0x54}, false};
    static constexpr ChaserAnim CLUB   = {6, 4, 0x38, {0x38, 0x39, 0x3A, 0x3B},
                                          {0x38, 0x39, 0x3A, 0x3B}, true};
    static constexpr ChaserAnim UFO    = {6, 6, 0x60, {0x60, 0x61, 0x62, 0x63, 0x64, 0x65},
                                          {0x65, 0x64, 0x63, 0x62, 0x61, 0x60}, false};
    switch (kind) {
        case EK_HORN:   return HORN;
        case EK_SPHERE: return SPHERE;
        case EK_ORB:    return ORB;
        case EK_CLUB:   return CLUB;
        default:        return UFO;
    }
}

void startEnemyAnim(Enemy& e) {
    e.animIdx = 0;
    e.animAcc = 0.0f;
    e.turn    = nullptr;
    switch (e.kind) {
        case EK_BIRD:      // 0x3372: heading right, first flap on the next frame
            e.code = 0x29; e.flip = true; e.animDir = 0; e.animTimer = 1; break;
        case EK_MUMMY:     // standing; its first walking frame turns it
            e.code = 0x04; e.flip = false; e.animDir = -1; e.animTimer = 6; break;
        default:
            e.code = chaserAnim(e.kind).first; e.flip = false; e.animTimer = 0; break;
    }
}

// The clone's birds steer freely; the arcade's fly one of four headings. Map
// the velocity to a heading, keeping the current axis until the other one
// clearly dominates so a bird near the diagonal doesn't keep turning.
static int birdHeading(const Enemy& e) {
    const float ax = std::fabs(e.vx), ay = std::fabs(e.vy);
    if (ax + ay < 1.0f) return e.animDir;
    const bool wasH = e.animDir <= 1;
    bool horiz = ax >= ay;
    if (wasH ? ax * 1.5f >= ay : ay * 1.5f >= ax) horiz = wasH;
    return horiz ? (e.vx < 0 ? 1 : 0) : (e.vy < 0 ? 2 : 3);
}

static void stepBird(Enemy& e) {
    if (e.turn) {                                    // 0x3463
        if (--e.animTimer > 0) return;
        e.animTimer = 6;
        if (*e.turn != 0xFF) { e.code = *e.turn++; return; }
        e.flip = e.turn[1] & 0x80;
        e.code = e.turn[2];
        e.turn = nullptr;
        e.animTimer = 6;
        e.animIdx   = 0;
        return;
    }
    const int h = birdHeading(e);
    if (h != e.animDir) {                            // 0x34CB
        const uint8_t* seq = BIRD_TURN[e.animDir][h];
        e.flip = seq[0] & 0x80;
        e.code = seq[1];
        e.turn = seq + 2;
        e.animTimer = 6;
        e.animDir   = h;
        return;
    }
    if (--e.animTimer > 0) return;                   // 0x3B7A
    e.animTimer = 6;
    e.code = BIRD_FLAP[e.animDir >= 2][e.animIdx];
    e.animIdx = (e.animIdx + 1) % 3;
}

static void stepMummy(Enemy& e) {
    if (e.phase == EP_FALL) {                        // 0x377A
        if (--e.animTimer > 0) return;
        e.animTimer = 4;
        e.code = MUMMY_FALL[e.animIdx];
        e.animIdx = (e.animIdx + 1) & 3;
        return;
    }
    if (e.phase != EP_WALK) return;
    const int dir = e.vx < 0 ? 1 : 0;                // 0x3728
    if (dir != e.animDir) {
        e.animDir   = dir;
        e.animIdx   = 0;
        e.animTimer = 6;
        e.code      = 0x04;
        return;
    }
    if (--e.animTimer > 0) return;
    e.animTimer = 6;
    e.code = MUMMY_WALK[dir][e.animIdx];
    e.animIdx = (e.animIdx + 1) & 3;
}

static void stepChaser(Enemy& e) {                   // 0x2AAA
    const ChaserAnim& a = chaserAnim(e.kind);
    if (++e.animTimer < a.period) return;
    e.animTimer = 0;
    e.animIdx = (e.animIdx + 1) % a.n;
    const bool right = e.vx >= 0;                    // sign bit of the X speed
    e.code = (right ? a.right : a.left)[e.animIdx];
    e.flip = right && a.flipRight;
}

void animateEnemy(Enemy& e, float dt) {
    if (e.phase == EP_INIT || e.phase == EP_DISAPPEAR) return;   // flash: timer-driven
    e.animAcc += dt * ARCADE_HZ;
    for (; e.animAcc >= 1.0f; e.animAcc -= 1.0f) {
        if (e.kind == EK_BIRD)       stepBird(e);
        else if (e.kind == EK_MUMMY) stepMummy(e);
        else                         stepChaser(e);
    }
}
