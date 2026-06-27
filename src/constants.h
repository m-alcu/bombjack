#pragma once

// World coordinate space (game logic + drawing)
constexpr int LOGW = 512;
constexpr int LOGH = 448;

// Screen layout: 16px top HUD, 224x224 play area, 16px bottom HUD → 224x256
constexpr int GAME_W   = 224;
constexpr int GAME_H   = 224;
constexpr int HUD_H    = 16;
constexpr int SCREEN_W = GAME_W;
constexpr int SCREEN_H = HUD_H + GAME_H + HUD_H;
constexpr int WIN_SCALE = 4;
// 3:4 window matches the TATE-mounted arcade cabinet (non-square pixels)
constexpr int WIN_H = SCREEN_H * WIN_SCALE;
constexpr int WIN_W = WIN_H * 3 / 4;

// Horizontal stretch correction: multiplied into sprite widths so they appear
// square on the final 768x1024 window (1.5 vs 2.0 win-px per world unit).
constexpr float SPRITE_AR = 4.0f / 3.0f;

// Player physics
constexpr float MOVE           = 170.0f;
constexpr float JUMP_VEL       = -630.0f;
constexpr float JUMP_HOLD_GRAV = 500.0f;
constexpr float FLUTTER        = 165.0f;
constexpr float FLUTTER_MIN    = -200.0f;
constexpr float GRAVITY        = 980.0f;
constexpr float GLIDE          = 150.0f;
constexpr float MAXFALL        = 520.0f;

constexpr float PW = 18.0f;
constexpr float PH = 24.0f;
constexpr float BORDER_SOLID_X = 8.0f * (float)LOGW / (float)GAME_W;
constexpr float BORDER_SOLID_Y = 8.0f * (float)LOGH / (float)GAME_H;
constexpr float BOMB_HALF_W = 6.0f * (float)LOGW / (float)GAME_W;
constexpr float BOMB_HALF_H = 8.0f * (float)LOGH / (float)GAME_H;
constexpr float FLOOR_TOP = 432.0f;

// Timing / game rules
constexpr float INVULN_TIME = 2.0f;
constexpr float CLEAR_TIME  = 3.2f;
constexpr float SB_BEGIN_TIME = 2.0f;
constexpr float SB_END_TIME   = 1.5f;
constexpr float SB_SCORE_TIME = 0.1f;
constexpr float FREEZE_TIME   = 5.0f;
constexpr float POWER_NEEDED  = 8.0f;
constexpr float ORB_SPEED     = 60.0f;
constexpr float ORB_R         = 10.0f;
constexpr int   BONUS_LIMIT   = 5000;
constexpr int   BONUS_POINTS  = 1000;
constexpr int   MAX_MULT      = 5;
constexpr float BONUS_W       = 26.0f;
constexpr float BONUS_H       = 26.0f;
constexpr float BONUS_SPEED   = 45.0f;
constexpr int   BONUS_S_CHANCE = 6;
constexpr int   BONUS_E_EVERY  = 8;
constexpr int   BONUS_S_POINTS = 5000;
constexpr float BONUSTAKEN_FRAME = 0.05f;
constexpr float BONUSTAKEN_SCALE = 1.8f;
constexpr float START_SPEED  = 2.0f;
constexpr float START_SLIDE  = 0.55f * START_SPEED;
constexpr float START_HOLD   = 0.65f * START_SPEED;
constexpr float START_BLACK  = 1.0f  * START_SPEED;
constexpr float START_TOTAL  = START_SLIDE + START_HOLD;
constexpr float DEATH_DANCE_TOTAL = 0.5f;
constexpr int   DEATH_DANCE_LOOPS = 2;
constexpr float DEATH_PLF_TOTAL  = 1.0f;
constexpr float DEATH_DEAD_TOTAL = 1.0f;
constexpr float DEATH_WAIT_TIME  = 3.0f;
constexpr int KILL_POINTS[]  = {100, 200, 300, 500, 800, 1200, 2000};
constexpr int POWER_POINTS[] = {100, 200, 300, 500, 800, 1000, 2000};

// Mummy lifecycle
constexpr float MUMMY_HALF_H        = 14.0f;
constexpr float MUMMY_SPAWN_Y       = 90.0f;
constexpr float MUMMY_DROP_GAP      = 8.0f;
constexpr float INIT_ENEMY_TIME     = 0.6f;
constexpr float MUMMY_APPEAR_TIME   = 1.1f;
constexpr float MUMMY_DISAPPEAR_TIME= 0.5f;
constexpr float MUMMY_SPAWN_DELAY   = 3.6f;
constexpr float MUMMY_WALK_SPEED    = 70.0f;
constexpr float FLY_SPEED           = 120.0f;

// Effect frame timing
constexpr float EXPL_FRAME    = 0.06f;
constexpr float PICKCOIN_FRAME = 0.06f;

// Common sprite cell side (most atlas sprites are 16×16 or 16×N).
constexpr int SIZE_16PX = 16;

// Font
constexpr int FONT_W = 7, FONT_H = 7, FONT_ADV = 8;

// Jack sprite sizes and animation
constexpr int   JACK_FH = 15;
constexpr float JACK_WALK_FRAME = 0.125f;

// Round-clear victory dance
constexpr int   VICTORY_STEPS = 16;
constexpr float VICTORY_FRAME = CLEAR_TIME / VICTORY_STEPS;
// JW_NORMAL=0, JW_LEFT=1, JW_RIGHT=2, JW_UP=3
constexpr int VICTORY_SEQ[VICTORY_STEPS] = {
    0, 1, 0, 2, 0, 3, 0, 3,
    0, 1, 0, 2, 0, 3, 0, 3,
};

// Bird enemy
constexpr int   BIRD_DECIDE_FRAMES = 250;
constexpr float BIRD_FLAP_FRAME = 0.1f;
constexpr float BIRD_PULSE_STEP = 0.06f;
constexpr float BIRD_SPAWN_L = 60.0f;
constexpr float BIRD_SPAWN_R = LOGW - 60.0f;
constexpr float BIRD_SPAWN_T = 70.0f;
constexpr float BIRD_SPAWN_B = LOGH - 90.0f;

// Mummy sprite size
constexpr float MUMMY_WALK_FRAME = 0.1f;

// Spawn flash cell size
constexpr int INIT_FW = 33, INIT_FH = 32;

// Game over sprite and animation
constexpr int   GAMEOVER_W     = 34,   GAMEOVER_H    = 28;
constexpr float GAMEOVER_SLIDE = 110.0f;
constexpr float GAMEOVER_HOLD  = 3.0f;

// Title banner
constexpr int   BANNER_W = 192, BANNER_H = 80;
constexpr int   BANNER_PHASES = 3;
constexpr float BANNER_ROT = 0.05f;

// HUD life icon


// Platform frame tab geometry
constexpr int PLAT_TAB_EXT   = 3;
constexpr int PLAT_TAB_NOTCH = 2;

// Classic 5-backdrop strip and optional 4x4 grid sets
constexpr int BG_W = 224, BG_H = 224;
constexpr int GRID_BG_COUNT = 7;

// OPTIONS screen rows and value bounds
constexpr int OPT_COUNT      = 3;
constexpr int OPT_MAX_LIVES  = 9;
constexpr int OPT_IMG_STYLES = GRID_BG_COUNT + 2;  // CLASSIC + 7 grids + NO
