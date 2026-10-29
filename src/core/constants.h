#pragma once
// Coordinate spaces and arcade timing shared by the simulation and the
// renderer. Constants used by a single module live in that module's header.

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

// The arcade ticks at 59.19 Hz in 8.8 fixed-point screen pixels; these convert
// px/frame to world units/s (x: 512/224 per px, y: 2 per px).
constexpr float ARCADE_HZ  = 59.185606f;
constexpr float PX_TO_WU_X = (float)LOGW / (float)GAME_W;
constexpr float PX_TO_WU_Y = (float)LOGH / (float)GAME_H;

// Gravity and fall-speed cap (Jack and the mummies), measured in MAME.
constexpr float GRAVITY = (10.0f / 256.0f) * ARCADE_HZ * ARCADE_HZ * PX_TO_WU_Y;
constexpr float MAXFALL = (810.0f / 256.0f) * ARCADE_HZ * PX_TO_WU_Y;

// Jack's collision box.
constexpr float PW = 18.0f;
constexpr float PH = 32.0f;   // 16 px tall like the arcade sprite (head bumps match)

// Play-area walls and floor.
constexpr float BORDER_SOLID_X = 8.0f * (float)LOGW / (float)GAME_W;
constexpr float BORDER_SOLID_Y = 8.0f * (float)LOGH / (float)GAME_H;
constexpr float FLOOR_TOP = 432.0f;

// Background picture sets beyond the classic one (an OPTIONS choice).
constexpr int GRID_BG_COUNT = 7;
