#include "game.h"
#include "audio.h"
#include "demo_data.h"
#include "demo_runs.h"
#include "enemy.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>

// Resume the in-game music once the "got power" jingle has finished, mirroring
// the TEsound finished-callback the Lua original attaches to that sound.
static void resumeMusic() { audio::resumeTag("music"); }

// Coin bounce: the arcade plays it on one channel, so a new bounce cuts the last.
static void bounceSound(float gain) {
    audio::stopTag("coin_bounce");
    audio::play(audio::SND_BONUS, "coin_bounce", false, nullptr, gain);
}

// In-game BGM comes from the round table's music index (0..2): rounds 1, 2, 3
// → BGM 1, BGM 3, BGM 2, i.e. arcade sound commands 0x23, 0x24, 0x22 (checked
// against the commands the arcade sends at the start of rounds 1-4 in MAME).
static audio::Sid musicForLevel(int level) {
    switch (leveldata::rounds[leveldata::roundIndex(level)].music) {
        case 0:  return audio::SND_MUSIC;
        case 1:  return audio::SND_MUSIC_3;
        default: return audio::SND_MUSIC_2;
    }
}

const leveldata::Round& currentRound(const Game& g) {
    return leveldata::rounds[leveldata::roundIndex(g.level)];
}

const leveldata::PlatLayout& currentPlatLayout(const Game& g) {
    return leveldata::platLayouts[currentRound(g).platLayout];
}

const leveldata::BombLayout& currentBombLayout(const Game& g) {
    return leveldata::bombLayouts[currentRound(g).bombLayout];
}

