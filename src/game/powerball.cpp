#include "game/powerball.h"
#include "game/game.h"
#include "core/geometry.h"
#include "platform/audio.h"
#include <algorithm>
#include <cmath>

void loadPal14(Game& g, int table) {
    g.pal14.table    = table;
    g.pal14.loadedAt = g.time;
}

static void releaseBall(Game& g) {
    g.powerMeter  = 0.0f;   // arcade resets the counter when the ball appears
    g.ball.active = true;
    g.ball.x      = LOGW / 2.0f;
    g.ball.y      = LOGH / 2.0f - 16.0f;
    g.ball.vx     = BALL_SPEED * 0.5f;
    g.ball.vy     = BALL_SPEED * 0.8660254f;
    g.ball.family = 1;
    loadPal14(g, 1);
    // Ball on screen: duck the music and loop the "appearing" tone.
    audio::pauseTag("music");
    audio::play(audio::SND_POWER_APPEARING, "power", /*loop=*/true);
}

void chargePower(Game& g, bool litBomb) {
    // Arcade: the power counter does not charge while the Power ball is on
    // screen; a lit bomb charges it twice as much as a plain one.
    if (g.ball.active) return;
    g.powerMeter += litBomb ? 1.0f : 0.5f;
    // The demo replays the arcade's own ball (attract.cpp).
    if (g.powerMeter >= POWER_NEEDED && !g.demo.active) releaseBall(g);
}

void cycleBallFamily(Game& g) {
    if (!g.ball.active) return;
    g.ball.family = g.ball.family % 7 + 1;
    loadPal14(g, g.ball.family);
}

// Bounce around the play area and off platforms.
static void moveBall(Game& g, float dt) {
    PowerBall& b = g.ball;
    float oldX = b.x;
    float oldY = b.y;
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    const float left  = BORDER_SOLID_X + BALL_R;
    const float right = LOGW - BORDER_SOLID_X - BALL_R;
    const float top   = BORDER_SOLID_Y + BALL_R;
    if (b.x < left)  { b.x = left;  b.vx =  std::fabs(b.vx); }
    if (b.x > right) { b.x = right; b.vx = -std::fabs(b.vx); }
    if (b.y < top)   { b.y = top;   b.vy =  std::fabs(b.vy); }
    if (b.y > FLOOR_TOP - BALL_R) {
        b.y  = FLOOR_TOP - BALL_R;
        b.vy = -std::fabs(b.vy);
    }
    for (const SDL_FRect& pl : g.platforms) {
        if (pl.y >= FLOOR_TOP - 1.0f) continue;
        if (!circleOverlapsRect(b.x, b.y, BALL_R, pl)) continue;
        const float eps = 0.001f;
        bool hitTop    = oldY + BALL_R <= pl.y + eps        && b.y + BALL_R > pl.y + eps;
        bool hitBottom = oldY - BALL_R >= pl.y + pl.h - eps && b.y - BALL_R < pl.y + pl.h - eps;
        if (hitTop) {
            b.y  = pl.y - BALL_R;
            b.vy = -std::fabs(b.vy);
        } else if (hitBottom) {
            b.y  = pl.y + pl.h + BALL_R;
            b.vy =  std::fabs(b.vy);
        } else {
            bool hitLeft  = oldX + BALL_R <= pl.x + eps        && b.x + BALL_R > pl.x + eps;
            bool hitRight = oldX - BALL_R >= pl.x + pl.w - eps && b.x - BALL_R < pl.x + pl.w - eps;
            if (hitLeft) {
                b.x  = pl.x - BALL_R;
                b.vx = -std::fabs(b.vx);
            } else if (hitRight) {
                b.x  = pl.x + pl.w + BALL_R;
                b.vx =  std::fabs(b.vx);
            } else if (std::fabs(b.vy) >= std::fabs(b.vx) && oldY <= pl.y) {
                b.y  = pl.y - BALL_R;
                b.vy = -std::fabs(b.vy);
            } else {
                if (oldX <= pl.x) {
                    b.x  = pl.x - BALL_R;
                    b.vx = -std::fabs(b.vx);
                } else {
                    b.x  = pl.x + pl.w + BALL_R;
                    b.vx =  std::fabs(b.vx);
                }
            }
        }
        break;
    }
}

// Jack takes the ball: freeze all enemies and make them collectable.
static void takeBall(Game& g) {
    g.ball.active = false;
    g.freezeTimer = FREEZE_TIME;
    g.killCount   = 0;
    // The power tune lasts exactly the freeze; the music resumes when the
    // freeze ends (updateFreeze), as on the arcade.
    audio::stopTag("power");
    audio::play(audio::SND_GOT_POWER, "power");
    // Points by colour family; palette 14 keeps that family's colours for
    // the whole freeze.
    int idx  = std::clamp(g.ball.family, 1, 7) - 1;
    int gain = POWER_POINTS[idx] * g.multiplier;
    g.ball.family = 0;
    g.score += gain;
    g.popups.push_back({g.ball.x, g.ball.y, 0.0f, gain});
}

void updatePowerBall(Game& g, float dt) {
    if (!g.ball.active) return;
    moveBall(g, dt);
    const Player& p = g.p;
    if (g.ball.x > p.x - BALL_R && g.ball.x < p.x + PW + BALL_R &&
        g.ball.y > p.y - BALL_R && g.ball.y < p.y + PH + BALL_R)
        takeBall(g);
}

void updateFreeze(Game& g, float dt) {
    if (g.freezeTimer <= 0.0f) return;
    g.freezeTimer -= dt;
    if (g.freezeTimer <= 0.0f && !g.ball.active) {
        loadPal14(g, 0);
        // Resume the in-game music once the "got power" jingle has finished,
        // mirroring the TEsound finished-callback of the Lua original.
        audio::stopTag("power");
        audio::resumeTag("music");
    }
}
