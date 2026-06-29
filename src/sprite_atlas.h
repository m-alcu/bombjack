#pragma once
#include "constants.h"

// ---------------------------------------------------------------------------
// Atlas layout: where every sprite lives inside sprites_full_png.
//
// This header is *data only* — the pixel coordinates of each sprite in the
// shared atlas, kept separate from the slicing/processing code in sprites.cpp
// and renderer.cpp. Tweak a sprite's position here; the build code stays put.
//
//   AtlasRect  — a single source rectangle {x, y, w, h}.
//   AtlasStrip — N evenly-spaced frames; frame i is at {x0 + i*dx, y, w, h}.
// ---------------------------------------------------------------------------

struct AtlasRect  { int x, y, w, h; };
struct AtlasStrip { int x0, y, w, h, dx; };

namespace atlas {

// Jack living poses (JF_COUNT), all SIZE_16PX x JACK_FH. Order matches JackFrame.
inline constexpr AtlasRect JACK[] = {
    {  4, 4, SIZE_16PX, JACK_FH},
    { 24, 4, SIZE_16PX, JACK_FH}, { 44, 4, SIZE_16PX, JACK_FH},
    { 64, 4, SIZE_16PX, JACK_FH}, { 84, 4, SIZE_16PX, JACK_FH},
    {104, 4, SIZE_16PX, JACK_FH}, {124, 4, SIZE_16PX, JACK_FH},
    {144, 4, SIZE_16PX, JACK_FH}, {164, 4, SIZE_16PX, JACK_FH},
    {224, 4, SIZE_16PX, JACK_FH}, {264, 4, SIZE_16PX, JACK_FH},
    {304, 4, SIZE_16PX, JACK_FH},
    {204, 4, SIZE_16PX, JACK_FH}, {284, 4, SIZE_16PX, JACK_FH},
    {324, 4, SIZE_16PX, JACK_FH},
};

// Jack death / win variable-size frames.
inline constexpr AtlasRect JACK_DANCE[] = {
    {249, 31, 16, 17}, {269, 31, 16, 17}, {64, 34, 15, 14},
};
inline constexpr AtlasRect JACK_PLF[] = {
    {44, 32, 15, 15}, {64, 32, 15, 16}, {84, 32, 15, 16}, {124, 32, 15, 15},
};
inline constexpr AtlasRect JACK_DEAD[] = {
    {145, 27, 16, 18}, {166, 29, 13, 16}, {184, 24, 17, 24}, {205, 27, 23, 21},
};
inline constexpr AtlasRect JACK_WIN[] = {
    {4, 32, 16, 16}, {24, 32, 16, 16}, {286, 32, 16, 16}, {44, 32, 16, 16},
};

// HUD GAME OVER banner (white mask).
inline constexpr AtlasRect GAMEOVER = {2, 155, GAMEOVER_W, GAMEOVER_H};

// Boxed multiplier indicators x / 1..5 (16x16 cells).
inline constexpr AtlasStrip MULT = {4, 192, SIZE_16PX, SIZE_16PX, 20};

// Frozen-enemy coin spin (7 frames).
inline constexpr AtlasStrip COIN = {307, 115, 14, 14, 14};

// Bonus B / E / S spin (4 frames each, shared y/h, irregular x and width).
inline constexpr AtlasRect BONUS_B[] = {
    {78, 114, 13, 13}, {98, 114, 12, 13}, {117, 114, 7, 13}, {130, 114, 12, 13},
};
inline constexpr AtlasRect BONUS_E[] = {
    {150, 114, 13, 13}, {170, 114, 12, 13}, {189, 114, 7, 13}, {202, 114, 12, 13},
};
inline constexpr AtlasRect BONUS_S[] = {
    {222, 114, 13, 13}, {242, 114, 12, 13}, {261, 114, 7, 13}, {274, 114, 12, 13},
};

// Transformed chaser frames (2 each).
inline constexpr AtlasRect SPHERE1 = {  5, 73, 14, 14};
inline constexpr AtlasRect SPHERE2 = { 85, 73, 14, 14};
inline constexpr AtlasRect ORB1    = {185, 74, 14, 13};
inline constexpr AtlasRect ORB2    = {245, 74, 14, 13};
inline constexpr AtlasRect CLUB1   = {305, 92, 15, 16};
inline constexpr AtlasRect CLUB2   = {345, 92, 15, 16};
inline constexpr AtlasRect UFO1    = {  4, 98, 16, 10};
inline constexpr AtlasRect UFO2    = { 64, 98, 16, 10};

// Horn frames (centred into a 16x16 cell at draw time).
inline constexpr AtlasRect HORN1 = {280, 54, 15, 14};
inline constexpr AtlasRect HORN2 = {318, 52, 11, 16};

// Bonus-collect flash (6 frames 32x32).
inline constexpr AtlasStrip BONUS_TAKEN = {266, 327, 32, 32, 33};

// Bomb frames: 0=resting, 1-6=lit fuse.
inline constexpr AtlasStrip BOMB = {46, 136, 12, 16, 20};

// Bomb-clear explosion (3 growing frames).
inline constexpr AtlasRect EXPL[] = {
    {24, 120, 8, 8}, {38, 114, 12, 12}, {56, 112, 16, 16},
};

// Bird flap frames (BF_COUNT: 3 headings x 3 frames, centred into 16x16).
inline constexpr AtlasRect BIRD[] = {
    {140, 53, 16, 16}, {160, 54, 16, 13}, {180, 55, 16, 11},
    {352, 53, 16, 16}, {373, 54, 16, 13}, {393, 54, 16, 11},
    {201, 53, 15, 15}, {221, 55, 15, 12}, {241, 55, 15, 11},
};

// Mummy frames (MF_COUNT, centred into 16x16).
inline constexpr AtlasRect MUMMY[] = {
    {  6, 53, 12, 15},
    { 45, 53, 11, 15}, { 61, 53, 11, 15}, { 76, 53, 13, 15},
    { 92, 53, 11, 15}, {108, 53, 11, 15}, {123, 53, 13, 15},
    { 25, 53, 14, 16},
};

// Power orb base cell (recoloured per family x 4 phases at build time).
inline constexpr AtlasRect ORB = {292, 116, 12, 12};

// Spawn flash (4 frames, white masks, centred into INIT_FW x INIT_FH).
inline constexpr AtlasRect INIT[] = {
    {40, 158, 18, 29}, {65, 157, 29, 31}, {101, 158, 29, 29}, {138, 159, 28, 28},
};

// "START!" halves (each 56x12, split into box + text layers).
inline constexpr AtlasRect START[] = {
    {240, 260, 56, 12}, {300, 260, 56, 12},
};

// Coin-pickup sparkle (4 frames, white masks).
inline constexpr AtlasRect PICK[] = {
    {175, 159, 26, 26}, {209, 159, 30, 28}, {244, 159, 32, 29}, {282, 156, 32, 32},
};

// Power-bar segments (BAR_STEPS each side, shared y/h, irregular x and width).
inline constexpr AtlasRect BAR_RIGHT[] = {
    {124, 200, 8, 8}, {136, 200, 8, 8}, {148, 200, 8, 8}, {160, 200, 4, 8},
    {168, 200, 4, 8}, {176, 200, 8, 8}, {188, 200, 8, 8}, {200, 200, 4, 8},
    {208, 200, 4, 8}, {216, 200, 8, 8}, {228, 200, 8, 8},
};
inline constexpr AtlasRect BAR_LEFT[] = {
    {240, 200, 4, 8}, {248, 200, 8, 8}, {260, 200, 8, 8}, {272, 200, 4, 8},
    {280, 200, 4, 8}, {288, 200, 8, 8}, {300, 200, 8, 8}, {312, 200, 4, 8},
    {320, 200, 4, 8}, {328, 200, 8, 8}, {340, 200, 8, 8},
};

// Font glyphs: two rows of FONT_W x FONT_H cells, plus punctuation.
inline constexpr int FONT_LETTER_Y = 212;   // A-Z
inline constexpr int FONT_DIGIT_Y  = 224;    // 0-9 and punctuation
inline constexpr int FONT_X0       = 5;
inline constexpr int FONT_DX       = 12;

} // namespace atlas
