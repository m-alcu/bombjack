#pragma once
// The Power ball, the enemy freeze it triggers, and palette 14, the rotating
// palette the ball shares with the HUD's multiplier box and power bar and with
// Jack while the freeze lasts.
#include "core/constants.h"

struct Game;

struct PowerBall {
    bool  active = false;
    float x = 0, y = 0, vx = 0, vy = 0;
    int   family = 0;   // arcade 0x845F: 1-7 while the ball is up (POWER_POINTS index + 1)
};

// Palette 14: the table it rotates through (0 rainbow, 1-7 the ball's colour
// family) and when that table was loaded, which restarts the rotation.
struct Palette14 {
    int   table    = 0;
    float loadedAt = 0;
};

constexpr float FREEZE_TIME  = 300.0f / ARCADE_HZ;   // 300 arcade frames (measured in MAME)
constexpr float POWER_NEEDED = 10.0f;   // arcade counter 20 (+1 plain bomb, +2 lit)
constexpr float BALL_SPEED   = 60.0f;
constexpr float BALL_R       = 10.0f;
constexpr int   POWER_POINTS[] = {100, 200, 300, 500, 800, 1200, 2000};   // by family (ROM 0x2F7E)

// Load a table into palette 14 (0 rainbow, 1-7 a colour family); its rotation
// restarts from the table's first entry (ROM 0x24A9).
void loadPal14(Game& g, int table);

// A bomb was taken: charge the power meter (twice as much for the lit bomb,
// nothing while the ball is up) and release the ball once it is full.
void chargePower(Game& g, bool litBomb);

// Jack took off: the ball steps to its next colour family, 7 wrapping to 1
// (ROM 0x23DB).
void cycleBallFamily(Game& g);

// Bounce the ball around the play area; when Jack takes it, score its family's
// points and freeze the enemies.
void updatePowerBall(Game& g, float dt);

// Count the freeze down; at its end the rainbow and the music come back.
void updateFreeze(Game& g, float dt);
