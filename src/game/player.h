#pragma once
// Jack: controls, movement physics and the death sequence.
#include "core/constants.h"

struct Game;

// Jack's controls for one simulation step.
struct Input { bool left, right, up, down, jumpHeld, jumpPressed; };

// Living poses. The simulation picks one each step; the renderer maps it to
// frames (walking is a 4-frame loop, the others a single frame).
enum JackPose {
    JP_IDLE, JP_WALK_L, JP_WALK_R,
    JP_FLY,  JP_FLY_L,  JP_FLY_R,
    JP_FALL, JP_FALL_L, JP_FALL_R,
};

struct Player {
    float x, y, vx, vy;
    bool  onGround;
    float invuln;
    int   face = 1;
    float crouch = 0.0f;    // >0 while crouching before a jump take-off
    float landing = 0.0f;   // >0 while held in place just after landing
    int   pose = JP_IDLE;   // living pose and how long it has been shown
    float poseTime = 0.0f;
};

enum DeathPhase { DP_NONE, DP_DANCING, DP_FALLING, DP_DEAD, DP_WAIT };

struct DeathState {
    bool  active = false;
    int   phase  = DP_NONE;
    int   frame  = 0, loops = 0;
    float anim   = 0.0f, timer = 0.0f;
};

// Player physics, measured from the arcade original running in MAME (8.8
// fixed-point screen pixels per frame, converted to world units/s).
constexpr float MOVE        = 1.5f * ARCADE_HZ * PX_TO_WU_X;               // 0x0180/frame
constexpr float JUMP_VEL    = -(810.0f / 256.0f) * ARCADE_HZ * PX_TO_WU_Y;  // launch speed
constexpr float JUMP_CROUCH = 8.0f / ARCADE_HZ;   // crouch before take-off
constexpr float LAND_HOLD   = 8.0f / ARCADE_HZ;   // no walking just after landing
constexpr float GRAV_UP     = 0.5f;   // Up held in the air: half gravity
constexpr float GRAV_DOWN   = 2.0f;   // Down held in the air: double gravity
constexpr float INVULN_TIME = 2.0f;

// Death sequence: dance, fall to the floor, lie dead, wait.
constexpr float DEATH_DANCE_TOTAL = 0.5f;
constexpr int   DEATH_DANCE_LOOPS = 2;
constexpr float DEATH_PLF_TOTAL   = 1.0f;
constexpr float DEATH_DEAD_TOTAL  = 1.0f;
constexpr float DEATH_WAIT_TIME   = 3.0f;

// Place Jack at his starting position.
void resetPlayer(Game& g, bool invuln);

// Walk, jump, fall and land against the platforms. Every take-off scores and
// steps the Power ball's colour family.
void updateJack(Game& g, const Input& in, float dt);

// Pick Jack's living pose from his motion and advance its clock.
void updateJackPose(Game& g, float dt);

// An enemy touched Jack: start the death sequence.
void hitJack(Game& g);

// Run the death sequence; true once it is over and a life should be lost.
bool updateDeath(Game& g, float dt);
