#include "game/enemy.h"
#include <cmath>

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
