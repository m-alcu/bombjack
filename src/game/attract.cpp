#include "game/attract.h"
#include "game/game.h"
#include "data/demo_data.h"
#include "data/demo_runs.h"
#include "platform/audio.h"

static_assert(demoruns::SLOT_COUNT == sizeof(DemoState::slotCode),
              "DemoState slot arrays must match demoruns::kSlots");

// Arcade sprite position (Jack's coordinates, see snapToTrack) → world centre.
static float demoWorldX(int x) { return LOGW / 2.0f + (x - 120.0f) * PX_TO_WU_X; }
static float demoWorldY(int y) { return LOGH / 2.0f + (y - 121.0f) * PX_TO_WU_Y; }

void enterAttract(Game& g, int step) {
    g.state         = TITLE;
    g.attract.step  = step;
    g.attract.timer = 0.0f;
    g.attract.showTable = step == 0;
}

void updateTitle(Game& g, const Input& in, float dt) {
    // As on the arcade (sound command 0x20), the title tune sounds once each
    // time the banner comes up, not while the table is shown.
    g.attract.showTable = g.attract.step == 0;
    if (g.attract.step != 2) {
        g.attract.titleTune = false;
    } else if (!g.attract.titleTune) {
        g.attract.titleTune = true;
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
}

void startDemo(Game& g, int round) {
    DemoState& d = g.demo;
    d = {};
    d.active     = true;
    d.round      = round;
    d.stream     = g.attract.cycle & 1;
    d.savedScore = g.score;
    d.savedLives = g.lives;
    d.savedRound = g.round;
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
    g.round      = round;
    g.powerMeter = 0.0f;
    const bool alwaysS = g.opt.alwaysS;   // the OPTIONS S coin is for real games only
    g.opt.alwaysS = false;
    startRound(g);
    g.opt.alwaysS = alwaysS;
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
    g.round      = d.savedRound;
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

// Show the decoded arcade sprites as the clone's enemies, Power ball and bonus
// coin. Enemies show the recorded ROM sprite code itself (see
// atlas::ENEMY_CELLS); the runs keep no attributes, so the flip the arcade
// gives right-moving birds, clubs and horns comes from the slot's motion.
// Other codes: 0xBC-0xBF flash in, 0x3F frozen enemy (coin), 0x3D Power ball,
// 0x0C-0x17 B/E/S coin. Score numbers, sparkles and the like are left out
// (the clone draws its own).
static void applyRun(Game& g) {
    DemoState& d = g.demo;

    const bool ballWas = g.ball.active;
    g.enemies.clear();
    g.ball.active  = false;
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
            if (!ballWas) {
                g.ball.family = 1;
                loadPal14(g, 1);
            }
            g.ball.active = true;
            g.ball.x = x;
            g.ball.y = y;
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
    if (!frozen && g.freezeTimer > 0.0f && !g.ball.active) loadPal14(g, 0);
    g.freezeTimer = frozen ? FREEZE_TIME : 0.0f;
}

static void finishDemo(Game& g) {
    const int round = g.demo.round;
    stopDemo(g);
    if (round == 2) g.attract.cycle++;
    enterAttract(g, round == 1 ? 2 : 0);
}

void updateDemo(Game& g, float dt) {
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
