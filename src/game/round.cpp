#include "game/round.h"
#include "game/game.h"
#include "platform/audio.h"
#include <cmath>

const leveldata::Round& currentRound(const Game& g) {
    return leveldata::rounds[leveldata::roundIndex(g.round)];
}

const leveldata::PlatLayout& currentPlatLayout(const Game& g) {
    return leveldata::platLayouts[currentRound(g).platLayout];
}

const leveldata::BombLayout& currentBombLayout(const Game& g) {
    return leveldata::bombLayouts[currentRound(g).bombLayout];
}

const leveldata::EnemyDef& currentEnemyDef(const Game& g) {
    return leveldata::enemyDefs[leveldata::roundIndex(g.round) % leveldata::ENEMY_DEF_COUNT];
}

int currentScreen(const Game& g) {
    return currentRound(g).screen;
}

void initPlatforms(Game& g) {
    g.platforms.clear();
    g.platforms.push_back({0, FLOOR_TOP, LOGW, LOGH - FLOOR_TOP});
    const leveldata::PlatLayout& lay = currentPlatLayout(g);   // empty on platform-less rounds
    for (int i = 0; i < lay.nplats; ++i) {
        const leveldata::Plat& p = lay.plats[i];
        g.platforms.push_back({p.x, p.y, p.w, p.h});
    }
}

static void spawnBombs(Game& g) {
    g.bombs.clear();
    const leveldata::BombLayout& lay = currentBombLayout(g);
    for (int i = 0; i < lay.nbombs; ++i)
        g.bombs.push_back({lay.bombs[i].x, lay.bombs[i].y, false});
    g.bombsLeft = (int)g.bombs.size();
    g.litBomb   = -1;
    g.fireBombs = 0;
}

void startRound(Game& g) {
    initPlatforms(g);
    spawnBombs(g);
    spawnBirds(g);
    resetPlayer(g, true);
    g.ball.active    = false;
    g.freezeTimer    = 0.0f;
    loadPal14(g, 0);
    g.killCount      = 0;
    g.death          = {};
    g.mummyTimer     = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx   = 0;
    g.popups.clear();
    g.explosions.clear();
    g.coinPickups.clear();
    g.bonusTakens.clear();
    g.multiplier     = 1;
    g.bonus.active   = false;
    g.nextBonusScore = (g.score / BONUS_LIMIT + 1) * BONUS_LIMIT;
    g.startAnim      = START_TOTAL;
    if (g.opt.alwaysS) spawnBonus(g, BK_S);  // OPTIONS: guaranteed S bonus per round
}

void restartEnemies(Game& g) {
    spawnBirds(g);
    g.mummyTimer     = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx   = 0;
    g.freezeTimer    = 0.0f;
    if (!g.ball.active) loadPal14(g, 0);
    g.killCount      = 0;
    g.ball.active    = false;
}

// In-game BGM comes from the round table's music index (0..2): rounds 1, 2, 3
// → BGM 1, BGM 3, BGM 2, i.e. arcade sound commands 0x23, 0x24, 0x22 (checked
// against the commands the arcade sends at the start of rounds 1-4 in MAME).
static audio::Sid musicForRound(const Game& g) {
    switch (currentRound(g).music) {
        case 0:  return audio::SND_MUSIC;
        case 1:  return audio::SND_MUSIC_3;
        default: return audio::SND_MUSIC_2;
    }
}

// The music loops while Jack is alive and the power jingle isn't holding the
// "power" tag (matches the Lua livegame music guard).
void keepMusicPlaying(const Game& g) {
    if (audio::tagCount("music") == 0 && audio::tagCount("power") == 0)
        audio::play(musicForRound(g), "music", /*loop=*/true);
}

void collectBombs(Game& g) {
    const Player& p = g.p;
    for (int i = 0; i < (int)g.bombs.size(); ++i) {
        Bomb& b = g.bombs[i];
        if (b.collected) continue;
        const float jackCx = p.x + PW / 2.0f;
        const float jackCy = p.y + PH - JACK_CENTER_ABOVE_FEET;
        if (std::fabs(b.x - jackCx) >= PICK_DX || std::fabs(b.y - jackCy) >= PICK_DY) continue;

        const bool wasLit  = (i == g.litBomb);
        const bool relight = (g.litBomb < 0) || wasLit;
        b.collected = true;
        g.bombsLeft--;
        g.explosions.push_back({b.x, b.y, 0.0f});
        audio::play(wasLit ? audio::SND_TAKE_EXPLODING_BOMB : audio::SND_TAKE_BOMB);
        if (wasLit) g.fireBombs++;
        if (relight) g.litBomb = nextLitBomb(g, i + 1);
        const int gain = (wasLit ? 200 : 100) * g.multiplier;
        g.score += gain;
        g.popups.push_back({b.x, b.y - 6, 0.0f, gain});
        chargePower(g, wasLit);
    }
}

int nextLitBomb(const Game& g, int orderMin) {
    for (int j = orderMin; j < (int)g.bombs.size(); ++j)
        if (!g.bombs[j].collected) return j;
    for (int j = 0; j < (int)g.bombs.size(); ++j)
        if (!g.bombs[j].collected) return j;
    return -1;
}

int specialBonusFor(int fireBombs) {
    switch (fireBombs) {
        case 20: return 10000;
        case 21: return 20000;
        case 22: return 30000;
        case 23: return 50000;
        default: return 0;
    }
}

void finishRound(Game& g) {
    // Round cleared: silence the loops and sound the "clear" jingle.
    audio::stopTag("music");
    audio::stopTag("power");
    audio::play(audio::SND_CLEAR);
    int bonus = specialBonusFor(g.fireBombs);
    if (bonus > 0) {
        g.state             = SPECIALBONUS;
        g.special.phase     = 0;
        g.special.timer     = SB_BEGIN_TIME;
        g.special.remaining = bonus;
        g.special.fireBombs = g.fireBombs;
        audio::play(audio::SND_SPECIAL_BONUS, "special_bonus", /*loop=*/true);
    } else {
        g.state      = ROUNDCLEAR;
        g.clearTimer = CLEAR_TIME;
    }
}
