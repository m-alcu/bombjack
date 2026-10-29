#include "game/player.h"
#include "game/game.h"
#include "platform/audio.h"
#include <algorithm>
#include <cmath>

void resetPlayer(Game& g, bool invuln) {
    g.p.x        = LOGW / 2.0f - PW / 2.0f;
    g.p.y        = LOGH / 2.0f - PH / 2.0f;
    g.p.vx       = g.p.vy = 0.0f;
    g.p.onGround = false;
    g.p.crouch   = 0.0f;
    g.p.landing  = 0.0f;
    g.p.invuln   = invuln ? INVULN_TIME : 0.0f;
}

void updateJack(Game& g, const Input& in, float dt) {
    Player& p = g.p;
    const bool oldOnGround = p.onGround;

    // Horizontal movement (frozen while crouching before take-off, and for a
    // moment after landing, as on the arcade).
    if (p.landing > 0.0f) p.landing -= dt;
    p.vx = p.crouch > 0.0f || p.landing > 0.0f
         ? 0.0f : (in.right ? MOVE : 0.0f) - (in.left ? MOVE : 0.0f);
    if (in.right && !in.left) p.face = 1;
    else if (in.left && !in.right) p.face = -1;
    p.x += p.vx * dt;
    p.x = std::clamp(p.x, BORDER_SOLID_X, LOGW - PW - BORDER_SOLID_X);

    // Jump logic, as in the arcade: every jump is full height (holding the
    // button changes nothing); a fresh press in the air cancels vertical speed,
    // which ends a rise or restarts a fall from rest (tapping = hovering).
    if (in.jumpPressed && p.crouch <= 0.0f) {
        if (p.onGround) {
            p.crouch = JUMP_CROUCH;
        } else {
            p.vy = 0;
        }
    }
    if (p.crouch > 0.0f) {
        p.crouch -= dt;
        if (p.crouch <= 0.0f) {
            p.crouch   = 0.0f;
            p.vy       = JUMP_VEL;
            p.onGround = false;
            audio::play(audio::SND_JUMP);   // arcade sounds the jump at take-off
        }
    }

    // Up halves and Down doubles gravity in the air, rising or falling.
    if (p.crouch <= 0.0f) {
        float grav = GRAVITY;
        if (in.up && !in.down)      grav *= GRAV_UP;
        else if (in.down && !in.up) grav *= GRAV_DOWN;
        p.vy += grav * dt;
        if (p.vy > MAXFALL) p.vy = MAXFALL;
    }

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
            break;
        }
    }

    // Platform sides act as walls: block horizontal movement through them, for
    // horizontal platforms as well as girders (measured on the arcade: Jack
    // hangs at a platform end until he sinks below it). The eps keeps standing
    // on top from counting as a side hit; Jack slips under a platform once his
    // top is 3 px from its bottom.
    const float wallEps = 2.0f, wallEpsUnder = 3.0f * PX_TO_WU_Y;
    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;                       // not the floor
        if (p.y + PH <= pl.y + wallEps || p.y >= pl.y + pl.h - wallEpsUnder) continue;
        if (p.x + PW <= pl.x || p.x >= pl.x + pl.w) continue;         // no overlap
        if (p.vx > 0)      p.x = pl.x - PW;       // hit left face
        else if (p.vx < 0) p.x = pl.x + pl.w;     // hit right face
    }

    if (!oldOnGround && p.onGround) p.landing = LAND_HOLD;
    // Walking off a ledge sounds like a jump take-off on the arcade (command 0x11).
    if (oldOnGround && !p.onGround && p.vy >= 0.0f) audio::play(audio::SND_JUMP);
    // Every take-off (jump or walking off a ledge) scores 10 points times the
    // multiplier, with no popup (ROM 0x10BE), and steps the Power ball's colour.
    if (oldOnGround && !p.onGround) {
        g.score += 10 * g.multiplier;
        cycleBallFamily(g);
    }

    // Footstep loop while walking on the ground (one at a time).
    if (p.onGround && (in.left || in.right) && audio::tagCount("walking") == 0)
        audio::play(audio::SND_WALKING, "walking");
}

void updateJackPose(Game& g, float dt) {
    Player& p = g.p;
    const bool moving = std::fabs(p.vx) > 1.0f;
    const bool left   = p.face < 0;
    int pose;
    if (p.onGround)       pose = moving ? (left ? JP_WALK_L : JP_WALK_R) : JP_IDLE;
    else if (p.vy < 0.0f) pose = moving ? (left ? JP_FLY_L  : JP_FLY_R)  : JP_FLY;
    else                  pose = moving ? (left ? JP_FALL_L : JP_FALL_R) : JP_FALL;
    if (pose != p.pose) {
        p.pose     = pose;
        p.poseTime = 0.0f;
    }
    p.poseTime += dt;
}

void hitJack(Game& g) {
    g.death.active = true;
    g.death.phase  = DP_DANCING;
    g.death.frame  = 0;
    g.death.loops  = 0;
    g.death.anim   = 0.0f;
    g.death.timer  = 0.0f;
    audio::stopTag("music");
    audio::stopTag("power");
    audio::play(audio::SND_TOUCHED, "touched");
    g.p.invuln   = 0.0f;
    g.p.onGround = false;
    g.p.vx       = 0.0f;
    g.p.vy       = 0.0f;
}

bool updateDeath(Game& g, float dt) {
    Player& p = g.p;
    DeathState& d = g.death;
    if (d.phase == DP_DANCING) {
        d.anim += dt;
        const float step = DEATH_DANCE_TOTAL / 3.0f;
        while (d.anim >= step) {
            d.anim -= step;
            d.frame = (d.frame + 1) % 3;
            if (d.frame == 0) {
                d.loops++;
                if (d.loops >= DEATH_DANCE_LOOPS) {
                    // Arcade: the "touched" tune is cut and the fall sound starts.
                    audio::stopTag("touched");
                    audio::play(audio::SND_DEATH_FALL);
                    d.phase = DP_FALLING;
                    d.frame = 0;
                    d.anim  = 0.0f;
                    p.vx = 0.0f;
                    p.vy = 0.0f;
                    break;
                }
            }
        }
    } else if (d.phase == DP_FALLING) {
        d.anim += dt;
        const float step = DEATH_PLF_TOTAL / 4.0f;
        while (d.anim >= step) {
            d.anim -= step;
            d.frame = (d.frame + 1) % 4;
        }
        p.vy += GRAVITY * dt;
        if (p.vy > MAXFALL) p.vy = MAXFALL;
        p.y += p.vy * dt;
        if (p.y + PH >= FLOOR_TOP) {
            p.y = FLOOR_TOP - PH;
            p.vy = 0.0f;
            p.onGround = true;
            d.phase = DP_DEAD;
            d.frame = 0;
            d.anim  = 0.0f;
        } else {
            p.onGround = false;
        }
    } else if (d.phase == DP_DEAD) {
        d.anim += dt;
        const float step = DEATH_DEAD_TOTAL / 4.0f;
        while (d.anim >= step) {
            d.anim -= step;
            if (d.frame < 3) d.frame++;
            else {
                d.phase = DP_WAIT;
                d.timer = DEATH_WAIT_TIME;
                break;
            }
        }
    } else if (d.phase == DP_WAIT) {
        d.timer -= dt;
        if (d.timer <= 0.0f) {
            d = {};
            return true;
        }
    }
    return false;
}