const leveldata::EnemyDef& levelDef(const Game& g) {
    return leveldata::enemyDefs[leveldata::roundIndex(g.level) % leveldata::ENEMY_DEF_COUNT];
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

void spawnBombs(Game& g) {
    g.bombs.clear();
    const leveldata::BombLayout& lay = currentBombLayout(g);
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
    startEnemyAnim(e);
    return e;
}

void spawnEnemies(Game& g) {
    g.enemies.clear();
    int birds = levelDef(g).birds;
    for (int i = 0; i < birds; ++i)
        g.enemies.push_back(makeBird(g, (i + 0.5f) / birds * 6.2831853f));
}

// Load a table into palette 14 (0 rainbow, 1-7 a Power ball colour family);
// its rotation restarts from the table's first entry (ROM 0x24A9).
static void loadPal14(Game& g, int table) {
    g.pal14     = table;
    g.pal14Time = g.time;
}

void restartEnemies(Game& g) {
    spawnEnemies(g);
    g.mummyTimer     = 0.0f;
    g.mummiesSpawned = 0;
    g.transformIdx   = 0;
    g.freezeTimer    = 0.0f;
    if (!g.orb.active) loadPal14(g, 0);
    g.killCount      = 0;
    g.orb.active     = false;
}

void resetPlayer(Game& g, bool invuln) {
    g.p.x        = LOGW / 2.0f - PW / 2.0f;
    g.p.y        = LOGH / 2.0f - PH / 2.0f;
    g.p.vx       = g.p.vy = 0.0f;
    g.p.onGround = false;
    g.p.crouch   = 0.0f;
    g.p.landing  = 0.0f;
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
    if (g.alwaysS) spawnBonus(g, BK_S);  // OPTIONS: guaranteed S bonus per level
}

void optionAdjust(Game& g, int d) {
    switch (g.optSel) {
        case 0: g.startLevel = std::clamp(g.startLevel + d, 1, leveldata::ROUND_COUNT); break;
        case 1: g.startLives = std::clamp(g.startLives + d, 1, OPT_MAX_LIVES);          break;
        case 2: g.imgStyle   = (g.imgStyle + d + OPT_IMG_STYLES) % OPT_IMG_STYLES;      break;
        case 3: g.alwaysS    = !g.alwaysS;                                              break;
        case 4: g.start100k  = !g.start100k;                                            break;
    }
}

void startGame(Game& g) {
    stopDemo(g);
    audio::stopTag("title");   // leave the title-screen music behind
    g.score     = g.start100k ? 100000 : 0;
    g.lives     = g.startLives;
    g.level     = g.startLevel;
    g.state     = PLAYING;
    g.bCoins    = 0;
    g.nextEAt   = BONUS_E_EVERY;
    g.livesLost = 0;
    initPlatforms(g);
    startRound(g);
}

// ---------------------------------------------------------------------------
// High-score table (name entry + persistence), mirroring the Godot game.
// ---------------------------------------------------------------------------
#ifndef ASSET_ROOT
#define ASSET_ROOT "."
#endif
static const char* kHiScorePath = ASSET_ROOT "/hiscores.dat";

static void setEntryName(HiScoreEntry& e, const char* name) {
    std::strncpy(e.name, name, NAME_MAX);
    e.name[NAME_MAX] = '\0';
}

// Default table matching the arcade "BEST PLAYERS" screen: ten 100000 / ROUND 1
// entries with the classic initials (10th place left as "...").
static void seedDefaultHiScores(Game& g) {
    static const char* const kNames[HISCORE_COUNT] = {
        "T.T", "E.E", "H.H", "K.K", "A.A", "N.N", "L.L", "T.T", "D.D", "...",
    };
    for (int i = 0; i < HISCORE_COUNT; ++i) {
        g.hi[i].score = 100000;
        g.hi[i].level = 1;
        setEntryName(g.hi[i], kNames[i]);
    }
}

void saveHiScores(const Game& g) {
    std::FILE* f = std::fopen(kHiScorePath, "w");
    if (!f) return;
    for (const HiScoreEntry& e : g.hi)
        std::fprintf(f, "%d %s %d\n", e.score, e.name[0] ? e.name : "-", e.level);
    std::fclose(f);
}

void loadHiScores(Game& g) {
    std::FILE* f = std::fopen(kHiScorePath, "r");
    if (!f) { seedDefaultHiScores(g); saveHiScores(g); return; }
    int n = 0;
    for (; n < HISCORE_COUNT; ++n) {
        char name[64] = {};
        int score = 0, level = 0;
        if (std::fscanf(f, "%d %63s %d", &score, name, &level) != 3) break;
        g.hi[n].score = score;
        g.hi[n].level = level;
        setEntryName(g.hi[n], name);
    }
    std::fclose(f);
    if (n < HISCORE_COUNT) { seedDefaultHiScores(g); saveHiScores(g); }  // corrupt/short → reseed
}

bool hiScoreQualifies(const Game& g) {
    return g.score > g.hi[HISCORE_COUNT - 1].score;
}

// Insert the current score under `name`, shifting lower rows down (Godot addScore).
void insertHiScore(Game& g, const char* name) {
    for (int i = 0; i < HISCORE_COUNT; ++i) {
        if (g.score > g.hi[i].score) {
            for (int j = HISCORE_COUNT - 1; j > i; --j) g.hi[j] = g.hi[j - 1];
            g.hi[i].score = g.score;
            g.hi[i].level = g.level;
            setEntryName(g.hi[i], name && name[0] ? name : "-");
            break;
        }
    }
}

void confirmNameEntry(Game& g) {
    insertHiScore(g, g.hs.entry);
    saveHiScores(g);
    audio::stopTag("nameentry");
    enterAttract(g, 0);   // show the fresh table first
}

// ---------------------------------------------------------------------------
// Attract mode and demo play
// ---------------------------------------------------------------------------
static void hitJack(Game& g);

static_assert(demoruns::SLOT_COUNT == sizeof(DemoState::slotCode),
              "DemoState slot arrays must match demoruns::kSlots");

// Arcade sprite position (Jack's coordinates, see snapToTrack) → world centre.
static float demoWorldX(int x) { return LOGW / 2.0f + (x - 120.0f) * PX_TO_WU_X; }
static float demoWorldY(int y) { return LOGH / 2.0f + (y - 121.0f) * PX_TO_WU_Y; }

void enterAttract(Game& g, int step) {
    g.state         = TITLE;
    g.attract.step  = step;
    g.attract.timer = 0.0f;
    g.hs.showTable  = step == 0;
}

void startDemo(Game& g, int round) {
    DemoState& d = g.demo;
    d = {};
    d.active     = true;
    d.round      = round;
    d.stream     = g.attract.cycle & 1;
    d.savedScore = g.score;
    d.savedLives = g.lives;
    d.savedLevel = g.level;
    d.savedPower = g.powerMeter;

    // Pick one of the arcade's runs for this recording, as often as it came up.
    const demoruns::RunSet& set = demoruns::kRunSets[round - 1][d.stream];
    int total = 0;
    for (int i = 0; i < set.count; ++i) total += set.runs[i].weight;
    int pick = (int)(g.rng() % (unsigned)total);
    while (pick >= set.runs[d.run].weight) pick -= set.runs[d.run++].weight;

    g.attract.step = round == 1 ? 1 : 3;
    g.score      = 0;
    g.lives      = 3;
    g.level      = round;
    g.powerMeter = 0.0f;
    const bool alwaysS = g.alwaysS;   // the OPTIONS S coin is for real games only
    g.alwaysS = false;
    startRound(g);
    g.alwaysS = alwaysS;
    // No "START!" intro: Jack stands still for the run's lead (30 frames in
    // round 1), then the joystick replay begins. No blinking either.
    g.startAnim = set.runs[d.run].lead / ARCADE_HZ;
    g.p.invuln  = 0.0f;
    g.state     = PLAYING;
    audio::setMuted(true);
}

void stopDemo(Game& g) {
    DemoState& d = g.demo;
    if (!d.active) return;
    d.active     = false;
    g.score      = d.savedScore;
    g.lives      = d.savedLives;
    g.level      = d.savedLevel;
    g.powerMeter = d.savedPower;
    audio::setMuted(false);
}

// Advance the replay by one arcade frame (ROM 0x4759): the current bits stay
// for `hold` more frames, then the next step is loaded; at the end of the
// recording the arcade starts it again. The bits then go through the arcade's
// input filter (ROM 0x03A2): a bit counts once it has been on for three
// frames in a row, and drops at once, so every press acts two frames late.
static void demoFrame(DemoState& d) {
    const demodata::Stream& s = demodata::kStreams[d.round - 1][d.stream];
    const bool wasJump = d.held & 0x10;
    if (d.hold > 0) {
        d.hold--;
    } else if (d.step >= s.count) {
        d.bits = 0;
        d.hold = 1;
        d.step = 0;
    } else {
        d.hold = s.steps[d.step].hold;
        d.bits = s.steps[d.step].bits;
        d.step++;
    }
    d.held    = d.bits & d.hist[0] & d.hist[1];
    d.hist[1] = d.hist[0];
    d.hist[0] = d.bits;
    d.pressed = (d.held & 0x10) && !wasJump;
    d.frame++;

    // Follow the arcade's recorded path for this frame.
    const demodata::Track& t = demodata::kTracks[d.round - 1][d.stream];
    if (d.frame == 1) {
        d.trackX  = t.x0;
        d.trackY  = t.y0;
        d.onTrack = true;
    } else if (d.frame - 1 < t.frames) {
        d.trackX += t.d[2 * (d.frame - 2)];
        d.trackY += t.d[2 * (d.frame - 2) + 1];
    } else {
        d.onTrack = false;
    }
}

// Put Jack on the arcade's recorded path, between the last replayed frame and
// the next one. The replayed inputs still drive his animation, jumps and
// pickups; this only removes the clone's physics drift.
static void snapToTrack(Game& g) {
    const DemoState& d = g.demo;
    if (!d.onTrack || g.death.active) return;
    const demodata::Track& t = demodata::kTracks[d.round - 1][d.stream];
    float dx = 0.0f, dy = 0.0f;
    if (d.frame < t.frames) {
        dx = t.d[2 * (d.frame - 1)];
        dy = t.d[2 * (d.frame - 1) + 1];
    }
    const float ax = (d.trackX + dx * d.frameAcc) / 16.0f;
    const float ay = (d.trackY + dy * d.frameAcc) / 16.0f;
    // Jack's start (120, 120) is the clone's centre; the arcade's Y sits one
    // pixel lower for the same height (standing on the floor is 217 vs 216).
    g.p.x  = LOGW / 2.0f - PW / 2.0f + (ax - 120.0f) * PX_TO_WU_X;
    g.p.y  = LOGH / 2.0f - PH / 2.0f + (ay - 121.0f) * PX_TO_WU_Y;
    g.p.vy = dy / 16.0f * PX_TO_WU_Y * ARCADE_HZ;
}

// Decode the arcade run up to frame `target` (demo_runs.h). A frozen enemy
// that Jack collects turns into the arcade's sparkle (0xA0): show the clone's
// pickup effect there.
static void decodeRun(Game& g, int target) {
    DemoState& d = g.demo;
    const demoruns::Run& run = demoruns::kRunSets[d.round - 1][d.stream].runs[d.run];
    while (d.runFrame < target && d.runFrame + 1 < run.frames) {
        d.runFrame++;
        for (int k = 0; k < demoruns::SLOT_COUNT; ++k) {
            d.prevX[k] = d.slotX[k];
            d.prevY[k] = d.slotY[k];
        }
        const uint8_t* p = run.data + d.runPos;
        const int mask = p[0] | p[1] << 8;
        p += 2;
        for (int k = 0; k < demoruns::SLOT_COUNT; ++k) {
            if (!(mask & 1 << k)) continue;
            const uint8_t b = *p++;
            if (b == 0x00) {
                d.slotCode[k] = 0;
            } else if (b & 0x80) {
                d.slotX[k] += ((b >> 3) & 7) - 3;
                d.slotY[k] += (b & 7) - 3;
            } else {
                if (d.slotCode[k] == 0) {   // new sprite: no motion to report
                    d.prevX[k] = p[1];
                    d.prevY[k] = p[2];
                }
                if (p[0] == 0xA0 && d.slotCode[k] == 0x3F)
                    g.coinPickups.push_back({demoWorldX(p[1]), demoWorldY(p[2]), 0.0f});
                d.slotCode[k] = p[0];
                d.slotX[k]    = p[1];
                d.slotY[k]    = p[2];
                p += 3;
            }
        }
        d.runPos = p - run.data;
    }
}

// Show the decoded arcade sprites as the clone's enemies, Power orb and bonus
// coin. Enemies show the recorded ROM sprite code itself (see
// atlas::ENEMY_CELLS); the runs keep no attributes, so the flip the arcade
// gives right-moving birds, clubs and horns comes from the slot's motion.
// Other codes: 0xBC-0xBF flash in, 0x3F frozen enemy (coin), 0x3D Power ball,
// 0x0C-0x17 B/E/S coin. Score numbers, sparkles and the like are left out
// (the clone draws its own).
static void applyRun(Game& g) {
    DemoState& d = g.demo;

    const bool orbWas = g.orb.active;
    g.enemies.clear();
    g.orb.active   = false;
    g.bonus.active = false;
    bool frozen = false;
    for (int k = 0; k < demoruns::SLOT_COUNT; ++k) {
        const int c = d.slotCode[k];
        if (c == 0) continue;
        const float x = demoWorldX(d.slotX[k]), y = demoWorldY(d.slotY[k]);
        const int dx = (int)d.slotX[k] - d.prevX[k], dy = (int)d.slotY[k] - d.prevY[k];
        if (dx) d.slotDir[k] = dx > 0 ? 1 : -1;
        const float vx = dx * PX_TO_WU_X * ARCADE_HZ, vy = dy * PX_TO_WU_Y * ARCADE_HZ;

        if (c == 0x3D) {
            if (!orbWas) {
                g.orb.family = 1;
                loadPal14(g, 1);
            }
            g.orb.active = true;
            g.orb.x = x;
            g.orb.y = y;
            continue;
        }
        if (c >= 0x0C && c <= 0x17) {
            g.bonus.active = true;
            g.bonus.kind   = (c - 0x0C) / 4 == 0 ? BK_B : (c - 0x0C) / 4 == 1 ? BK_E : BK_S;
            g.bonus.x      = x;
            g.bonus.y      = y;
            continue;
        }

        Enemy e;
        e.x = x;
        e.y = y;
        e.r = 10.0f;
        e.phase = EP_FLY;
        e.vx = vx;
        e.vy = vy;
        e.code = c;
        e.flip = d.slotDir[k] > 0 &&
                 ((c >= 0x29 && c <= 0x2B) || (c >= 0x38 && c <= 0x3B) || (c >= 0x50 && c <= 0x53));
        if (c >= 0x04 && c <= 0x0B) {
            e.kind  = EK_MUMMY;
            e.phase = c == 0x05 ? EP_FALL : EP_WALK;
        } else if (c >= 0x29 && c <= 0x30) {
            e.kind = EK_BIRD;
        } else if (c >= 0x38 && c <= 0x3B) {
            e.kind = EK_CLUB;
        } else if (c >= 0x50 && c <= 0x53) {
            e.kind = EK_HORN;
        } else if (c >= 0x54 && c <= 0x5A) {
            e.kind = EK_ORB;
        } else if (c >= 0x60 && c <= 0x65) {
            e.kind = EK_UFO;
        } else if (c == 0x34 || (c >= 0x67 && c <= 0x6B)) {
            e.kind = EK_SPHERE;
        } else if (c >= 0xBC && c <= 0xBF) {
            e.kind      = EK_MUMMY;
            e.phase     = EP_INIT;
            e.timer     = INIT_ENEMY_TIME * (1.0f - (c - 0xBC + 0.5f) / 4.0f);
        } else if (c == 0x3F) {
            e.kind = d.slotKind[k];
            frozen = true;
        } else {
            continue;
        }
        d.slotKind[k] = e.kind;
        g.enemies.push_back(e);
    }
    // Frozen enemies show as coins; keep the timer clear of the end-of-freeze
    // blinking, which the arcade run already shows by itself.
    if (!frozen && g.freezeTimer > 0.0f && !g.orb.active) loadPal14(g, 0);
    g.freezeTimer = frozen ? FREEZE_TIME : 0.0f;
}

static void finishDemo(Game& g) {
    const int round = g.demo.round;
    stopDemo(g);
    if (round == 2) g.attract.cycle++;
    enterAttract(g, round == 1 ? 2 : 0);
}

static void updateDemo(Game& g, float dt) {
    DemoState& d = g.demo;
    Input in{};
    if (g.startAnim <= 0.0f) {
        d.frameAcc += dt * ARCADE_HZ;
        while (d.frameAcc >= 1.0f) {
            d.frameAcc -= 1.0f;
            const bool pressed = d.pressed;
            demoFrame(d);
            d.pressed = d.pressed || pressed;   // keep an unconsumed press
        }
        in.right    = d.held & 0x01;
        in.left     = d.held & 0x02;
        in.up       = d.held & 0x04;
        in.down     = d.held & 0x08;
        in.jumpHeld = d.held & 0x10;
        in.jumpPressed = d.pressed;
        d.pressed = false;
        snapToTrack(g);
    }
    updatePlaying(g, in, dt);
    if (g.startAnim <= 0.0f && g.state == PLAYING) snapToTrack(g);
    g.score = 0;   // the demo scores nothing on the arcade's HUD

    // Replay the arcade run up to the current frame: its enemies, and whether
    // they catch Jack. Frames count from the start of the run's lead.
    const demoruns::Run& run = demoruns::kRunSets[d.round - 1][d.stream].runs[d.run];
    const int frame = g.startAnim > 0.0f
        ? (int)((run.lead / ARCADE_HZ - g.startAnim) * ARCADE_HZ)
        : run.lead - 1 + d.frame;
    decodeRun(g, frame);
    applyRun(g);
    g.bonus.anim += dt;
    if (run.hit >= 0 && frame >= run.hit && !g.death.active) hitJack(g);

    // The arcade ends the demo as soon as the round is cleared (no victory
    // dance), or once Jack has been caught and the run is over. A clearing run
    // gets a second of slack in case the last pickup lands a frame late.
    const int end = run.hit >= 0 ? run.frames - 1 : run.frames + (int)ARCADE_HZ;
    if (g.state != PLAYING || g.bombsLeft <= 0 || frame >= end) finishDemo(g);
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
    // Level cleared: silence the loops and sound the "clear" jingle.
    audio::stopTag("music");
    audio::stopTag("power");
    audio::play(audio::SND_CLEAR);
    int bonus = specialBonusFor(g.catched);
    if (bonus > 0) {
        g.state        = SPECIALBONUS;
        g.sb.phase     = 0;
        g.sb.timer     = SB_BEGIN_TIME;
        g.sb.remaining = bonus;
        g.sb.catched   = g.catched;
        audio::play(audio::SND_SPECIAL_BONUS, "special_bonus", /*loop=*/true);
    } else {
        g.state      = ROUNDCLEAR;
        g.clearTimer = CLEAR_TIME;
    }
}

// Jack touched an enemy: start the death sequence.
static void hitJack(Game& g) {
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
                        // Arcade: the "touched" tune is cut and the fall sound starts.
                        audio::stopTag("touched");
                        audio::play(audio::SND_DEATH_FALL);
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
                    audio::play(audio::SND_GAMEOVER, "gameover");
                } else {
                    resetPlayer(g, true);
                    restartEnemies(g);
                    g.startAnim = START_TOTAL;
                }
            }
        }
        return;
    }

    // Hold simulation during the "START!" intro. Kick off the in-game music as
    // soon as the "START!" sprite begins sliding toward the centre so it plays
    // under the whole intro rather than only once the hold ends.
    if (g.startAnim > 0.0f) {
        if (audio::tagCount("music") == 0 && audio::tagCount("power") == 0)
            audio::play(musicForLevel(g.level), "music", /*loop=*/true);
        g.startAnim -= dt;
        driveJack(g, dt);   // keep his sprite positioned/idle during the hold
        return;
    }

    if (p.invuln > 0) p.invuln -= dt;

    // Keep the in-game music looping while Jack is alive and the power jingle
    // isn't holding the "power" tag (matches the Lua livegame music guard).
    if (audio::tagCount("music") == 0 && audio::tagCount("power") == 0)
        audio::play(musicForLevel(g.level), "music", /*loop=*/true);

    const bool oldOnGround = p.onGround;

    // Horizontal movement (frozen while crouching before take-off, and for a
    // moment after landing, as on the arcade).
    if (p.landing > 0.0f) p.landing -= dt;
    p.vx = p.crouch > 0.0f || p.landing > 0.0f
         ? 0.0f : (in.right ? MOVE : 0.0f) - (in.left ? MOVE : 0.0f);
    if (in.right && !in.left) p.face = 1;
    else if (in.left && !in.right) p.face = -1;
    p.x += p.vx * dt;
    float clampedX = std::clamp(p.x, BORDER_SOLID_X, LOGW - PW - BORDER_SOLID_X);
    p.x = clampedX;

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
    // Every take-off (jump or walking off a ledge) steps the Power ball to its
    // next colour family, 7 wrapping to 1, and reloads palette 14 (ROM 0x23DB).
    if (g.orb.active && oldOnGround && !p.onGround) {
        g.orb.family = g.orb.family % 7 + 1;
        loadPal14(g, g.orb.family);
    }

    // Footstep loop while walking on the ground (one at a time).
    if (p.onGround && (in.left || in.right) && audio::tagCount("walking") == 0)
        audio::play(audio::SND_WALKING, "walking");

    // Bomb collection.
    for (int i = 0; i < (int)g.bombs.size(); ++i) {
        Bomb& b = g.bombs[i];
        if (b.collected) continue;
        const float jackCx = p.x + PW / 2.0f;
        const float jackCy = p.y + PH - JACK_CENTER_ABOVE_FEET;
        if (std::fabs(b.x - jackCx) < PICK_DX && std::fabs(b.y - jackCy) < PICK_DY) {
            b.collected = true;
            g.bombsLeft--;
            g.explosions.push_back({b.x, b.y, 0.0f});
            audio::play(i == g.litBomb ? audio::SND_TAKE_EXPLODING_BOMB
                                       : audio::SND_TAKE_BOMB);
            const bool wasLit  = (i == g.litBomb);
            const bool relight = (g.litBomb < 0) || wasLit;
            int gain;
            // Arcade: the power counter does not charge while the Power ball is
            // on screen; a lit bomb charges it twice as much as a plain one.
            const float charge = g.orb.active ? 0.0f : (wasLit ? 1.0f : 0.5f);
            g.powerMeter += charge;
            if (wasLit) {
                g.catched++;
                gain = 200;
            } else {
                gain = 100;
            }
            if (relight) g.litBomb = nextLitBomb(g, i + 1);
            gain *= g.multiplier;
            g.score += gain;
            g.popups.push_back({b.x, b.y - 6, 0.0f, gain});
            if (!g.orb.active && g.powerMeter >= POWER_NEEDED && !g.demo.active) {
                g.powerMeter   = 0.0f;   // arcade resets the counter when the ball appears
                g.orb.active   = true;
                g.orb.x        = LOGW / 2.0f;
                g.orb.y        = LOGH / 2.0f - 16.0f;
                g.orb.vx       = ORB_SPEED * 0.5f;
                g.orb.vy       = ORB_SPEED * 0.8660254f;
                g.orb.family   = 1;
                loadPal14(g, 1);
                // Orb on screen: duck the music and loop the "appearing" tone.
                audio::pauseTag("music");
                audio::play(audio::SND_POWER_APPEARING, "power", /*loop=*/true);
            }
        }
    }

    // In the demo the enemies, the Power orb and the bonus coin are replayed from
    // an arcade run (updateDemo) instead of simulated.
    const bool replay = g.demo.active;

    // Power orb movement: bounces around the play area and off platforms.
    if (g.orb.active && !replay) {
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
    if (g.orb.active && !replay) {
        if (g.orb.x > p.x - ORB_R && g.orb.x < p.x + PW + ORB_R &&
            g.orb.y > p.y - ORB_R && g.orb.y < p.y + PH + ORB_R) {
            g.orb.active  = false;
            g.freezeTimer = FREEZE_TIME;
            g.killCount   = 0;
            // The power tune lasts exactly the freeze; the music resumes when the
            // freeze ends (below), as on the arcade.
            audio::stopTag("power");
            audio::play(audio::SND_GOT_POWER, "power");
            // Points by colour family; palette 14 keeps that family's colours
            // for the whole freeze.
            int idx  = std::clamp(g.orb.family, 1, 7) - 1;
            int gain = POWER_POINTS[idx] * g.multiplier;
            g.orb.family = 0;
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
    if (g.bonus.active && !replay) {
        g.bonus.anim += dt;
        g.bonus.x    += g.bonus.vx * dt;
        const float lo = g.bonus.platX + BONUS_W * 0.5f;
        const float hi = g.bonus.platX + g.bonus.platW - BONUS_W * 0.5f;
        const float bounceGain = g.bonus.kind == BK_S ? 0.5f : 1.0f;  // S bonus quieter
        if (g.bonus.x < lo) { g.bonus.x = lo; g.bonus.vx =  std::fabs(g.bonus.vx); bounceSound(bounceGain); }
        if (g.bonus.x > hi) { g.bonus.x = hi; g.bonus.vx = -std::fabs(g.bonus.vx); bounceSound(bounceGain); }

        if (g.bonus.x > p.x - BONUS_W * 0.5f && g.bonus.x < p.x + PW + BONUS_W * 0.5f &&
            g.bonus.y > p.y - BONUS_H * 0.5f && g.bonus.y < p.y + PH + BONUS_H * 0.5f) {
            g.bonus.active = false;
            audio::play(audio::SND_TAKE_BONUS);
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
                    audio::stopTag("music");
                    audio::stopTag("power");
                    audio::play(audio::SND_GOT_SPECIAL, "special_coin");
                    g.state       = LUCKY;   // "YOU ARE LUCKY / ONE MORE PLAY" screen
                    g.lucky.phase = 0;
                    g.lucky.timer = 0.0f;
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
    if (frozen && !replay) {
        g.freezeTimer -= dt;
        if (g.freezeTimer <= 0.0f && !g.orb.active) {
            loadPal14(g, 0);
            audio::stopTag("power");
            resumeMusic();
        }
    }

    // Mummy drop-in schedule.
    if (!frozen && !replay) {
        const leveldata::EnemyDef& L = levelDef(g);
        g.mummyTimer += dt;
        if (g.mummyTimer >= MUMMY_SPAWN_DELAY && g.mummiesSpawned < L.nmummies) {
            g.mummyTimer = 0.0f;
            g.mummiesSpawned++;
            spawnMummy(g);
        }
    }

    // Enemy update and player-enemy collision.
    float pcx = p.x + PW / 2, pcy = p.y + PH / 2;
    for (auto it = g.enemies.begin(); it != g.enemies.end() && !replay;) {
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
            // As on the arcade (sound command 0x20), the title tune sounds once
            // each time the banner comes up, not while the table is shown.
            g.hs.showTable = g.attract.step == 0;
            if (g.attract.step != 2) {
                g.hs.titleTune = false;
            } else if (!g.hs.titleTune) {
                g.hs.titleTune = true;
                audio::stopTag("title");
                audio::play(audio::SND_TITLE, "title");
            }
            // Attract cycle: table → demo round 1 → banner → demo round 2.
            g.attract.timer += dt;
            if (g.attract.step == 0 && g.attract.timer >= ATTRACT_TABLE_TIME)
                startDemo(g, 1);
            else if (g.attract.step == 2 && g.attract.timer >= ATTRACT_BANNER_TIME)
                startDemo(g, 2);
            if (in.jumpPressed) startGame(g);
            break;
        case OPTIONS:
            break;
        case PLAYING:
            if (g.demo.active) updateDemo(g, dt);
            else               updatePlaying(g, in, dt);
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
                    audio::stopTag("special_bonus");
                    g.level++;
                    startRound(g);
                    g.state = PLAYING;
                }
            }
            break;
        case LUCKY:
            g.lucky.timer += dt;
            if (g.lucky.phase == 0) {
                // Text glides in from the corners; once it settles, hold.
                if (g.lucky.timer >= LUCKY_SLIDE_TIME) g.lucky.phase = 1;
            } else if (g.lucky.timer >= LUCKY_HOLD_TIME &&
                       audio::tagCount("special_coin") == 0) {
                // Stay until the "special coin" jingle finishes (the floor keeps
                // a muted build from flashing the screen away instantly).
                g.level++;
                startRound(g);
                g.state = PLAYING;
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
                if (g.go.timer <= 0.0f) {
                    audio::stopTag("gameover");
                    if (hiScoreQualifies(g)) {
                        // Made the table: type a name inline at its rank row.
                        g.hs.entry[0] = '\0';
                        g.hs.len      = 0;
                        g.hs.rank     = 0;
                        while (g.hs.rank < HISCORE_COUNT &&
                               g.score <= g.hi[g.hs.rank].score)
                            g.hs.rank++;
                        // The arcade plays tune 0x29 for a new #1 score and
                        // 0x2A for the other ranks (ROM 0x4865).
                        audio::play(g.hs.rank == 0 ? audio::SND_NAME_ENTRY
                                                   : audio::SND_NAME_ENTRY_2,
                                    "nameentry", /*loop=*/true);
                        g.state = NAMEENTRY;
                    } else {
                        // Didn't qualify: back to the title, table shown first.
                        enterAttract(g, 0);
                    }
                }
            }
            break;
        }
        case NAMEENTRY:
            break;   // passive; keyboard handled in the event loop (main.cpp)
    }
}
