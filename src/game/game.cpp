#include "game/game.h"
#include "platform/audio.h"
#include <algorithm>

void startGame(Game& g) {
    stopDemo(g);
    audio::stopTag("title");   // leave the title-screen music behind
    g.score     = g.opt.start100k ? 100000 : 0;
    g.lives     = g.opt.lives;
    g.round     = g.opt.firstRound;
    g.state     = PLAYING;
    g.bCoins    = 0;
    g.nextEAt   = BONUS_E_EVERY;
    g.livesLost = 0;
    initPlatforms(g);
    startRound(g);
}

void optionAdjust(Game& g, int d) {
    Options& o = g.opt;
    switch (o.sel) {
        case 0: o.firstRound = std::clamp(o.firstRound + d, 1, leveldata::ROUND_COUNT); break;
        case 1: o.lives      = std::clamp(o.lives + d, 1, OPT_MAX_LIVES);               break;
        case 2: o.imgStyle   = (o.imgStyle + d + OPT_IMG_STYLES) % OPT_IMG_STYLES;      break;
        case 3: o.alwaysS    = !o.alwaysS;                                              break;
        case 4: o.start100k  = !o.start100k;                                            break;
    }
}

// The death sequence is over: lose a life, then restart the round or end the game.
static void loseLife(Game& g) {
    g.lives--;
    g.livesLost++;
    if (g.lives <= 0) {
        g.state          = GAMEOVER;
        g.gameOver.x     = (g.p.x + PW / 2) * (float)GAME_W / LOGW;
        g.gameOver.y     = HUD_H + (g.p.y + PH / 2) * (float)GAME_H / LOGH;
        g.gameOver.stage = 0;
        g.gameOver.timer = GAMEOVER_HOLD;
        audio::play(audio::SND_GAMEOVER, "gameover");
    } else {
        resetPlayer(g, true);
        restartEnemies(g);
        g.startAnim = START_TOTAL;
    }
}

void updatePlaying(Game& g, const Input& in, float dt) {
    if (g.death.active) {
        if (updateDeath(g, dt)) loseLife(g);
        return;
    }

    // Hold simulation during the "START!" intro. The music starts as soon as
    // the "START!" sprite begins sliding in, so it plays under the whole intro.
    if (g.startAnim > 0.0f) {
        keepMusicPlaying(g);
        g.startAnim -= dt;
        updateJackPose(g, dt);
        return;
    }

    if (g.p.invuln > 0) g.p.invuln -= dt;
    keepMusicPlaying(g);

    updateJack(g, in, dt);
    collectBombs(g);

    // In the demo the enemies, the Power ball and the bonus coin are replayed
    // from an arcade run (updateDemo) instead of simulated.
    const bool replay = g.demo.active;
    if (!replay) updatePowerBall(g, dt);
    updateBonus(g, dt);
    updateEffects(g, dt);

    if (g.bombsLeft <= 0) {
        finishRound(g);
        return;
    }

    if (!replay) {
        const bool frozen = g.freezeTimer > 0.0f;
        updateFreeze(g, dt);
        if (updateEnemies(g, dt, frozen)) return;   // Jack was caught
    }

    updateJackPose(g, dt);
}

// Round cleared (or bonus screen over): on to the next one.
static void nextRound(Game& g) {
    g.round++;
    startRound(g);
    g.state = PLAYING;
}

static void updateSpecialBonus(Game& g, float dt) {
    SpecialBonusState& sb = g.special;
    sb.timer -= dt;
    if (sb.phase == 0) {
        if (sb.timer <= 0) { sb.phase = 1; sb.timer = SB_SCORE_TIME; }
    } else if (sb.phase == 1) {
        while (sb.phase == 1 && sb.timer <= 0) {
            sb.remaining -= 1000;
            g.score      += 1000;
            if (sb.remaining <= 0) { sb.phase = 2; sb.timer += SB_END_TIME; }
            else                   { sb.timer += SB_SCORE_TIME; }
        }
    } else if (sb.timer <= 0) {
        audio::stopTag("special_bonus");
        nextRound(g);
    }
}

static void updateLucky(Game& g, float dt) {
    g.lucky.timer += dt;
    if (g.lucky.phase == 0) {
        // Text glides in from the corners; once it settles, hold.
        if (g.lucky.timer >= LUCKY_SLIDE_TIME) g.lucky.phase = 1;
    } else if (g.lucky.timer >= LUCKY_HOLD_TIME &&
               audio::tagCount("special_coin") == 0) {
        // Stay until the "special coin" jingle finishes (the floor keeps
        // a muted build from flashing the screen away instantly).
        nextRound(g);
    }
}

// "GAME OVER" glides to the centre row, then column, holds, and leads to name
// entry if the score made the table.
static void updateGameOver(Game& g, float dt) {
    GameOverState& go = g.gameOver;
    const float cx   = SCREEN_W / 2.0f;
    const float cy   = SCREEN_H / 2.0f;
    const float move = GAMEOVER_SLIDE * dt;
    if (go.stage == 0) {
        if (go.y < cy) go.y = std::min(cy, go.y + move);
        else           go.y = std::max(cy, go.y - move);
        if (go.y == cy) go.stage = 1;
    } else if (go.stage == 1) {
        if (go.x < cx) go.x = std::min(cx, go.x + move);
        else           go.x = std::max(cx, go.x - move);
        if (go.x == cx) go.stage = 2;
    } else {
        go.timer -= dt;
        if (go.timer <= 0.0f) {
            audio::stopTag("gameover");
            if (hiScoreQualifies(g)) startNameEntry(g);
            else                     enterAttract(g, 0);   // table shown first
        }
    }
}

void update(Game& g, const Input& in, float dt) {
    g.time += dt;
    switch (g.state) {
        case TITLE:
            updateTitle(g, in, dt);
            break;
        case PLAYING:
            if (g.demo.active) updateDemo(g, dt);
            else               updatePlaying(g, in, dt);
            break;
        case ROUNDCLEAR:
            g.clearTimer -= dt;
            if (g.clearTimer <= 0) nextRound(g);
            break;
        case SPECIALBONUS:
            updateSpecialBonus(g, dt);
            break;
        case LUCKY:
            updateLucky(g, dt);
            break;
        case GAMEOVER:
            updateGameOver(g, dt);
            break;
        case OPTIONS:
        case NAMEENTRY:
            break;   // driven by the keyboard (platform/input.cpp)
    }
}
