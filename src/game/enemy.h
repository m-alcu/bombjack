#pragma once
// Enemies: the opening birds, the mummies that drop in and transform into
// chasers, and the arcade animation each one shows.
#include <cstdint>
#include "core/constants.h"

struct Game;

enum EKind  { EK_BIRD, EK_MUMMY, EK_SPHERE, EK_ORB, EK_HORN, EK_CLUB, EK_UFO };
enum EPhase { EP_INIT, EP_FLY, EP_APPEAR, EP_WALK, EP_FALL, EP_DISAPPEAR };

struct Enemy {
    float x, y, vx, vy, r;
    int   kind    = EK_BIRD;
    int   phase   = EP_FLY;
    float timer   = 0.0f;
    int   bounces = 0;
    int   becomes = EK_SPHERE;
    int   dirX = 0, dirY = 0;
    int   tick = 0;
    float tgtX = 0.0f, tgtY = 0.0f;
    // Arcade sprite shown (ROM sprite code + horizontal flip) and the state
    // the arcade keeps per object to step it (see animateEnemy).
    int   code = 0;
    bool  flip = false;
    int   animTimer = 0, animIdx = 0;
    int   animDir = -1;                 // mummy: 0 right / 1 left; bird: heading
    const uint8_t* turn = nullptr;      // bird heading-change poses still to show
    float animAcc = 0.0f;               // arcade frames owed to the animation
};

// Mummy lifecycle
constexpr float MUMMY_HALF_H      = 14.0f;
constexpr float MUMMY_DROP_GAP    = 8.0f;
// Flash-in (0xBC-0xBF, 6 frames each), also played when a mummy transforms,
// then 30 frames standing before it walks (main ROM 0x321D / 0x3266).
constexpr float INIT_ENEMY_TIME   = 24.0f / ARCADE_HZ;
constexpr float MUMMY_APPEAR_TIME = 30.0f / ARCADE_HZ;
constexpr float MUMMY_SPAWN_DELAY = 3.6f;
constexpr float MUMMY_WALK_SPEED  = 70.0f;
constexpr float FLY_SPEED         = 120.0f;

// Birds
constexpr int   BIRD_DECIDE_FRAMES = 250;
constexpr float BIRD_SPAWN_L = 60.0f;
constexpr float BIRD_SPAWN_R = LOGW - 60.0f;
constexpr float BIRD_SPAWN_T = 70.0f;
constexpr float BIRD_SPAWN_B = LOGH - 90.0f;

// Points for each frozen enemy collected during one freeze.
constexpr int KILL_POINTS[] = {100, 200, 300, 500, 800, 1200, 2000};

// Replace the enemies with the round's opening set of birds.
void spawnBirds(Game& g);

// Drop a mummy in at one of the layout's spawn points, away from Jack.
void spawnMummy(Game& g);

// Run the mummy drop-in schedule, move and animate every enemy (all held
// still while `frozen`), and resolve their contact with Jack: collected while
// frozen, otherwise he is caught. Returns true if Jack was caught.
bool updateEnemies(Game& g, float dt, bool frozen);

// Arcade animation (enemy_anim.cpp). Set an enemy's first pose and reset its
// animation state (spawn, mummy appearing, transform), and step it by the
// arcade frames elapsed in dt.
void startEnemyAnim(Enemy& e);
void animateEnemy(Enemy& e, float dt);
